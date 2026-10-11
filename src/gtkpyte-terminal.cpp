// src/gtkpyte-terminal.cpp
//
// GtkpyteTerminal implementation. pyte owns what the screen contains; this
// file owns how it looks and what the keyboard sends. Nothing here parses an
// escape sequence, and nothing in pyte knows about GTK.

#include "gtkpyte-terminal.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "keymap.h"
#include "palette.h"
#include "pyte/screen.h"
#include "pyte/stream.h"

namespace {

constexpr int kDefaultScrollback = 5000;
constexpr const char *kDefaultFont = "Monospace 11";

// DEC private modes this widget acts on. Everything else is observed only.
constexpr int kModeCursorKeys = 1;       // DECCKM
constexpr int kModeCursorVisible = 25;   // DECTCEM
constexpr int kModeBracketedPaste = 2004;

// Mouse-wheel notches scroll this many lines; touchpads scroll by pixels.
constexpr double kLinesPerWheelStep = 3.0;

enum { SIGNAL_COMMIT, SIGNAL_RESIZED, N_SIGNALS };
guint g_signals[N_SIGNALS];

// Everything with a C++ constructor lives here. GObject allocates instance
// structs with g_malloc0 and never runs constructors, so C++ members cannot sit
// in the instance struct directly; one owned pointer keeps that problem in one
// place (new in init, delete in finalize).
struct State {
    pyte::Screen screen{80, 24, kDefaultScrollback};
    pyte::Stream stream{screen};
    gtkpyte::Palette palette;
    gtkpyte::KeyModes key_modes;

    PangoFontDescription *font = nullptr;
    int cell_w = 8;
    int cell_h = 16;
    // False when glyph advances in the primary font are not all the same
    // whole number of pixels. Runs then cannot be laid out as one string and
    // still land on the grid, so every cell is positioned on its own.
    bool uniform_advance = true;

    int cols = 80;
    int rows = 24;

    bool cursor_visible = true;
    bool bracketed_paste = false;
    bool focused = false;

    double scroll_accum = 0.0;

    ~State() {
        if (font != nullptr) {
            pango_font_description_free(font);
        }
    }
};

}  // namespace

struct _GtkpyteTerminal {
    GtkWidget parent_instance;
    State *st;
};

G_DEFINE_FINAL_TYPE(GtkpyteTerminal, gtkpyte_terminal, GTK_TYPE_WIDGET)

namespace {

// --- outbound bytes ---------------------------------------------------------

void emit_commit(GtkpyteTerminal *self, const std::string &bytes) {
    if (bytes.empty()) {
        return;
    }
    GBytes *data = g_bytes_new(bytes.data(), bytes.size());
    g_signal_emit(self, g_signals[SIGNAL_COMMIT], 0, data);
    g_bytes_unref(data);
}

// --- font metrics -----------------------------------------------------------

PangoLayout *make_layout(GtkpyteTerminal *self) {
    PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), nullptr);
    pango_layout_set_font_description(layout, self->st->font);
    return layout;
}

// Cell size comes from laying out real text with the widget's own Pango
// context, rather than from font metrics: that way the number reflects the
// hinting and glyph-position rounding GTK will actually render with.
void measure_cells(GtkpyteTerminal *self) {
    State *st = self->st;
    PangoLayout *layout = make_layout(self);

    int w_wide = 0;
    int h = 0;
    pango_layout_set_text(layout, "MMMMMMMMMM", -1);
    pango_layout_get_pixel_size(layout, &w_wide, &h);

    int w_narrow = 0;
    pango_layout_set_text(layout, "iiiiiiiiii", -1);
    pango_layout_get_pixel_size(layout, &w_narrow, nullptr);

    int w_mixed = 0;
    pango_layout_set_text(layout, "W.i W.i W.i W.i W.", -1);  // 18 glyphs
    pango_layout_get_pixel_size(layout, &w_mixed, nullptr);

    g_object_unref(layout);

    st->cell_w = std::max(1, static_cast<int>(std::lround(w_wide / 10.0)));
    st->cell_h = std::max(1, h);
    st->uniform_advance = (w_wide == st->cell_w * 10) && (w_narrow == w_wide) &&
                          (w_mixed == st->cell_w * 18);
}

