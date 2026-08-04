/*
 * ssshell.c — web replacement for the Win32 screensaver shell.
 *
 * Replaces (is linked INSTEAD of) these original framework files, which are
 * pure Win32 scaffolding:
 *   COMMON/SCRNSAVE.CXX  (message pump, password handling)
 *   COMMON/SSWPROC.CXX   (wndprocs, WM_TIMER dispatch)
 *   COMMON/SSWINDOW.CXX  (HWND/window management, wgl setup)
 *   COMMON/SSINIT.CXX    (callback registration tied to SSW windows)
 *   COMMON/GLSCRNSV.CXX  (SCRNSAVE class, pixel formats)
 *   COMMON/PALETTE.CXX   (256-color palette handling)
 *   COMMON/DLGDRAW.CXX, FASTDIB.C, SSDIB.C, SSA8.C, SSIMAGE.C (GDI/DIB)
 *
 * The replacement reproduces the original lifecycle exactly
 * (see GLSCRNSV.CXX SCRNSAVE::Init, SSWINDOW.CXX SSW::InitGL,
 *  SSWPROC.CXX ScreenSaverProc):
 *
 *   1. ss_RandInit()            [SCRNSAVE::Init, GLSCRNSV.CXX:147]
 *   2. pssc = ss_Init()         [GLSCRNSV.CXX:161 — client init, reads settings]
 *   3. create GL context        [SSW::hrcSetupGL — single-buffered, 16-bit z]
 *   4. (*InitFunc)(DataPtr)     [SSW::InitGL, SSWINDOW.CXX:676]
 *   5. Reshape()                [SSW::InitGL, SSWINDOW.CXX:685]
 *   6. SetTimer(16ms); each WM_TIMER -> (*UpdateFunc)(DataPtr)
 *                               [SSWPROC.CXX:58 uiTimeOut=16, SSWINDOW.CXX:1107]
 *
 * Settings come from a small in-memory "profile" store with the same
 * key names the original wrote to control.ini / the registry
 * ([Screen Saver.3DPipes] JointType/SurfStyle/Flex/MultiPipes/...); the
 * store is pre-loaded with NT4 out-of-the-box values and can be
 * overridden from JS (window.PIPES_CONFIG) before startup.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <GL/gl.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include <windows.h>   /* our shim */

/* ---- resource-string ids we must resolve (values from the original
 *      PIPES/DIALOG.H, PIPES/RESOURCE.H and COMMON/SSCOMMON.H) ---- */
#define IDS_SAVERNAME           1002
#define IDS_JOINTTYPE           1008
#define IDS_SURFSTYLE           1009
#define IDS_TEXQUAL             1010
#define IDS_FLEX                1020
#define IDS_MULTIPIPES          1021
#define IDS_TEXTURE_COUNT       1029
#define IDS_GENNAME             9003
#define IDS_INIFILE             9006
#define IDS_TEXTURE             9126
#define IDS_TEXTURE_FILE_OFFSET 9127
#define IDS_TESSELATION         9130

/* ---- callback types (mirrors SSCOMMON.H) ---- */
typedef void (*SSINITPROC)(void *);
typedef void (*SSRESHAPEPROC)(int, int, void *);
typedef void (*SSREPAINTPROC)(LPRECT, void *);
typedef void (*SSUPDATEPROC)(void *);
typedef void (*SSFINISHPROC)(void *);
typedef void (*SSFLOATERBOUNCEPROC)(void *);

typedef struct {
    int  width;
    int  height;
} ISIZE;

/* SSContext layout must match SSCOMMON.H exactly (returned by ss_Init) */
typedef struct { float x, y; } POINT2D_SH;
typedef struct { POINT2D_SH posInc, posIncVary; } MOTION_INFO_SH;
typedef struct { ISIZE size; struct { int x, y; } pos; MOTION_INFO_SH motionInfo; } CHILD_INFO_SH;
typedef void (*SSCHILDSIZEPROC_SH)(ISIZE *, CHILD_INFO_SH *);
typedef struct { BOOL bMotion; BOOL bSubWindow; SSCHILDSIZEPROC_SH ChildSizeFunc; } FLOATER_INFO_SH;
typedef struct { HDC hdc; ISIZE size; HBITMAP hbm; HBITMAP hbmOld; } SS_BITMAP_SH;
typedef struct { BOOL bRatioMode; float widthRatio, heightRatio; int baseWidth, baseHeight; SS_BITMAP_SH ssbm; } STRETCH_INFO_SH;
typedef struct {
    BOOL bFloater;
    FLOATER_INFO_SH floaterInfo;
    BOOL bStretch;
    STRETCH_INFO_SH stretchInfo;
    BOOL bDoubleBuf;
    int  depthType;
} SSContext;

