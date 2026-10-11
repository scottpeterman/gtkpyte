/* src/gtkpyte-terminal.h
 *
 * GtkpyteTerminal: a GTK4 terminal widget over the pyte emulation core.
 *
 * The widget owns a screen and nothing else -- no pty, no shell, no SSH.
 * Bytes come in through gtkpyte_terminal_feed() and go out through the
 * "commit" signal; where they come from is the caller's business. That is the
 * same split qtpyte makes, and it is what lets one widget sit on a local pty,
 * an SSH channel or a recorded capture without knowing which.
 *
 * The API is plain C so the header stays usable from C and from GObject
 * Introspection later; the implementation is C++ because pyte is.
 */
#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTKPYTE_TYPE_TERMINAL (gtkpyte_terminal_get_type())
G_DECLARE_FINAL_TYPE(GtkpyteTerminal, gtkpyte_terminal, GTKPYTE, TERMINAL, GtkWidget)

GtkWidget *gtkpyte_terminal_new(void);

/* Push received bytes into the emulator and schedule a redraw. */
void gtkpyte_terminal_feed(GtkpyteTerminal *self, const char *data, gsize len);

/* Send text as if pasted: newline conversion and bracketed paste applied,
 * emitted through "commit". */
void gtkpyte_terminal_paste(GtkpyteTerminal *self, const char *utf8);

/* Paste the clipboard contents (what Ctrl+Shift+V does). */
void gtkpyte_terminal_paste_clipboard(GtkpyteTerminal *self);

/* Font, as a Pango description string ("Monospace 11"). Changing it re-derives
 * the cell size and so the grid. */
void gtkpyte_terminal_set_font(GtkpyteTerminal *self, const char *font_desc);

void gtkpyte_terminal_set_scrollback(GtkpyteTerminal *self, int lines);

int gtkpyte_terminal_get_columns(GtkpyteTerminal *self);
int gtkpyte_terminal_get_rows(GtkpyteTerminal *self);
int gtkpyte_terminal_get_cell_width(GtkpyteTerminal *self);
int gtkpyte_terminal_get_cell_height(GtkpyteTerminal *self);

/* The visible screen as text, one line per row, trailing spaces kept. */
char *gtkpyte_terminal_get_screen_text(GtkpyteTerminal *self);

/* Selection. Copying is never automatic: selecting puts the text on the
 * primary selection (middle-click paste, X11/Wayland convention) but leaves
 * the clipboard alone until copy_clipboard() -- Ctrl+Shift+C -- is called. */
gboolean gtkpyte_terminal_has_selection(GtkpyteTerminal *self);
char *gtkpyte_terminal_get_selected_text(GtkpyteTerminal *self);
void gtkpyte_terminal_copy_clipboard(GtkpyteTerminal *self);
void gtkpyte_terminal_select_all(GtkpyteTerminal *self);
void gtkpyte_terminal_clear_selection(GtkpyteTerminal *self);

/* Scrollback is exposed through GtkScrollable (in lines, not pixels): put the
 * terminal in a GtkScrolledWindow for a scrollbar.
 *
 * Signals
 *
 *   "commit"  (GtkpyteTerminal *, GBytes *data)
 *       Bytes to send upstream: keystrokes, pastes, and replies the emulator
 *       owes to Device Status Report and friends. Drop it and applications
 *       that query the terminal will hang waiting.
 *
 *   "resized" (GtkpyteTerminal *, int columns, int rows)
 *       The grid changed shape. Whatever is producing the bytes needs to know
 *       (TIOCSWINSZ for a pty, window-change for SSH).
 *
 *   "selection-changed" (GtkpyteTerminal *)
 *       A selection appeared or went away -- for enabling a Copy menu item.
 */

G_END_DECLS
