# OpenGL / GLU / GLAUX / WGL Inventory — 3D Pipes (NT4 SDK SCRSAVE)

Scope: `PIPES/*.CXX`, `PIPES/*.H`, and `COMMON/{MATERIAL.C, CLEAR.CXX, TEXTURE.C, GLSCRNSV.CXX, PALETTE.CXX, UTIL.CXX, SSUTIL.CXX}` under
`/home/user/pipes/original/MSTOOLS/SAMPLES/OPENGL/SCRSAVE/`. All paths below are relative to that directory. Adjacent COMMON files are noted where they matter for the port (DLGDRAW.CXX, SSWINDOW.CXX).

Grep patterns used: `\b(gl|glu|aux|wgl)[A-Z][A-Za-z0-9_]*` plus `GetProcAddress`, plus targeted searches for evaluators, buffers, and exotic features. 228 raw identifier hits total in the scoped files.

---

## 1. Per-file inventory (every distinct GL/GLU/GLAUX/WGL function)

### PIPES/EVAL.CXX  (flex-pipe evaluator engine)
Includes `<GL/gl.h>`, `<GL/glu.h>`, `<GL/glaux.h>` (lines 17–19).
- `glEnable` — EVAL.CXX:107 `GL_MAP2_TEXTURE_COORD_2` (only if textured), :109 `GL_MAP2_VERTEX_3`, :110 `GL_AUTO_NORMAL` (all inside `ResetEvaluator()`)
- `glFrontFace(GL_CW)` — EVAL.CXX:111 (flex geometry is clockwise-wound; comment notes normal pipes need CCW)
- `glMap2f` — EVAL.CXX:369 (`GL_MAP2_TEXTURE_COORD_2`, TEX_ORDER×TEX_ORDER control net) and :377 (`GL_MAP2_VERTEX_3`, uOrder×vOrder control net)
- `glMapGrid2f` — EVAL.CXX:384
- `glEvalMesh2( GL_FILL, 0, uDiv, 0, vDiv )` — EVAL.CXX:385
- Debug only, compiled under `#if EVAL_DBG` (off by default, line 25 `//#define EVAL_DBG 1`): `glColor3f` :402, `glPointSize` :403, `glBegin(GL_POINTS)` :405, `glVertex3fv` :407, `glEnd` :409

### PIPES/FPIPE.CXX  (flex pipe drawing — matrix ops only; geometry goes through EVAL)
- `glPushMatrix` — 228, 293
- `glPopMatrix` — 243, 315, 364, 460
- `glTranslatef` — 526, 541, 572
- `glRotatef` — 529, 532, 535, 538
- Creates/drives the evaluator: `new EVAL(bTexture)` FPIPE.CXX:59; `pEval->ProcessXCPrimBendSimple` :522, `ProcessXCPrimLinear` :569, `ProcessXCPrimSingularity` :608, `SetTextureControlPoints` :640 — each ends in `EVAL::Evaluate()` (EVAL.CXX:352) which issues the glMap2f/glMapGrid2f/glEvalMesh2 sequence.

### PIPES/NPIPE.CXX  (normal pipe drawing — matrix ops only; geometry via display lists in OBJECTS.CXX)
- `glPushMatrix` — 80, 142, 202, 208, 319, 346
- `glPopMatrix` — 94, 111, 164, 212, 217, 329, 362
- `glTranslatef` — 105, 160, 324, 337, 351, 364, 372
- `glRotatef` — 413, 459