extern SSContext *ss_Init(void);   /* PIPES/SSPIPES.CXX */
extern void ss_RandInit(void);     /* COMMON/UTIL.CXX (srand -> msvc_srand) */
extern void msvc_srand(unsigned int seed);
extern int  ss_QueryPalettedTextureEXT(void);  /* COMMON/TEXTURE.C */

/* ---- scrnsave.h globals ---- */
HINSTANCE hMainInstance = (HINSTANCE)1;
HWND      hMainWindow   = (HWND)1;
BOOL      fChildPreview = FALSE;
char      szName[64];
char      szAppName[64];
char      szIniFile[64];
char      szScreenSaver[64];

/* ---- registered client callbacks (replaces SSINIT.CXX globals) ---- */
static SSINITPROC          gInitFunc    = NULL;
static SSRESHAPEPROC       gReshapeFunc = NULL;
static SSREPAINTPROC       gRepaintFunc = NULL;
static SSUPDATEPROC        gUpdateFunc  = NULL;
static SSFINISHPROC        gFinishFunc  = NULL;
static SSFLOATERBOUNCEPROC gBounceFunc  = NULL;
static void               *gDataPtr     = NULL;

void ss_InitFunc(SSINITPROC f)             { gInitFunc = f; }
void ss_ReshapeFunc(SSRESHAPEPROC f)       { gReshapeFunc = f; }
void ss_RepaintFunc(SSREPAINTPROC f)       { gRepaintFunc = f; }
void ss_UpdateFunc(SSUPDATEPROC f)         { gUpdateFunc = f; }
void ss_FinishFunc(SSFINISHPROC f)         { gFinishFunc = f; }
void ss_FloaterBounceFunc(SSFLOATERBOUNCEPROC f) { gBounceFunc = f; }
void ss_DataPtr(void *p)                   { gDataPtr = p; }

/* ---- mode queries (SSUTIL.CXX equivalents; we always run the
 *      full-screen, non-preview, GL 1.1 path) ---- */
BOOL ss_fPreviewMode(void)    { return FALSE; }
BOOL ss_fFullScreenMode(void) { return TRUE; }
BOOL ss_fWindowMode(void)     { return FALSE; }
BOOL ss_fConfigMode(void)     { return FALSE; }
BOOL ss_fOnWin95(void)        { return FALSE; }
BOOL ss_fOnNT35(void)         { return FALSE; }

HWND ss_GetHWND(void)   { return hMainWindow; }
HWND ss_GetGLHWND(void) { return hMainWindow; }

static ISIZE gScreenSize = { 640, 480 };

void ss_GetScreenSize(ISIZE *size) { *size = gScreenSize; }

BOOL ss_SetWindowAspectRatio(float aspect) { (void)aspect; return FALSE; }
void ss_RandomWindowPos(void) {}
BOOL ss_ChangeDisplaySettings(int w, int h, int bpp) { (void)w; (void)h; (void)bpp; return FALSE; }
void ss_QueryDisplaySettings(void) {}
void ss_QueryOSVersion(void) {}
BOOL ss_RedrawDesktop(void) { return TRUE; }

/* ---- settings store ----
 * Key names and section are exactly what the original wrote to
 * control.ini / registry (PIPES/SSPIPES.RC stringtable).  Values here are
 * the NT4 out-of-the-box configuration of the shipped sspipes.scr
 * (multiple pipes, mixed joints, solid surface, mid tesselation).
 * The SDK sample's compiled-in fallbacks (single pipe, elbow joints) apply
 * only when a value is removed from this store AND the code's iDefault is
 * used; see PATCHES.md "Settings" for discussion. */
typedef struct { const char *key; int val; int present; } PROFILE_INT;

