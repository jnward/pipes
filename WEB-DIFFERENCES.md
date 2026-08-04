# Where the web build cannot match the original exactly

The simulation, geometry, RNG stream and defaults are the original
code's.  These are the places where the browser platform forces an
observable difference, and why.

1. **The dissolve/wipe clears are not visible as animations.**
   The original renders single-buffered to the front buffer, so the
   digital-dissolve scene clear (CLEAR.CXX) appeared progressively over
   ~2 seconds, `glFlush` by `glFlush`.  A browser composites the canvas
   only when the JS task yields; the whole dissolve runs inside one
   16 ms tick, so a scene reset appears as an instant clear.  Making it
   visible would require rewriting `STATE::FrameReset`'s synchronous
   structure — exactly the kind of edit this port refuses to make.
   Related: the calibration clamp documented in PATCHES.md.

2. **Mid-tick partial drawing is not visible.**
   Same mechanism: the original's per-pipe `glFlush` (STATE.CXX:736,
   936, 964) made each pipe's segment appear the instant it was drawn.
   In the browser all pipes drawn in one tick appear together.  At the
   original's 16 ms cadence this is one frame's worth of difference —
   invisible in practice, but philosophically a swap-per-tick rather
   than true front-buffer rendering.  `preserveDrawingBuffer: true`
   provides the frame-to-frame accumulation the original relied on.

3. **Quad diagonals.** `GL_QUAD_STRIP` is drawn as `GL_TRIANGLE_STRIP`
   (identical vertex order and surface; WebGL has no quads).  Each quad
   is split along a fixed diagonal, whereas native GL drivers split
   quads as they pleased — with smooth-shaded lighting the difference
   is not visually detectable.

4. **Back-face materials are dropped.** MATERIAL.C sets GL_BACK
   materials alongside GL_FRONT; emscripten's emulation supports only
   GL_FRONT/GL_FRONT_AND_BACK.  Pipes enables back-face culling
   (STATE.CXX:280-281), so back materials never reach the screen.

5. **Wireframe surface style (`SurfStyle=2`) does not render as lines.**
   It uses `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` (STATE.CXX:62),
   which neither WebGL nor the emulation supports.  It was never a
   default; solid and textured are faithful.

6. **Timer granularity.** The release build re-arms a 16 ms `SetTimer`;
   NT4's timer actually fired at ~10–16 ms granularity depending on the
   machine.  The port uses a 16 ms `setTimeout` main loop
   (`EM_TIMING_SETTIMEOUT`), i.e. the nominal spec of the original,
   not any particular machine's jitter.  RequestAnimationFrame is
   deliberately not used: on a 120 Hz display it would double the
   animation speed.

7. **RNG seeding.** `ss_RandInit` seeds with the millisecond field of
   the wall clock (`srand(time.millitm)`, UTIL.CXX:167) — only 1000
   possible startup sequences, faithfully reproduced.  The port adds an
   optional `Seed` override (via MSVC's LCG, see PATCHES.md) for
   reproducibility; leaving it unset gives original behavior.
   Note that even on real Windows, decision sequences diverge across
   machines after the first scene reset, because the dissolve clear
   consumes a hardware-speed-dependent number of `ss_iRand` calls.

8. **Multiple GL contexts are virtualized.** Each pipe "draw thread"
   gets its own `wglCreateContext` with `wglShareLists` in the
   original; per-context state (accumulated modelview, material,
   texture binding, front face, evaluator enables) is shadowed and
   swapped over the single WebGL context (port/gl11compat.c).  State
   the original relied on is preserved; state it never varied
   per-context (projection, lighting, depth) is shared as before.

9. **Evaluator arithmetic.** Flex-pipe surfaces and the teapot go
   through the port's C implementation of GL 1.x 2D evaluators
   (Bernstein basis, `GL_AUTO_NORMAL` = normalized du×dv per the GL 1.1
   spec) rather than Microsoft's software OpenGL.  Same math, but not
   bit-identical floating point — differences are sub-pixel.

10. **Config-dialog defaults vs. shipped defaults.** With no registry,
    the SDK sample's compiled-in defaults are single pipe, elbow
    joints, tesselation 0.  The famous NT4 look (up to 4 simultaneous
    pipes, mixed joints) came from registry values.  The port's
    settings store boots with `MultiPipes=1`, `JointType=2` (mixed),
    `Tesselation=100` (16 slices — the mid-slider value matching the
    remembered NT4 appearance); all overridable per run from
    `window.PIPES_CONFIG` / the URL query string, including down to the
    SDK fallbacks.
