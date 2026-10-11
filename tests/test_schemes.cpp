// tests/test_schemes.cpp
#include <doctest/doctest.h>

#include <cstring>
#include <set>
#include <string>

#include "palette.h"
#include "schemes.h"

TEST_CASE("scheme names are unique and found by name") {
    std::size_t count = 0;
    const gtkpyte::ColorScheme *all = gtkpyte::schemes(count);
    REQUIRE(count > 0);
    std::set<std::string> names;
    for (std::size_t i = 0; i < count; ++i) {
        CHECK(names.insert(all[i].name).second);
        CHECK(gtkpyte::find_scheme(all[i].name) == &all[i]);
    }
    CHECK(gtkpyte::find_scheme("no-such-scheme") == nullptr);
    CHECK(gtkpyte::find_scheme(nullptr) == nullptr);
}

TEST_CASE("the xterm scheme is the widget's default palette") {
    const gtkpyte::ColorScheme *xterm = gtkpyte::find_scheme("xterm");
    REQUIRE(xterm != nullptr);
    const gtkpyte::Palette defaults;
    const auto hex = [](const GdkRGBA &c) {
        return (static_cast<unsigned>(c.red * 255.0f + 0.5f) << 16) |
               (static_cast<unsigned>(c.green * 255.0f + 0.5f) << 8) |
               static_cast<unsigned>(c.blue * 255.0f + 0.5f);
    };
    CHECK(hex(defaults.foreground()) == xterm->foreground);
    CHECK(hex(defaults.background()) == xterm->background);
    for (int i = 0; i < 16; ++i) {
        CHECK(hex(defaults.ansi(i)) == xterm->ansi[i]);
    }
}
