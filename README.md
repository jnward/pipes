# 3D Pipes (OpenGL) — the original, in your browser

The Microsoft **3D Pipes** screensaver (`sspipes.scr`), compiled to
WebAssembly from the original C++ source that shipped as sample code in
the Windows NT 4.0 SDK (August 1996) — not a reimplementation.  Same
pipe-growth algorithm, same RNG-driven decisions (MSVC's exact `rand()`
LCG), same display-list joint geometry, same GLAUX teapot easter egg
(1/1000 per joint, mixed-joints mode, as ever).

## Run it

```
./build.sh                 # or use the checked-in dist/
python3 -m http.server     # any static server
# open http://localhost:8000/
```

Settings ride the URL (original `control.ini` value names):

```
?Flex=1                          curvy (flex) pipes
?JointType=0|1|2|3               elbow / ball / mixed / cycle
?SurfStyle=1                     textured pipes (the classic STRIPE.BMP)
?MultiPipes=0                    single pipe
?Tesselation=0..200              surface detail (100 = default)
?TextureQuality=1                linear filtering
?Seed=12345                      reproducible run (omit = time-seeded,
                                 srand(millitm), like the original)
```

Defaults match an NT4 machine: up to 4 simultaneous pipes, mixed
joints, solid surface.

## What's in the repo

- `original/` — pristine SDK source (SCRSAVE + GLAUX), plus the five
  one-line-scale fixes documented in `PATCHES.md` (git history holds
  the untouched extraction).
- `port/` — the replacement for Win32: a small browser shell
  (`ssshell.c`), MSVC's RNG (`msrand.c`), and a GL 1.1 compatibility
  layer (`gl11compat.c`: display lists, evaluators, quad strips,
  virtual WGL contexts) over emscripten's `-sLEGACY_GL_EMULATION`.
- `build.sh` — fetches the SDK ISO from archive.org if `original/` is
  absent, builds mesa GLU's utility library to wasm, compiles and links
  everything into `dist/`.
- `PATCHES.md` — every modification to original files, with
  justification.
- `WEB-DIFFERENCES.md` — the short list of places the browser cannot
  behave exactly like 1996, and why.

Building requires a case-sensitive filesystem (Linux or WSL): the
repo tracks shim files with backslashes in their names (matching the
originals' `#include <GL\gl.h>` directives) and a lowercase `gl -> GL`
symlink, neither of which survives checkout on NTFS or default macOS.

Microsoft SDK sample code © 1994–1996 Microsoft Corporation; teapot
code © 1993 Silicon Graphics, Inc. (permission notice in TEAPOT.C).