// --- grid -------------------------------------------------------------------

void update_grid(GtkpyteTerminal *self, int width, int height) {
    State *st = self->st;
    const int cols = std::max(1, width / st->cell_w);
    const int rows = std::max(1, height / st->cell_h);
    if (cols == st->cols && rows == st->rows) {
        return;
    }
    st->cols = cols;
    st->rows = rows;
    // Emulator first, so it lays out what it holds to the new shape; then the
    // producer, so the application redraws. Skipping the second half is the
    // usual reason a program does not repaint after a window resize.
    st->screen.resize(cols, rows);
    g_signal_emit(self, g_signals[SIGNAL_RESIZED], 0, cols, rows);
}

void handle_mode(GtkpyteTerminal *self, int mode, bool set, bool private_mode) {
    if (!private_mode) {
        return;
    }
    State *st = self->st;
    switch (mode) {
        case kModeCursorKeys:
            st->key_modes.application_cursor = set;
            break;
        case kModeCursorVisible:
            st->cursor_visible = set;
            gtk_widget_queue_draw(GTK_WIDGET(self));
            break;
        case kModeBracketedPaste:
            st->bracketed_paste = set;
            break;
        default:
            break;
    }
}

// --- painting ---------------------------------------------------------------

void append_utf8(std::string &out, char32_t cp) {
    char buf[6];
    const int n = g_unichar_to_utf8(static_cast<gunichar>(cp), buf);
    out.append(buf, static_cast<std::size_t>(n));
}

