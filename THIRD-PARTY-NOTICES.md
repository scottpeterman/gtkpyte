# Third-party notices

gtkpyte is licensed under the GNU General Public License v3.0; see
`LICENSE`. It incorporates or links the components below. Full licence
texts are in `licenses/`, and `meson install` places this file and that
directory under `share/doc/gtkpyte/`.

Maintained by hand: update this file when a dependency is added, removed
or re-pinned.

## Compiled into gtkpyte

### pyte C++ core (`pyte/`) -- not the Python pyte library

- Copyright Scott Peterman
- GNU General Public License v3.0
- Vendored from https://github.com/scottpeterman/anytermqt

The headless terminal emulator under the widget: escape-sequence parser,
screen buffer, scrollback. It shares a name with Python pyte because it
descends from it, but it is a separate implementation, and the third step
of a line of ports by the same author:

1. **pyte** (Python) -- the original, by George Shuklin, Sergei Lebedev and
   contributors. See below.
2. **gopyte** (Go) -- Scott Peterman's port of Python pyte to Go, and the
   first of these ports. GPL-3.0; its public copy lives in PathfinderSSH:
   https://github.com/scottpeterman/pathfinderssh/tree/main/internal/gopyte
3. **pyte C++** (C++17) -- Scott Peterman's port of gopyte, living in
   anytermqt and vendored here.

The Qt widget (qtpyte) and this GTK widget (gtkpyte) both sit on step 3.

### pyte (Python), the original

- Copyright George Shuklin, Sergei Lebedev and contributors
- GNU Lesser General Public License v3.0 (`licenses/LGPL-3.0.txt`)
- https://github.com/selectel/pyte

The design and behaviour the ports follow: the split between a byte-stream
parser and a screen it drives, and how each escape sequence acts on the
screen. The ports are translations rather than copies, and as derived works
they are conveyed under GPL-3.0, which LGPL-3.0 permits.

### utf8proc

- Copyright 2014-2021 Steven G. Johnson, Jiahao Chen, Tony Kelman,
  Jonas Fonseca and other contributors
- Copyright 2009, 2013 Public Software Group e. V., Berlin, Germany
- MIT "expat" licence
- Unicode data tables: Copyright 1991-2007 Unicode, Inc., distributed under
  the Unicode Terms of Use
- Version 2.11.3, fetched by `subprojects/utf8proc.wrap` and statically
  linked
- https://github.com/JuliaStrings/utf8proc

UTF-8 decoding and the East Asian width table. Full text, including the
Unicode data licence: `licenses/utf8proc.md`.

## Linked from the system

Dynamically linked against the unmodified shared libraries the platform
provides. gtkpyte is conveyed under GPL-3.0, which these licences permit.

### GTK 4, GLib, Pango

- Copyright the GTK, GLib and Pango authors
- GNU Lesser General Public License v2.1 or later (`licenses/LGPL-2.1.txt`)
- https://gtk.org/

Widget toolkit, main loop and text layout. Graphene (geometry types used by
GTK's rendering API) is pulled in with GTK and is MIT-licensed.

### GNU C Library (forkpty, libutil)

- Copyright the Free Software Foundation and contributors
- GNU Lesser General Public License v2.1 or later
- https://www.gnu.org/software/libc/

## Build and test only (not in any installed binary)

### doctest

- Copyright 2016-2023 Viktor Kirilov
- MIT licence (`licenses/doctest.txt`)
- Version 2.5.3, fetched by `subprojects/doctest.wrap`
- https://github.com/doctest/doctest

Unit test framework for `pyte_tests` and `gtkpyte_tests`.

### Meson, Ninja

Build tools; nothing from either is included in the output.

## Colour schemes

The built-in schemes in `src/schemes.cpp` reproduce published colour
values. Credited to their designers:

| Scheme | Designer | Licence of the original |
|---|---|---|
| solarized-dark, solarized-light | Ethan Schoonover, https://ethanschoonover.com/solarized/ | MIT, Copyright 2011 Ethan Schoonover |
| dracula | Dracula Theme, https://draculatheme.com/ | MIT, Copyright 2023 Dracula Theme |
| gruvbox-dark | morhetz, https://github.com/morhetz/gruvbox | MIT/X11 |
| nord | Sven Greb, https://www.nordtheme.com/ | MIT, Copyright 2016-present Sven Greb |
| tango-dark, tango-light | Tango Desktop Project palette | |
| xterm | xterm's default ANSI colours | |
