/* Stub <commdlg.h>: the texture file-picker dialog code compiles but is
 * never invoked in the web port (no config dialog). */
#ifndef _SHIM_COMMDLG_H
#define _SHIM_COMMDLG_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UINT_PTR (*LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagOFNA {
    DWORD  lStructSize;
    HWND   hwndOwner;
    HINSTANCE hInstance;
    LPCSTR lpstrFilter;
    LPSTR  lpstrCustomFilter;
    DWORD  nMaxCustFilter;
    DWORD  nFilterIndex;
    LPSTR  lpstrFile;
    DWORD  nMaxFile;
    LPSTR  lpstrFileTitle;
    DWORD  nMaxFileTitle;
    LPCSTR lpstrInitialDir;
    LPCSTR lpstrTitle;
    DWORD  Flags;
    WORD   nFileOffset;
    WORD   nFileExtension;
    LPCSTR lpstrDefExt;
    LPARAM lCustData;
    LPOFNHOOKPROC lpfnHook;
    LPCSTR lpTemplateName;
} OPENFILENAMEA, OPENFILENAME, *LPOPENFILENAMEA, *LPOPENFILENAME;

#define OFN_FILEMUSTEXIST   0x00001000
#define OFN_PATHMUSTEXIST   0x00000800
#define OFN_HIDEREADONLY    0x00000004
#define OFN_NOCHANGEDIR     0x00000008

typedef UINT_PTR (*LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);

static __inline BOOL GetOpenFileNameA(OPENFILENAMEA *ofn)
    { (void)ofn; return FALSE; }
#define GetOpenFileName GetOpenFileNameA

#ifdef __cplusplus
}
#endif

#endif