PangoAttrList *attrs_for(const pyte::Attrs &a) {
    PangoAttrList *list = pango_attr_list_new();
    if (a.bold) {
        pango_attr_list_insert(list, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    }
    if (a.italic) {
        pango_attr_list_insert(list, pango_attr_style_new(PANGO_STYLE_ITALIC));
    }
    if (a.underline) {
        pango_attr_list_insert(list, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
    }
    if (a.strikethrough) {
        pango_attr_list_insert(list, pango_attr_strikethrough_new(TRUE));
    }
    return list;
}

void draw_text_at(GtkSnapshot *snapshot, PangoLayout *layout, const std::string &text,
                  float x, float y, const GdkRGBA &fg) {
    pango_layout_set_text(layout, text.c_str(), static_cast<int>(text.size()));
    gtk_snapshot_save(snapshot);
    graphene_point_t origin = GRAPHENE_POINT_INIT(x, y);
    gtk_snapshot_translate(snapshot, &origin);
    gtk_snapshot_append_layout(snapshot, layout, &fg);
    gtk_snapshot_restore(snapshot);
}

void paint_row(GtkpyteTerminal *self, GtkSnapshot *snapshot, PangoLayout *layout, int y) {
    State *st = self->st;
    const int cols = std::min(st->cols, st->screen.cols());
    const float cw = static_cast<float>(st->cell_w);
    const float ch = static_cast<float>(st->cell_h);
    const float top = static_cast<float>(y) * ch;

    int x = 0;
    while (x < cols) {
        if (st->screen.at(x, y).wide_continuation) {
            ++x;
            continue;
        }

        // A run of cells sharing attributes: one background rectangle, and
        // one layout when the run is safe to draw as a single string.
        const pyte::Attrs attrs = st->screen.at(x, y).attrs;
        const int run_start = x;
        bool plain_ascii = true;
        bool blank = true;
        while (x < cols) {
            const pyte::Cell &cell = st->screen.at(x, y);
            if (!cell.wide_continuation && cell.attrs != attrs) {
                break;
            }
            if (cell.wide_continuation || cell.ch < 0x20 || cell.ch > 0x7E) {
                plain_ascii = false;
            }
            if (!cell.wide_continuation && cell.ch != U' ' && cell.ch != U'\0') {
                blank = false;
            }
            ++x;
        }
        const int span = x - run_start;

        GdkRGBA fg = st->palette.color(attrs.fg, st->palette.foreground(), attrs.bold);
        GdkRGBA bg = st->palette.color(attrs.bg, st->palette.background());
        if (attrs.reverse) {
            std::swap(fg, bg);
        }

        if (!gdk_rgba_equal(&bg, &st->palette.background())) {
            graphene_rect_t r = GRAPHENE_RECT_INIT(static_cast<float>(run_start) * cw, top,
                                                   static_cast<float>(span) * cw, ch);
            gtk_snapshot_append_color(snapshot, &bg, &r);
        }

        if (blank && !attrs.underline && !attrs.strikethrough) {
            continue;
        }

        PangoAttrList *list = attrs_for(attrs);
        pango_layout_set_attributes(layout, list);
        pango_attr_list_unref(list);

        if (st->uniform_advance && plain_ascii) {
            // The common case: narrow glyphs the primary font certainly has,
            // with whole-pixel advances equal to the cell width.
            std::string text;
            text.reserve(static_cast<std::size_t>(span));
            for (int i = run_start; i < x; ++i) {
                const char32_t cp = st->screen.at(i, y).ch;
                text += (cp == U'\0') ? ' ' : static_cast<char>(cp);
            }
            draw_text_at(snapshot, layout, text, static_cast<float>(run_start) * cw, top, fg);
            continue;
        }

        // Fallback glyphs (box drawing, braille, CJK) come from substitute
        // fonts whose advances differ from the primary's, so a run drawn as
        // one string drifts off the grid. Place each cell on its own column;
        // a double-width glyph is drawn once from its left cell.
        for (int i = run_start; i < x; ++i) {
            const pyte::Cell &cell = st->screen.at(i, y);
            if (cell.wide_continuation) {
                continue;
            }
            const char32_t cp = (cell.ch == U'\0') ? U' ' : cell.ch;
            if (cp == U' ' && !attrs.underline && !attrs.strikethrough) {
                continue;
            }
            std::string glyph;
            append_utf8(glyph, cp);
            draw_text_at(snapshot, layout, glyph, static_cast<float>(i) * cw, top, fg);
        }
    }
    pango_layout_set_attributes(layout, nullptr);
}

void paint_cursor(GtkpyteTerminal *self, GtkSnapshot *snapshot, PangoLayout *layout) {
    State *st = self->st;
    // No cursor while scrolled back: it marks where output lands, and that is
    // not on screen.
    if (!st->cursor_visible || st->screen.viewing_history()) {
        return;
    }
    const int cx = st->screen.cursor_x();
    const int cy = st->screen.cursor_y();
    if (cx < 0 || cx >= st->screen.cols() || cy < 0 || cy >= st->screen.rows()) {
        return;
    }
    const int span =
        (cx + 1 < st->screen.cols() && st->screen.at(cx + 1, cy).wide_continuation) ? 2 : 1;
    const graphene_rect_t r = GRAPHENE_RECT_INIT(
        static_cast<float>(cx * st->cell_w), static_cast<float>(cy * st->cell_h),
        static_cast<float>(span * st->cell_w), static_cast<float>(st->cell_h));

    if (!st->focused) {
        // Outline when unfocused: the cheapest signal that keystrokes are
        // going somewhere else.
        GskRoundedRect rr;
        gsk_rounded_rect_init_from_rect(&rr, &r, 0.0f);
        const float widths[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        const GdkRGBA colors[4] = {st->palette.cursor(), st->palette.cursor(),
                                   st->palette.cursor(), st->palette.cursor()};
        gtk_snapshot_append_border(snapshot, &rr, widths, colors);
        return;
    }

    gtk_snapshot_append_color(snapshot, &st->palette.cursor(), &r);
    const pyte::Cell &cell = st->screen.at(cx, cy);
    if (cell.ch != U'\0' && cell.ch != U' ') {
        PangoAttrList *list = attrs_for(cell.attrs);
        pango_layout_set_attributes(layout, list);
        pango_attr_list_unref(list);
        std::string glyph;
        append_utf8(glyph, cell.ch);
        draw_text_at(snapshot, layout, glyph, r.origin.x, r.origin.y, st->palette.background());
        pango_layout_set_attributes(layout, nullptr);
    }
}

// --- scrollback -------------------------------------------------------------

void scroll_history(GtkpyteTerminal *self, int lines) {
    State *st = self->st;
    if (lines < 0) {
        st->screen.scroll_history_up(-lines);
    } else if (lines > 0) {
        st->screen.scroll_history_down(lines);
    }
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

gboolean on_scroll(GtkEventControllerScroll *controller, double, double dy, gpointer data) {
    auto *self = GTKPYTE_TERMINAL(data);
    State *st = self->st;
    // Full-screen applications have no scrollback of their own to show.
    if (st->screen.alternate_screen()) {
        return FALSE;
    }
    const GdkScrollUnit unit = gtk_event_controller_scroll_get_unit(controller);
    const double lines = (unit == GDK_SCROLL_UNIT_WHEEL)
                             ? dy * kLinesPerWheelStep
                             : dy / static_cast<double>(st->cell_h);
    st->scroll_accum += lines;
    const int whole = static_cast<int>(st->scroll_accum);  // truncates toward zero
    if (whole != 0) {
        st->scroll_accum -= whole;
        scroll_history(self, whole);
    }
    return TRUE;
}

// --- keyboard ---------------------------------------------------------------

gboolean on_key_pressed(GtkEventControllerKey *, guint keyval, guint, GdkModifierType state,
                        gpointer data) {
    auto *self = GTKPYTE_TERMINAL(data);
    State *st = self->st;
    const GdkModifierType mods =
        static_cast<GdkModifierType>(state & gtk_accelerator_get_default_mod_mask());
    const bool ctrl_shift = (mods & GDK_CONTROL_MASK) && (mods & GDK_SHIFT_MASK) &&
                            !(mods & GDK_ALT_MASK);

    // Terminal-owned shortcuts. Ctrl+Shift so the plain Ctrl combinations
    // stay with the application -- and Ctrl+Shift+C has to be swallowed even
    // before copy exists, or it falls through and sends ^C to the shell.
    if (ctrl_shift) {
        switch (keyval) {
            case GDK_KEY_V:
            case GDK_KEY_v:
                gtkpyte_terminal_paste_clipboard(self);
                return TRUE;
            case GDK_KEY_C:
            case GDK_KEY_c:
                return TRUE;
            default:
                break;
        }
    }

    // Scrollback paging belongs to the widget, but only with Shift held and
    // only off the alternate screen: full-screen apps want the plain keys.
    if ((mods & GDK_SHIFT_MASK) && !st->screen.alternate_screen()) {
        if (keyval == GDK_KEY_Page_Up) {
            scroll_history(self, -std::max(1, st->rows / 2));
            return TRUE;
        }
        if (keyval == GDK_KEY_Page_Down) {
            scroll_history(self, std::max(1, st->rows / 2));
            return TRUE;
        }
    }

    const std::string bytes = gtkpyte::encode_key(keyval, mods, st->key_modes);
    if (bytes.empty()) {
        return FALSE;
    }

    // Typing returns to the live screen, as output does.
    if (st->screen.viewing_history()) {
        st->screen.scroll_history_to_bottom();
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
    emit_commit(self, bytes);
    return TRUE;
}

void on_focus_enter(GtkEventControllerFocus *, gpointer data) {
    auto *self = GTKPYTE_TERMINAL(data);
    self->st->focused = true;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void on_focus_leave(GtkEventControllerFocus *, gpointer data) {
    auto *self = GTKPYTE_TERMINAL(data);
    self->st->focused = false;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void on_click_pressed(GtkGestureClick *, int, double, double, gpointer data) {
    gtk_widget_grab_focus(GTK_WIDGET(data));
}

void on_realize(GtkWidget *widget, gpointer) {
    // The widget's Pango context only carries the display's font options once
    // it is on a display, so metrics taken in init can be off by a pixel.
    measure_cells(GTKPYTE_TERMINAL(widget));
    gtk_widget_queue_resize(widget);
}

void on_clipboard_text(GObject *source, GAsyncResult *result, gpointer data) {
    auto *self = GTKPYTE_TERMINAL(data);
    char *text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source), result, nullptr);
    if (text != nullptr) {
        gtkpyte_terminal_paste(self, text);
        g_free(text);
    }
    g_object_unref(self);
}

// --- GtkWidget vfuncs -------------------------------------------------------

void gtkpyte_terminal_measure(GtkWidget *widget, GtkOrientation orientation, int,
                              int *minimum, int *natural, int *minimum_baseline,
                              int *natural_baseline) {
    State *st = GTKPYTE_TERMINAL(widget)->st;
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        *minimum = st->cell_w * 10;
        *natural = st->cell_w * 80;
    } else {
        *minimum = st->cell_h * 3;
        *natural = st->cell_h * 24;
    }
    *minimum_baseline = -1;
    *natural_baseline = -1;
}

void gtkpyte_terminal_size_allocate(GtkWidget *widget, int width, int height, int) {
    update_grid(GTKPYTE_TERMINAL(widget), width, height);
}

void gtkpyte_terminal_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    auto *self = GTKPYTE_TERMINAL(widget);
    State *st = self->st;

    const graphene_rect_t all = GRAPHENE_RECT_INIT(
        0.0f, 0.0f, static_cast<float>(gtk_widget_get_width(widget)),
        static_cast<float>(gtk_widget_get_height(widget)));
    gtk_snapshot_append_color(snapshot, &st->palette.background(), &all);

    PangoLayout *layout = make_layout(self);
    const int rows = std::min(st->rows, st->screen.rows());
    for (int y = 0; y < rows; ++y) {
        paint_row(self, snapshot, layout, y);
    }
    paint_cursor(self, snapshot, layout);
    g_object_unref(layout);

    st->screen.clear_dirty();
}

// --- GObject ----------------------------------------------------------------

void gtkpyte_terminal_finalize(GObject *object) {
    auto *self = GTKPYTE_TERMINAL(object);
    delete self->st;
    self->st = nullptr;
    G_OBJECT_CLASS(gtkpyte_terminal_parent_class)->finalize(object);
}

}  // namespace

static void gtkpyte_terminal_class_init(GtkpyteTerminalClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = gtkpyte_terminal_finalize;
    widget_class->measure = gtkpyte_terminal_measure;
    widget_class->size_allocate = gtkpyte_terminal_size_allocate;
    widget_class->snapshot = gtkpyte_terminal_snapshot;

    g_signals[SIGNAL_COMMIT] =
        g_signal_new("commit", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, nullptr,
                     nullptr, nullptr, G_TYPE_NONE, 1, G_TYPE_BYTES);
    g_signals[SIGNAL_RESIZED] =
        g_signal_new("resized", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0, nullptr,
                     nullptr, nullptr, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

    gtk_widget_class_set_css_name(widget_class, "gtkpyte-terminal");
}

static void gtkpyte_terminal_init(GtkpyteTerminal *self) {
    self->st = new State();
    State *st = self->st;
    st->font = pango_font_description_from_string(kDefaultFont);

    GtkWidget *widget = GTK_WIDGET(self);
    gtk_widget_set_focusable(widget, TRUE);
    gtk_widget_set_cursor_from_name(widget, "text");
    gtk_widget_set_overflow(widget, GTK_OVERFLOW_HIDDEN);

    st->stream.set_responder([self](const std::string &reply) { emit_commit(self, reply); });
    st->stream.set_mode_handler(
        [self](int mode, bool set, bool private_mode) { handle_mode(self, mode, set, private_mode); });

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), self);
    gtk_widget_add_controller(widget, keys);

    GtkEventController *focus = gtk_event_controller_focus_new();
    g_signal_connect(focus, "enter", G_CALLBACK(on_focus_enter), self);
    g_signal_connect(focus, "leave", G_CALLBACK(on_focus_leave), self);
    gtk_widget_add_controller(widget, focus);

    GtkEventController *scroll =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), self);
    gtk_widget_add_controller(widget, scroll);

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(on_click_pressed), self);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));

    g_signal_connect(widget, "realize", G_CALLBACK(on_realize), nullptr);

    measure_cells(self);
}

