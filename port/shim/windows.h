/*
 * Minimal <windows.h> replacement for compiling the original NT4 SDK
 * 3D Pipes source with emscripten.  Provides only the types, macros and
 * functions the original translation units actually use; no Win32
 * behavior is emulated beyond what the screensaver needs.
 */
#ifndef _SHIM_WINDOWS_H
#define _SHIM_WINDOWS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/timeb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- calling-convention / annotation macros ---- */
#define CALLBACK
#define WINAPI
#define APIENTRY
#define APIPRIVATE
#define PASCAL
#define CDECL
#define __cdecl
#define __stdcall
#define FAR
#define NEAR
#define CONST const
#define VOID void

/* ---- integral types ---- */
typedef int             BOOL, *PBOOL, *LPBOOL;
typedef unsigned char   BYTE, *PBYTE, *LPBYTE, UCHAR;
typedef unsigned short  WORD, *PWORD, *LPWORD, USHORT;
typedef unsigned int    UINT, *PUINT;
typedef int             INT, *PINT, *LPINT;
typedef long            LONG, *PLONG, *LPLONG;
typedef unsigned long   ULONG, *PULONG;
typedef unsigned int    DWORD, *PDWORD, *LPDWORD;  /* 32-bit on win32 */
typedef float           FLOAT, *PFLOAT;
typedef double          DOUBLE;
typedef short           SHORT;
typedef char            CHAR;
typedef void            *PVOID, *LPVOID;
typedef const void      *LPCVOID;
typedef intptr_t        INT_PTR, LONG_PTR;
typedef uintptr_t       UINT_PTR, ULONG_PTR, DWORD_PTR;
typedef UINT_PTR        WPARAM;
typedef LONG_PTR        LPARAM;
typedef LONG_PTR        LRESULT;

/* ---- character types (ANSI build; the original was not compiled -DUNICODE) ---- */
typedef char            TCHAR, *PTCHAR, *PCH, *LPCH, *PSZ;
typedef char           *PSTR, *LPSTR, *LPTSTR, *PTSTR;
typedef const char     *PCSTR, *LPCSTR, *LPCTSTR, *PCTSTR;
typedef unsigned short  WCHAR, *PWCHAR, *LPWSTR;
typedef const WCHAR    *LPCWSTR;
#define TEXT(s) s
#define _T(s)   s

/* ---- handle types: opaque pointers, never dereferenced by ported code ---- */
typedef void *HANDLE, *HWND, *HDC, *HINSTANCE, *HMODULE, *HBITMAP, *HPALETTE,
    *HGDIOBJ, *HICON, *HCURSOR, *HBRUSH, *HMENU, *HRSRC, *HGLOBAL, *HGLRC,
    *HKEY, *HFONT, *HACCEL, *HPEN;
typedef HKEY *PHKEY;
typedef void *HGLRC;

#define DECLARE_HANDLE(name) typedef void *name

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

/* Referenced by COMMON headers (palette.hxx); never used at runtime. */
typedef struct tagPIXELFORMATDESCRIPTOR {
    WORD  nSize, nVersion;
    DWORD dwFlags;
    BYTE  iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits,
          cGreenShift, cBlueBits, cBlueShift, cAlphaBits, cAlphaShift,
          cAccumBits, cAccumRedBits, cAccumGreenBits, cAccumBlueBits,
          cAccumAlphaBits, cDepthBits, cStencilBits, cAuxBuffers,
          iLayerType, bReserved;
    DWORD dwLayerMask, dwVisibleMask, dwDamageMask;
} PIXELFORMATDESCRIPTOR, *PPIXELFORMATDESCRIPTOR, *LPPIXELFORMATDESCRIPTOR;