static PROFILE_INT gProfile[] = {
    { "JointType",      2, 1 },   /* JOINT_MIXED */
    { "SurfStyle",      0, 1 },   /* SURFSTYLE_SOLID */
    { "TextureQuality", 0, 1 },   /* TEXQUAL_DEFAULT */
    { "Tesselation",  100, 1 },   /* -> fTesselFact 1.0 -> 16 slices */
    { "Flex",           0, 1 },   /* normal (non-flex) pipes */
    { "MultiPipes",     1, 1 },   /* up to MAX_DRAW_THREADS=4 pipes */
    { "TextureCount",   0, 1 },
    { "TexOffset",      0, 1 },
};
#define N_PROFILE (sizeof(gProfile)/sizeof(gProfile[0]))

static char gTexturePath[MAX_PATH] = "";  /* "Texture" string setting */

EMSCRIPTEN_KEEPALIVE
void pipes_set_setting(const char *key, int val)
{
    for (unsigned i = 0; i < N_PROFILE; i++) {
        if (!strcasecmp(gProfile[i].key, key)) {
            gProfile[i].val = val;
            gProfile[i].present = 1;
            return;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void pipes_set_texture_path(const char *path)
{
    strncpy(gTexturePath, path, MAX_PATH - 1);
    gTexturePath[MAX_PATH - 1] = '\0';
}

/* ---- resource strings (values from the original .RC stringtables) ---- */
static const struct { int id; const char *str; } gStrings[] = {
    { IDS_SAVERNAME,   "Screen Saver.3DPipes" },
    { IDS_GENNAME,     "ScreenSaver" },
    { IDS_INIFILE,     "control.ini" },
    { IDS_JOINTTYPE,   "JointType" },
    { IDS_SURFSTYLE,   "SurfStyle" },
    { IDS_TEXQUAL,     "TextureQuality" },
    { IDS_FLEX,        "Flex" },
    { IDS_MULTIPIPES,  "MultiPipes" },
    { IDS_TEXTURE_COUNT, "TextureCount" },
    { IDS_TESSELATION, "Tesselation" },
    { IDS_TEXTURE,     "Texture" },
    { IDS_TEXTURE_FILE_OFFSET, "TexOffset" },
};

int LoadStringA(HINSTANCE hinst, UINT id, char *buf, int bufMax)
{
    (void)hinst;
    for (unsigned i = 0; i < sizeof(gStrings)/sizeof(gStrings[0]); i++) {
        if ((UINT)gStrings[i].id == id) {
            strncpy(buf, gStrings[i].str, bufMax - 1);
            buf[bufMax - 1] = '\0';
            return (int)strlen(buf);
        }
    }
    if (bufMax > 0) buf[0] = '\0';
    return 0;
}

UINT GetPrivateProfileIntA(const char *section, const char *key,
                           int iDefault, const char *file)
{
    (void)section; (void)file;
    for (unsigned i = 0; i < N_PROFILE; i++)
        if (gProfile[i].present && !strcasecmp(gProfile[i].key, key))
            return (UINT)gProfile[i].val;
    return (UINT)iDefault;
}

DWORD GetPrivateProfileStringA(const char *section, const char *key,
                               const char *lpDefault, char *dest,
                               DWORD size, const char *file)
{
    (void)section; (void)file;
    const char *src = lpDefault ? lpDefault : "";
    if (!strcasecmp(key, "Texture"))
        src = gTexturePath;
    strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    return (DWORD)strlen(dest);
}

BOOL WritePrivateProfileStringA(const char *section, const char *key,
                                const char *val, const char *file)
{
    (void)section; (void)key; (void)val; (void)file;
    return TRUE;
}

/* ---- resource API: serves the embedded STRIPE.BMP -----------------
 * TEXTURE.C's ss_LoadTextureResource does FindResource(name=IDB_DEFTEX=99,
 * type=RT_MYBMP=100) -> LoadResource -> LockResource and parses the raw
 * .bmp bytes from memory.  We serve the verbatim STRIPE.BMP payload. */
#include "stripe_bmp.h"

#define RES_STRIPE ((HRSRC)0x5717)

HMODULE GetModuleHandleA(const char *name) { (void)name; return (HMODULE)1; }

HRSRC FindResourceA(HMODULE mod, const char *name, const char *type)
{
    (void)mod;
    if ((ULONG_PTR)name == 99 && (ULONG_PTR)type == 100)
        return RES_STRIPE;
    return (HRSRC)0;
}

HGLOBAL LoadResource(HMODULE mod, HRSRC res)
{
    (void)mod;
    return (HGLOBAL)res;
}

PVOID LockResource(HGLOBAL h)
{
    if (h == (HGLOBAL)RES_STRIPE)
        return (PVOID)g_stripe_bmp;
    return NULL;
}

BOOL FreeResource(HGLOBAL h) { (void)h; return TRUE; }

DWORD SizeofResource(HMODULE mod, HRSRC res)
{
    (void)mod;
    return (res == RES_STRIPE) ? (DWORD)sizeof(g_stripe_bmp) : 0;
}

DWORD SearchPathA(const char *path, const char *file, const char *ext,
                  DWORD buflen, char *buf, char **filepart)
{
    /* User texture files are looked up in the (MEMFS) filesystem. */
    (void)path; (void)ext; (void)filepart;
    if (!file || !file[0])
        return 0;
    FILE *f = fopen(file, "rb");
    if (!f)
        return 0;
    fclose(f);
    strncpy(buf, file, buflen - 1);
    buf[buflen - 1] = '\0';
    return (DWORD)strlen(buf);
}

/* ---- GL version / extension queries (replaces SSUTIL.CXX) ----------
 * The emulated context supports GL 1.1 texture objects (glGenTextures/
 * glBindTexture are native WebGL), matching NT4's OpenGL 1.1. */
extern BOOL gbTextureObjects;   /* defined in COMMON/TEXTURE.C */
static BOOL gbGLv1_1 = TRUE;

void ss_QueryGLVersion(void)
{
    gbGLv1_1 = TRUE;
    gbTextureObjects = TRUE;
}

BOOL ss_fOnGL11(void) { return gbGLv1_1; }

/* ---- image loaders (replace GDI-based SSDIB.C / SSIMAGE.C / SSA8.C) --
 * ss_DIBImageLoad parses a raw .bmp file image from memory into the
 * packed bottom-up GL_RGB layout the original produced (SSDIB.C via GDI:
 * 24bpp DIB section, then R/B swap + row unpadding).  Supports the
 * formats GDI handled that matter here: 24bpp BI_RGB and 8/4/1bpp
 * palettized BI_RGB. */

typedef struct {
    int width; int height;
    GLenum format; GLsizei components;
    float origAspectRatio;
    unsigned char *data;
    GLuint texObj;
    int pal_size; int iPalRot;
    RGBQUAD *pal;
} TEXTURE_SH;   /* layout mirror of TEXTURE in SSCOMMON.H */

BOOL ss_DIBImageLoad(PVOID pvFile, TEXTURE_SH *ptex)
{
    const unsigned char *p = (const unsigned char *)pvFile;
    if (!p) return FALSE;

    const BITMAPFILEHEADER *bf = (const BITMAPFILEHEADER *)p;
    const unsigned char *bits = NULL;
    const BITMAPINFOHEADER *bi;

    if (bf->bfType == 0x4D42) {          /* 'BM' */
        bi = (const BITMAPINFOHEADER *)(p + sizeof(BITMAPFILEHEADER));
        bits = p + bf->bfOffBits;
    } else {
        bi = (const BITMAPINFOHEADER *)p;
    }
    if (bi->biSize < sizeof(BITMAPINFOHEADER) || bi->biCompression != BI_RGB)
        return FALSE;

    int w = (int)bi->biWidth;
    int h = (int)bi->biHeight;
    int bpp = bi->biBitCount;
    if (w <= 0 || h <= 0)
        return FALSE;

    int palCount = 0;
    const RGBQUAD *pal = (const RGBQUAD *)((const unsigned char *)bi + bi->biSize);
    if (bpp <= 8) {
        palCount = bi->biClrUsed ? (int)bi->biClrUsed : (1 << bpp);
        if (!bits)
            bits = (const unsigned char *)(pal + palCount);
    } else if (!bits) {
        bits = (const unsigned char *)pal;
    }

    int srcStride = ((w * bpp + 31) / 32) * 4;
    unsigned char *out = (unsigned char *)malloc((size_t)w * h * 3);
    if (!out) return FALSE;

    for (int y = 0; y < h; y++) {                 /* keep bottom-up order */
        const unsigned char *src = bits + (size_t)y * srcStride;
        unsigned char *dst = out + (size_t)y * w * 3;
        for (int x = 0; x < w; x++) {
            unsigned char r, g, b;
            if (bpp == 24) {
                b = src[x * 3]; g = src[x * 3 + 1]; r = src[x * 3 + 2];
            } else if (bpp == 32) {
                b = src[x * 4]; g = src[x * 4 + 1]; r = src[x * 4 + 2];
            } else {
                int idx;
                if (bpp == 8)       idx = src[x];
                else if (bpp == 4)  idx = (src[x / 2] >> ((x & 1) ? 0 : 4)) & 0xF;
                else                idx = (src[x / 8] >> (7 - (x & 7))) & 1;
                if (idx >= palCount) idx = 0;
                r = pal[idx].rgbRed; g = pal[idx].rgbGreen; b = pal[idx].rgbBlue;
            }
            dst[x * 3] = r; dst[x * 3 + 1] = g; dst[x * 3 + 2] = b;
        }
    }

    ptex->width = w;
    ptex->height = h;
    ptex->format = 0x1907;      /* GL_RGB */
    ptex->components = 3;
    ptex->data = out;
    ptex->pal_size = 0;
    ptex->pal = NULL;
    return TRUE;
}

BOOL ss_RGBImageLoad(PVOID pvFile, TEXTURE_SH *ptex)
{
    /* SGI .rgb textures: only reachable via user-supplied files. */
    (void)pvFile; (void)ptex;
    return FALSE;
}

BOOL ss_A8ImageLoad(PVOID pvFile, TEXTURE_SH *ptex)
{
    (void)pvFile; (void)ptex;
    return FALSE;   /* .a8 resources are not used by pipes */
}

/* verify helpers referenced by TEXTURE.C (originals live in the GDI
 * files SSDIB.C / SSIMAGE.C; signatures match SSDIB.C:405/SSIMAGE.C:213) */
BOOL bVerifyDIB(char *fname, ISIZE *pSize)
{
    FILE *f = fopen(fname, "rb");
    if (!f) return FALSE;
    unsigned char hdr[54];
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    if (n < 54 || hdr[0] != 'B' || hdr[1] != 'M')
        return FALSE;
    pSize->width  = (int)(hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24));
    pSize->height = (int)(hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24));
    return TRUE;
}

BOOL bVerifyRGB(char *fname, ISIZE *pSize)
{
    (void)fname; (void)pSize;
    return FALSE;
}

/* GLAUX image loaders (TEXTURE.C file path for user textures) */
typedef struct { GLint sizeX, sizeY; unsigned char *data; } AUX_RGBImageRec_SH;

AUX_RGBImageRec_SH *auxDIBImageLoad(const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc(size > 0 ? (size_t)size : 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f); free(buf); return NULL;
    }
    fclose(f);
    TEXTURE_SH t;
    memset(&t, 0, sizeof t);
    if (!ss_DIBImageLoad(buf, &t)) {
        free(buf);
        return NULL;
    }
    free(buf);
    AUX_RGBImageRec_SH *rec = (AUX_RGBImageRec_SH *)malloc(sizeof(*rec));
    rec->sizeX = t.width;
    rec->sizeY = t.height;
    rec->data = t.data;
    return rec;
}

