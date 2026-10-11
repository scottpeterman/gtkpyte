// tests/test_mouse.cpp
#include <doctest/doctest.h>

#include "mouse.h"

using gtkpyte::encode_mouse;
using gtkpyte::MouseAction;
using gtkpyte::MouseModes;
using gtkpyte::MouseMods;

namespace {

MouseModes modes(int tracking, bool sgr) {
    MouseModes m;
    m.apply(tracking, true);
    if (sgr) {
        m.apply(1006, true);
    }
    return m;
}

}  // namespace

TEST_CASE("nothing is reported until the application asks") {
    MouseModes m;
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 0, 0, {}).empty());
}

TEST_CASE("SGR press, release and wheel") {
    const MouseModes m = modes(1000, true);
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 4, 9, {}) == "\x1b[<0;5;10M");
    CHECK(encode_mouse(m, MouseAction::Release, gtkpyte::kMouseLeft, 4, 9, {}) == "\x1b[<0;5;10m");
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kWheelUp, 0, 0, {}) == "\x1b[<64;1;1M");
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kWheelDown, 0, 0, {}) == "\x1b[<65;1;1M");
    // A wheel step has no release.
    CHECK(encode_mouse(m, MouseAction::Release, gtkpyte::kWheelUp, 0, 0, {}).empty());
}

TEST_CASE("SGR has no coordinate limit") {
    const MouseModes m = modes(1000, true);
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 299, 0, {}) ==
          "\x1b[<0;300;1M");
}

TEST_CASE("classic encoding packs bytes and loses the release button") {
    const MouseModes m = modes(1000, false);
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 0, 0, {}) ==
          std::string("\x1b[M") + char(32) + char(33) + char(33));
    CHECK(encode_mouse(m, MouseAction::Release, gtkpyte::kMouseRight, 0, 0, {}) ==
          std::string("\x1b[M") + char(35) + char(33) + char(33));
    // Past column 223 the classic encoding cannot express it; drop, don't wrap.
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 230, 0, {}).empty());
}

TEST_CASE("modifiers fold into the button code") {
    const MouseModes m = modes(1000, true);
    MouseMods mods;
    mods.ctrl = true;
    mods.shift = true;
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 0, 0, mods) == "\x1b[<20;1;1M");
}

TEST_CASE("motion depends on the tracking mode") {
    const MouseModes normal = modes(1000, true);
    const MouseModes button = modes(1002, true);
    const MouseModes any = modes(1003, true);

    CHECK(encode_mouse(normal, MouseAction::Motion, gtkpyte::kMouseLeft, 1, 1, {}).empty());
    CHECK(encode_mouse(button, MouseAction::Motion, gtkpyte::kMouseLeft, 1, 1, {}) ==
          "\x1b[<32;2;2M");
    CHECK(encode_mouse(button, MouseAction::Motion, -1, 1, 1, {}).empty());
    CHECK(encode_mouse(any, MouseAction::Motion, -1, 1, 1, {}) == "\x1b[<35;2;2M");
}

TEST_CASE("X10 reports presses only, without modifiers") {
    const MouseModes m = modes(9, true);
    MouseMods mods;
    mods.ctrl = true;
    CHECK(encode_mouse(m, MouseAction::Press, gtkpyte::kMouseLeft, 0, 0, mods) == "\x1b[<0;1;1M");
    CHECK(encode_mouse(m, MouseAction::Release, gtkpyte::kMouseLeft, 0, 0, {}).empty());
}

TEST_CASE("mode bookkeeping") {
    MouseModes m;
    CHECK(m.alternate_scroll);
    CHECK(m.apply(1002, true));
    CHECK(m.reporting());
    // Resetting a mode that is not the active one leaves tracking alone.
    m.apply(1000, false);
    CHECK(m.tracking == MouseModes::kButtonMotion);
    m.apply(1002, false);
    CHECK_FALSE(m.reporting());
    CHECK(m.apply(1007, false));
    CHECK_FALSE(m.alternate_scroll);
    CHECK_FALSE(m.apply(25, true));
}
