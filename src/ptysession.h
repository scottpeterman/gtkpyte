// src/ptysession.h
#pragma once

#include <glib.h>

#include <functional>
#include <string>
#include <vector>

#include "gtkpyte-terminal.h"
#include "pty.h"

namespace gtkpyte {

// Wires a local child process to a GtkpyteTerminal: pty output into
// gtkpyte_terminal_feed(), "commit" into the pty, "resized" into TIOCSWINSZ.
//
// The pty master is watched with g_unix_fd_add on the default main context,
// so everything here runs on the GTK thread and the widget is never touched
// from anywhere else.
class PtySession {
public:
    using ExitHandler = std::function<void(int wait_status)>;

    explicit PtySession(GtkpyteTerminal *terminal);
    ~PtySession();

    PtySession(const PtySession &) = delete;
    PtySession &operator=(const PtySession &) = delete;

    // Start `program` (searched on PATH) at the terminal's current grid size.
    bool start(const std::string &program, const std::vector<std::string> &args,
               const std::vector<std::string> &env = {});

    // Called once, after the child has gone and its output is drained.
    void set_exit_handler(ExitHandler handler) { on_exit_ = std::move(handler); }

    const std::string &error() const { return pty_.error(); }
    long pid() const { return pty_.pid(); }

private:
    static gboolean on_readable(gint fd, GIOCondition condition, gpointer data);
    static void on_commit(GtkpyteTerminal *terminal, GBytes *bytes, gpointer data);
    static void on_resized(GtkpyteTerminal *terminal, int cols, int rows, gpointer data);
    static gboolean on_reap_retry(gpointer data);

    // Output has hit end of file. The child may still be on its way out, so
    // its status is collected on a short retry before the handler runs.
    void child_closed();
    void finish(int wait_status);

    GtkpyteTerminal *terminal_;
    Pty pty_;
    guint watch_ = 0;
    guint reap_timer_ = 0;
    int reap_tries_ = 0;
    gulong commit_handler_ = 0;
    gulong resized_handler_ = 0;
    bool finished_ = false;
    ExitHandler on_exit_;
};

}  // namespace gtkpyte