### PIPES/OBJECTS.CXX  (display-list geometry builders: cylinder, elbow, ball joint, sphere)
Includes `<GL/gl.h>` only (line 15).
- `glGenLists(1)` — 28 (`OBJECT::OBJECT`)
- `glDeleteLists(listNum, 1)` — 37 (`OBJECT::~OBJECT`)
- `glCallList(listNum)` — 49 (`OBJECT::Draw`)
- `glNewList(listNum, GL_COMPILE)` — 302 (elbow), 430 (ball joint), 526 (cylinder), 623 (sphere)
- `glEndList` — 329, 465, 551, 711
- `glBegin` — 175 `GL_QUAD_STRIP` (MakeQuadStrip), 532 `GL_QUAD_STRIP` (cylinder), 643 & 663 `GL_TRIANGLE_FAN` (sphere caps, untextured only), 687 `GL_QUAD_STRIP` (sphere body)
- `glEnd` — 188, 548, 653, 672, 708
- `glNormal3fv` — 178, 182; `glNormal3f` — 534, 639, 647, 660, 666, 689, 698
- `glTexCoord2f` — 180, 184, 536, 542, 693, 702
- `glVertex3fv` — 181, 185; `glVertex3f` — 539, 545, 644, 650, 664, 669, 696, 705
- Note: comment `// 'glu' routines` at OBJECTS.CXX:468 — the cylinder/sphere builders are GLU-quadric-derived code **inlined** into the file; no actual GLU calls.

### PIPES/PIPE.CXX  (shared pipe base; teapot easter egg)
- `glTranslatef` — 222; `glRotatef` — 273, 276, 279, 282, 285, 288 (align/translate to node directions)
- `PIPE::DrawTeapot()` PIPE.CXX:61-70: `glFrontFace(GL_CW)` :63, `glEnable(GL_NORMALIZE)` :64, **`auxSolidTeapot(2.5 * radius)`** :65, `glDisable(GL_NORMALIZE)` :66, `glFrontFace(GL_CCW)` :67, then calls `ResetEvaluator()` if flex-mode (teapot uses GL evaluators internally and clobbers map state, per comment at :68-70).

### PIPES/STATE.CXX  (global GL state, contexts, per-frame draw/clear)
- WGL: `wglGetCurrentContext` — 53, 846; `wglGetCurrentDC` — 54, 463; `wglDeleteContext` — 145; `wglCreateContext` — 464; `wglShareLists(shareRC, …)` — 474; `wglMakeCurrent` — 847 (`DRAW_THREAD::MakeRCCurrent`)
- `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` — 62 (only when `ulSurfStyle == SURFSTYLE_WIREFRAME`)
- `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` — 208 (`LoadTextureFiles`)
- `GLInit()` (250-289): `glClearColor(0,0,0,0)` :260; `glFrontFace(GL_CCW)` :262; `glDepthFunc(GL_LEQUAL)` :264; `glEnable(GL_DEPTH_TEST)` :265; `glEnable(GL_AUTO_NORMAL)` :267 ("needed for GL_MAP2_VERTEX (tea)"); `glLightModelfv(GL_LIGHT_MODEL_AMBIENT, …)` :270/:272; `glLightfv(GL_LIGHT0, GL_AMBIENT|GL_DIFFUSE|GL_POSITION)` :274-276 (directional light, w=0); `glEnable(GL_LIGHT0)` :277; `glEnable(GL_LIGHTING)` :278; `glCullFace(GL_BACK)` :280; `glEnable(GL_CULL_FACE)` :281; if textured: `glEnable(GL_TEXTURE_2D)` :285, `glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE)` :286
- `InitTexParams()` (299-317): `glTexParameteri` wrap S/T `GL_REPEAT` :301-302; mag/min filter `GL_LINEAR` :306-307 (TEXQUAL_HIGH) or `GL_NEAREST` :311-312 (default)
- Per-pipe modelview setup: `glLoadIdentity` :481, `glTranslatef` :482, `glRotatef` :485 (scene y-rot), :500-501 (flex x/z rot)
- `STATE::Clear()` (623-642): `glClear(GL_DEPTH_BUFFER_BIT)` :628 every reset; then either the transitional dissolve clear (CLEAR.CXX) or `glClear(GL_COLOR_BUFFER_BIT)` :640
- `glFlush` — 736 (end of `DrawPipesG` frame), 936 (`DRAW_THREAD::DrawPipe`), 964 (`DRAW_THREAD::StartPipe`)

### PIPES/VIEW.CXX  (projection)
- `glViewport(0, 0, w, h)` — 57
- `glMatrixMode(GL_PROJECTION)` — 70; `glLoadIdentity` — 71
- **`gluPerspective(persp.viewAngle, aspectRatio, persp.zNear, persp.zFar)`** — 75 (perspective mode)
- `glOrtho` — 80 (orthographic fallback when `bProjMode` false)
- `glMatrixMode(GL_MODELVIEW)` — 83

