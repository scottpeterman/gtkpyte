// tests/test_keymap.cpp
#include <doctest/doctest.h>

#include "keymap.h"

using gtkpyte::encode_key;
using gtkpyte::encode_text;
using gtkpyte::KeyModes;

namespace {
constexpr auto kNone = static_cast<GdkModifierType>(0);
GdkModifierType mods(int m) { return static_cast<GdkModifierType>(m); }
}  // namespace

TEST_CASE("printable keys send their UTF-8") {
    CHECK(encode_key(GDK_KEY_a, kNone, {}) == "a");
    CHECK(encode_key(GDK_KEY_A, mods(GDK_SHIFT_MASK), {}) == "A");
    CHECK(encode_key(GDK_KEY_eacute, kNone, {}) == "\xc3\xa9");
}

TEST_CASE("bare modifiers send nothing") {
    CHECK(encode_key(GDK_KEY_Shift_L, mods(GDK_SHIFT_MASK), {}).empty());
    CHECK(encode_key(GDK_KEY_Control_R, mods(GDK_CONTROL_MASK), {}).empty());
}

TEST_CASE("control characters") {
    CHECK(encode_key(GDK_KEY_c, mods(GDK_CONTROL_MASK), {}) == "\x03");
    CHECK(encode_key(GDK_KEY_bracketleft, mods(GDK_CONTROL_MASK), {}) == "\x1b");
    CHECK(encode_key(GDK_KEY_space, mods(GDK_CONTROL_MASK), {}) == std::string(1, '\0'));
}

TEST_CASE("alt prefixes ESC") {
    CHECK(encode_key(GDK_KEY_b, mods(GDK_ALT_MASK), {}) == "\x1b" "b");
}

TEST_CASE("cursor keys follow DECCKM") {
    KeyModes normal;
    KeyModes app;
    app.application_cursor = true;
    CHECK(encode_key(GDK_KEY_Up, kNone, normal) == "\x1b[A");
    CHECK(encode_key(GDK_KEY_Up, kNone, app) == "\x1bOA");
    // A modified cursor key is CSI even in application mode.
    CHECK(encode_key(GDK_KEY_Right, mods(GDK_CONTROL_MASK), app) == "\x1b[1;5C");
}

TEST_CASE("editing and function keys") {
    CHECK(encode_key(GDK_KEY_Return, kNone, {}) == "\r");
    CHECK(encode_key(GDK_KEY_BackSpace, kNone, {}) == "\x7f");
    CHECK(encode_key(GDK_KEY_ISO_Left_Tab, mods(GDK_SHIFT_MASK), {}) == "\x1b[Z");
    CHECK(encode_key(GDK_KEY_Delete, kNone, {}) == "\x1b[3~");
    CHECK(encode_key(GDK_KEY_Page_Up, mods(GDK_SHIFT_MASK), {}) == "\x1b[5;2~");
    CHECK(encode_key(GDK_KEY_F1, kNone, {}) == "\x1bOP");
    CHECK(encode_key(GDK_KEY_F5, kNone, {}) == "\x1b[15~");
}

TEST_CASE("paste converts newlines and brackets on request") {
    CHECK(encode_text("a\nb\r\nc", false) == "a\rb\rc");
    CHECK(encode_text("x", true) == "\x1b[200~x\x1b[201~");
}
