// tests/test_resize.cpp
#include <doctest/doctest.h>

#include <string>

#include "pyte/screen.h"
#include "pyte/stream.h"

namespace {

std::string rtrim(const std::string &s) {
    const std::size_t end = s.find_last_not_of(' ');
    return end == std::string::npos ? std::string() : s.substr(0, end + 1);
}

}  // namespace

TEST_CASE("shrinking drops blank rows under the cursor before content above it") {
    pyte::Screen screen(20, 24);
    pyte::Stream stream(screen);
    stream.feed(std::string("one\r\ntwo\r\nthree\r\n$ "));
    REQUIRE(screen.cursor_y() == 3);

    screen.resize(20, 7);

    CHECK(screen.history_size() == 0);
    CHECK(rtrim(screen.line_text(0)) == "one");
    CHECK(rtrim(screen.line_text(3)) == "$");
    CHECK(screen.cursor_y() == 3);
    CHECK(screen.cursor_x() == 2);
}

TEST_CASE("shrinking past the cursor pushes only the overflow into history") {
    pyte::Screen screen(20, 10);
    pyte::Stream stream(screen);
    stream.feed(std::string("a\r\nb\r\nc\r\nd\r\ne\r\n$ "));
    REQUIRE(screen.cursor_y() == 5);

    screen.resize(20, 3);

    CHECK(screen.history_size() == 3);
    CHECK(rtrim(screen.line_text(0)) == "d");
    CHECK(rtrim(screen.line_text(2)) == "$");
    CHECK(screen.cursor_y() == 2);
}

TEST_CASE("rows with content below the cursor are kept, not dropped") {
    pyte::Screen screen(20, 6);
    pyte::Stream stream(screen);
    // Text on row 5, then the cursor back up to row 1.
    stream.feed(std::string("top\x1b[6;1Hbottom\x1b[2;1H"));
    REQUIRE(screen.cursor_y() == 1);

    screen.resize(20, 3);

    // Row 5 has content, so nothing below the cursor is blank-droppable from
    // the bottom up; the old bottom-anchored behaviour applies and nothing is
    // lost: "top" goes to history, "bottom" stays on screen.
    CHECK(rtrim(screen.line_text(2)) == "bottom");
    CHECK(screen.history_size() == 3);
}

TEST_CASE("growing after a shrink keeps the prompt where it was") {
    pyte::Screen screen(20, 24);
    pyte::Stream stream(screen);
    stream.feed(std::string("one\r\ntwo\r\n$ "));
    screen.resize(20, 5);
    screen.resize(20, 24);
    CHECK(rtrim(screen.line_text(0)) == "one");
    CHECK(rtrim(screen.line_text(2)) == "$");
    CHECK(screen.cursor_y() == 2);
}