### PIPES — files with ZERO GL/GLU/aux/wgl calls
`SSPIPES.CXX` (but sets `ssc.bDoubleBuf = FALSE;` at :68 — see §f), `NODE.CXX`, `XC.CXX` (includes GL headers at :17-19, no calls), `FSTATE.CXX`, `NSTATE.CXX`, `DIALOG.C`, and every `PIPES/*.H` (SSPIPES.H includes `<GL/gl.h>`, `<GL/glu.h>`, `<GL/glaux.h>` at :13-15).

### COMMON/MATERIAL.C
- `glMaterialfv` — 196 (`GL_FRONT, GL_AMBIENT`), 197 (`GL_BACK, GL_AMBIENT`), 198/199 (`GL_DIFFUSE`), 200/201 (`GL_SPECULAR`)
- `glMaterialf` — 202/203 (`GL_SHININESS`, specExp*128)
That is the entire GL surface of MATERIAL.C.

### COMMON/CLEAR.CXX  (scissored wipe/dissolve clears — the famous screen wipe)
- `glClearColor(0,0,0,0)` — 57
- `glEnable(GL_SCISSOR_TEST)` — 59 (`ss_RectWipeClear`), 244 (`SS_DIGITAL_DISSOLVE_CLEAR::Clear`); `glDisable(GL_SCISSOR_TEST)` — 110, 287
- `glScissor` — 73, 77, 81, 85 (shrinking bottom/left/right/top 1-px frames), 279 (random dissolve rects)
- `glClear(GL_COLOR_BUFFER_BIT)` — 74, 78, 82, 86, 280
- `glFlush` — 88 ("to eliminate 'bursts'"), 281 — flush after every scissored clear so the wipe is visible **in the front buffer** in real time

### COMMON/TEXTURE.C
Includes only `<GL/gl.h>` (line 19); GLU/GLAUX prototypes come via `sscommon.h` → `<GL\glaux.h>` (SSCOMMON.H:14).
- GLAUX: `auxDIBImageLoad` — 106/108 (`ss_LoadBMPTextureFile`), 139/141 (`ss_LoadTextureFile`, BMP branch); `auxRGBImageLoad` — 145/147 (SGI .rgb branch). Returns `AUX_RGBImageRec*` consumed at ProcessTkTexture (388-409).
- GLU: `gluScaleImage` — 297 (`ValidateTextureSize`, rescale to power-of-2, capped at min(256, GL_MAX_TEXTURE_SIZE))
- `glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glMaxTexDim)` — 253 (`glMaxTexDim` at 244/254/260/262/265/267/270 is a **local GLint variable**, not a GL call — regex false positive)
- `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` — 295
- `glTexParameteri` — 330-333 (`SetDefaultTextureParams`: REPEAT/REPEAT/NEAREST/NEAREST)
- `glGenTextures` — 356 (`ProcessTexture`), 459 (`ss_CopyTexture`)
- `glBindTexture(GL_TEXTURE_2D, …)` — 357, 424 (`ss_SetTexture`), 488, 648, 665
- `glTexImage2D(GL_TEXTURE_2D, 0, pTex->components, w, h, 0, pTex->format, GL_UNSIGNED_BYTE, data)` — 362, 428, 493, 666
- `glDeleteTextures` — 691 (`ss_DeleteTexture`)
- `glTexGeni(GL_S|GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR)` — 1091, 1096; `glTexGenfv(GL_S|GL_T, GL_OBJECT_PLANE, …)` — 1094, 1099; `glEnable(GL_TEXTURE_GEN_S)` 1101, `glEnable(GL_TEXTURE_GEN_T)` 1102, `glEnable(GL_TEXTURE_2D)` 1103 — all inside `ss_InitAutoTexture` (1085-1105), **not called anywhere in PIPES** (only caller-facing decl at SSCOMMON.H:436; used by other savers)
- WGL/extension pointers (see §e): `wglGetProcAddress` — 749 `"glColorTableEXT"`, 753 `"glColorSubTableEXT"`, 759 `"glGetColorTableParameterivEXT"`; called through `pfnColorTableEXT` (368, 434, 499, 649, 764), `pfnColorSubTableEXT` (531, 535), `pfnGetColorTableParameterivEXT` (766)

