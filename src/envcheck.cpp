// src/envcheck.cpp
//
// Environment check for gtkpyte: proves the toolchain end to end.
//   - the vendored pyte core compiles and links under Meson
//   - utf8proc builds from the pinned wrap
//   - GTK4 opens a window and Pango renders monospace text
//
// The emulator output is also printed to stdout, so a headless run is still
// a meaningful check:
//
//   ./build/gtkpyte-envcheck            window stays open until closed
//   ./build/gtkpyte-envcheck --smoke    window closes itself after 1.5 s

#include <gtk/gtk.h>

#include <cstring>
#include <iostream>
#include <string>

#include "pyte/screen.h"
#include "pyte/stream.h"

namespace {

constexpr int kCols = 60;
constexpr int kRows = 10;

bool g_smoke = false;

// Cursor addressing, erase, SGR and a double-width glyph: enough to show the
// parser and the width table are both live, not just pass-through text.
const char kSample[] =
    "\x1b[2J\x1b[H"
    "gtkpyte environment check\r\n"
    "\x1b[1;32mbold green\x1b[0m  \x1b[4munderline\x1b[0m\r\n"
    "wide: \xe6\xbc\xa2\xe5\xad\x97 <- two cells each\r\n"
    "\x1b[6;10Hcursor jumped to row 6, col 10\r\n"
    "line to be erased\x1b[2K\r"
    "erase-in-line worked\r\n";

std::string render_screen() {
    pyte::Screen screen(kCols, kRows);
    pyte::Stream stream(screen);
    stream.feed(kSample, sizeof(kSample) - 1);

    std::string out;
    for (int y = 0; y < screen.rows(); ++y) {
        out += screen.line_text(y);
        out += '\n';
    }
    if (stream.unhandled_count() > 0) {
        out += "(unhandled sequences: " + std::to_string(stream.unhandled_count()) + ")\n";
    }
    return out;
}

gboolean close_window(gpointer window) {
    gtk_window_destroy(GTK_WINDOW(window));
    return G_SOURCE_REMOVE;
}

void on_activate(GtkApplication *app, gpointer) {
    const std::string text = render_screen();
    std::cout << text << std::flush;

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "gtkpyte envcheck");

    GtkWidget *label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_widget_set_margin_start(label, 12);
    gtk_widget_set_margin_end(label, 12);
    gtk_widget_set_margin_top(label, 12);
    gtk_widget_set_margin_bottom(label, 12);

    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_family_new("Monospace"));
    gtk_label_set_attributes(GTK_LABEL(label), attrs);
    pango_attr_list_unref(attrs);

    gtk_window_set_child(GTK_WINDOW(window), label);
    gtk_window_present(GTK_WINDOW(window));

    std::cout << "GTK " << gtk_get_major_version() << "." << gtk_get_minor_version()
              << "." << gtk_get_micro_version() << ", Pango " << pango_version_string()
              << ", display " << gdk_display_get_name(gtk_widget_get_display(window))
              << std::endl;

    if (g_smoke) {
        g_timeout_add(1500, close_window, window);
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke") == 0) {
            g_smoke = true;
        }
    }

    GtkApplication *app =
        gtk_application_new("io.github.scottpeterman.gtkpyte.envcheck",
                            static_cast<GApplicationFlags>(0));
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);

    // Our own flag is consumed above; GApplication would reject it.
    const int status = g_application_run(G_APPLICATION(app), 0, nullptr);
    g_object_unref(app);
    return status;
}
