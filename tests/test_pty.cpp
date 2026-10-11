// tests/test_pty.cpp
#include <doctest/doctest.h>

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "pty.h"

namespace {

// Read until end of file or a 5 s timeout; the pty is non-blocking.
std::string drain(gtkpyte::Pty &pty) {
    std::string out;
    char buf[4096];
    for (int i = 0; i < 500; ++i) {
        pollfd pfd{pty.fd(), POLLIN, 0};
        ::poll(&pfd, 1, 10);
        const long n = pty.read(buf, sizeof buf);
        if (n > 0) {
            out.append(buf, static_cast<std::size_t>(n));
        } else if (n == 0) {
            break;
        }
    }
    return out;
}

}  // namespace

TEST_CASE("child output arrives through the master") {
    gtkpyte::Pty pty;
    REQUIRE(pty.start("/bin/sh", {"-c", "printf hello"}, 80, 24));
    CHECK(drain(pty).find("hello") != std::string::npos);
}

TEST_CASE("the child sees the window size and TERM") {
    gtkpyte::Pty pty;
    REQUIRE(pty.start("/bin/sh", {"-c", "stty size; printf '%s' \"$TERM\""}, 100, 30));
    const std::string out = drain(pty);
    CHECK(out.find("30 100") != std::string::npos);
    CHECK(out.find("xterm-256color") != std::string::npos);
}

TEST_CASE("input written to the master reaches the child") {
    gtkpyte::Pty pty;
    REQUIRE(pty.start("/bin/sh", {"-c", "read line; printf 'got:%s' \"$line\""}, 80, 24));
    pty.write("ping\r");
    CHECK(drain(pty).find("got:ping") != std::string::npos);
}

TEST_CASE("exit status is collected") {
    gtkpyte::Pty pty;
    REQUIRE(pty.start("/bin/sh", {"-c", "exit 3"}, 80, 24));
    drain(pty);
    int status = 0;
    for (int i = 0; i < 100 && !pty.reap(&status); ++i) {
        ::usleep(10000);
    }
    REQUIRE(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 3);
}