AUX_RGBImageRec_SH *auxRGBImageLoad(const char *file)
{
    (void)file;
    return NULL;
}

/* ---- misc Win32 shims ---- */
DWORD GetTickCount(void) { return (DWORD)emscripten_get_now(); }

void OutputDebugStringA(const char *s) { fputs(s, stderr); }

int MessageBoxA(HWND h, const char *text, const char *caption, UINT type)
{
    (void)h; (void)type;
    fprintf(stderr, "[MessageBox] %s: %s\n", caption ? caption : "", text ? text : "");
    return 1;
}

BOOL GdiFlush(void) { glFlush(); return TRUE; }

/* Debug aid: dump current matrices (callable from JS) */
extern void glGetFloatv(GLenum pname, GLfloat *out);
EMSCRIPTEN_KEEPALIVE
void pipes_debug_matrices(void)
{
    GLfloat mv[16], pr[16];
    glGetFloatv(0x0BA6 /* GL_MODELVIEW_MATRIX */, mv);
    glGetFloatv(0x0BA7 /* GL_PROJECTION_MATRIX */, pr);
    fprintf(stderr, "MV : ");
    for (int i = 0; i < 16; i++) fprintf(stderr, "%.3f ", mv[i]);
    fprintf(stderr, "\nPRJ: ");
    for (int i = 0; i < 16; i++) fprintf(stderr, "%.3f ", pr[i]);
    fprintf(stderr, "\n");
}

