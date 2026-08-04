# PATCHES.md — every modification to original files

The `original/` tree is the Windows NT 4.0 SDK sample source, extracted
verbatim from `MSTOOLS/SAMPLES/OPENGL/SCRSAVE` and
`MSTOOLS/SAMPLES/OPENGL/GLAUX` on the Win32 SDK ISO (August 1996;
archive.org item `msdn-disc9-august-1996-0896-partno-92908`,
`1_WIN32SDK.iso`).  The first commit of this repository contains the
pristine extraction, so `git log -p -- original/` shows the exact diff of
every change below.

The porting rule was: **never touch simulation or drawing logic**.  All
Win32 scaffolding lives in replacement files under `port/`; the handful
of edits to original files below exist only because the code would not
compile or could not run at all otherwise.

## Source edits

### 1. `PIPES/STATE.CXX` — 3 sites: pre-standard `for`-scope (compile fix)

MSVC (pre-C++98) leaked `for (int i ...)` loop variables into the
enclosing scope, and the code relies on it.  Modern C++ rejects the
reuse.  Fixes preserve MSVC semantics exactly:

- `CalcTexRepFactors` (~line 186): second loop reused `i` from the first
  loop but re-initializes it; declared `int i` in the second loop.
- `LoadTextureFiles` (~line 213): `i` is used after the loop
  (`nTextures = i`); hoisted `int i;` above the loop.
- `STATE::Draw` (~line 688): `i` reused by the second loop; hoisted
  `int i;` above the first loop.

No behavior change; all three are scoping-only.

### 2. `COMMON/CLEAR.CXX` — 2 sites: dissolve-clear calibration clamp (runtime fix)

`SS_DIGITAL_DISSOLVE_CLEAR::CalibrateClear` times a test clear and sizes
the dissolve rectangles so the full-screen dissolve takes ~2 s.  WebGL
executes asynchronously — `glClear`/`glFlush` return before any work is
done — so the measured `elapsed` is ~0 and the computed `rectSize`
becomes 1.  At modern resolutions that is hundreds of thousands of
scissored clear+flush calls inside a single animation tick (tens of
seconds of frozen tab).  Both assignments that could produce a
pathological size now clamp to a minimum of 4 pixels, squarely inside
the range real period hardware produced.  The dissolve's code path and
its `ss_iRand` consumption pattern are otherwise untouched.

Marked with `/* PORT: ... */` comments at both sites.

## Compile-time renames (no source edits)

These apply to every original translation unit via the compiler command
line (see `build.sh`); the source text is unchanged.

- `-Drand=msvc_rand -Dsrand=msvc_srand` — routes the CRT RNG to MSVC's
  exact LCG (`seed*214013+2531011`, returns `(seed>>16)&0x7fff`),
  implemented in `port/msrand.c`.  musl's `rand()` is a different
  generator; every pipe-turn/joint/teapot/material decision would
  diverge from a real Windows machine.
- `-include port/glwrap.h` — force-included header that:
  - redefines `RAND_MAX` to MSVC's `0x7fff` after `<stdlib.h>`
    (musl says `0x7fffffff`, which rescales every probability in
    `ss_iRand`/`ss_iRand2`/`ss_fRand` to ~zero: no elbow/ball mix, no
    teapot odds, no flex variation);
  - renames 18 GL immediate-mode/state calls (`glBegin`, `glVertex3f`,
    `glEnable`, `glMaterialfv`, ...) to the `pipes_gl*` entry points of
    `port/gl11compat.c`, which supplies what emscripten's
    `-sLEGACY_GL_EMULATION` lacks: display lists, GL 1.x evaluators
    (`glMap2f`/`glMapGrid2f`/`glEvalMesh2` + `GL_AUTO_NORMAL`),
    `GL_QUAD_STRIP`/`GL_POLYGON` begin modes, per-vertex re-emission of
    current normal/texcoord (the emulation's interleaved stream can't
    express GL's current-attribute model), and virtual WGL contexts.

## Filesystem accommodations (no source edits)

- ISO9660 extraction yields UPPERCASE filenames; the source includes
  lowercase names (`#include "sspipes.h"`).  `build.sh` generates a
  lowercase **symlink farm** under `build/lc/` instead of renaming or
  editing anything.
- Some includes use backslashes (`#include <GL\gl.h>`,
  `<sys\timeb.h>`).  `port/shim/` contains files literally named
  `GL\gl.h`, `GL\glaux.h` and `sys\timeb.h` (backslash is a valid
  filename character on Linux) that forward to the real headers.

## Files replaced rather than compiled

The Win32 framework files are **not** compiled; `port/ssshell.c`
implements their interface (callback registration, lifecycle, settings,
resources, timers) on the browser, reproducing the original order of
operations documented in its header comment:

| Original | Role | Replacement |
|---|---|---|
| `COMMON/SCRNSAVE.CXX` | message pump, password handling | `port/ssshell.c` main loop (16 ms `setTimeout`, the release build's `SetTimer` interval from SSWPROC.CXX:58) |
| `COMMON/SSWPROC.CXX` | wndprocs, WM_TIMER | same |
| `COMMON/SSWINDOW.CXX` | window/GL-context management | same + `port/gl11compat.c` virtual WGL contexts |
| `COMMON/SSINIT.CXX` | callback registration | same |
| `COMMON/GLSCRNSV.CXX` | SCRNSAVE class, pixel formats | same (single-buffered semantics via `preserveDrawingBuffer`) |
| `COMMON/SSUTIL.CXX` | pixel formats, OS/GL queries | `ss_QueryGLVersion` et al. in `port/ssshell.c` (GL 1.1 = true) |
| `COMMON/PALETTE.CXX` | 256-color palettes | dropped (no palettized displays) |
| `COMMON/DLGDRAW.CXX` | config-dialog GL preview | dropped |
| `COMMON/SSDIB.C`, `SSIMAGE.C`, `SSA8.C`, `FASTDIB.C` | GDI/DIB image decoding | `ss_DIBImageLoad` (pure-C BMP parser, same output layout) in `port/ssshell.c` |
| GLAUX `TK*.C`, `GLAUX.C`, `SHAPES.C`, ... | aux windowing/imaging | only `TEAPOT.C` is used and it is compiled **original**; `auxDIBImageLoad` reimplemented over the BMP parser |
| `scrnsave.lib`, registry APIs | Win32 | `LoadString`/`GetPrivateProfileInt` shims over a JS-settable store with the original value names (`[Screen Saver.3DPipes]` `JointType`/`SurfStyle`/`Flex`/`MultiPipes`/`Tesselation`/`TextureQuality`/`Texture` in `control.ini`) |
| resource section (`SSPIPES.RC`) | STRIPE.BMP payload | `port/stripe_bmp.h`, byte-for-byte embed of `PIPES/STRIPE.BMP`, served through `FindResource(99, RT_MYBMP)` exactly as the original looked it up |

Everything else — all 13 `PIPES/*` translation units, `COMMON/UTIL.CXX`
(RNG helpers), `MATERIAL.C`, `MATH.C`, `COLOR.C`, `CLEAR.CXX`,
`TEXTURE.C`, `COMMON/DIALOG.C` (registry helpers), `PIPES/DIALOG.C`
(`getIniSettings` + defaults), and GLAUX `TEAPOT.C` — is compiled from
the original sources.
