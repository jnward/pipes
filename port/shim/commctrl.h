#ifndef _SHIM_COMMCTRL_H
#define _SHIM_COMMCTRL_H
#include <windows.h>
#define TBM_GETPOS (WM_USER)
#define TBM_SETRANGE (WM_USER+6)
#define TBM_SETPOS (WM_USER+5)
#define TBM_SETLINESIZE (WM_USER+23)
#define TBM_SETPAGESIZE (WM_USER+21)
static __inline void InitCommonControls(void) {}
#endif
