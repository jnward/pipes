# Texture pipeline + frame/timer plumbing (NT4 SCRSAVE PIPES + COMMON)

Base dir: `/home/user/pipes/original/MSTOOLS/SAMPLES/OPENGL/SCRSAVE` (all paths below relative to it).

---

## (a) TEXTURE.C — texture load pipeline end-to-end

### Data structures

- `TEXTURE` struct — COMMON/SSCOMMON.H:89-100: `{ int width, height; GLenum format; GLsizei components; float origAspectRatio; unsigned char *data; GLuint texObj; int pal_size; int iPalRot; RGBQUAD *pal; }`
- `TEX_RES` — SSCOMMON.H:131-134: `{ int type; int name; }` (type is TEX_RGB/TEX_BMP/TEX_A8, SSCOMMON.H:124-129).
- Resource type IDs — SSCOMMON.H:119-121:
  ```c
  #define RT_RGB          99
  #define RT_MYBMP        100
  #define RT_A8           101
  ```
- `TEXFILE` — SSCOMMON.H:275-278: `{ int nOffset; TCHAR szPathName[MAX_PATH]; }`.

### ss_LoadTextureResource (COMMON/TEXTURE.C:165-227) — RESOURCE path (what PIPES uses for STRIPE.BMP)

1. `GetModuleHandle(NULL)` (line 175); maps `pTexRes->type` to a resource type: `TEX_RGB → MAKEINTRESOURCE(RT_RGB)`, `TEX_BMP → MAKEINTRESOURCE(RT_MYBMP)`, `TEX_A8 → MAKEINTRESOURCE(RT_A8)` (176-187).
2. Bytes come from the EXE's resource section: `FindResource(ghmodule, MAKEINTRESOURCE(pTexRes->name), lpType)` → `LoadResource` → `pv = LockResource(hg)` (189-203). **`pv` is a raw pointer to the resource bytes in memory** — for `RT_MYBMP` this is the verbatim on-disk .bmp file (including `BITMAPFILEHEADER`), because the .rc statement embeds the file as-is.
3. Parser dispatch on type (205-216): `TEX_RGB → ss_RGBImageLoad(pv, pTex)`, `TEX_BMP → ss_DIBImageLoad(pv, pTex)`, `TEX_A8 → ss_A8ImageLoad(pv, pTex)`. All three take `(PVOID pv, TEXTURE *ptex)` — i.e. **memory-buffer parsers** (prototypes SSCOMMON.H:430-432).
4. `FreeResource(hr)` (219), then `ProcessTexture(pTex)` (226) does size validation + GL upload (see below).

For PIPES specifically: STATE.CXX:29-32 defines `TEX_RES gTexRes[1] = { { TEX_BMP, IDB_DEFTEX } }`; `IDB_DEFTEX = 99` (PIPES/DIALOG.H:121), and PIPES/SSPIPES.RC:80:
```
IDB_DEFTEX              100     DISCARDABLE     "stripe.bmp"
```
i.e. resource *name* 99 (IDB_DEFTEX), resource *type* 100 (= RT_MYBMP), payload = raw stripe.bmp file bytes. STRIPE.BMP itself is **100x100, 24bpp, BI_RGB, BITMAPINFOHEADER, bfOffBits=54, 30054 bytes** (verified by parsing the file; STRIPECY.BMP is identical in format).

### ss_LoadTextureFile (TEXTURE.C:125-155) and ss_LoadBMPTextureFile (100-115) — FILE path (user-chosen texture)

- `ss_LoadTextureFile`: `VerifyTextureFile(pTexFile)` (134, impl 1024-1073) → `GetTexFileType` decides TEX_BMP/TEX_RGB by filename extension (1116-1137); validity check via `bVerifyDIB`/`bVerifyRGB` (1040/1043; those open the file themselves with `CreateFile`/`CreateFileMapping`/`MapViewOfFile` — SSDIB.C:404-496, SSIMAGE.C:212-256), plus a max-size check `TEX_WIDTH_MAX/TEX_HEIGHT_MAX` (1060-1070).
- Then the actual pixel load goes through **glaux**: `auxDIBImageLoad(pszBmpfile)` for BMP or `auxRGBImageLoad` for RGB (137-149; `ss_LoadBMPTextureFile` 105-109 does auxDIBImageLoad only, no verification). These parse from *filename*, not memory (glaux library, not in this source tree).
- Result `AUX_RGBImageRec*` → `ProcessTkTexture(image, pTex)` (154; impl 388-409): fills `pTex` with `width/height = image->sizeX/sizeY`, `format = GL_RGB`, `components = 3`, `data = image->data`, no palette; calls `ProcessTexture`; on success `free(image)` (record only — pixel data ownership moves to TEXTURE).

