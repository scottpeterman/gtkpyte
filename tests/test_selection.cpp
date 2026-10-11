// tests/test_selection.cpp
#include <doctest/doctest.h>

#include <string>

#include "pyte/screen.h"
#include "pyte/stream.h"
#include "selection.h"

using gtkpyte::Selection;

TEST_CASE("normalized orders a backward drag") {
    Selection s;
    s.anchor_line = 5;
    s.anchor_col = 10;
    s.focus_line = 3;
    s.focus_col = 2;
    int sl = 0, sc = 0, el = 0, ec = 0;
    s.normalized(sl, sc, el, ec);
    CHECK(sl == 3);
    CHECK(sc == 2);
    CHECK(el == 5);
    CHECK(ec == 10);
}

TEST_CASE("span_on_line covers first, middle and last lines") {
    Selection s;
    s.active = true;
    s.anchor_line = 10;
    s.anchor_col = 4;
    s.focus_line = 12;
    s.focus_col = 7;
    int from = 0, to = 0;

    REQUIRE(s.span_on_line(10, 80, from, to));
    CHECK(from == 4);
    CHECK(to == 80);

    REQUIRE(s.span_on_line(11, 80, from, to));
    CHECK(from == 0);
    CHECK(to == 80);

    REQUIRE(s.span_on_line(12, 80, from, to));
    CHECK(from == 0);
    CHECK(to == 7);

    CHECK_FALSE(s.span_on_line(9, 80, from, to));
    CHECK_FALSE(s.span_on_line(13, 80, from, to));
}

TEST_CASE("an inactive or empty selection paints nothing") {
    Selection s;
    int from = 0, to = 0;
    CHECK_FALSE(s.span_on_line(0, 80, from, to));
    s.active = true;
    CHECK(s.empty());
    CHECK_FALSE(s.span_on_line(0, 80, from, to));
}

TEST_CASE("word_bounds takes paths and addresses whole") {
    pyte::Screen screen(60, 3);
    pyte::Stream stream(screen);
    stream.feed(std::string("ping 10.1.2.3 via /etc/hosts now"));

    int start = 0, end = 0;
    REQUIRE(gtkpyte::word_bounds(screen, 0, 7, start, end));  // inside 10.1.2.3
    CHECK(screen.line_text(0).substr(start, end - start) == "10.1.2.3");

    REQUIRE(gtkpyte::word_bounds(screen, 0, 20, start, end));  // inside /etc/hosts
    CHECK(screen.line_text(0).substr(start, end - start) == "/etc/hosts");

    CHECK_FALSE(gtkpyte::word_bounds(screen, 0, 4, start, end));  // the space
    CHECK(start == 4);
    CHECK(end == 5);
}

TEST_CASE("word_bounds keeps a double-width glyph whole") {
    pyte::Screen screen(20, 2);
    pyte::Stream stream(screen);
    stream.feed(std::string("a \xe6\xbc\xa2\xe5\xad\x97 b"));  // a, space, 漢字 (4 cells), space, b

    int start = 0, end = 0;
    REQUIRE(gtkpyte::word_bounds(screen, 0, 3, start, end));  // right half of 漢
    CHECK(start == 2);
    CHECK(end == 6);
    CHECK(screen.text_in_range(screen.base_line(), start, screen.base_line(), end) ==
          "\xe6\xbc\xa2\xe5\xad\x97");
}

TEST_CASE("autoscroll speeds up with overshoot and caps at a page") {
    CHECK(gtkpyte::autoscroll_step(1, 18, 24) == 1);
    CHECK(gtkpyte::autoscroll_step(18, 18, 24) == 2);
    CHECK(gtkpyte::autoscroll_step(54, 18, 24) == 8);
    CHECK(gtkpyte::autoscroll_step(10000, 18, 24) == 24);
    CHECK(gtkpyte::autoscroll_step(-5, 18, 24) == 1);
}
