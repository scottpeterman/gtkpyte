// src/main.cpp
//
// gtkpyte: a terminal window running your shell on a local pty.
//
//   gtkpyte
//   gtkpyte --font "DejaVu Sans Mono 12"
//   gtkpyte --scheme solarized-dark
//   gtkpyte --list-schemes
//   gtkpyte -- htop
//
// Drag to select (double-click a word, triple-click a line); Ctrl+Shift+C
// copies, Ctrl+Shift+V pastes, Ctrl+Shift+A selects all, middle-click pastes
// the primary selection. Shift+PageUp/PageDown, the wheel or the scrollbar
// move through scrollback.

#include <gtk/gtk.h>

#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "gtkpyte-terminal.h"
#include "ptysession.h"

namespace {

struct Options {
    std::string font;
    std::string scheme;
    std::string program;
    std::vector<std::string> args;
};

Options g_options;

std::string user_shell() {
    const char *env = std::getenv("SHELL");
    if (env != nullptr && *env != '\0') {
        return env;
    }
    const passwd *pw = getpwuid(getuid());
    if (pw != nullptr && pw->pw_shell != nullptr && *pw->pw_shell != '\0') {
        return pw->pw_shell;
    }
    return "/bin/sh";
}

gboolean destroy_window(gpointer window) {
    gtk_window_destroy(GTK_WINDOW(window));
    g_object_unref(window);
    return G_SOURCE_REMOVE;
}

void delete_session(gpointer session) {
    delete static_cast<gtkpyte::PtySession *>(session);
}

void on_activate(GtkApplication *app, gpointer) {
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *terminal = gtkpyte_terminal_new();
    auto *term = GTKPYTE_TERMINAL(terminal);
    if (!g_options.font.empty()) {
        gtkpyte_terminal_set_font(term, g_options.font.c_str());
    }
    if (!g_options.scheme.empty()) {
        gtkpyte_terminal_set_color_scheme(term, g_options.scheme.c_str());
    }

    const std::string program = g_options.program.empty() ? user_shell() : g_options.program;
    gtk_window_set_title(GTK_WINDOW(window), program.c_str());

    // The terminal is a GtkScrollable, so the scrolled window supplies the
    // scrollbar and the terminal keeps owning what a scroll means. Natural
    // size propagates so the window opens at the terminal's 80x24.
    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroller), TRUE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroller), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), terminal);
    gtk_window_set_child(GTK_WINDOW(window), scroller);

    auto *session = new gtkpyte::PtySession(term);
    // Owned by the window: closing it hangs up the child.
    g_object_set_data_full(G_OBJECT(window), "gtkpyte-session", session, delete_session);

    session->set_exit_handler([window](int status) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
            std::cerr << "gtkpyte: could not run the program (exit 127)\n";
        }
        // Deferred: this runs inside the session's own callback, and closing
        // the window deletes the session.
        g_idle_add(destroy_window, g_object_ref(window));
    });

    if (!session->start(program, g_options.args)) {
        std::cerr << "gtkpyte: " << session->error() << "\n";
        gtk_window_destroy(GTK_WINDOW(window));
        return;
    }

    gtk_window_present(GTK_WINDOW(window));
    gtk_widget_grab_focus(terminal);
}

void usage() {
    std::cerr << "usage: gtkpyte [--font \"Family Size\"] [--scheme NAME] [--list-schemes]"
                 " [-- program [args...]]\n";
}

bool scheme_known(const std::string &name) {
    char **names = gtkpyte_terminal_list_color_schemes();
    bool found = false;
    for (char **n = names; *n != nullptr; ++n) {
        found = found || name == *n;
    }
    g_strfreev(names);
    return found;
}

void list_schemes() {
    char **names = gtkpyte_terminal_list_color_schemes();
    for (char **n = names; *n != nullptr; ++n) {
        std::cout << *n << "\n";
    }
    g_strfreev(names);
}

}  // namespace

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            g_options.font = argv[++i];
        } else if (std::strncmp(argv[i], "--font=", 7) == 0) {
            g_options.font = argv[i] + 7;
        } else if (std::strcmp(argv[i], "--scheme") == 0 && i + 1 < argc) {
            g_options.scheme = argv[++i];
        } else if (std::strncmp(argv[i], "--scheme=", 9) == 0) {
            g_options.scheme = argv[i] + 9;
        } else if (std::strcmp(argv[i], "--list-schemes") == 0) {
            list_schemes();
            return 0;
        } else if (std::strcmp(argv[i], "--") == 0) {
            if (i + 1 < argc) {
                g_options.program = argv[i + 1];
                for (int j = i + 2; j < argc; ++j) {
                    g_options.args.emplace_back(argv[j]);
                }
            }
            break;
        } else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else {
            usage();
            return 2;
        }
    }

    // Checked before any window exists, so a typo is an error, not a silently
    // default-coloured terminal.
    if (!g_options.scheme.empty() && !scheme_known(g_options.scheme)) {
        std::cerr << "gtkpyte: unknown scheme \"" << g_options.scheme
                  << "\" (see --list-schemes)\n";
        return 2;
    }

    // Non-unique: each launch is its own terminal, not a request to an
    // already-running instance to open one.
    GtkApplication *app =
        gtk_application_new("io.github.scottpeterman.gtkpyte", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    const int status = g_application_run(G_APPLICATION(app), 0, nullptr);
    g_object_unref(app);
    return status;
}
