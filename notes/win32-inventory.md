# Win32/MSVC Symbol Inventory — files we must compile

Source root: `/home/user/pipes/original/MSTOOLS/SAMPLES/OPENGL/SCRSAVE` (subdirs `PIPES/`, `COMMON/`).
All line numbers are 1-based against the files as extracted. "Clean" = no Win32 *calls*; only needs
`windows.h`-style typedefs (BOOL, TRUE/FALSE, ULONG, FLOAT, LP*, TCHAR, …) shimmed via a stub header.

Every `.CXX` in PIPES includes `<windows.h>` plus MSVC CRT headers (`<sys/timeb.h>`, `<time.h>`); via
`sspipes.h` (lines 13–17) all of them also pull in `<GL/gl.h>`, `<GL/glu.h>`, `<GL/glaux.h>`, and
`<commctrl.h>` — so the shim must at minimum provide fake `windows.h`, `scrnsave.h`, `commdlg.h`,
`commctrl.h`, `GL/glaux.h`. `sscommon.h` also includes `<GL\glaux.h>` (COMMON/SSCOMMON.H:14).
NOTE: `DO_TIMING` is unconditionally `#define`d =1 in `PIPES/SSPIPES.H:21`, so the `#ifdef DO_TIMING`
blocks (`_ftime`, `Timer()`, `pipeCount`) ARE compiled.

---

## PIPES

### SSPIPES.CXX (163 lines)
| Symbol | Kind | Lines |
|---|---|---|
| `<sys/timeb.h>` include | MSVC CRT | 15 |
| `<windows.h>` include | Win32 | 17 |
| `LPRECT` | type (Repaint cb signature) | 88 |
| `FALSE` | macro | 68, 70 |
| `struct _timeb` | MSVC CRT type (DO_TIMING **compiled**) | 122, 123, 145 |
| `_ftime()` | MSVC CRT time call | 127, 150 |
| `SendMessage`, `WM_SETTEXT`, `LPARAM`, `ss_GetHWND()` | Win32 msg (only under `#ifdef SS_DEBUG`, normally OFF) | 139 |
| `SS_DBGMSG`-family | none used here | — |

Also calls common-lib: `getIniSettings()` 47 (PIPES/DIALOG.C), `ss_VerifyTextureFile` 54 (TEXTURE.C),
`ss_InitFunc` 65, `ss_UpdateFunc/ss_ReshapeFunc/ss_RepaintFunc/ss_FinishFunc/ss_DataPtr` 114–118 (SSINIT.CXX).
Globals consumed: `ulSurfStyle` 49, `gnTextures`/`gTexFile` 53–60, `bFlexMode`/`bMultiPipes` 112 (defined in PIPES/DIALOG.C).
**Shim needs**: `_ftime`/`struct _timeb` (real logic → wall clock), LPRECT typedef. Category (4): `_ftime` at 127, 150.

### STATE.CXX (993 lines) — the only PIPES algorithm file with real Win32 API calls (wgl)
| Symbol | Kind | Lines |
|---|---|---|
| `wglGetCurrentContext()` | wgl | 53, 846 |
| `wglGetCurrentDC()` | wgl | 54, 463 |
| `wglDeleteContext()` | wgl | 145 |
| `wglCreateContext()` | wgl | 464 |
| `wglShareLists()` | wgl | 474 |
| `wglMakeCurrent()` | wgl | 847 |
| `HDC` | type | 463, 818 (`hdc=0`), 856; member decl STATE.H:49 |
| `HGLRC` | type | 856, 889; members STATE.H:53,77 |
| `HTEXTURE` (= `TEXTURE*`, SSCOMMON.H:100) | typedef | 821, 903 |
| `LPRECT` | type | 329 |
| `SS_DBGINFO` | SSDEBUG.H macro | 402 |
| `Timer()` / `pipeCount` | DO_TIMING (compiled) | 399, 551, 732 |
| `TRUE/FALSE/BOOL` | typedefs | 56, 59, 203, 445, 592, 677–678, 878, 946 |

