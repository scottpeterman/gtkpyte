// src/ptysession.cpp
#include "ptysession.h"

#include <glib-unix.h>

#include <cerrno>

namespace gtkpyte {
namespace {

// One read() and one callback's worth of output. The per-callback budget is
// what keeps a firehose (cat of a large file, `yes`) from starving input and
// redraws: the callback yields after this much and the main loop gets a turn.
constexpr unsigned long kReadChunk = 64 * 1024;
constexpr unsigned long kReadBudget = 512 * 1024;

// After end of file, how long to wait for the child's status: 20 ms x 100.
constexpr guint kReapIntervalMs = 20;
constexpr int kReapMaxTries = 100;

}  // namespace

PtySession::PtySession(GtkpyteTerminal *terminal)
    : terminal_(GTKPYTE_TERMINAL(g_object_ref(terminal))) {
    commit_handler_ = g_signal_connect(terminal_, "commit", G_CALLBACK(on_commit), this);
    resized_handler_ = g_signal_connect(terminal_, "resized", G_CALLBACK(on_resized), this);
}

PtySession::~PtySession() {
    if (watch_ != 0) {
        g_source_remove(watch_);
        watch_ = 0;
    }
    if (reap_timer_ != 0) {
        g_source_remove(reap_timer_);
        reap_timer_ = 0;
    }
    g_signal_handler_disconnect(terminal_, commit_handler_);
    g_signal_handler_disconnect(terminal_, resized_handler_);
    g_object_unref(terminal_);
    // pty_'s destructor sends SIGHUP and reaps whatever is still running.
}

bool PtySession::start(const std::string &program, const std::vector<std::string> &args,
                       const std::vector<std::string> &env) {
    const int cols = gtkpyte_terminal_get_columns(terminal_);
    const int rows = gtkpyte_terminal_get_rows(terminal_);
    if (!pty_.start(program, args, cols, rows, env)) {
        return false;
    }
    watch_ = g_unix_fd_add(pty_.fd(), static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR),
                           on_readable, this);
    return true;
}

gboolean PtySession::on_readable(gint, GIOCondition, gpointer data) {
    auto *self = static_cast<PtySession *>(data);
    char buf[kReadChunk];
    unsigned long total = 0;

    while (total < kReadBudget) {
        const long n = self->pty_.read(buf, sizeof buf);
        if (n > 0) {
            gtkpyte_terminal_feed(self->terminal_, buf, static_cast<gsize>(n));
            total += static_cast<unsigned long>(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return G_SOURCE_CONTINUE;  // drained; wait for more
        }
        // 0 is end of file (Pty folds EIO into it); any other error is fatal
        // to the session just the same. The source is removed by returning
        // G_SOURCE_REMOVE, so clear the id before anything can delete us.
        self->watch_ = 0;
        self->child_closed();
        return G_SOURCE_REMOVE;  // `self` may be gone: touch nothing
    }
    return G_SOURCE_CONTINUE;  // budget spent; the main loop gets a turn
}

void PtySession::child_closed() {
    int status = 0;
    if (pty_.reap(&status)) {
        finish(status);
        return;
    }
    reap_tries_ = 0;
    reap_timer_ = g_timeout_add(kReapIntervalMs, on_reap_retry, this);
}

gboolean PtySession::on_reap_retry(gpointer data) {
    auto *self = static_cast<PtySession *>(data);
    int status = 0;
    if (!self->pty_.reap(&status)) {
        if (++self->reap_tries_ < kReapMaxTries) {
            return G_SOURCE_CONTINUE;
        }
        // The terminal closed but the process will not exit -- it detached
        // from its controlling tty, or is ignoring SIGHUP. End it.
        self->pty_.terminate();
        self->pty_.reap(&status);
    }
    self->reap_timer_ = 0;
    self->finish(status);
    return G_SOURCE_REMOVE;  // `self` may be gone
}

void PtySession::finish(int wait_status) {
    if (finished_) {
        return;
    }
    finished_ = true;
    // Last thing done: the handler is free to delete this session.
    if (on_exit_) {
        ExitHandler handler = std::move(on_exit_);
        handler(wait_status);
    }
}

void PtySession::on_commit(GtkpyteTerminal *, GBytes *bytes, gpointer data) {
    auto *self = static_cast<PtySession *>(data);
    gsize len = 0;
    const char *ptr = static_cast<const char *>(g_bytes_get_data(bytes, &len));
    if (ptr != nullptr && len > 0) {
        self->pty_.write(ptr, static_cast<unsigned long>(len));
    }
}

void PtySession::on_resized(GtkpyteTerminal *, int cols, int rows, gpointer data) {
    static_cast<PtySession *>(data)->pty_.resize(cols, rows);
}

}  // namespace gtkpyte