### COMMON/SSUTIL.CXX
- `glGetString(GL_VERSION)` — 274 (`ss_QueryGLVersion`: `strstr(…, "1.1")` sets `gbGLv1_1`/`gbTextureObjects`)
- No other GL calls; pixel-format work (`ss_ChoosePixelFormat`, 50-130) is GDI `ChoosePixelFormat`/`DescribePixelFormat`, honoring `SS_DOUBLEBUF_BIT` → `PFD_DOUBLEBUFFER` (77-78, 126)

### COMMON/GLSCRNSV.CXX, COMMON/PALETTE.CXX, COMMON/UTIL.CXX
**Zero** GL/GLU/aux/wgl calls in all three (verified by grep count = 0). GLSCRNSV.CXX only translates `pssc->bDoubleBuf`/`depthType` into `GLc.pfFlags` bits (170-181). PALETTE.CXX is pure GDI palette code. UTIL.CXX includes `<GL/gl.h>` (:15) but calls nothing.

### Adjacent COMMON files (outside requested set, relevant to the port)
- **COMMON/DLGDRAW.CXX:141 — `gluOrtho2D(-1, 1, -1, 1)`** in the config-dialog GL preview path, alongside glCullFace/glEnable/glFrontFace/glShadeModel(GL_FLAT)/glColor3f/glPixelStorei/glTexParameteri (135-148)
- **COMMON/SSWINDOW.CXX:1483 — `wglGetProcAddress("glAddSwapHintRectWIN")`** (`ss_QueryAddSwapHintRect`, fn ptr declared :17, called :887 and :1180; falls back to no-op `MyAddSwapHintRect` :1485). `SwapBuffers` wrapper at :1399 — only used when the saver requests double buffering, which Pipes does not.
- COMMON/SCRNSAVE.CXX has three plain Win32 `GetProcAddress` calls (:293 IMM, :539/:707 password DLLs) — no GL relevance.

---

## 2. Direct answers

### (a) Full GLU list
Exactly **three** GLU entry points in the entire scoped code (plus one in the adjacent dialog code):
1. `gluPerspective` — PIPES/VIEW.CXX:75 (`VIEW::SetProjMatrix`, the one and only projection setup; `glOrtho` at :80 is the alternate branch)
2. `gluScaleImage` — COMMON/TEXTURE.C:297 (power-of-2 rescale of loaded BMP/RGB textures)
3. `gluOrtho2D` — COMMON/DLGDRAW.CXX:141 (config-dialog preview only; not in the screensaver draw path)

**No** quadrics of any kind: no `gluNewQuadric`, `gluDeleteQuadric`, `gluQuadricNormals`, `gluQuadricTexture`, `gluQuadricDrawStyle`, `gluSphere`, `gluCylinder`, `gluDisk`. No `gluLookAt`, no `gluErrorString`, no `gluBuild2DMipmaps`, no `gluProject/UnProject`, no NURBS/tessellator. (Verified with an explicit whole-tree grep over PIPES/ and COMMON/.) The "'glu' routines" comment at OBJECTS.CXX:468 marks GLU-derived code copied inline — the pipe cylinder/sphere/elbow geometry is hand-built into display lists, not GLU calls.

