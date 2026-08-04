/*
 * glwrap.h — force-included (-include) when compiling ORIGINAL sources
 * (PIPES/*, the needed COMMON/* files, and GLAUX TEAPOT.C).
 *
 * Renames the GL calls that need port-side handling to the pipes_gl*
 * implementations in port/gl11compat.c (display-list recording, evaluator
 * enables, virtual-context state shadowing, GL_QUAD_STRIP translation,
 * GL_BACK material dropping).  The macros rewrite the declarations in
 * <GL/gl.h> too, so the pipes_gl* symbols get correct prototypes for
 * free.  Functions not listed here go straight to emscripten's GL.
 *
 * The display-list and evaluator entry points (glNewList, glCallList,
 * glMap2f, glEvalMesh2, ...) are NOT renamed: emscripten's emulation
 * doesn't define them at all, so port/gl11compat.c owns those names
 * directly.
 */
#ifndef _PORT_GLWRAP_H
#define _PORT_GLWRAP_H

/* MSVC CRT randomness semantics.  The originals are also compiled with
 * -Drand=msvc_rand -Dsrand=msvc_srand (MSVC's LCG, port/msrand.c), and
 * RAND_MAX must be MSVC's 0x7fff: musl's stdlib.h says 0x7fffffff, which
 * would rescale every probability in ss_iRand/ss_iRand2/ss_fRand
 * (COMMON/UTIL.CXX) to ~zero — no ball/elbow mix, no teapot odds, no
 * flex variation.  stdlib.h is pulled in here first so its definition
 * can be overridden once, before any original code sees it. */
#include <stdlib.h>
#undef RAND_MAX
#define RAND_MAX 0x7fff

#define glBegin       pipes_glBegin
#define glEnd         pipes_glEnd
#define glVertex3f    pipes_glVertex3f
#define glVertex3fv   pipes_glVertex3fv
#define glNormal3f    pipes_glNormal3f
#define glNormal3fv   pipes_glNormal3fv
#define glTexCoord2f  pipes_glTexCoord2f
#define glPushMatrix  pipes_glPushMatrix
#define glPopMatrix   pipes_glPopMatrix
#define glRotatef     pipes_glRotatef
#define glScalef      pipes_glScalef
#define glTranslatef  pipes_glTranslatef
#define glEnable      pipes_glEnable
#define glDisable     pipes_glDisable
#define glFrontFace   pipes_glFrontFace
#define glBindTexture pipes_glBindTexture
#define glMaterialfv  pipes_glMaterialfv
#define glMaterialf   pipes_glMaterialf

#endif