PIPES only reaches this path for user textures from the registry (`gTexFile`/`gnTextures`, DIALOG.C:53-54, 101-113); the default is the resource path.

### The three memory parsers and resulting pixel formats

| Parser | Input bytes | Output format/components | Notes |
|---|---|---|---|
| `ss_DIBImageLoad` (COMMON/SSDIB.C:114-390) | Raw .bmp file image (with or without BITMAPFILEHEADER; BITMAPINFOHEADER or BITMAPCOREHEADER) | `GL_RGB`, `components=3`, packed 24bpp, `pal=NULL` (354-360) | Uses GDI to convert arbitrary DIB formats: `CreateCompatibleDC` (278), `CreateDIBSection` 24bpp (285-302), `SetDIBits` (317) — "GDI will do the work of translating whatever format the DIB file has into RGB", then `GdiFlush()` (322). Final repack loop 334-349 swaps R/B (DIB is BGR) into `malloc`'d packed buffer; `padBytes = biWidth % sizeof(LONG)` (337) skips DWORD row padding (correct for the 24bpp case since 3w mod 4 == -w mod 4). Rows are copied in DIB (bottom-up) order, so `data` row 0 = image bottom, standard GL orientation. |
| `ss_RGBImageLoad` (COMMON/SSIMAGE.C:182-198) | SGI .rgb (IRIS) image; 512-byte header, optional RLE (type 0x01xx), byte-swap via IMAGIC_SWAP (SSIMAGE.C:16-21, 68-74) | `GL_RGB`, `components=3` (191-192) | `RawImageOpen` reads header from memory (`raw->data = pv`, 96); `RawImageGetData` (159-180) decodes R,G,B channel rows and interleaves into `malloc((sizeX+1)*(sizeY+1)*4)` (164) but packs 3 bytes/px. |
| `ss_A8ImageLoad` (COMMON/SSA8.C:143-247) | Custom `.a8` alpha format: signature 0xa0a1a2a3, w, h, depth==8, compress flag, 256-DWORD palette, then 8bpp indices (raw or RLE `HwuRld`) | If `ss_PalettedTextureEnabled()`: `format=GL_COLOR_INDEX`, `components=GL_COLOR_INDEX8_EXT`, `pal_size=256`, `pal` = copy of the BGRA palette (180-210). Else: expands indices through palette to 32bpp, `format=GL_BGRA_EXT`, `components=4` (211-244). | PIPES never uses A8 (that is MAZE's rat/start icons etc.). |

### ProcessTexture → GL upload path (TEXTURE.C:346-375)

1. `ValidateTextureSize` (238-319):
   - `origAspectRatio = height/width` (251 — note the struct comment says "width/height" but the code computes h/w; FPIPE.CXX relies on the code's h/w behavior).
   - `glGetIntegerv(GL_MAX_TEXTURE_SIZE)`; for non-`GL_COLOR_INDEX` textures clamps to `min(256, glMaxTexDim)` "for performance reasons" (260).
   - Scales **up** to the next power of 2 in each dimension (275-282: "Always scale to higher nearest power") using `glPixelStorei(GL_UNPACK_ALIGNMENT,1)` + **`gluScaleImage(format, w, h, GL_UNSIGNED_BYTE, data, xSize2, ySize2, GL_UNSIGNED_BYTE, pData)`** (295-300); replaces `pTex->data/width/height` (307-311). STRIPE.BMP 100x100 → **128x128**. Paletted textures skip scaling entirely (313-317, assumed already power-of-2).
2. If `gbTextureObjects` (GL 1.1 texture objects; set by `ss_QueryGLVersion` in COMMON/SSUTIL.CXX:270-285 — literally `strstr(glGetString(GL_VERSION), "1.1")`; called from `SSW::ConfigureForGL`, SSWINDOW.CXX:600):
   - `glGenTextures(1,&pTex->texObj)` + `glBindTexture(GL_TEXTURE_2D, ...)` (356-357),
   - `SetDefaultTextureParams` (327-334): WRAP_S/T=`GL_REPEAT`, MAG/MIN filter=`GL_NEAREST` (no mipmap filter),
   - **`glTexImage2D(GL_TEXTURE_2D, 0, pTex->components, w, h, 0, pTex->format, GL_UNSIGNED_BYTE, pTex->data)` — level 0 only. There is no `gluBuild2DMipmaps` and no mipmapping anywhere in this codebase.** (362-364)
   - Paletted-texture EXT: if `gbPalettedTexture && pTex->pal`, `pfnColorTableEXT(GL_TEXTURE_2D, GL_RGBA, pal_size, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pTex->pal)` (366-370).
3. Else `pTex->texObj = 0` (372) — GL 1.0 fallback.

Paletted EXT plumbing: `ss_QueryPalettedTextureEXT` (742-774) resolves `glColorTableEXT`/`glColorSubTableEXT`/`glGetColorTableParameterivEXT` via `wglGetProcAddress` and proxy-tests a 256-entry table; sets static `gbPalettedTexture`. Called from SSWINDOW.CXX:603. `ss_SetTexturePalette` (516-539) does palette *rotation* via two `pfnColorSubTableEXT` calls. (Under emscripten `wglGetProcAddress` shim returning NULL makes all this cleanly disable itself; PIPES never uses palettes anyway.)

### ss_SetTexture (TEXTURE.C:417-437)

- If texture objects: just `glBindTexture(GL_TEXTURE_2D, pTex->texObj)` and return (423-426).
- GL 1.0 path: **re-issues the full `glTexImage2D(..., 0, components, w, h, 0, format, GL_UNSIGNED_BYTE, data)` every call** (428-430), plus `pfnColorTableEXT` if paletted (432-436). Called per pipe-thread texture switch by `DRAW_THREAD::SetTexture` (PIPES/STATE.CXX:902-910, guarded by an `htex != hnewtex` cache). Porting note: if the emulated `glGetString(GL_VERSION)` doesn't contain "1.1", `gbTextureObjects` stays FALSE and you get a full texture re-upload each time a draw thread with a different texture runs — functionally correct, just slow; with only 1 default texture the cache means it uploads once.

### ss_InitAutoTexture (TEXTURE.C:1085-1104) — texgen

```c
GLfloat sgenparams[] = {1.0f, 0.0f, 0.0f, 0.0f};
GLfloat tgenparams[] = {0.0f, 1.0f, 0.0f, 0.0f};
glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);  // sgenparams[0] = pTexRep->s if given
glTexGenfv(GL_S, GL_OBJECT_PLANE, sgenparams);
glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);  // tgenparams[0] = pTexRep->t if given
glTexGenfv(GL_T, GL_OBJECT_PLANE, tgenparams);
glEnable(GL_TEXTURE_GEN_S); glEnable(GL_TEXTURE_GEN_T); glEnable(GL_TEXTURE_2D);
```
i.e. object-linear texgen: s = (texRep.s)*x_obj, t = (texRep.t)*y_obj. **PIPES never calls it** (no hits in PIPES/*); it's for other savers (3DFO etc.). Not needed for the pipes port unless texgen comes along via common-lib linkage.

### Misc TEXTURE.C entry points

- `ss_SetTextureTransparency` (619-676): rewrites alpha in data or palette; `ConvertTextureToRGBA` (576-602) RGB→RGBA. Unused by PIPES.
- `ss_CopyTexture` (448-504): note pre-existing bug at line 469 — tests `pTexDst->pal == NULL` where it means `pTexDst->data`. Unused by PIPES.
- `ss_DeleteTexture` (684-700): `glDeleteTextures` + free data/pal. Used by ~STATE (PIPES/STATE.CXX:133-137).
- `ss_LoadTextureResourceStrings` (47-79), `ss_SelectTextureFile` (834-950), `ss_GetDefaultBmpFile` (970-1011): dialog/registry-only scaffolding (LoadString, GetOpenFileName, registry) — config mode only.

### Feeding STRIPE.BMP from a memory buffer — is there a pure parse-from-memory entry point?

**Yes: `ss_DIBImageLoad(PVOID pv, TEXTURE *ptex)` (SSDIB.C:114) is exactly that** — it takes a pointer to the raw .bmp file bytes (it accepts a leading `BITMAPFILEHEADER` via the `'BM'` check at 141, computing bits at `bfOffBits`, or a raw BITMAPINFO/CORE header). `ss_LoadTextureResource` is already just "resolve resource → pointer → ss_DIBImageLoad → ProcessTexture". So the minimal wasm approach keeping all algorithm code intact:

1. Embed stripe.bmp bytes (C array or `--embed-file`) and shim the four Win32 resource calls used at TEXTURE.C:189-219: `FindResource(name=99/IDB_DEFTEX, type=100/RT_MYBMP)` → handle; `LoadResource`/`LockResource` → pointer to the bytes; `FreeResource` → no-op. Nothing in TEXTURE.C itself needs changing.
2. `ss_DIBImageLoad`'s *interface* is pure-memory but its *implementation* routes pixel conversion through GDI (Win32 scaffolding): `CreateCompatibleDC` (SSDIB.C:278), `CreateDIBSection` 24bpp (301), `SelectObject` (309), `SetDIBits` (317), `GdiFlush` (322), `DeleteDC/DeleteObject/LocalAlloc/LocalFree`. For the shim, a DIB section is a malloc'd bottom-up 24bpp BGR buffer with DWORD-aligned rows, and `SetDIBits` for a 24bpp BI_RGB source (STRIPE.BMP's actual format) is a stride-respecting row copy; 8/4/1bpp sources would need palette expansion in `SetDIBits` if you also want arbitrary user BMPs. The R/B swap + un-padding at SSDIB.C:334-349 is portable C already.
3. `ProcessTexture` needs `gluScaleImage` (TEXTURE.C:297) — supplied by your wasm GLU build — plus `glGetIntegerv(GL_MAX_TEXTURE_SIZE)`.
4. The `auxDIBImageLoad`/`auxRGBImageLoad` file path (glaux) can be stubbed to fail (return NULL): PIPES then falls back to the resource texture automatically via STATE::LoadTextureFiles (STATE.CXX:225-235). Also stub `SearchPath` in `ss_VerifyTextureFile` (TEXTURE.C:797) to fail so `gnTextures` ends up 0 (or just keep registry shim returning no user textures so `gnTextures==0` from the start, DIALOG.C:100-115).

---

## (b) PIPES texture usage

### Who calls common texture functions

- **PIPES/SSPIPES.CXX**: `ss_VerifyTextureFile(&gTexFile[i])` at 53-62 (pre-GL validation of user textures, inside `ss_Init`). Also sets `ssc.bDoubleBuf = FALSE; ssc.depthType = SS_DEPTH16; ssc.bFloater = FALSE` (68-70).
- **PIPES/STATE.CXX**: the texture owner.
  - Ctor 56-60: `bTexture = FALSE; if (ulSurfStyle == SURFSTYLE_TEX) { if (LoadTextureFiles(gTexFile, gnTextures, &gTexRes[0])) bTexture = TRUE; }`
  - `STATE::LoadTextureFiles` (203-240): `glPixelStorei(GL_UNPACK_ALIGNMENT,1)` (208); loops `ss_LoadTextureFile(&pTexFile[i], &texture[i])` (214) + `InitTexParams()` per object if `ss_TextureObjectsEnabled()` (216-217); if zero valid user textures, `nTextures = DEF_TEX_COUNT (=1)` and `ss_LoadTextureResource(pTexRes, &texture[i])` (225-235); then `CalcTexRepFactors()` (237).
  - `CalcTexRepFactors` (155-193): texRep[i].x/y = round(winSize/texSize/8) when >= 1 ("repeat textures smaller than 1/8th of screen width or height"); for display-list (normal) pipes collapses to the smallest rep in texRep[0] (182-192).
  - `GLInit` (250-289): when `bTexture` — dimmer ambient `lmodel_ambientTex {0.6,0.6,0.6,0}` (257, 269-270), `glEnable(GL_TEXTURE_2D)`, `glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE)`, `InitTexParams()` (284-288). `InitTexParams` (298-315): WRAP_S/T=REPEAT; filters NEAREST (TEXQUAL_DEFAULT) or LINEAR (TEXQUAL_HIGH) from the dialog setting.
  - Materials: `ss_InitTexMaterials()` vs `ss_InitTeaMaterials()` (111-114).
  - `FrameReset` 518-527: `PickRandomTexture(i, nTextures)` (583-614, random-without-replacement per frame) → `pThread->SetTexture(&texture[index])`; flex pipes additionally get `((FLEX_PIPE*)pNewPipe)->SetTexParams(&texture[index], &texRep[index])` (524-526).
  - `DRAW_THREAD::SetTexture` (902-910) → `ss_SetTexture(htex)` with handle cache.
  - `~STATE` 133-137 → `ss_DeleteTexture(&texture[i])`.
- **PIPES/DIALOG.C**: config-mode only — `ss_LoadTextureResourceStrings()` (77), `ss_SelectTextureFile(hDlg, &gTexFile[0])` (290), registry read/write of `gTexFile[MAX_TEXTURES]`/`gnTextures` (53-54, 101-113, 148-155). `MAX_TEXTURES = 8` (SSPIPES.H:54).
- **NPIPE.CXX / FPIPE.CXX / EVAL.CXX / OBJECTS.CXX / NSTATE.CXX / FSTATE.CXX**: never call `ss_*Texture*` — they only branch on the `bTexture` flag (copied from STATE in PIPE ctor, PIPE.CXX:30) and generate texture coordinates themselves.

### Pipe tex-coord generation

- **Normal pipes (display lists, OBJECTS.CXX)** — coords baked into `glNewList/glEndList` at build time:
  - `MakeQuadStrip` (161-189): per vertex `glTexCoord2f(tex_s[0], *tex_t)` / `glTexCoord2f(tex_s[1], *tex_t++)` when `bTexture` (179-185).
  - `ELBOW_OBJECT::Build` (226-330): `tex_t[i] = (GLfloat)i * texRep->y / slices` around circumference (255-260); `tex_s[1] = s_start + s_delta*i/stacks` along the bend (316); comment block 194-224 explains the 4 "notch"-oriented elbows exist precisely so texture coords mate with adjoining cylinders.
  - `PIPE_OBJECT::Build` (486-552): cylinder quad strips; `glTexCoord2f(s_start + s_delta*j/stacks, i*texRep->y/slices)` (535-543).
  - `BALLJOINT_OBJECT::Build` (ends ~465) and `SPHERE_OBJECT::Build` (560+): same pattern (sphere 692-702); sphere ends are done as quad strips instead of triangle fans when texturing (625-630: "We don't do it when bTexture because we need to respecify the texture coordinates").
  - s-coordinate plumbing from NSTATE.CXX::BuildObjects (91-149): `s_max = texRep->y`, `s_trans = s_max*2*radius/divSize` (105-106); short pipe `(divSize-2r, s_trans, s_max)`, long pipe `(divSize, 0, s_max)` (109-111); elbows/balljoints `(i, 0, s_trans)` (114-117); textured end-cap s-range computed at 123-129 with ROOT_TWO radius compensation.
- **Flex pipes (evaluators)**: `EVAL::EVAL(bTexture)` allocates `texPts` (EVAL.CXX:71-75); `ResetEvaluator` enables `GL_MAP2_TEXTURE_COORD_2` (106-108); `FLEX_PIPE::SetTexParams` (FPIPE.CXX:138-159) computes `t_start = texRep->y`, `t_end=0`, `s_length = t_size / pTex->origAspectRatio`; `CalcEvalLengthParams` (FPIPE.CXX:620-642) advances `s_start/s_end` per section, keeping s in (0..1); `EVAL::SetTextureControlPoints` (EVAL.CXX:128+) builds 2x2 control nets per section; `EVAL::Evaluate` maps them with `glMap2f(GL_MAP2_TEXTURE_COORD_2, 0,1, TDIM, TEX_ORDER, 0,1, TEX_ORDER*TDIM, TEX_ORDER, ptexPts)` (368-373) before `glMap2f(GL_MAP2_VERTEX_3...)`, `glMapGrid2f`, `glEvalMesh2(GL_FILL,...)` (377-385).
- No texgen path in PIPES at all (see (a); `ss_InitAutoTexture` unused).

### The pipe-count constants (quoted)

PIPES/NSTATE.H:17-18 (note: NSTATE.H, included by STATE.H:17; the values are *used* in STATE.CXX):
```c
#define NORMAL_PIPE_COUNT       5
#define NORMAL_TEX_PIPE_COUNT   3
```
Used at:
- STATE.CXX:569 (`CalcMaxPipesPerFrame`): `nCount = bTexture ? NORMAL_TEX_PIPE_COUNT : NORMAL_PIPE_COUNT;`
- FSTATE.CXX:96 (`FLEX_STATE::GetMaxPipesPerFrame`): `return bTexture ? NORMAL_TEX_PIPE_COUNT : NORMAL_PIPE_COUNT;` (turnomania scheme instead returns `TURNOMANIA_PIPE_COUNT` = 10, FPIPE.H:43; the `/2` textured variant at FSTATE.CXX:93-94 is dead code after the `return` at 92).
- FrameReset multiplies by 1.5 when multi-pipe (STATE.CXX:438-440).

So: textured runs draw 3 pipes per frame vs 5 untextured — an original fidelity detail worth preserving.

---

## (c) Frame/timer plumbing (COMMON)

Correction to the prompt: `ss_TimerProc` lives in **COMMON/SSWPROC.CXX**, not SSWINDOW.CXX; SSWINDOW.CXX ~1050-1250 holds the `UpdateWindow` machinery it invokes. Both below.

### Timer creation (SSWPROC.CXX)

- SSWPROC.CXX:53-59: `static UINT idTimer = 0;` and
  ```c
  #ifdef SS_DEBUG
      static UINT uiTimeOut = 2;  // Let it rip !
  #else
      static UINT uiTimeOut = 16; // Cap at ~60 fps
  #endif
  ```
- `SS_WM_START` (90-117): "the main GL startup point" — `idTimer = 1; SetTimer(hwnd, idTimer, uiTimeOut, 0);` (106-107) on the top-level window. Killed on `WM_DESTROY` (128-132).
- `WM_TIMER` (263-290): skip if `bSuspend` (set when `WM_SIZE` says `SIZE_MINIMIZED`, 243-248); optional `SS_WIN95_TIMER_HACK` kills the timer, draws, then re-`SetTimer`s to avoid flooding the queue while idle (267-289); core call is `ss_TimerProc();` (282); `return 0`.

### ss_TimerProc (SSWPROC.CXX:448-477)

```c
static void ss_TimerProc()
{
    static int busy = FALSE;
    ...
    if (busy) return;
    busy = TRUE;
    gpss->psswMain->UpdateWindow();
    ... // SS_DEBUG: update-rate stats
    busy = FALSE;
}
```
Re-entrancy guard + one `SSW::UpdateWindow()` on the main window per tick. This is the whole animation heartbeat → in the wasm port, one `UpdateWindow()` per `requestAnimationFrame` (16 ms cap matches 60 Hz rAF nicely).

### SSW::UpdateWindow (SSWINDOW.CXX:1069-1109) → UpdateFunc

```c
void SSW::UpdateWindow()
{
    // update any children first
    PSSW pssw = psswChildren;
    while (pssw) { pssw->UpdateWindow(); pssw = pssw->psswSibling; }

    if( psswParent && psswParent->bValidateBg && pGLc &&
        !(pGLc->pfFlags & SS_GENERIC_UNACCELERATED_BIT) )
    {   // Clear the entire parent window
        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        psswParent->bValidateBg = FALSE;
    }

    if( !UpdateFunc ) return;

    if( bDoubleBuf || pStretch ) {
        UpdateDoubleBufWin();          // 1119-1163: UpdateFunc then SwapSSBuffers
    } else {
        if( pMotion ) MoveSSWindow( TRUE );   // floater only
        (*UpdateFunc)( DataPtr );             // <-- PIPES path: line 1107
    }
}
```
- `UpdateFunc` = `gUpdateFunc` registered by client via `ss_UpdateFunc(Draw)` (PIPES/SSPIPES.CXX:114; latched into the SSW at SSWINDOW.CXX:611-617 during `ConfigureForGL`). For pipes it's `Draw → STATE::Draw` (SSPIPES.CXX:75-79).
- PIPES config: `bDoubleBuf=FALSE`, `bFloater=FALSE` (SSPIPES.CXX:68-70) ⇒ no `pMotion`, no `pStretch` ⇒ **each timer tick is exactly one `STATE::Draw` call directly into the front buffer, no swap.**
- The `bValidateBg` block only matters for *sub-window* configurations (pipes has none): the flag is set on the main pssw by `WM_PAINT`/`WM_MOVE` when `pssw->iSubWindow` (SSWPROC.CXX:230-235, 252-261) so hardware double-buffered children clear stale garbage behind them.

### Repaint path

- `WM_PAINT` on main window → `pssw->Repaint(TRUE)` (SSWPROC.CXX:201-205); `WM_ERASEBKGND` → `Repaint(FALSE)` (372-377); `WM_SETFOCUS` on Win95 fullscreen → `Repaint(FALSE)` (185-193).
- `SSW::Repaint(BOOL bCheckUpdateRect)` (SSWINDOW.CXX:820-839): if checking, `GetUpdateRect(hwnd,&rect,FALSE)` and bail on all-zero rect (macro `NULL_UPDATE_RECT`, 814-818); then `(*RepaintFunc)(pRect, DataPtr)`.
- PIPES `RepaintFunc` = `STATE::Repaint` (STATE.CXX:328-332): just `resetStatus |= RESET_REPAINT_BIT;` — header comment 318-326: "The paint will overwrite the frame buffer, screwing up the scene if pipes is in single buffer mode. We set resetStatus accordingly to clear things up on next draw." Next tick, `STATE::Draw → DrawValidate` (654-661) sees nonzero `resetStatus` and calls `FrameReset()` (391-553), which clears and starts a new frame.

### When Reshape fires

`SSW::Reshape()` (SSWINDOW.CXX:1412-1441): picks window size (or stretch-bitmap size), does `glViewport(0, 0, w, h)` when the window has an `hrc` and `hwnd` (1422-1424; sub-window variant clears + offsets viewport 1425-1435), then `(*ReshapeFunc)(w, h, DataPtr)` (1438-1440).
Fire sites:
1. End of `SSW::InitGL` (SSWINDOW.CXX:682-685): "Send another Reshape, since initial one triggered by window creation would have been received before GL init'd" — this is the one that always happens at startup (STATE.H comment `Reshape ... always called on app startup`).
2. `WM_SIZE` → `SS_ScreenSaverProc` → `pssw->Resize(LOWORD,HIWORD)` (SSWPROC.CXX:386-389) → `SSW::Resize` ends with `Reshape()` (SSWINDOW.CXX:802; also 793-797 for hwnd-less children).
3. `SetAspectRatio` for sub-windows (SSWINDOW.CXX:937-940). Not used by pipes.
PIPES `ReshapeFunc` = `STATE::Reshape` (STATE.CXX:343-348): `if (view.SetWinSize(width,height)) resetStatus |= RESET_RESIZE_BIT;` — actual GL projection setup is deferred to `ResetView()` inside the next `FrameReset` (416-418 → 357-378, which calls `view.SetGLView()` per RC).

### GdiFlush / glFlush / glFinish inventory

- `GdiFlush`: SSWINDOW.CXX:1383 (`SwapStretchBuffers`, after Blt), 1460 (`GdiClear`), 1537; SSDIB.C:322 (after `SetDIBits` — "make sure that SetDIBits executes"); CLEAR.CXX:338, 405 (GDI dissolve clear); FASTDIB.C:94. All are GDI sync points → no-ops in the wasm shim.
- `glFinish`: SSWINDOW.CXX:1033, in `MoveSSWindow` ("Synchronize with OpenGL" before moving the floater window) — floater-only, unreached by pipes.
- `glFlush`: driven by the *client*: STATE.CXX:736 (end of `STATE::Draw`), 936 (end of `DRAW_THREAD::DrawPipe`), 964 (end of `DRAW_THREAD::StartPipe`); CLEAR.CXX:88, 281 (dissolve clear, "to eliminate 'bursts'"); DLGDRAW.CXX:226. These matter in single-buffer mode: partial-frame progress becomes visible because each flush pushes work to the front buffer. In a browser you can't literally show mid-rAF partial results without rendering to the (single) canvas backbuffer and letting the compositor present each rAF — the per-pipe glFlush granularity collapses to once per tick, which matches one `ss_TimerProc` tick anyway.

### Single-buffer confirmation

- PIPES requests it: `ssc.bDoubleBuf = FALSE;` (SSPIPES.CXX:68).
- GLSCRNSV.CXX:177-178: `if (pssc->bDoubleBuf) GLc.pfFlags |= SS_DOUBLEBUF_BIT;` — not set for pipes. GLSCRNSV.CXX contains **no** SwapBuffers/wglSwapBuffers call at all.
- SSWINDOW.CXX:499: `bDoubleBuf = SS_HAS_DOUBLEBUF(pfFlags);` → FALSE. Pixel-format selection then *requires* a non-double-buffered format: SSUTIL.CXX:77-78 rejects `PFD_DOUBLEBUFFER` formats when `!bDoubleBuf` (and 125-126 only adds `PFD_DOUBLEBUFFER` when requested).
- The **only** `SwapBuffers(` call in COMMON+PIPES is SSWINDOW.CXX:1399 inside:
  ```c
  void SSW::SwapSSBuffers()
  {
      if( pStretch )        SwapStretchBuffers();
      else if( bDoubleBuf ) SwapBuffers( hdc );
  }
  ```
  and `SwapSSBuffers` is reached only from `UpdateDoubleBufWin`/`UpdateDoubleBufSubWin` (1162, 1217), which pipes never enters (`UpdateWindow` branch at 1102-1108). ⇒ **Confirmed: pipes renders single-buffered to the front buffer; no swap ever occurs.** Port consequence: the canvas must be treated as a persistent framebuffer (`preserveDrawingBuffer`-like semantics or render-to-texture accumulation), since pipes accumulates imagery across many ticks and only clears on `FrameReset`.

---

## (d) VIEW.CXX — projection, viewport, world extents; STATE::Reshape/Repaint

### VIEW constructor (PIPES/VIEW.CXX:24-46)

- `bProjMode = GL_TRUE` (perspective; the `glOrtho` branch is dead in practice).
- `zTrans = -75.0f; viewDist = -zTrans;` (30-31) — camera pulled back 75 units in `FrameReset` via `glTranslatef(0,0,view.zTrans)` (STATE.CXX:482), followed by `glRotatef(view.yRot, 0,1,0)` (485).
- `numDiv = NUM_DIV` = **16** (SSPIPES.H:52, "divisions in window in longest dimension"); `divSize = 7.0f` (38); node array in longest dimension is `NUM_NODE = NUM_DIV-1 = 15` (NODE.H:16).
- `persp.viewAngle = 90.0f; persp.zNear = 1.0f;` (40-41). `yRot = 0`, `winSize = 0x0` (43-45).

### SetWinSize (124-147) — world extents from screen aspect

```c
aspectRatio = winSize.height == 0 ? 1.0f : (float)winSize.width/winSize.height;
if( winSize.width >= winSize.height ) {
    world.x = numDiv * divSize;      // 16*7 = 112 world units
    world.y = world.x / aspectRatio;
    world.z = world.x;
} else {
    world.y = numDiv * divSize;
    world.x = world.y * aspectRatio;
    world.z = world.y;
}
```
Returns FALSE if the size didn't change (guards `RESET_RESIZE_BIT`).

### SetGLView / SetProjMatrix (54-84) — viewport + gluPerspective params

```c
glViewport(0, 0, winSize.width, winSize.height);   // SetGLView, 57
glMatrixMode(GL_PROJECTION); glLoadIdentity();
persp.zFar = viewDist + world.z*2;                 // 75 + 224 = 299 for landscape
gluPerspective( persp.viewAngle,   // 90.0 (vertical FOV)
                aspectRatio,       // w/h
                persp.zNear,       // 1.0
                persp.zFar );      // viewDist + 2*world.z
glMatrixMode(GL_MODELVIEW);
```
(75-83; ortho alternative 80-81 uses ±world/2 and ±world.z, unused since `bProjMode` is always TRUE.)
Called from `STATE::ResetView` (STATE.CXX:368-377) once per active RC, *not* directly from the Reshape callback.

### CalcNodeArraySize (94-114)

Node grid dims derived from aspect: larger axis gets `numDiv-1` (=15) nodes, the other gets `15/aspectRatio` (or `15*aspectRatio`), clamped ≥ 1; `z` copies the larger axis. Comment 97-98: "if aspect ratio deviates too much from 1, then nodes will get clipped as view rotates". Consumed by `STATE::ResetView` → `nodes->Resize(&numNodes)` (STATE.CXX:360-366).

### IncrementSceneRotation (154-161)

`yRot += 9.73156f;` wrap at 360. Applied on `RESET_NORMAL_BIT` frame resets (STATE.CXX:543-545), i.e. the whole scene yaws ~9.73° each new frame-of-pipes.

### STATE::Reshape / Repaint behavior (PIPES/STATE.CXX)

- `STATE::Reshape(w,h,data)` (343-348): `view.SetWinSize` → set `RESET_RESIZE_BIT` only on a real change. Nothing GL happens immediately.
- `STATE::Repaint(pRect,data)` (328-332): `resetStatus |= RESET_REPAINT_BIT;` only.
- Both flags funnel into `DrawValidate` (654-661) → `FrameReset` (391-553) on the next timer tick:
  - `RESET_RESIZE_BIT` → `ResetView()` (416-418): recompute node array + `view.SetGLView()` per RC (i.e. viewport+projection are re-established here).
  - `Clear()` (623-642): resize → `ddClear.CalibrateClear` (window already black); normal frame-end (`RESET_NORMAL_BIT`, set when all draw threads die, 716-719) → SS_DIGITAL_DISSOLVE_CLEAR transition; anything else (startup/repaint) → plain fast `glClear(GL_COLOR_BUFFER_BIT)` (640). Depth is always cleared first (628).
  - `RESET_STARTUP_BIT` is the ctor's initial state (47), so the very first Draw performs a FrameReset before anything renders.

### Port notes for (d)

- `gluPerspective` is the only GLU view dependency (plus `gluScaleImage` in textures); both must come from the wasm GLU build.
- Because projection setup happens in `ResetView` (frame reset), not on every tick, a canvas resize in the browser just needs to route through `ReshapeFunc(w,h)` and the original logic handles the rest, including node-array re-dimensioning.
- Multi-RC loop in `ResetView` (STATE.CXX:370-377) and `FrameReset` (462-477: `wglCreateContext`/`wglShareLists`/`MakeRCCurrent` per draw "thread") — with a single WebGL context, shim `wglCreateContext` to return the one shared context and `wglMakeCurrent`/`wglGetCurrentContext` accordingly; all "threads" are already cooperative (no real Win32 threads anywhere in this code path).