/* ---- basic structs ---- */
typedef struct tagPOINT { LONG x, y; } POINT, *PPOINT, *LPPOINT;
typedef struct tagSIZE  { LONG cx, cy; } SIZE, *PSIZE, *LPSIZE;
typedef struct tagRECT  { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT;
typedef const RECT *LPCRECT;

typedef struct tagRGBQUAD {
    BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved;
} RGBQUAD;

typedef struct tagRGBTRIPLE {
    BYTE rgbtBlue, rgbtGreen, rgbtRed;
} RGBTRIPLE;

/* DIB structures: TEXTURE.C parses .BMP files from memory */
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
    WORD  bfType;
    DWORD bfSize;
    WORD  bfReserved1, bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER, *LPBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth, biHeight;
    WORD  biPlanes, biBitCount;
    DWORD biCompression, biSizeImage;
    LONG  biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct tagBITMAPCOREHEADER {
    DWORD bcSize;
    WORD  bcWidth, bcHeight, bcPlanes, bcBitCount;
} BITMAPCOREHEADER, *PBITMAPCOREHEADER;

typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, *PBITMAPINFO, *LPBITMAPINFO;

#define BI_RGB  0
#define BI_RLE8 1
#define BI_RLE4 2

/* ---- misc constants ---- */
#define TRUE  1
#define FALSE 0
#ifndef NULL
#define NULL  0
#endif
#define MAX_PATH 260
#define WM_USER 0x0400

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

extern UINT GetWindowsDirectoryA(LPSTR buf, UINT size);
#define GetWindowsDirectory GetWindowsDirectoryA

/* Registry stubs (only the dead wallpaper-lookup path uses these) */
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)0x80000002u)
#define HKEY_CURRENT_USER  ((HKEY)(ULONG_PTR)0x80000001u)
#define KEY_QUERY_VALUE 1
#define KEY_READ        1
#define ERROR_SUCCESS   0
#define REG_SZ          1
static __inline LONG RegOpenKeyExA(HKEY k, LPCSTR sub, DWORD opt, DWORD sam, PHKEY out)
    { (void)k; (void)sub; (void)opt; (void)sam; (void)out; return 1; }
#define RegOpenKeyEx RegOpenKeyExA
static __inline LONG RegQueryValueExA(HKEY k, LPCSTR name, LPDWORD rsvd, LPDWORD type, LPBYTE data, LPDWORD len)
    { (void)k; (void)name; (void)rsvd; (void)type; (void)data; (void)len; return 1; }
#define RegQueryValueEx RegQueryValueExA
static __inline LONG RegCloseKey(HKEY k) { (void)k; return 0; }

#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))

/* ---- CRT-style aliases used by the original code ---- */
#define _timeb  timeb
#define _ftime  ftime
#define _fmemcpy memcpy

#define lstrlen  strlen
#define lstrcpy  strcpy
#define lstrcat  strcat
#define lstrcmp  strcmp
#define lstrcmpi strcasecmp
#define wsprintf sprintf

/* ---- functions implemented in port/ssshell.c ---- */
extern DWORD GetTickCount(void);
extern void  OutputDebugStringA(const char *s);
#define OutputDebugString OutputDebugStringA
extern int   MessageBoxA(HWND h, const char *text, const char *caption, UINT type);
#define MessageBox MessageBoxA
#define MB_OK 0

/* GdiFlush: the original calls this after front-buffer draws; glFlush is
 * the equivalent for a GL-only pipeline. */
extern BOOL GdiFlush(void);

/* Profile ("registry") access — implemented over a JS-settable store in
 * port/ssshell.c with the same key names the original used. */
extern int   LoadStringA(HINSTANCE h, UINT id, char *buf, int bufMax);
#define LoadString LoadStringA
extern UINT  GetPrivateProfileIntA(const char *sect, const char *key,
                                   int def, const char *file);
#define GetPrivateProfileInt GetPrivateProfileIntA
extern DWORD GetPrivateProfileStringA(const char *sect, const char *key,
                                      const char *def, char *dest,
                                      DWORD size, const char *file);
#define GetPrivateProfileString GetPrivateProfileStringA
extern BOOL  WritePrivateProfileStringA(const char *sect, const char *key,
                                        const char *val, const char *file);
#define WritePrivateProfileString WritePrivateProfileStringA

/* Dialog/control stubs: the config dialog is never shown in the web port,
 * but PIPES/DIALOG.C must compile unmodified.  All are inert. */
#define WM_INITDIALOG 0x0110
#define WM_COMMAND    0x0111
#define WM_DESTROY    0x0002
#define WM_CLOSE      0x0010
#define WM_PAINT      0x000F
#define WM_SETTEXT    0x000C
#define IDOK          1
#define IDCANCEL      2
#define CB_ADDSTRING  0x0143
#define CB_GETCURSEL  0x0147
#define CB_SETCURSEL  0x014E
#define CBN_SELCHANGE 1
#define CBN_EDITCHANGE 5
static __inline LRESULT SendDlgItemMessageA(HWND h, int id, UINT m, WPARAM w, LPARAM l)
    { (void)h; (void)id; (void)m; (void)w; (void)l; return 0; }
