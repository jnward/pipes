/*
 * Stub for <scrnsave.h> (Win32 screensaver framework header from
 * scrnsave.lib).  The message-pump framework is replaced by
 * port/ssshell.c; this header only satisfies compiles of original files
 * that include it for the globals below.
 */
#ifndef _SHIM_SCRNSAVE_H
#define _SHIM_SCRNSAVE_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

extern HINSTANCE hMainInstance;
extern HWND      hMainWindow;
extern BOOL      fChildPreview;
extern char      szName[64];
extern char      szAppName[64];
extern char      szIniFile[64];
extern char      szScreenSaver[64];

#define WS_GT 0

#ifdef __cplusplus
}
#endif

#endif
