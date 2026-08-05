/*
 * Shim for <GL/glaux.h> (Microsoft's OpenGL auxiliary library header).
 * Declares only what this codebase uses:
 *   - auxSolidTeapot / auxWireTeapot: provided by compiling the ORIGINAL
 *     GLAUX source (original/MSTOOLS/SAMPLES/OPENGL/GLAUX/TEAPOT.C) on top
 *     of the GL-1.1 compatibility layer in port/gl11compat.c.
 *   - AUX_RGBImageRec + auxDIBImageLoad/auxRGBImageLoad: image loaders
 *     used by COMMON/TEXTURE.C; implemented in port/ssshell.c (the
 *     original TK loaders are Win32 GDI code).
 */
#ifndef _SHIM_GLAUX_H
#define _SHIM_GLAUX_H

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GL 1.1 / extension tokens missing from emscripten's <GL/gl.h> that the
 * original code expects from Microsoft's headers */
#ifndef GL_PROXY_TEXTURE_2D
#define GL_PROXY_TEXTURE_2D       0x8064
#endif
#ifndef GL_COLOR_TABLE_WIDTH_EXT
#define GL_COLOR_TABLE_WIDTH_EXT  0x80D9
#endif
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT               0x80E1
#endif

/* MSVC's C compiler allowed the implicit declaration TEXTURE.C relies on */
int gluScaleImage(GLenum format,
                  GLsizei wIn, GLsizei hIn, GLenum typeIn, const void *dataIn,
                  GLsizei wOut, GLsizei hOut, GLenum typeOut, void *dataOut);

typedef struct _AUX_RGBImageRec {
    GLint sizeX, sizeY;
    unsigned char *data;
} AUX_RGBImageRec;

void auxSolidTeapot(GLdouble scale);
void auxWireTeapot(GLdouble scale);

AUX_RGBImageRec *auxDIBImageLoad(const char *file);
AUX_RGBImageRec *auxRGBImageLoad(const char *file);

#ifdef __cplusplus
}
#endif

#endif