#define SendDlgItemMessage SendDlgItemMessageA
static __inline LRESULT SendMessageA(HWND h, UINT m, WPARAM w, LPARAM l)
    { (void)h; (void)m; (void)w; (void)l; return 0; }
#define SendMessage SendMessageA
static __inline BOOL CheckDlgButton(HWND h, int id, UINT check)
    { (void)h; (void)id; (void)check; return TRUE; }
static __inline HWND GetDlgItem(HWND h, int id)
    { (void)h; (void)id; return (HWND)0; }
static __inline BOOL EnableWindow(HWND h, BOOL b)
    { (void)h; (void)b; return TRUE; }
static __inline BOOL EndDialog(HWND h, INT_PTR r)
    { (void)h; (void)r; return TRUE; }

/* WGL: implemented in port/gl11compat.c as virtual contexts over the
 * single WebGL context (the original gives each pipe draw-thread its own
 * GL rendering context with shared display lists). */
typedef int (*PROC)(void);
extern HGLRC wglCreateContext(HDC hdc);
extern BOOL  wglDeleteContext(HGLRC hrc);
extern BOOL  wglMakeCurrent(HDC hdc, HGLRC hrc);
extern HGLRC wglGetCurrentContext(void);
extern HDC   wglGetCurrentDC(void);
extern BOOL  wglShareLists(HGLRC h1, HGLRC h2);
extern PROC  wglGetProcAddress(LPCSTR name);

/* Rtl/Win32 memory macros */
#define RtlCopyMemory(d, s, n)  memcpy((d), (s), (n))
#define RtlMoveMemory(d, s, n)  memmove((d), (s), (n))
#define RtlZeroMemory(d, n)     memset((d), 0, (n))
#define RtlFillMemory(d, n, v)  memset((d), (v), (n))
#define CopyMemory RtlCopyMemory
#define MoveMemory RtlMoveMemory
#define ZeroMemory RtlZeroMemory
#define FillMemory RtlFillMemory

/* Resource API — implemented in port/ssshell.c over embedded data
 * (the original loads STRIPE.BMP from the .scr's resource section). */
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEA
extern HMODULE GetModuleHandleA(LPCSTR name);
#define GetModuleHandle GetModuleHandleA
extern HRSRC   FindResourceA(HMODULE mod, LPCSTR name, LPCSTR type);
#define FindResource FindResourceA
extern HGLOBAL LoadResource(HMODULE mod, HRSRC res);
extern PVOID   LockResource(HGLOBAL h);
extern BOOL    FreeResource(HGLOBAL h);
extern DWORD   SizeofResource(HMODULE mod, HRSRC res);
extern DWORD   SearchPathA(LPCSTR path, LPCSTR file, LPCSTR ext,
                           DWORD buflen, LPSTR buf, LPSTR *filepart);
#define SearchPath SearchPathA

/* GDI stubs (dead code paths: GDI dissolve clear, dialog drawing) */
static __inline int FillRect(HDC hdc, const RECT *r, HBRUSH br)
    { (void)hdc; (void)r; (void)br; return 1; }
static __inline HBRUSH CreateSolidBrush(DWORD color)
    { (void)color; return (HBRUSH)0; }
static __inline BOOL DeleteObject(HGDIOBJ o) { (void)o; return TRUE; }
static __inline HGDIOBJ GetStockObject(int i) { (void)i; return (HGDIOBJ)0; }
static __inline HDC GetDC(HWND w) { (void)w; return (HDC)0; }
static __inline int ReleaseDC(HWND w, HDC dc) { (void)w; (void)dc; return 1; }
#define BLACK_BRUSH 4
#define RGB(r,g,b) ((DWORD)(((BYTE)(r))|(((WORD)((BYTE)(g)))<<8)|(((DWORD)((BYTE)(b)))<<16)))

/* Memory helpers (used by texture loading paths) */
#define GlobalAlloc(flags, size) malloc(size)
#define GlobalFree(p)            free(p)
#define LocalAlloc(flags, size)  malloc(size)
#define LocalFree(p)             free(p)
#define GMEM_FIXED 0
#define LMEM_FIXED 0
#define LPTR 0

#ifdef __cplusplus
}
#endif

#endif /* _SHIM_WINDOWS_H */