### (b) GL evaluator usage — YES, and it is core to flex pipes
- All in **PIPES/EVAL.CXX**; driven exclusively by **PIPES/FPIPE.CXX** (flex pipes). NPIPE (normal pipes) never touches evaluators.
- **Float variants only**: `glMap2f` (EVAL.CXX:369 tex coords, :377 vertices), `glMapGrid2f` (:384), `glEvalMesh2(GL_FILL, …)` (:385). No `glMap2d`, no `glMap1*`, no `glEvalMesh1`/`glMapGrid1*`, no `glEvalCoord*`/`glEvalPoint*` anywhere.
- Enables: `glEnable(GL_MAP2_TEXTURE_COORD_2)` EVAL.CXX:107 (textured only), `glEnable(GL_MAP2_VERTEX_3)` :109, `glEnable(GL_AUTO_NORMAL)` :110 — plus a second `glEnable(GL_AUTO_NORMAL)` at STATE.CXX:267 in `GLInit()` (comment: needed for the teapot's GL_MAP2_VERTEX).
- `glEnable/glDisable(GL_NORMALIZE)` only at PIPE.CXX:64/66 bracketing `auxSolidTeapot`.
- Core-ness: every flex-pipe surface (bends, straight extrusions, end caps/singularities) is emitted by `EVAL::Evaluate()` (EVAL.CXX:352-387) via glEvalMesh2 — flex pipes draw **nothing** by any other path (FPIPE.CXX only does matrix push/pop/rotate/translate around ProcessXCPrim* calls). The teapot easter egg also depends on evaluator state (PIPE.CXX:68-70 re-inits via `ResetEvaluator`). LEGACY_GL_EMULATION does **not** implement evaluators, so glMap2f/glMapGrid2f/glEvalMesh2/GL_AUTO_NORMAL is the biggest porting gap; normal-pipes mode is unaffected.

### (c) Display lists — YES, entire normal-pipe geometry
All in PIPES/OBJECTS.CXX: `glGenLists(1)` :28 (per-OBJECT ctor), `glDeleteLists` :37, `glCallList` :49 (`OBJECT::Draw`), `glNewList(listNum, GL_COMPILE)` :302 (elbow), :430 (ball joint), :526 (cylinder), :623 (sphere), matching `glEndList` :329/:465/:551/:711. Lists contain GL_QUAD_STRIP / GL_TRIANGLE_FAN immediate-mode geometry with per-vertex normals and optional texcoords. Lists are shared across the per-pipe RCs via `wglShareLists(shareRC, …)` STATE.CXX:474. Nothing else creates lists (aux teapot builds its own internally). LEGACY_GL_EMULATION supports display lists? — **No, it does not** (emscripten's emulation lacks glNewList/glCallList); these must be shimmed or the OBJECT class re-targeted, but that is Win32-adjacent scaffolding only if done at the GL-shim layer, not by altering OBJECTS.CXX.

### (d) GLAUX calls — three functions total
1. **`auxSolidTeapot(2.5 * radius)`** — PIPES/PIPE.CXX:65 (`PIPE::DrawTeapot`, the rare teapot easter egg; uses GL evaluators internally, note at :68-70)
2. **`auxDIBImageLoad`** — COMMON/TEXTURE.C:106/108 and 139/141 (BMP loading; UNICODE/ANSI variants under `#ifdef UNICODE`)
3. **`auxRGBImageLoad`** — COMMON/TEXTURE.C:145/147 (SGI .rgb loading)
Plus the `AUX_RGBImageRec` struct type (TEXTURE.C:102, 127, 389-…). Headers pulled in at PIPES/SSPIPES.H:15, PIPES/EVAL.CXX:19, PIPES/XC.CXX:19, COMMON/SSCOMMON.H:14. No other aux* usage (no auxInitDisplayMode/auxMainLoop windowing — the saver has its own framework).

### (e) Texture API level
- **Core GL 1.1 texture objects, gated at runtime**: `glGenTextures`/`glBindTexture`/`glDeleteTextures` (TEXTURE.C:356-357, 424, 459, 488, 648, 665, 691) are called only when `gbTextureObjects` is TRUE, set by `ss_QueryGLVersion()` string-matching `"1.1"` in `glGetString(GL_VERSION)` (SSUTIL.CXX:274-280). On GL 1.0 the code falls back to re-issuing `glTexImage2D` per bind (`ss_SetTexture`, TEXTURE.C:418-437, `pTex->texObj = 0` :375). **No `glBindTextureEXT`/`glGenTexturesEXT` anywhere** (verified) — it is core-1.1 naming, not the EXT_texture_object path, and it is *not* fetched via wglGetProcAddress.
- **glTexImage2D format usage**: internalformat is passed as the legacy *component count* (`pTex->components`: 3 or 4 — TEXTURE.C:362, 428, 493, 666), format is `pTex->format` = `GL_RGB` (ProcessTkTexture :394-395), `GL_RGBA` (after `ConvertTextureToRGBA` :600-601), or `GL_COLOR_INDEX` with components `GL_COLOR_INDEX8_EXT` for paletted A8 resources (see ss_CopyTexture's `components != GL_COLOR_INDEX8_EXT` test :465, format checks :257, :631). Type is always `GL_UNSIGNED_BYTE`.
- **Paletted-texture EXT path (GL_EXT_paletted_texture)** — TEXTURE.C only, all through `wglGetProcAddress`: `ss_QueryPalettedTextureEXT()` (TEXTURE.C:741-771) fetches `glColorTableEXT` :749, `glColorSubTableEXT` :753, `glGetColorTableParameterivEXT` :759 into `pfnColorTableEXT`/`pfnColorSubTableEXT`/`pfnGetColorTableParameterivEXT` (statics :33-34), probes a 256-entry `GL_PROXY_TEXTURE_2D` color table (:764-768, `GL_COLOR_TABLE_WIDTH_EXT`) and sets `gbPalettedTexture`. Palettes are uploaded as `GL_RGBA`/`GL_BGRA_EXT`/`GL_UNSIGNED_BYTE` (:368, 434, 499, 649) and animated via `glColorSubTableEXT` (`ss_SetTexturePalette` :531/:535). **Pipes never loads A8/paletted textures** (it loads user BMP/RGB files → GL_RGB), so this whole path is dead code for the Pipes port unless texture transparency of paletted textures is exercised (it is not: SSPIPES uses SURFSTYLE_TEX with BMP/RGB only).
- **glTexGen (auto texgen)**: only `ss_InitAutoTexture` (TEXTURE.C:1085-1105 — `glTexGeni` GL_OBJECT_LINEAR for S/T :1091/:1096, `glTexGenfv` GL_OBJECT_PLANE :1094/:1099, `glEnable(GL_TEXTURE_GEN_S/T)` :1101-1102). **No PIPES code calls it** (grep of PIPES/ finds nothing; declared SSCOMMON.H:436 for other savers). Pipes uses explicit `glTexCoord2f` (OBJECTS.CXX) and evaluator-generated GL_MAP2_TEXTURE_COORD_2 coords (EVAL.CXX).
- One more extension via GetProcAddress, outside the requested set: `glAddSwapHintRectWIN` (GL_WIN_swap_hint), COMMON/SSWINDOW.CXX:1483, no-op fallback :1485; only meaningful for double-buffered savers.

### (f) glDrawBuffer / glReadBuffer / front-buffer rendering
- **There are zero `glDrawBuffer` or `glReadBuffer` calls anywhere** in PIPES or COMMON (whole-tree grep). No `SwapBuffers` in PIPES either.
- Pipes explicitly requests a **single-buffered** pixel format: `ssc.bDoubleBuf = FALSE;` PIPES/SSPIPES.CXX:68 → GLSCRNSV.CXX:177-178 leaves `SS_DOUBLEBUF_BIT` clear → SSUTIL.CXX:77-78/:126 selects a PFD **without** `PFD_DOUBLEBUFFER`. In a single-buffered GL context the default draw buffer is `GL_FRONT`, so all rendering goes straight to the screen with `glFlush()` making progress visible (STATE.CXX:736/:936/:964; CLEAR.CXX:88/:281 flushes mid-wipe "to eliminate 'bursts'"). This incremental front-buffer accumulation (pipes persist frame-over-frame; only depth is cleared per reset, STATE.CXX:628) is the load-bearing rendering model — a WebGL port must emulate it (e.g. preserveDrawingBuffer / never-cleared offscreen target), since browsers are inherently double-buffered.
- The only GL_FRONT/GL_BACK token uses are non-buffer ones: `glMaterialfv/f(GL_FRONT|GL_BACK, …)` MATERIAL.C:196-203, `glCullFace(GL_BACK)` STATE.CXX:280, `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` STATE.CXX:62. COMMON/SSWINDOW.CXX's SwapBuffers wrapper (:1399) is scaffolding used only when `SS_DOUBLEBUF_BIT` is set — never for Pipes.

### (g) Exotic features
Whole-scope grep for `glClipPlane, glStencil*/GL_STENCIL, glAccum/GL_ACCUM, glFeedbackBuffer, glRenderMode, glSelectBuffer, glPickMatrix, glLogicOp, glFog*, glBlend*, glAlphaFunc, glLineStipple, glPolygonStipple, glDrawPixels, glReadPixels, glCopyPixels, glBitmap, glRasterPos` → **no hits at all** in the scoped files. Findings:
- **`glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)`** — PIPES/STATE.CXX:62, only when the user picks the wireframe surface style (`SURFSTYLE_WIREFRAME`). This is the single glPolygonMode call; LEGACY_GL_EMULATION/WebGL has no wireframe polygon mode, so this option needs special handling.
- **Scissored clears** — `glEnable(GL_SCISSOR_TEST)`/`glScissor`/`glClear` wipe machinery in CLEAR.CXX (§1) — mundane GL but behaviorally exotic (animated front-buffer wipe timed with a calibration loop, CLEAR.CXX:96-107).
- **Multiple GL contexts on one window with shared display lists** — wglCreateContext per draw "thread" (STATE.CXX:464), `wglShareLists` :474, `wglMakeCurrent` switching per pipe :847. Under emscripten there is effectively one context; this is Win32 scaffolding to flatten (all state that GLInit sets per-RC — STATE.CXX:466-470 — becomes one-time).
- **GL evaluators + AUTO_NORMAL** (§b) and **auxSolidTeapot** (§d) are the other non-trivial features. No stencil, no accum, no selection/feedback, no clip planes, no blending, no fog, no pixel transfers anywhere in Pipes.

---

## 3. Consolidated distinct-function summary (scoped files)

| API | Functions (distinct) |
|---|---|
| GL (28 in draw code) | glBegin, glEnd, glVertex3f, glVertex3fv, glNormal3f, glNormal3fv, glTexCoord2f, glColor3f (dbg), glPointSize (dbg), glPushMatrix, glPopMatrix, glTranslatef, glRotatef, glLoadIdentity, glMatrixMode, glOrtho, glViewport, glNewList, glEndList, glGenLists, glDeleteLists, glCallList, glMap2f, glMapGrid2f, glEvalMesh2, glEnable, glDisable, glFrontFace |
| GL (state/tex/clear) | glClear, glClearColor, glScissor, glFlush, glDepthFunc, glCullFace, glPolygonMode, glLightfv, glLightModelfv, glMaterialf, glMaterialfv, glShadeModel (DLGDRAW only), glTexEnvi, glTexParameteri, glTexImage2D, glGenTextures, glBindTexture, glDeleteTextures, glTexGeni, glTexGenfv, glPixelStorei, glGetIntegerv, glGetString |
| GL extensions (wglGetProcAddress) | glColorTableEXT, glColorSubTableEXT, glGetColorTableParameterivEXT (TEXTURE.C); glAddSwapHintRectWIN (SSWINDOW.CXX) |
| GLU (3) | gluPerspective (VIEW.CXX:75), gluScaleImage (TEXTURE.C:297), gluOrtho2D (DLGDRAW.CXX:141) |
| GLAUX (3) | auxSolidTeapot (PIPE.CXX:65), auxDIBImageLoad, auxRGBImageLoad (TEXTURE.C) |
| WGL (7) | wglCreateContext, wglDeleteContext, wglMakeCurrent, wglGetCurrentContext, wglGetCurrentDC, wglShareLists, wglGetProcAddress |

Enable/disable caps touched: GL_DEPTH_TEST, GL_LIGHTING, GL_LIGHT0, GL_CULL_FACE, GL_TEXTURE_2D, GL_SCISSOR_TEST, GL_AUTO_NORMAL, GL_NORMALIZE, GL_MAP2_VERTEX_3, GL_MAP2_TEXTURE_COORD_2, GL_TEXTURE_GEN_S, GL_TEXTURE_GEN_T (last three texgen ones unused by Pipes).
