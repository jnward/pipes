#!/usr/bin/env bash
#
# build.sh — build the original NT4 3D Pipes screensaver to WebAssembly.
#
# Stages (each skipped when its output already exists):
#   1. original/  — the pristine SDK source.  If missing, downloads the
#      Win32 SDK ISO (Aug 1996, archive.org) and extracts
#      MSTOOLS/SAMPLES/OPENGL/SCRSAVE and MSTOOLS/SAMPLES/OPENGL/GLAUX.
#   2. emsdk      — uses emcc from PATH if available, otherwise installs
#      emsdk into ./emsdk.
#   3. GLU        — downloads mesa glu 9.0.3 and compiles src/libutil
#      (gluPerspective, gluScaleImage) to a wasm static library.
#   4. lowercase symlink farm — the ISO files are UPPERCASE, the source
#      includes lowercase names; symlinks bridge the two without
#      touching the originals.
#   5. compile + link into dist/sspipes.{js,wasm}; open index.html via
#      any static file server to run.
#
set -euo pipefail
cd "$(dirname "$0")"

ISO_URL="https://archive.org/download/msdn-disc9-august-1996-0896-partno-92908/1_WIN32SDK.iso"
GLU_URL="https://archive.mesa3d.org/glu/glu-9.0.3.tar.xz"
GLU_URL_ALT="https://mesa.freedesktop.org/archive/glu/glu-9.0.3.tar.xz"

SCRSAVE=original/MSTOOLS/SAMPLES/OPENGL/SCRSAVE
GLAUX=original/MSTOOLS/SAMPLES/OPENGL/GLAUX

# ---- 1. original source -------------------------------------------------
if [ ! -f "$SCRSAVE/PIPES/SSPIPES.CXX" ]; then
    echo "==> original/ not present; fetching Win32 SDK ISO (~640 MB)"
    command -v 7z >/dev/null || { echo "need 7z (p7zip-full) to extract the ISO"; exit 1; }
    mkdir -p dl
    [ -f dl/WIN32SDK.iso ] || curl -SL --retry 4 -o dl/WIN32SDK.iso "$ISO_URL"
    7z x -y dl/WIN32SDK.iso "MSTOOLS/SAMPLES/OPENGL/SCRSAVE" "MSTOOLS/SAMPLES/OPENGL/GLAUX" -ooriginal >/dev/null
    echo "==> extracted $SCRSAVE"
    echo "NOTE: a fresh extraction lacks the patches listed in PATCHES.md;"
    echo "      use the checked-in original/ tree for the patched build."
fi

# ---- 2. emscripten -------------------------------------------------------
if ! command -v emcc >/dev/null; then
    if [ ! -d emsdk ]; then
        echo "==> installing emsdk"
        git clone --depth 1 https://github.com/emscripten-core/emsdk.git
        ./emsdk/emsdk install latest
        ./emsdk/emsdk activate latest
    fi
    source ./emsdk/emsdk_env.sh
fi
echo "==> using $(emcc --version | head -1)"

# ---- 3. GLU (gluPerspective + gluScaleImage) -----------------------------
if [ ! -f dl/glu-9.0.3/build/libgluutil.a ]; then
    echo "==> building mesa GLU libutil"
    mkdir -p dl
    [ -f dl/glu-9.0.3.tar.xz ] || curl -SL --retry 3 -o dl/glu-9.0.3.tar.xz "$GLU_URL" \
        || curl -SL --retry 3 -o dl/glu-9.0.3.tar.xz "$GLU_URL_ALT"
    tar -C dl -xf dl/glu-9.0.3.tar.xz
    mkdir -p dl/glu-9.0.3/build
    for f in dl/glu-9.0.3/src/libutil/*.c; do
        emcc -c -O2 -Idl/glu-9.0.3/include -Idl/glu-9.0.3/src/include \
             -Idl/glu-9.0.3/src/libutil "$f" \
             -o "dl/glu-9.0.3/build/$(basename "$f" .c).o"
    done
    emar rcs dl/glu-9.0.3/build/libgluutil.a dl/glu-9.0.3/build/*.o
fi

# ---- 4. lowercase symlink farm -------------------------------------------
mkdir -p build/lc/pipes build/lc/common build/lc/glaux build/obj
for f in "$SCRSAVE"/PIPES/*;  do ln -sf "../../../$f" "build/lc/pipes/$(basename "$f"  | tr 'A-Z' 'a-z')"; done
for f in "$SCRSAVE"/COMMON/*; do ln -sf "../../../$f" "build/lc/common/$(basename "$f" | tr 'A-Z' 'a-z')"; done
for f in "$GLAUX"/*;          do ln -sf "../../../$f" "build/lc/glaux/$(basename "$f"  | tr 'A-Z' 'a-z')"; done

# ---- 5. compile + link ----------------------------------------------------
# Original sources: compiled with
#   -include port/glwrap.h  (GL wrapper renames + MSVC RAND_MAX, PATCHES.md)
#   -Drand=msvc_rand -Dsrand=msvc_srand  (MSVC LCG, port/msrand.c)
CFLAGS_ORIG="-O2 -Iport/shim -Ibuild/lc/common -Ibuild/lc/pipes \
    -Idl/glu-9.0.3/include -include port/glwrap.h \
    -Drand=msvc_rand -Dsrand=msvc_srand \
    -Wno-nonportable-include-path -Wno-writable-strings -Wno-comment"

echo "==> compiling original PIPES sources"
for f in node xc objects eval view pipe npipe fpipe nstate fstate state sspipes; do
    em++ -c $CFLAGS_ORIG "build/lc/pipes/$f.cxx" -o "build/obj/$f.o"
done
# .c files are compiled as C (emcc), matching the original build's language mode
emcc -c $CFLAGS_ORIG build/lc/pipes/dialog.c -o build/obj/dialog.o

echo "==> compiling original COMMON sources"
for f in util.cxx clear.cxx; do
    em++ -c $CFLAGS_ORIG "build/lc/common/$f" -o "build/obj/common_${f%.*}.o"
done
for f in material.c math.c color.c texture.c dialog.c; do
    emcc -c $CFLAGS_ORIG "build/lc/common/$f" -o "build/obj/common_${f%.*}.o"
done

echo "==> compiling original GLAUX teapot"
emcc -c -O2 -Iport/shim -Ibuild/lc/common -Ibuild/lc/glaux \
    -include port/glwrap.h -Drand=msvc_rand -Dsrand=msvc_srand \
    -Wno-nonportable-include-path -Wno-comment \
    build/lc/glaux/teapot.c -o build/obj/glaux_teapot.o

echo "==> compiling port layer"
emcc -c -O2 port/msrand.c -o build/obj/msrand.o
emcc -c -O2 -Iport/shim port/gl11compat.c -o build/obj/gl11compat.o
emcc -c -O2 -Iport/shim -Iport port/ssshell.c -o build/obj/ssshell.o

echo "==> linking dist/sspipes.js"
mkdir -p dist
em++ build/obj/*.o dl/glu-9.0.3/build/libgluutil.a -O2 \
    -sLEGACY_GL_EMULATION=1 -sGL_UNSAFE_OPTS=0 -sALLOW_MEMORY_GROWTH=1 \
    -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$Browser' \
    -sEXPORTED_FUNCTIONS=_main,_pipes_set_setting,_pipes_set_texture_path,_pipes_resize,_msvc_srand,_malloc,_free,_pipes_debug_matrices \
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToNewUTF8 \
    -o dist/sspipes.js

echo "==> done: dist/sspipes.js + dist/sspipes.wasm"
echo "    serve the repo root (e.g. python3 -m http.server) and open index.html"
