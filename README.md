# Color Linez (Win32 reconstruction)

A buildable, from-scratch source reconstruction of **Color Linez v1.21**
(`WINLINEZ.EXE`, 60 KB, 1999) — the Win32 port of the 1992 DOS classic
*Color Lines* — recovered entirely by static analysis of the original
binary.

```
 Lines of 5+ same-colour balls score points.  Beat the "king"'s score
 (Handicap, 100 points) and your king takes the crown.
```

| | |
|---|---|
| Original DOS game | *Color Lines* (c) 1992 GAMOS LTD, Russia — code: Olga Demina, art: Igor Ivkin & Gennady Denisov |
| Win32 port | (c) 1998-1999 Ivan Golubev (m53group) — freeware, "written as a birthday present to my brother" |
| Original toolchain | GCC/egcs (mingw32) + GNU ld, no CRT, hand-written startup |
| This reconstruction | clean C99 + Win32 API, builds with MinGW (i686) via CMake |

The original binary is available at
<https://archive.org/details/ColorLinez_1020>.

## Project layout

```
color-linez/
├── CMakeLists.txt                     build script (native or mingw-cross)
├── cmake/mingw-i686.toolchain.cmake   cross toolchain file
├── src/
│   ├── winlinez.c                     all game logic (original addresses
│   │                                  quoted as FUN_004xxxxx in comments)
│   ├── inflate.c                      the raw-DEFLATE decompressor embedded
│   │                                  in the original EXE, rewritten cleanly
│   ├── gfxdata.c                      the compressed sprite sheet, extracted
│   │                                  verbatim from the original .data section
│   ├── resource.h / res/winlinez.rc   resources byte-matched to the original
│   └── test_inflate.c                 CRC-32 regression test for gfxdata
└── res/
    ├── icon.ico                       game icon (re-packed from RT_ICON)
    ├── about_king.bmp / about_mask.bmp  About-dialog portraits
    └── sprites.bmp                    decompressed 511x585 sprite sheet
                                       (reference copy; the build embeds the
                                       compressed blob from src/gfxdata.c)
```

## Build

Requirements: a 32-bit Windows compiler (MinGW-w64 i686 recommended; an
MSVC toolchain also works). On Linux/Unix, install `gcc-mingw-w64-i686`
— CMake detects it automatically.

```sh
cmake -B build
cmake --build build
```

Explicit toolchain (optional):

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.toolchain.cmake
cmake --build build
```

Output: `build/winlinez.exe` (runs natively on Windows, or under
Wine on Unix — `wine build/winlinez.exe`).

## Test

```sh
ctest --test-dir build
```

Verifies that the embedded sprite blob inflates to the original
300,598-byte bitmap (CRC-32 `cd043a57`).

## How faithful is it?

* All game logic (BFS path-finding, scoring, king race, hiscores,
  statistics, menus/dialogs, animations) follows the original code,
  with original binary addresses quoted in comments (`FUN_004xxxxx`).
* Graphics pipeline identical: a raw-DEFLATE-compressed 511x585 8bpp
  BMP is embedded in `.data` and inflated at WM_CREATE into a DIB +
  palette — the same bytes, the same inflater logic.
* Resources (menu incl. the right-justified `MF_HELP` Help popup,
  accelerators, five dialogs, icon, About bitmaps) are byte-matched
  against the original templates.
* Original quirks are preserved on purpose (7 ball colours with value 8
  reserved as the path-finder seed marker, the `>7` flood-mark wipe,
  per-colour statistics credited to the last removed colour, ...).

Differences worth knowing about are documented in `src/winlinez.c`
header comments and the fix log below.

## Credits

* Original game: GAMOS LTD 1992 — Olga Demina (code),
  Igor Ivkin & Gennady Denisov (art).
* Win32 port: Ivan Golubev, 1998-1999, m53group.
  Original download: <https://archive.org/details/ColorLinez_1020>.
* Reconstruction: reverse-engineered with Ghidra by the **Ox Alpha** model
  running [OpenCode](https://opencode.ai) — decompilation, resource
  parsing, sprite-sheet recovery and the rewritten C source were all
  produced in that session.
