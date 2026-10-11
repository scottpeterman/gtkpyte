# pyte C++

The terminal emulation core under gtkpyte: a headless C++17 terminal
emulator. Bytes in, screen buffer out; no GUI, no platform code, no event
loop.

This is **not** the Python [pyte](https://github.com/selectel/pyte) library.
It is Scott Peterman's C++ port of
[gopyte](https://github.com/scottpeterman/pathfinderssh/tree/main/internal/gopyte), his earlier Go port of Python
pyte, and it lives upstream in
[anytermqt](https://github.com/scottpeterman/anytermqt), where the Qt widget
(qtpyte) uses it. The directory and `pyte::` namespace keep their upstream
names so the two copies stay easy to diff.

Python pyte (George Shuklin, Sergei Lebedev and contributors, LGPL-3.0) is
the design this follows: a `Stream` that parses bytes and calls a `Screen`
that holds the buffer. Attribution and licences: `../THIRD-PARTY-NOTICES.md`.

```
include/pyte/   Cell, RowStore, Screen, Stream, unicode helpers
src/            implementation
tests/          doctest suite (meson test)
tools/ptdump/   feed a capture, print the resulting screen
```

Changes made here and not yet carried back to anytermqt:

- `RowStore::drop_bottom()` and the shrink path in `Screen::resize()`:
  blank rows below the cursor are dropped before content above it is pushed
  into scrollback (`tests/test_resize.cpp`).