/* ---- main loop ---- */

static void tick(void)
{
    /* SSWPROC.CXX WM_TIMER -> ss_TimerProc -> SSW::Update ->
     * (single-buffer, non-floater path) (*UpdateFunc)(DataPtr)   */
    if (gUpdateFunc)
        (*gUpdateFunc)(gDataPtr);
}

EMSCRIPTEN_KEEPALIVE
void pipes_resize(int width, int height)
{
    gScreenSize.width = width;
    gScreenSize.height = height;
    if (gReshapeFunc)
        (*gReshapeFunc)(width, height, gDataPtr);
}

EM_JS(int, create_gl_context, (void), {
    var canvas = Module['canvas'] || document.getElementById('canvas');
    var attrs = {
        alpha: false, depth: true, stencil: false, antialias: false,
        preserveDrawingBuffer: true, majorVersion: 1, minorVersion: 0
    };
    var ctx = Browser.createContext(canvas, true, true, attrs);
    return ctx ? 1 : 0;
});

/* Read window.PIPES_CONFIG (set by index.html before startup) into the
 * settings store; returns the RNG seed to use, or -1 for "original
 * behavior" (seed from millisecond field of current time, as
 * ss_RandInit/UTIL.CXX:167 does). */
EM_JS(int, apply_js_config, (void), {
    var cfg = (typeof window !== 'undefined' && window.PIPES_CONFIG) || {};
    var names = ['JointType','SurfStyle','TextureQuality','Tesselation',
                 'Flex','MultiPipes','TextureCount'];
    for (var i = 0; i < names.length; i++) {
        if (typeof cfg[names[i]] === 'number') {
            var n = stringToNewUTF8(names[i]);
            _pipes_set_setting(n, cfg[names[i]] | 0);
            _free(n);
        }
    }
    if (typeof cfg.Texture === 'string') {
        var t = stringToNewUTF8(cfg.Texture);
        _pipes_set_texture_path(t);
        _free(t);
    }
    return (typeof cfg.Seed === 'number') ? (cfg.Seed >>> 0) & 0x7fffffff : -1;
});