Common-lib calls: `ss_GetScreenSize` 161, `ss_LoadTextureFile` 214, `ss_TextureObjectsEnabled` 216,
`ss_LoadTextureResource` 230, `ss_DeleteTexture` 135, `ss_InitTexMaterials` 112, `ss_InitTeaMaterials` 114,
`ss_fRand` 431–432, `ss_iRand` 446, 604, 793, `ss_iRand2` 443, `ss_SetTexture` 908.
Default texture resource table: `TEX_RES gTexRes = {{TEX_BMP, IDB_DEFTEX}}` lines 30–32 (IDB_DEFTEX=99, DIALOG.H:121; maps to `stripe.bmp` via SSPIPES.RC:80).
**Category (5) real logic**: the wgl shim. `FrameReset` (456–477) creates *one GL context per draw
"thread"* (up to `MAX_DRAW_THREADS`) via `wglCreateContext`+`wglShareLists`, then round-robins
`wglMakeCurrent` in `DrawPipe`/`StartPipe` (923, 949 via `MakeRCCurrent` 843–848). Under emscripten
there is exactly one context: shim should return the same non-zero dummy `HGLRC` from
`wglCreateContext`/`wglGetCurrentContext`, make `wglMakeCurrent`/`wglShareLists`/`wglDeleteContext`
no-op successes. Consequences are benign: display lists/state are naturally shared; `GLInit()` (468)
just re-executes idempotent state; destructor 142–147 skips deleting handles equal to `shareRC`
(so returning shareRC's value from wglCreateContext avoids double-delete entirely).

### NSTATE.CXX (198 lines) — **100% clean** (category 1)
Includes windows.h (17), sys/timeb.h (15). Only typedefs used: `BOOL` (93), `NULL` (74, 119, 137).
Consumes dialog global `ulJointType` 35 and JOINT_* enums; `ss_iRand` 188, 192. No Win32 calls.

### FSTATE.CXX (98 lines) — **100% clean** (category 1)
windows.h (17). `BOOL` (61). `ss_iRand` 48. No Win32 calls.

### PIPE.CXX (344 lines)
| Symbol | Kind | Lines |
|---|---|---|
| `auxSolidTeapot(2.5 * radius)` | **glaux library call** (MS-only lib, not core GL) | 65 |
| `BOOL/TRUE/FALSE` | typedefs | 58, 170, 203 |
Common: `ss_RandomTexMaterial` 47, `ss_RandomTeaMaterial` 49. Everything else pure GL/logic.
**Category (5)**: `auxSolidTeapot` needs a real implementation (the famous teapot joint at
NSTATE.CXX:189/NPIPE.CXX:369). Port glaux's teapot (evaluator-based) or an equivalent; note
PIPE::DrawTeapot expects it to leave evaluator state dirty (it calls `ResetEvaluator` after, 71).

### NPIPE.CXX (460 lines) — **100% clean** (category 1)
windows.h (17). `GLint/GLfloat` (GL). `ss_iRand` 48, 51, 86; `ss_iRand2` 49. `printf` only under
`#if PIPES_DEBUG` (310, 356, 379, 453 – off by default). No Win32 symbols beyond header include.

### FPIPE.CXX (709 lines) — **100% clean** (category 1)
windows.h (25). `BOOL` 592, `TRUE/FALSE` 592. `ss_fRand` 105, 396, 422; `ss_iRand` 114, 190, 234, 302;
`ss_iRand2` 115. Types `TEXTURE`, `IPOINT2D` (sscommon.h). No Win32 calls.

### VIEW.CXX (161 lines) — **100% clean** (category 1)
windows.h (15). `gluPerspective` 75 (GLU, already ported). `SS_ASSERT` 34 (SSDEBUG.H → `assert` in
release). `BOOL/TRUE/FALSE` 124–146, `GL_TRUE` 26. No Win32 calls.

### NODE.CXX (892 lines) — **100% clean** (category 1)
windows.h (16). `SS_ASSERT` 72, 137, 192, 308, 326, 364, 484, 533, 641 (macro). `BOOL/TRUE/FALSE`
360, 528, 543, 571, 698, 703, 754, 791, 840. `ss_iRand` 160, 225, 278, 324, 707–709, 860;
`ss_iRand2` 795–796. `SS_MAX` 843. No Win32 calls.

### OBJECTS.CXX (712 lines) — **100% clean** (category 1)
windows.h (14), `<GL/gl.h>` (15). `BOOL` 167, 245, 364, 501, 579. Uses COMMON/MATH.C helpers
(`ss_matrixIdent/Translate/Rotate/Mult` 117–132, `ss_xformPoint` 136, `ss_normalizeNorm` 149). All
display-list geometry, no Win32 calls.

### EVAL.CXX (537 lines)
| Symbol | Kind | Lines |
|---|---|---|
| `<GL/glaux.h>` include | glaux header | 19 |
| `LocalAlloc(LMEM_FIXED, size)` | Win32 heap | 66, 73 |
| `LocalFree()` | Win32 heap | 91, 93 |
| `SS_ASSERT` | SSDEBUG.H | 67, 74 |
| `BOOL` | typedef | 58, 104, 282, 299 |
**Shim**: `LocalAlloc(LMEM_FIXED,n)`→`malloc(n)`, `LocalFree`→`free` (trivial, category 5-lite).
Rest is GL evaluators + MATH.C calls (`ss_matrix*`/`ss_xformPoint` 517–535).

### XC.CXX (439 lines)
| Symbol | Kind | Lines |
|---|---|---|
| `<GL/glaux.h>` include | glaux header | 19 |
| `LocalFree()` | Win32 heap | 371 |
| `LocalAlloc(LMEM_FIXED, …)` | Win32 heap | 386, 394 |
| `RtlCopyMemory()` | winnt.h macro (= memcpy) | 396 |
| `SS_ASSERT` | macro | 387, 395 |
`FLT_MAX` (263–264) is `<float.h>`, standard. `ss_fRand` 171, 188–222. **Shim**: same as EVAL.CXX
plus `#define RtlCopyMemory(d,s,n) memcpy(d,s,n)`.

### DIALOG.C (326 lines) — config dialog + settings; Win32-heavy but mostly replaceable UI
Algorithm-relevant part is only the **globals** it defines (`bFlexMode` 29, `bMultiPipes` 30,
`ulJointType` 34, `ulSurfStyle` 38, `ulTexQuality` 42, `fTesselFact` 47, `gTexFile[MAX_TEXTURES]` 53,
`gnTextures` 54) and `getIniSettings()` (65–117). Symbol list:
| Symbol | Kind | Lines |
|---|---|---|
| `<commdlg.h> <scrnsave.h> <commctrl.h> <sys\timeb.h>` | includes | 11, 12, 21, 23 |
| `ULONG`, `BOOL`, `TCHAR`, `MAX_PATH`, `HWND`, `HANDLE`, `UINT`, `WPARAM`, `LPARAM` | types | 29–53, 101, 124, 168, 236, 248–249 |
| `LoadString()` | resource string load | 72, 183 |
| `hMainInstance` | global HINSTANCE (defined COMMON/SCRNSAVE.CXX:35) | 72, 81, 138, 183 |
| `szScreenSaver` | global TCHAR[22] (COMMON/SCRNSAVE.CXX:745) | 72 |
| `ss_RegistrySetup` / `ss_GetRegistryInt` / `ss_GetRegistryString` | COMMON/DIALOG.C (see below) | 81; 83, 85, 87, 89, 93, 95, 102; 101 |
| `ss_WriteRegistryInt` / `ss_WriteRegistryString` | COMMON/DIALOG.C | 140–146, 149; 148 |
| `ss_GetTrackbarPos` / `ss_SetupTrackbar` / `InitCommonControls` | commctrl helpers | 144; 178; 175 |
| `SendDlgItemMessage`, `CB_ADDSTRING/CB_SETCURSEL/CB_GETCURSEL`, `CBN_EDITCHANGE/CBN_SELCHANGE` | dialog msgs | 185, 188, 299–300; 296–297 |
| `CheckDlgButton`, `EnableWindow`, `GetDlgItem` | dialog | 205–233 |
| `EndDialog` | dialog | 309, 313 |
| `RegisterDialogClasses(HANDLE)` `BOOL WINAPI` | scrnsave.lib export | 236 |
| `ScreenSaverConfigureDialog`, `WM_INITDIALOG`, `WM_COMMAND`, `LOWORD/HIWORD`, `IDOK/IDCANCEL` | dialog proc | 248–313 |
| `ss_SelectTextureFile` | common-dlg file picker (TEXTURE.C) | 290 |
| `ss_LoadTextureResourceStrings` | TEXTURE.C | 77 |
**Port note**: compile only `getIniSettings()` + globals (or keep whole file with dialog-proc
stubs). `getIniSettings` needs LoadString + registry shims that return defaults / query-string
overrides (category 2/3/5).

---

## COMMON

### UTIL.CXX (200 lines) — RNG + timing; category (4) central site
| Symbol | Kind | Lines |
|---|---|---|
| `<sys/timeb.h>` | MSVC CRT | 14, 17 |
| `struct _timeb` | MSVC CRT type | 32, 164 |
| `_ftime()` | MSVC CRT — **all game timing flows through here** (SS_TIME::Update used by SS_TIMER, used by CLEAR.CXX calibration) | 34, 166 |
| `FLOAT` | windows typedef | 147, 149 |
| `ULONG`, `PCH` | types (DbgPrint, `#if DBG` only) | 185 |
| `OutputDebugStringA()` | Win32 dbg (`#if DBG` only) | 194 |
RNG: `rand()` 117, 137, 152, 170, `srand(time.millitm)` 167, `RAND_MAX` 117, 137, 152 — all
standard C; **seed source is `_ftime().millitm`** (166–167). `GetTickCount` is used **nowhere** in
any audited file; `time()` never called directly — the only time source is `_ftime` (here,
SSPIPES.CXX:127/150, and transitively CLEAR.CXX via SS_TIMER).
**Shim**: `struct _timeb {time_t time; unsigned short millitm;}` + `_ftime()` via
`gettimeofday`/`emscripten_get_now` (real logic; must be monotonic-ish for clear calibration).

### SSUTIL.CXX (412 lines) — pixel-format/OS plumbing; ~all Win32, mostly droppable
| Symbol | Kind | Lines |
|---|---|---|
| `OSVERSIONINFO` static | type | 25 |
| `PIXELFORMATDESCRIPTOR`, `PFD_*` flags | types/consts | 43–136, 152, 163–164, 181–183, 196, 202 |
| `GetDeviceCaps(hdc, BITSPIXEL/PLANES)` | GDI | 48 |
| `DescribePixelFormat` | GDI/wgl | 61, 163, 202 |
| `ChoosePixelFormat` | GDI/wgl | 140 |
| `SetPixelFormat` | GDI/wgl | 159 |
| `GetPixelFormat` | GDI/wgl | 200 |
| `Sleep(1000)` | kernel32 | 169 |
| `SS_WARNING`, `SS_DBGLEVEL1` | SSDEBUG.H | 138, 160 |
| `HDC` | type | 40, 152, 196, 219 |
| `DEVMODE`, `DM_PELSWIDTH/HEIGHT`, `DM_BITSPERPEL`, `CDS_FULLSCREEN`, `DISP_CHANGE_SUCCESSFUL` | display-mode | 222–240 |
| `ChangeDisplaySettings` | user32 | 235 |
| `EnumDisplaySettings` | user32 | 256 |
| `GetVersionEx`, `VER_PLATFORM_WIN32_NT`, `VER_PLATFORM_WIN32_WINDOWS` | version | 311; 332; 356 |
| `gpss->type`, `SS_TYPE_PREVIEW/FULLSCREEN/CONFIG/NORMAL` | internal (SSINTRNL.HXX) | 372, 385, 391, 397 |
| `RedrawWindow`, `RDW_*` | user32 | 410–411 |
| `DbgPrint` | `#ifdef SS_DEBUG` | 283 |
Functions the algorithm path actually needs: `ss_QueryGLVersion` (270–285: `glGetString(GL_VERSION)`
274, sets `gbGLv1_1`/`gbTextureObjects`), `ss_fOnGL11` (295), `ss_fOnWin95` (348–360: return FALSE
in shim — gates MessageBox in TEXTURE.C:808 and bmp-name logic 976). Everything else
(pixel format, display settings, preview/fullscreen queries) is Win32 scaffolding replaced by the
emscripten canvas context. If compiled as-is, needs the full PFD type set stubbed.

### MATERIAL.C (351 lines) — **100% clean** (category 1)
windows.h (16). `BOOL` 289, 308, 325, 343; `FLOAT` 232, 244. `ss_iRand` 294, 312, 330, 347.
Material-name constants from `matname.h`. Pure GL (`glMaterialfv` 196–203). No Win32 calls.

### MATH.C (197 lines) — **100% clean** (category 1)
windows.h (10). `ULONG` 181. Pure math + GL typedefs. No Win32 calls.

### COLOR.C (68 lines) — **100% clean** (category 1)
windows.h (1). `RGBA` struct (sscommon.h). Pure math. No Win32 calls.

### CLEAR.CXX (407 lines)
| Symbol | Kind | Lines |
|---|---|---|
| `<sys/timeb.h>` | MSVC CRT (via SS_TIMER in util.hxx) | 14, 17 |
| `SS_TIMER` timing (→ `_ftime`) | category (4), indirect | 47, 64, 98, 158, 165, 174 |
| `LocalFree()` | Win32 heap | 141, 318 |
| `LocalAlloc(LMEM_FIXED, …)` | Win32 heap | 314 |
| `BOOL/TRUE/FALSE` | typedefs | 44, 207–290 passim |
| `DrawGdiRect(HDC, HBRUSH, RECT*)`: `FillRect`, `GdiFlush` | GDI — small helper, used by window/dialog scaffolding only, not by pipes draw path; safe to stub | 331–339 (calls 337, 338) |
| `ss_GdiRectWipeClear`: `GetDC` 369, `CreateSolidBrush`/`RGB` 371, `FillRect` 377–386, `DeleteObject` 401, `ReleaseDC` 403, `GdiFlush` 405, `HWND/HDC/HBRUSH/RECT` | GDI — **entire function inside `#ifdef SS_INITIAL_CLEAR` (341, 407), not compiled by default** | 341–407 |
`ss_iRand` 248. Quirk (pre-existing bug, keep as-is): `searchUp` used uninitialized at 260/274.
The dissolve clear itself (207–290) is pure GL scissor+clear.

### TEXTURE.C (1137 lines) — the Win32-heaviest common file
| Symbol | Kind | Lines |
|---|---|---|
| `<scrnsave.h>` (for `hMainInstance`) | include | 17, 21 |
| `hMainInstance` | global | 53–75 (9 LoadString calls) |
| `LoadString()` | resources (strings) — category (3) | 53, 55, 56, 57, 60, 63, 68, 69, 72, 74 |
| `lstrlen/lstrcpy/lstrcat/lstrcmpi` | kernel32 str (→ strlen/strcpy/strcat/strcasecmp) | 59, 61–62, 64, 795, 806, 849, 859–887, 945, 978, 994–1009, 1128, 1131, 1133 |
| `TEXT()` / `TCHAR/LPTSTR/LPCTSTR/PTSTR` | tchar types | 62, 66, 791–793, 978–1009, 1119–1136 |
| `auxDIBImageLoad()` | **glaux** — loads .bmp FILE from disk (user textures path) | 106/108, 139/141 |
| `auxRGBImageLoad()` | **glaux** — loads .rgb FILE from disk | 145/147 |
| `GetModuleHandle(NULL)` | kernel32 | 175 |
| `HMODULE/HRSRC/HGLOBAL/LPVOID/PSZ` | types | 168–172, 199 |
| `MAKEINTRESOURCE` | macro | 179, 182, 185, 189 |
| `FindResource` / `LoadResource` / `LockResource` / `FreeResource` | **resource loading — category (3)** | 189; 194; 199; 219 |
| `min()` | windef macro | 260 |
| `gluScaleImage()` | GLU (already ported) | 297 |
| `wglGetProcAddress()` | wgl — paletted-texture ext probe; shim returns NULL ⇒ feature off | 749, 753, 759 |
| `SearchPath()` | kernel32 path search | 797, 867 |
| `MessageBox()`, `wsprintf()` | user32 | 811/810, 1053/1052, 1067/1065 |
| `OPENFILENAME`, `OFN_*`, `GetOpenFileName()` | commdlg (texture picker UI — droppable) | 837, 891–910, 919 |
| `GetWindowsDirectory()` | kernel32 | 886 |
| `RegOpenKeyEx/HKEY_LOCAL_MACHINE/KEY_QUERY_VALUE/RegQueryValueEx/RegCloseKey/HKEY/LPDWORD/LPBYTE` | **registry — category (2)** (only in `ss_GetDefaultBmpFile`, which Pipes never calls) | 973–998 |
| `LocalAlloc/LocalFree` | heap | 585, 598 |
| `RGBQUAD`, `BYTE` | GDI types | 557, 582, 634–642, 289 |
| `PFNGLCOLORTABLEEXTPROC` etc. | ext typedefs (sscommon.h fallback def) | 33–35 |
| `MAX_PATH`, `GEN_STRING_SIZE` | consts | 68–75, 791 etc. |

**Where TEXTURE.C gets BMP bytes** — two distinct paths:
1. *User texture files*: `ss_LoadTextureFile` (126–155) → `auxDIBImageLoad(pathname)` /
   `auxRGBImageLoad(pathname)`; the file I/O happens inside the glaux library (not visible in this
   source; no fopen/CreateFile here). Only reached when registry supplies a texture path.
2. *Default resource*: `ss_LoadTextureResource` (166–227) → `FindResource(ghmodule,
   MAKEINTRESOURCE(pTexRes->name), RT_MYBMP=100)` (189) → `LoadResource` (194) → `LockResource`
   (199) yielding a raw pointer `pv` to the **complete .BMP file image in memory**, then
   `ss_DIBImageLoad(pv, pTex)` (211). For Pipes the resource is `IDB_DEFTEX` (=99, PIPES/DIALOG.H:121)
   = `stripe.bmp` (SSPIPES.RC:80). **Port strategy**: embed STRIPE.BMP bytes and have the
   FindResource/LoadResource/LockResource shims hand back that buffer — `ss_DIBImageLoad` then
   consumes it (see SSDIB.C below for the GDI caveat).

### SSINIT.CXX (225 lines)
| Symbol | Kind | Lines |
|---|---|---|
| `<commdlg.h> <scrnsave.h>` | includes | 10, 11 |
| `LPRECT` | type (gRepaintFunc ptr) | 27 |
| `HWND` | type (`ss_GetHWND` 191–197, `ss_GetGLHWND` 205–211) | 191, 205 |
| `FLOAT` | type | 164 |
| `gpss` / `PSSW` / `psswGL` / `psswMain` | internal window-class globals from SSINTRNL.HXX (Win32-laden: HWND/HDC members) | 66–67, 79–80, 92–93, 105–106, 118–119, 133–134, 147–148, 166–167, 181–182, 194–195, 208–209, 222–223 |
| debug globals `ssDebugMsg/ssDebugLevel` | `#if DBG` | 34–42 |
No direct Win32 *calls*, but every function dereferences `gpss->psswGL`/`psswMain` (SSWindow C++
classes from the scaffolding that is being replaced). **Port**: either provide a minimal `PSS`/`SSW`
shim struct carrying the callback pointers + window size + dummy HWND, or reimplement these 15
small setters/getters in the shim layer (they are the callback-registration API the pipes code uses:
`ss_InitFunc/ss_UpdateFunc/ss_ReshapeFunc/ss_RepaintFunc/ss_FinishFunc/ss_DataPtr/ss_GetScreenSize`).

### DIALOG.C (161 lines) — **implements ss_GetRegistryInt — category (2) core**
| Symbol | Kind | Lines |
|---|---|---|
| `<commdlg.h> <commctrl.h>` | includes | 14, 15 |
| `TCHAR` statics, `HINSTANCE` | types | 19–23 |
| `LoadString()` | resource strings — category (3) | 34, 35, 53, 69, 84, 99 |
| `GetPrivateProfileInt()` | **WinINI API** | 54 |
| `GetPrivateProfileString()` | WinINI | 70 |
| `WritePrivateProfileString()` | WinINI | 86, 100 |
| `wsprintf`, `TEXT()` | user32/tchar | 85 |
| `SendDlgItemMessage`, `TBM_GETPOS/TBM_SETRANGE/TBM_SETPOS/TBM_SETPAGESIZE/TBM_SETLINESIZE`, `MAKELONG`, `WPARAM/LPARAM`, `HWND` | commctrl trackbar | 111–161 (114–117, 133–160, 138) |

Quoted logic — the whole "registry" layer is actually the Private-Profile (.INI) API; on NT these
calls are transparently redirected to the registry by the IniFileMapping of `control.ini`
(section `"Screen Saver.3DPipes"` per SSPIPES.RC:171, file `"control.ini"` per SSCOMMON.RC:24):

```c
/* COMMON/DIALOG.C:32-41 */
BOOL ss_RegistrySetup( HINSTANCE hinst, int section, int file )
{
    if( LoadString(hInstance, section, szSectName, BUF_SIZE) &&
        LoadString(hInstance, file, szFname, BUF_SIZE) )
    {
        hInstance = hinst;
        return TRUE;
    }
    return FALSE;
}

/* COMMON/DIALOG.C:51-57 */
int  ss_GetRegistryInt( int name, int iDefault )
{
    if( LoadString( hInstance, name, szItemName, BUF_SIZE ) ) {
        return GetPrivateProfileInt(szSectName, szItemName, iDefault, szFname);
    }
    return 0;
}
```
i.e. the "GetPrivateProfileInt fallback" IS the implementation — there is no direct RegQueryValue
here; `iDefault` is returned by `GetPrivateProfileInt` when the key is absent, and **0** (not
iDefault!) if the string-table lookup fails. (Note pre-existing quirk: `ss_RegistrySetup` reads
`hInstance` *before* assigning it, so the first LoadString pair runs with hInstance=0 = main exe —
harmless on Windows, but a shim must not assert on a NULL instance.)
**Shim**: implement LoadString from a static ID→string table (IDs in PIPES/DIALOG.H:42-48 →
"JointType","SurfStyle","TextureQuality","Flex","MultiPipes","Tesselation","Texture",
"TextureFileOffset") and GetPrivateProfileInt/String over localStorage/query params/defaults.

### SSIMAGE.C (256 lines) — .rgb (SGI) images
| Symbol | Kind | Lines |
|---|---|---|
| `HANDLE file` member | type | 32 |
| `RtlCopyMemory()` | winnt macro (= memcpy) | 97, 98, 132, 155 |
| `PVOID`, `BOOL`, `DWORD`, `LPVOID`, `LPTSTR`, `LPOVERLAPPED` | types | 53, 182, 213, 216, 226, 233 |
| `LocalAlloc(LMEM_FIXED\|LMEM_ZEROINIT)` / `LocalFree` | heap | 220; 253 |
| `CreateFile(GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING)` | file I/O (bVerifyRGB only) | 226 |
| `INVALID_HANDLE_VALUE` | const | 229 |
| `ReadFile()` | file I/O | 233 |
| `CloseHandle()` | file I/O | 250 |
`ss_RGBImageLoad(PVOID pv, TEXTURE*)` (182–198) parses an .rgb image **from a memory pointer** —
pure logic once RtlCopyMemory is defined; usable as-is. `bVerifyRGB` (213–256) is real file I/O but
is only reached for user-selected .rgb files (TEXTURE.C:1043) — stub FALSE or implement via stdio.

### SSDIB.C (496 lines) — .bmp images; **category (5): GDI does real work here**
| Symbol | Kind | Lines |
|---|---|---|
| `WORD/DWORD/PWORD/PBYTE/PVOID/VOID FAR*/LONG/BOOL` | types | 27–31, 45, 114–125, 337 |
| `UNALIGNED` keyword | **MSVC-specific** (define empty) | 48, 49, 119, 120 |
| `BITMAPFILEHEADER/BITMAPINFOHEADER/BITMAPCOREHEADER/BITMAPINFO/RGBQUAD/RGBTRIPLE` | GDI structs (shim must define, packed correctly) | 48–52, 119–131, 139–260, 408–410 |
| `LocalAlloc(LMEM_FIXED…)` / `LocalFree` | heap | 172, 286; 375, 378 |
| `HDC`, `CreateCompatibleDC(NULL)` | GDI | 128, 278 |
| `HBITMAP`, `CreateDIBSection(…, DIB_RGB_COLORS, &pjBitsRGB, NULL, 0)` | GDI | 129, 301 |
| `SelectObject()` | GDI | 309 |
| `SetDIBits()` | GDI — **does the actual format conversion** (any bpp/RLE → 24bpp) | 317 |
| `GdiFlush()` | GDI | 322 |
| `DeleteDC()` / `DeleteObject()` | GDI | 369; 372 |
| `BI_RGB` | const | 229, 297 |
| `CreateFile/CreateFileMapping/MapViewOfFile/UnmapViewOfFile/CloseHandle/INVALID_HANDLE_VALUE/PAGE_READONLY/FILE_MAP_READ` | file mapping (bVerifyDIB only) | 419, 423, 427, 487, 490, 493 |

**Can `ss_DIBImageLoad` parse a raw .BMP file image from memory?** Yes — that is exactly its
contract: `ss_DIBImageLoad(PVOID pv, TEXTURE *ptex)` (114) takes a pointer to a complete BMP file
image (it checks `bfType=='BM'` at 141 and also accepts a headerless raw BITMAPINFOHEADER/
BITMAPCOREHEADER blob at 152–157), locates the pixel bits (148–149 via `bfOffBits`, or 214–216 /
257–259 after the color table), and normalizes the header into `pbmiSource` (171–264) — all
portable. **BUT** the pixel decoding is delegated to GDI: it creates a 24bpp DIB section (278–312)
and calls `SetDIBits` (317) to let GDI convert the source format, then swaps R/B and strips row
padding into a packed RGB buffer (334–349, `padBytes = biWidth % sizeof(LONG)` at 337 — correct for
24bpp rows). So the shim needs *real logic*: either (a) implement CreateDIBSection (malloc
`biSizeImage` at pbmiRGB->bmiHeader values, 292–299) + SetDIBits as a BMP decoder for the formats we
care about, or (b) note that the only BMP the wasm build must decode is the embedded `STRIPE.BMP`,
which is **BITMAPINFOHEADER, 100×100, 1 plane, 24bpp, BI_RGB uncompressed** (verified bytes:
`42 4d … 28 00 00 00 64 00 00 00 64 00 00 00 01 00 18 00 00 00 00 00`), for which SetDIBits is a
straight row copy (width 100 ⇒ padBytes 0). A 24bpp+8bpp-palette SetDIBits shim covers any
reasonable user BMP too. `bVerifyDIB` (405–496) uses file mapping and is only reached for
user-picked files — stub FALSE or reimplement with stdio.

---

## Cross-cutting summaries

### (1) Files that are 100% clean (only need windows.h-style typedefs shimmed)
- PIPES: **NSTATE.CXX, FSTATE.CXX, NPIPE.CXX, FPIPE.CXX, VIEW.CXX, NODE.CXX, OBJECTS.CXX**
- COMMON: **MATERIAL.C, MATH.C, COLOR.C**
- Nearly clean (only LocalAlloc/LocalFree/RtlCopyMemory macros): **EVAL.CXX, XC.CXX**;
  CLEAR.CXX is clean once `DrawGdiRect` (331–339) is stubbed (GDI-clear variant is `#ifdef`'d out)
  and Local*→malloc mapped.
- PIPE.CXX is clean except the **auxSolidTeapot** glaux call (65).

### (2) Registry / WinINI call sites
- `ss_GetRegistryInt` is implemented in **COMMON/DIALOG.C:51–57** via `GetPrivateProfileInt`
  (COMMON/DIALOG.C:54); siblings `ss_GetRegistryString` :70, `ss_WriteRegistryInt` :86,
  `ss_WriteRegistryString` :100, setup `ss_RegistrySetup` :32–41 (logic quoted above). Section/file
  strings: `"Screen Saver.3DPipes"` (SSPIPES.RC:171), `"control.ini"` (SSCOMMON.RC:24).
- Callers: PIPES/DIALOG.C getIniSettings 83–113 & saveIniSettings 140–156.
- True registry API (`RegOpenKeyEx`/`RegQueryValueEx`/`RegCloseKey`): only
  COMMON/TEXTURE.C:980–998 (`ss_GetDefaultBmpFile`, never called by Pipes) and the scaffolding
  SCRNSAVE.CXX (not in our compile list).

### (3) Resource loading
- Strings: `LoadString` in PIPES/DIALOG.C:72,183; COMMON/DIALOG.C:34,35,53,69,84,99;
  COMMON/TEXTURE.C:53–75. Needs an ID→string table shim (IDs: PIPES/DIALOG.H, COMMON/SSCOMMON.H:295+,
  values in PIPES/SSPIPES.RC:120–175 and COMMON/SSCOMMON.RC:22–29).
- Binary: `FindResource/LoadResource/LockResource/FreeResource` + `GetModuleHandle` only in
  COMMON/TEXTURE.C:175–219, resource type `RT_MYBMP`(=100)/`RT_RGB`(=99)/`RT_A8`(=101)
  (SSCOMMON.H:119–121). Pipes' only resource texture: IDB_DEFTEX=99 → stripe.bmp (24bpp 100×100).
  Shim: return pointer to embedded stripe.bmp bytes; ss_DIBImageLoad consumes it (with the SetDIBits
  caveat in SSDIB.C above).

### (4) Time call sites (`GetTickCount` / `time()` / `_ftime`)
- `GetTickCount`: **not used** in any audited file. `time()`: never called (header included only).
- `_ftime(struct _timeb*)`: COMMON/UTIL.CXX:34 (SS_TIME::Update → SS_TIMER → CLEAR.CXX clear
  calibration at 64/98/165/174), COMMON/UTIL.CXX:166 (`ss_RandInit` RNG seed from `.millitm`),
  PIPES/SSPIPES.CXX:127,150 (DO_TIMING pipe-rate meter — compiled because SSPIPES.H:21 defines
  DO_TIMING). One shim function covers all.

### (5) Shims requiring real logic (not just stubs)
1. **wgl context family** (STATE.CXX 53–54, 145, 463–474, 846–847): single-context emulation —
   return one dummy non-zero HGLRC everywhere; MakeCurrent/ShareLists/Delete no-ops. Multi-pipe
   mode then works on one shared context.
2. **`_ftime`/`struct _timeb`** (UTIL.CXX 32–35, 164–167; SSPIPES.CXX 122–150): real wall-clock
   (seconds + millitm) — feeds RNG seed and dissolve-clear calibration.
3. **`auxSolidTeapot`** (PIPE.CXX:65): real teapot geometry (glaux port) — visible feature
   (teapot easter-egg joints).
4. **BMP decode path** (SSDIB.C 278–322): CreateDIBSection+SetDIBits replacement (24bpp copy +
   8/4/1bpp palette expansion) so `ss_DIBImageLoad` can decode the embedded stripe.bmp/user BMPs;
   plus resource shims (TEXTURE.C 175–219) returning embedded bytes.
5. **LoadString + Private-Profile settings** (COMMON/DIALOG.C 34–100, TEXTURE.C 53–75,
   PIPES/DIALOG.C 72): string-table shim + settings store; must reproduce
   `GetPrivateProfileInt(section,item,iDefault,file)` default semantics.
6. **`ss_iRand`/`ss_fRand` RNG** already portable (UTIL.CXX 115–152, plain `rand()`); only the
   `srand(millitm)` seeding (167) depends on shim #2.
7. **glaux image loaders** `auxDIBImageLoad`/`auxRGBImageLoad` (TEXTURE.C 106–147): only needed if
   user texture files are supported; can route to `ss_DIBImageLoad`/`ss_RGBImageLoad` over
   fetched/preloaded bytes, or return NULL to force the resource fallback in
   STATE::LoadTextureFiles (STATE.CXX 225–235).
8. **SSINIT.CXX internals**: needs a minimal `gpss`/`PSSW` shim struct (callback ptrs, ISIZE size,
   dummy HWND) — or reimplementation of its 15 tiny functions in the new main loop.
9. `wglGetProcAddress` (TEXTURE.C 749–759): return NULL — cleanly disables paletted-texture path
   (`gbPalettedTexture` stays FALSE; Pipes never uses TEX_A8).
10. `SearchPath`/`MessageBox`/`GetOpenFileName`/commctrl/dialog APIs: stub (return failure /
   no-op) — they sit on user-texture/config-UI paths only. `ss_VerifyTextureFile` (TEXTURE.C:787)
   is called from ss_Init (SSPIPES.CXX:54) whenever `ulSurfStyle==SURFSTYLE_TEX`; a SearchPath stub
   returning 0 makes it return FALSE ⇒ user textures dropped ⇒ default resource texture used.

### Debug macro / global inventory (SSDEBUG.H)
`SS_ASSERT` → plain `assert` when `DBG` undefined (SSDEBUG.H:77–79); all other `SS_*` macros expand
to nothing in release (SSDEBUG.H:63–99). Used at: VIEW.CXX:34; NODE.CXX:72,137,192,308,326,364,484,
533,641; EVAL.CXX:67,74; XC.CXX:387,395; STATE.CXX:402 (SS_DBGINFO); SSUTIL.CXX:138 (SS_WARNING),
160 (SS_DBGLEVEL1); TEXTURE.C:247,291,303 (SS_WARNING). `DbgPrint` prototype (SSDEBUG.H:13) uses
Win32 `ULONG`/`PCH`; definition (UTIL.CXX:185, `#if DBG`) uses `OutputDebugStringA` (UTIL.CXX:194).
Debug globals `ssDebugMsg`/`ssDebugLevel` defined in SSINIT.CXX:34–42 under `#if DBG` only.
Compile with `DBG` undefined and none of this needs Win32.