// --- public API -------------------------------------------------------------

GtkWidget *gtkpyte_terminal_new(void) {
    return GTK_WIDGET(g_object_new(GTKPYTE_TYPE_TERMINAL, nullptr));
}

void gtkpyte_terminal_feed(GtkpyteTerminal *self, const char *data, gsize len) {
    g_return_if_fail(GTKPYTE_IS_TERMINAL(self));
    if (data == nullptr || len == 0) {
        return;
    }
    self->st->stream.feed(data, len);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void gtkpyte_terminal_paste(GtkpyteTerminal *self, const char *utf8) {
    g_return_if_fail(GTKPYTE_IS_TERMINAL(self));
    if (utf8 == nullptr || *utf8 == '\0') {
        return;
    }
    if (self->st->screen.viewing_history()) {
        self->st->screen.scroll_history_to_bottom();
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
    emit_commit(self, gtkpyte::encode_text(utf8, self->st->bracketed_paste));
}

void gtkpyte_terminal_paste_clipboard(GtkpyteTerminal *self) {
    g_return_if_fail(GTKPYTE_IS_TERMINAL(self));
    GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(self));
    gdk_clipboard_read_text_async(clipboard, nullptr, on_clipboard_text, g_object_ref(self));
}

void gtkpyte_terminal_set_font(GtkpyteTerminal *self, const char *font_desc) {
    g_return_if_fail(GTKPYTE_IS_TERMINAL(self));
    g_return_if_fail(font_desc != nullptr);
    State *st = self->st;
    if (st->font != nullptr) {
        pango_font_description_free(st->font);
    }
    st->font = pango_font_description_from_string(font_desc);
    measure_cells(self);
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

void gtkpyte_terminal_set_scrollback(GtkpyteTerminal *self, int lines) {
    g_return_if_fail(GTKPYTE_IS_TERMINAL(self));
    self->st->screen.set_max_scrollback(std::max(0, lines));
}

int gtkpyte_terminal_get_columns(GtkpyteTerminal *self) {
    g_return_val_if_fail(GTKPYTE_IS_TERMINAL(self), 0);
    return self->st->cols;
}

int gtkpyte_terminal_get_rows(GtkpyteTerminal *self) {
    g_return_val_if_fail(GTKPYTE_IS_TERMINAL(self), 0);
    return self->st->rows;
}

int gtkpyte_terminal_get_cell_width(GtkpyteTerminal *self) {
    g_return_val_if_fail(GTKPYTE_IS_TERMINAL(self), 0);
    return self->st->cell_w;
}

int gtkpyte_terminal_get_cell_height(GtkpyteTerminal *self) {
    g_return_val_if_fail(GTKPYTE_IS_TERMINAL(self), 0);
    return self->st->cell_h;
}

char *gtkpyte_terminal_get_screen_text(GtkpyteTerminal *self) {
    g_return_val_if_fail(GTKPYTE_IS_TERMINAL(self), nullptr);
    std::string out;
    const pyte::Screen &screen = self->st->screen;
    for (int y = 0; y < screen.rows(); ++y) {
        out += screen.line_text(y);
        out += '\n';
    }
    return g_strdup(out.c_str());
}