int main(void)
{
    /* Canvas size = CSS size (index.html keeps it fullscreen) */
    double cw, ch;
    emscripten_get_element_css_size("#canvas", &cw, &ch);
    if (cw < 1) cw = 640;
    if (ch < 1) ch = 480;
    emscripten_set_canvas_element_size("#canvas", (int)cw, (int)ch);
    gScreenSize.width  = (int)cw;
    gScreenSize.height = (int)ch;

    int seed = apply_js_config();

    /* --- SCRNSAVE::Init order (GLSCRNSV.CXX:143-161) --- */
    ss_RandInit();                    /* original: srand(millitm) */
    if (seed >= 0)
        msvc_srand((unsigned)seed);   /* reproducible override */

    SSContext *pssc = ss_Init();      /* original client init */
    (void)pssc; /* bDoubleBuf=FALSE, SS_DEPTH16 — matched by context attrs */

    /* --- GL context (SSW::hrcSetupGL equivalent) ---
     * Single-buffered pixel format: the original draws incrementally into
     * the front buffer and never clears per frame, so the canvas must
     * preserve the drawing buffer between frames.
     * Created through Browser.createContext because that is the only path
     * that initializes the legacy-GL emulation (GLEmulation/GLImmediate
     * hook moduleContextCreatedCallbacks). */
    if (!create_gl_context()) {
        fprintf(stderr, "sspipes: WebGL context creation failed\n");
        return 1;
    }

    /* SSW::hrcSetupGL queries GL capabilities right after context
     * creation (SSWINDOW.CXX:600-603): GL version (enables the GL 1.1
     * texture-object path in TEXTURE.C) and the paletted-texture
     * extension (absent here; wglGetProcAddress returns NULL). */
    ss_QueryGLVersion();
    ss_QueryPalettedTextureEXT();

    /* --- SSW::InitGL order (SSWINDOW.CXX:666-685) --- */
    if (gInitFunc)
        (*gInitFunc)(gDataPtr);

    if (gReshapeFunc)
        (*gReshapeFunc)(gScreenSize.width, gScreenSize.height, gDataPtr);

    /* --- animation timer: SetTimer(hwnd, 1, 16, 0)  [SSWPROC.CXX:58,107] ---
     * EM_TIMING_SETTIMEOUT with 16ms reproduces the WM_TIMER cadence
     * (release build capped at ~60 fps). */
    emscripten_set_main_loop(tick, 0, 0);
    emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 16);

    return 0;
}
