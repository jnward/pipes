/*
 * gl11compat.c — the GL 1.1 features the original code needs that
 * emscripten's -sLEGACY_GL_EMULATION does not provide:
 *
 *   1. Display lists  (glGenLists/glNewList/glEndList/glCallList/
 *      glDeleteLists/glIsList) — OBJECTS.CXX compiles all normal-pipe
 *      geometry into lists; GLAUX TEAPOT.C compiles the teapot into one.
 *
 *   2. 2D evaluators  (glMap2f/glMapGrid2f/glEvalMesh2 + the
 *      GL_MAP2_VERTEX_3 / GL_MAP2_TEXTURE_COORD_2 / GL_AUTO_NORMAL
 *      enables) — EVAL.CXX draws every flex-pipe surface through these,
 *      and TEAPOT.C uses them for the teapot patches.  Implemented per
 *      the OpenGL 1.1 spec (Bernstein evaluation; AUTO_NORMAL =
 *      normalize(dP/du x dP/dv); EvalMesh2 GL_FILL emits quad strips).
 *
 *   3. GL_QUAD_STRIP / GL_POLYGON begin modes — the emulation aborts on
 *      modes above GL_QUADS, so glBegin translates QUAD_STRIP ->
 *      TRIANGLE_STRIP and POLYGON -> TRIANGLE_FAN (identical vertex
 *      ordering semantics; only the internal quad diagonal differs).
 *
 *   4. Virtual WGL contexts — STATE.CXX gives each pipe draw-thread its
 *      own wglCreateContext (sharing display lists via wglShareLists)
 *      and relies on per-context persistence of the modelview matrix
 *      (the pipe tip transform accumulates across ticks), the current
 *      material, texture binding, front-face and evaluator enables.
 *      Over the single WebGL context we shadow exactly that state per
 *      virtual context and swap it in wglMakeCurrent.
 *
 *   5. glMaterial* GL_BACK calls are dropped (the emulation supports
 *      GL_FRONT/GL_FRONT_AND_BACK only; pipes culls back faces, so back
 *      materials are never visible).
 *
 * Original sources are compiled with -include port/glwrap.h, which
 * renames their calls to the pipes_gl* entry points below; the real gl*
 * names stay bound to the JS emulation and are called directly here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <emscripten/emscripten.h>

/* Declarations for the JS legacy-GL emulation functions we call.
 * These are unshadowed: nothing native defines them, so they resolve to
 * emscripten's -sLEGACY_GL_EMULATION implementations at link time.
 * <GL/gl.h> in emscripten already declares most of them; the externs
 * below cover any that its header set misses. */
extern void glBegin(GLenum mode);
extern void glEnd(void);
extern void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
extern void glNormal3f(GLfloat x, GLfloat y, GLfloat z);
extern void glTexCoord2f(GLfloat s, GLfloat t);
extern void glPushMatrix(void);
extern void glPopMatrix(void);
extern void glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z);
extern void glScalef(GLfloat x, GLfloat y, GLfloat z);
extern void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
extern void glEnable(GLenum cap);
extern void glDisable(GLenum cap);
extern void glMaterialfv(GLenum face, GLenum pname, const GLfloat *p);
/* the emulation has no scalar glMaterialf; route through glMaterialfv */
static void emuMaterialf(GLenum face, GLenum pname, GLfloat v)
{
    GLfloat tmp[4] = { v, 0.0f, 0.0f, 0.0f };
    glMaterialfv(face, pname, tmp);
}
extern void glBindTexture(GLenum target, GLuint tex);
extern void glFrontFace(GLenum dir);
extern void glMatrixMode(GLenum mode);
extern void glLoadMatrixf(const GLfloat *m);
extern void glGetFloatv(GLenum pname, GLfloat *out);

/* GL tokens the emscripten <GL/gl.h> may not define */
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP 0x0008
#endif
#ifndef GL_POLYGON
#define GL_POLYGON 0x0009
#endif
#ifndef GL_MAP2_TEXTURE_COORD_2
#define GL_MAP2_TEXTURE_COORD_2 0x0DB3
#endif
#ifndef GL_MAP2_VERTEX_3
#define GL_MAP2_VERTEX_3 0x0DB7
#endif
#ifndef GL_AUTO_NORMAL
#define GL_AUTO_NORMAL 0x0D80
#endif
#ifndef GL_NORMALIZE
#define GL_NORMALIZE 0x0BA1
#endif
#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX 0x0BA6
#endif
#ifndef GL_MODELVIEW
#define GL_MODELVIEW 0x1700
#endif
#ifndef GL_FILL
#define GL_FILL 0x1B02
#endif
#ifndef GL_LINE
#define GL_LINE 0x1B01
#endif
#ifndef GL_FRONT_AND_BACK
#define GL_FRONT_AND_BACK 0x0408
#endif
#ifndef GL_BACK
#define GL_BACK 0x0405
#endif
#ifndef GL_FRONT
#define GL_FRONT 0x0404
#endif
#ifndef GL_CCW
#define GL_CCW 0x0901
#endif
#ifndef GL_AMBIENT
#define GL_AMBIENT 0x1200
#endif
#ifndef GL_DIFFUSE
#define GL_DIFFUSE 0x1201
#endif
#ifndef GL_SPECULAR
#define GL_SPECULAR 0x1202
#endif
#ifndef GL_SHININESS
#define GL_SHININESS 0x1601
#endif
#ifndef GL_TEXTURE_2D
#define GL_TEXTURE_2D 0x0DE1
#endif
#ifndef GL_COMPILE
#define GL_COMPILE 0x1300
#endif

/* =====================================================================
 * Evaluator state (per current virtual context; see VCTX below)
 * ===================================================================== */

#define MAX_EVAL_ORDER 8

typedef struct {
    int     defined;
    GLfloat u1, u2, v1, v2;
    int     uorder, vorder;
    int     comps;                 /* 3 for vertex, 2 for texcoord */
    GLfloat ctrl[MAX_EVAL_ORDER][MAX_EVAL_ORDER][4]; /* [u][v][comp] */
} MAP2;

typedef struct {
    MAP2    vertex3;
    MAP2    tex2;
    int     un, vn;
    GLfloat gu1, gu2, gv1, gv2;
} EVALSTATE;

/* =====================================================================
 * Virtual WGL contexts
 * ===================================================================== */

typedef struct {
    GLfloat  ambient[4], diffuse[4], specular[4];
    GLfloat  shininess;
    int      valid;
} MATSHADOW;

typedef struct VCTX {
    int       inUse;
    GLfloat   modelview[16];
    MATSHADOW mat;
    GLuint    boundTex2D;
    GLenum    frontFace;
    unsigned  capMap2Vertex3 : 1;
    unsigned  capMap2Tex2    : 1;
    unsigned  capAutoNormal  : 1;
    unsigned  capNormalize   : 1;
    EVALSTATE eval;
} VCTX;

#define MAX_VCTX 16
static VCTX gVctx[MAX_VCTX];       /* index 0 = the main context */
static int  gCurVctx = 0;
static const GLfloat kIdentity[16] =
    { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

static void vctxInit(VCTX *v)
{
    memset(v, 0, sizeof(*v));
    v->inUse = 1;
    memcpy(v->modelview, kIdentity, sizeof(kIdentity));
    v->frontFace = GL_CCW;
    /* GL default material */
    v->mat.ambient[0] = v->mat.ambient[1] = v->mat.ambient[2] = 0.2f;
    v->mat.ambient[3] = 1.0f;
    v->mat.diffuse[0] = v->mat.diffuse[1] = v->mat.diffuse[2] = 0.8f;
    v->mat.diffuse[3] = 1.0f;
    v->mat.specular[3] = 1.0f;
    v->mat.shininess = 0.0f;
    v->mat.valid = 1;
}

__attribute__((constructor)) static void vctxSetup(void)
{
    vctxInit(&gVctx[0]);
}

#define CUR (&gVctx[gCurVctx])

/* =====================================================================
 * Display lists
 * ===================================================================== */

typedef enum {
    OP_BEGIN, OP_END, OP_VERTEX3F, OP_NORMAL3F, OP_TEXCOORD2F,
    OP_PUSHMATRIX, OP_POPMATRIX, OP_ROTATEF, OP_SCALEF, OP_TRANSLATEF,
    OP_ENABLE, OP_DISABLE, OP_FRONTFACE, OP_BINDTEXTURE,
    OP_MATERIALFV, OP_MATERIALF,
    OP_MAP2F, OP_MAPGRID2F, OP_EVALMESH2,
    OP_CALLLIST
} DLOP;

typedef struct {
    DLOP    op;
    GLenum  e1, e2;
    GLfloat f[6];
    GLint   i[4];
    MAP2   *map;            /* deep-copied control net for OP_MAP2F */
} DLCMD;

typedef struct {
    DLCMD  *cmds;
    int     count, cap;
    int     defined;
    int     alloced;
} DLIST;

#define MAX_LISTS 512
static DLIST gLists[MAX_LISTS];    /* index = list id - 1 */

static int   gRecording = 0;       /* list id being compiled, 0 = none */

static void dlAppend(DLCMD *cmd)
{
    DLIST *l = &gLists[gRecording - 1];
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 64;
        l->cmds = (DLCMD *)realloc(l->cmds, l->cap * sizeof(DLCMD));
    }
    l->cmds[l->count++] = *cmd;
}

static void dlExec(const DLCMD *c);   /* forward */

GLuint glGenLists(GLsizei range)
{
    /* pipes/glaux only ever ask for 1 at a time */
    for (int i = 0; i < MAX_LISTS; i++) {
        if (!gLists[i].alloced) {
            if (range != 1)
                fprintf(stderr, "gl11compat: glGenLists(%d) unsupported\n", range);
            gLists[i].alloced = 1;
            gLists[i].defined = 0;
            gLists[i].count = 0;
            return (GLuint)(i + 1);
        }
    }
    fprintf(stderr, "gl11compat: out of display lists\n");
    return 0;
}

void glNewList(GLuint list, GLenum mode)
{
    if (list == 0 || list > MAX_LISTS || !gLists[list - 1].alloced) {
        fprintf(stderr, "gl11compat: glNewList(%u): bad list\n", list);
        return;
    }
    if (mode != GL_COMPILE)
        fprintf(stderr, "gl11compat: glNewList: only GL_COMPILE supported\n");
    for (int k = 0; k < gLists[list - 1].count; k++)
        free(gLists[list - 1].cmds[k].map);   /* redefinition replaces */
    gLists[list - 1].count = 0;
    gRecording = (int)list;
}

void glEndList(void)
{
    if (gRecording) {
        gLists[gRecording - 1].defined = 1;
        gRecording = 0;
    }
}

#ifdef DL_TRACE
static const char *opName(int op)
{
    static const char *n[] = {"BEGIN","END","V3F","N3F","T2F","PUSHM","POPM",
        "ROT","SCALE","TRANS","ENABLE","DISABLE","FRONTFACE","BINDTEX",
        "MATFV","MATF","MAP2F","MAPGRID2F","EVALMESH2","CALLLIST"};
    return n[op];
}
#endif

void glCallList(GLuint list)
{
    if (list == 0 || list > MAX_LISTS || !gLists[list - 1].defined)
        return;
#ifdef DL_TRACE
    {
        DLIST *tl = &gLists[list - 1];
        fprintf(stderr, "[dl] call list %u (%d cmds): ", list, tl->count);
        for (int k = 0; k < tl->count && k < 40; k++)
            fprintf(stderr, "%s ", opName(tl->cmds[k].op));
        fprintf(stderr, "\n");
    }
#endif
    if (gRecording) {
        /* not needed by this codebase, but honor GL semantics: record the
         * call itself */
        DLCMD c; memset(&c, 0, sizeof c);
        c.op = OP_CALLLIST; c.i[0] = (GLint)list;
        dlAppend(&c);
        return;
    }
    DLIST *l = &gLists[list - 1];
    for (int i = 0; i < l->count; i++)
        dlExec(&l->cmds[i]);
}

void glDeleteLists(GLuint list, GLsizei range)
{
    for (GLuint id = list; id < list + (GLuint)range; id++) {
        if (id == 0 || id > MAX_LISTS) continue;
        DLIST *l = &gLists[id - 1];
        for (int i = 0; i < l->count; i++)
            free(l->cmds[i].map);
        free(l->cmds);
        memset(l, 0, sizeof(*l));
    }
}

GLboolean glIsList(GLuint list)
{
    return (list != 0 && list <= MAX_LISTS && gLists[list - 1].defined)
        ? GL_TRUE : GL_FALSE;
}

/* =====================================================================
 * Immediate-mode execution helpers
 *
 * The JS emulation only accepts glNormal3f/glTexCoord2f between
 * glBegin/glEnd, but GL allows setting the *current* normal/texcoord
 * outside (OBJECTS.CXX sets the sphere-cap normal before glBegin).
 * Track current attributes and re-emit them right after each glBegin.
 * ===================================================================== */

static int     gInBegin = 0;
static GLfloat gCurNormal[3];
static int     gHaveNormal = 0;
static GLfloat gCurTex[2];
static int     gHaveTex = 0;

#ifdef DL_TRACE
static int trcN, trcV, trcT, trcReemit;
#endif

/* The emulation builds a rigid interleaved stream: every vertex in a
 * begin/end block must be preceded by exactly the same attribute calls,
 * but GL immediate mode is a "current attribute" model (e.g. the
 * cylinder in OBJECTS.CXX issues one glNormal3f per quad pair, and the
 * sphere caps set the normal before glBegin).  So current normal and
 * texcoord are fully virtualized here: glNormal3f/glTexCoord2f only
 * update the current values, and every vertex re-emits them, producing
 * the regular stream the emulation needs with identical GL semantics. */

static void execBegin(GLenum mode)
{
    glBegin(mode);
    gInBegin = 1;
}

static void execEnd(void)
{
    glEnd();
    gInBegin = 0;
}

static void execNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
    gCurNormal[0] = x; gCurNormal[1] = y; gCurNormal[2] = z;
    gHaveNormal = 1;
}

static void execVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (gHaveNormal)
        glNormal3f(gCurNormal[0], gCurNormal[1], gCurNormal[2]);
    if (gHaveTex)
        glTexCoord2f(gCurTex[0], gCurTex[1]);
    glVertex3f(x, y, z);
}

static void execTexCoord2f(GLfloat s, GLfloat t)
{
    gCurTex[0] = s; gCurTex[1] = t;
    gHaveTex = 1;
}

/* =====================================================================
 * Evaluator implementation (OpenGL 1.1 spec semantics)
 * ===================================================================== */

/* Bernstein basis B_i^n(t) and its derivative, order = n+1 points */
static void bernstein(int order, GLfloat t, GLfloat *b)
{
    b[0] = 1.0f;
    for (int j = 1; j < order; j++) {
        GLfloat saved = 0.0f;
        for (int k = 0; k < j; k++) {
            GLfloat tmp = b[k];
            b[k] = saved + (1.0f - t) * tmp;
            saved = t * tmp;
        }
        b[j] = saved;
    }
}

static void map2Eval(const MAP2 *m, GLfloat uu, GLfloat vv, GLfloat *out)
{
    GLfloat bu[MAX_EVAL_ORDER], bv[MAX_EVAL_ORDER];
    GLfloat u = (uu - m->u1) / (m->u2 - m->u1);
    GLfloat v = (vv - m->v1) / (m->v2 - m->v1);
    bernstein(m->uorder, u, bu);
    bernstein(m->vorder, v, bv);
    for (int c = 0; c < m->comps; c++) out[c] = 0.0f;
    for (int i = 0; i < m->uorder; i++)
        for (int j = 0; j < m->vorder; j++) {
            GLfloat w = bu[i] * bv[j];
            for (int c = 0; c < m->comps; c++)
                out[c] += w * m->ctrl[i][j][c];
        }
}

/* partial derivatives of the position map (for GL_AUTO_NORMAL) */
static void map2EvalDeriv(const MAP2 *m, GLfloat uu, GLfloat vv,
                          GLfloat *du, GLfloat *dv)
{
    GLfloat bu[MAX_EVAL_ORDER], bv[MAX_EVAL_ORDER];
    GLfloat dbu[MAX_EVAL_ORDER], dbv[MAX_EVAL_ORDER];
    GLfloat u = (uu - m->u1) / (m->u2 - m->u1);
    GLfloat v = (vv - m->v1) / (m->v2 - m->v1);
    int nu = m->uorder, nv = m->vorder;

    bernstein(nu, u, bu);
    bernstein(nv, v, bv);

    /* d/dt B_i^{n-1}(t) via lower-order basis */
    GLfloat blo[MAX_EVAL_ORDER];
    bernstein(nu - 1, u, blo);
    for (int i = 0; i < nu; i++) {
        GLfloat a = (i > 0)      ? blo[i - 1] : 0.0f;
        GLfloat b = (i < nu - 1) ? blo[i]     : 0.0f;
        dbu[i] = (GLfloat)(nu - 1) * (a - b);
    }
    bernstein(nv - 1, v, blo);
    for (int j = 0; j < nv; j++) {
        GLfloat a = (j > 0)      ? blo[j - 1] : 0.0f;
        GLfloat b = (j < nv - 1) ? blo[j]     : 0.0f;
        dbv[j] = (GLfloat)(nv - 1) * (a - b);
    }

    for (int c = 0; c < 3; c++) du[c] = dv[c] = 0.0f;
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++) {
            for (int c = 0; c < 3; c++) {
                du[c] += dbu[i] * bv[j] * m->ctrl[i][j][c];
                dv[c] += bu[i] * dbv[j] * m->ctrl[i][j][c];
            }
        }
    /* chain rule for the u/v domain mapping */
    GLfloat su = 1.0f / (m->u2 - m->u1);
    GLfloat sv = 1.0f / (m->v2 - m->v1);
    for (int c = 0; c < 3; c++) { du[c] *= su; dv[c] *= sv; }
}

static void evalCoord2Emit(GLfloat u, GLfloat v)
{
    VCTX *cx = CUR;
    if (cx->capMap2Tex2 && cx->eval.tex2.defined) {
        GLfloat st[2];
        map2Eval(&cx->eval.tex2, u, v, st);
        execTexCoord2f(st[0], st[1]);
    }
    if (cx->capMap2Vertex3 && cx->eval.vertex3.defined) {
        if (cx->capAutoNormal) {
            GLfloat du[3], dv[3], n[3];
            map2EvalDeriv(&cx->eval.vertex3, u, v, du, dv);
            n[0] = du[1] * dv[2] - du[2] * dv[1];
            n[1] = du[2] * dv[0] - du[0] * dv[2];
            n[2] = du[0] * dv[1] - du[1] * dv[0];
            GLfloat len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
            if (len > 1e-12f) {
                n[0] /= len; n[1] /= len; n[2] /= len;
            }
            execNormal3f(n[0], n[1], n[2]);
        }
        GLfloat p[3];
        map2Eval(&cx->eval.vertex3, u, v, p);
        execVertex3f(p[0], p[1], p[2]);
    }
}

static void execMap2f(GLenum target, const MAP2 *src)
{
    VCTX *cx = CUR;
    if (target == GL_MAP2_VERTEX_3)
        cx->eval.vertex3 = *src;
    else if (target == GL_MAP2_TEXTURE_COORD_2)
        cx->eval.tex2 = *src;
}

static void execMapGrid2f(GLint un, GLfloat u1, GLfloat u2,
                          GLint vn, GLfloat v1, GLfloat v2)
{
    VCTX *cx = CUR;
    cx->eval.un = un; cx->eval.gu1 = u1; cx->eval.gu2 = u2;
    cx->eval.vn = vn; cx->eval.gv1 = v1; cx->eval.gv2 = v2;
}

static void execEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    VCTX *cx = CUR;
    if (mode != GL_FILL) {
        /* GL_LINE / GL_POINT only reachable via auxWireTeapot (unused) */
        return;
    }
    GLfloat du = (cx->eval.gu2 - cx->eval.gu1) / (GLfloat)cx->eval.un;
    GLfloat dv = (cx->eval.gv2 - cx->eval.gv1) / (GLfloat)cx->eval.vn;
    /* spec: at i==un / j==vn the domain endpoint must be hit exactly
     * (i*du + u1 can miss u2 by an ulp) */
    #define EVAL_U(i) ((i) == cx->eval.un ? cx->eval.gu2 : cx->eval.gu1 + (i) * du)
    #define EVAL_V(j) ((j) == cx->eval.vn ? cx->eval.gv2 : cx->eval.gv1 + (j) * dv)
    for (GLint j = j1; j < j2; j++) {
        execBegin(GL_TRIANGLE_STRIP);   /* == GL_QUAD_STRIP geometry */
        for (GLint i = i1; i <= i2; i++) {
            evalCoord2Emit(EVAL_U(i), EVAL_V(j));
            evalCoord2Emit(EVAL_U(i), EVAL_V(j + 1));
        }
        execEnd();
    }
    #undef EVAL_U
    #undef EVAL_V
}

/* =====================================================================
 * Enable/disable interception (evaluator caps stay internal)
 * ===================================================================== */

#ifndef GL_SCISSOR_TEST
#define GL_SCISSOR_TEST 0x0C11
#endif
static int gScissorOn = 0;   /* only CLEAR.CXX's wipe/dissolve use scissor */

static int isInternalCap(GLenum cap)
{
    return cap == GL_MAP2_VERTEX_3 || cap == GL_MAP2_TEXTURE_COORD_2 ||
           cap == GL_AUTO_NORMAL   || cap == GL_NORMALIZE;
}

static void execEnable(GLenum cap, int on)
{
    VCTX *cx = CUR;
    switch (cap) {
    case GL_MAP2_VERTEX_3:        cx->capMap2Vertex3 = on; return;
    case GL_MAP2_TEXTURE_COORD_2: cx->capMap2Tex2 = on;    return;
    case GL_AUTO_NORMAL:          cx->capAutoNormal = on;  return;
    case GL_NORMALIZE:            cx->capNormalize = on;   return;
        /* NORMALIZE: evaluator normals are always normalized here;
         * fixed-function normal rescaling is handled by the emulation's
         * shader path, so nothing further to do. */
    default:
        if (cap == GL_SCISSOR_TEST)
            gScissorOn = on;
        if (on) glEnable(cap); else glDisable(cap);
    }
}

/* =====================================================================
 * Command execution (shared by immediate path and glCallList replay)
 * ===================================================================== */

static void execMaterialfv(GLenum face, GLenum pname, const GLfloat *p)
{
    VCTX *cx = CUR;
    if (face == GL_BACK)
        return;  /* culled; emulation would abort on GL_BACK */
    switch (pname) {
    case GL_AMBIENT:  memcpy(cx->mat.ambient,  p, 4 * sizeof(GLfloat)); break;
    case GL_DIFFUSE:  memcpy(cx->mat.diffuse,  p, 4 * sizeof(GLfloat)); break;
    case GL_SPECULAR: memcpy(cx->mat.specular, p, 4 * sizeof(GLfloat)); break;
    case GL_SHININESS: cx->mat.shininess = p[0]; break;
    }
    glMaterialfv(face == GL_FRONT ? GL_FRONT : face, pname, p);
}

static void execMaterialf(GLenum face, GLenum pname, GLfloat v)
{
    VCTX *cx = CUR;
    if (face == GL_BACK)
        return;
    if (pname == GL_SHININESS)
        cx->mat.shininess = v;
    emuMaterialf(face == GL_FRONT ? GL_FRONT : face, pname, v);
}

static void dlExec(const DLCMD *c)
{
    switch (c->op) {
    case OP_BEGIN:      execBegin(c->e1); break;
    case OP_END:        execEnd(); break;
    case OP_VERTEX3F:   execVertex3f(c->f[0], c->f[1], c->f[2]); break;
    case OP_NORMAL3F:   execNormal3f(c->f[0], c->f[1], c->f[2]); break;
    case OP_TEXCOORD2F: execTexCoord2f(c->f[0], c->f[1]); break;
    case OP_PUSHMATRIX: glPushMatrix(); break;
    case OP_POPMATRIX:  glPopMatrix(); break;
    case OP_ROTATEF:    glRotatef(c->f[0], c->f[1], c->f[2], c->f[3]); break;
    case OP_SCALEF:     glScalef(c->f[0], c->f[1], c->f[2]); break;
    case OP_TRANSLATEF: glTranslatef(c->f[0], c->f[1], c->f[2]); break;
    case OP_ENABLE:     execEnable(c->e1, 1); break;
    case OP_DISABLE:    execEnable(c->e1, 0); break;
    case OP_FRONTFACE:  CUR->frontFace = c->e1; glFrontFace(c->e1); break;
    case OP_BINDTEXTURE:
        CUR->boundTex2D = (GLuint)c->i[0];
        glBindTexture(c->e1, (GLuint)c->i[0]);
        break;
    case OP_MATERIALFV: execMaterialfv(c->e1, c->e2, c->f); break;
    case OP_MATERIALF:  execMaterialf(c->e1, c->e2, c->f[0]); break;
    case OP_MAP2F:      execMap2f(c->e1, c->map); break;
    case OP_MAPGRID2F:
        execMapGrid2f(c->i[0], c->f[0], c->f[1], c->i[1], c->f[2], c->f[3]);
        break;
    case OP_EVALMESH2:
        execEvalMesh2(c->e1, c->i[0], c->i[1], c->i[2], c->i[3]);
        break;
    case OP_CALLLIST:   glCallList((GLuint)c->i[0]); break;
    }
}

/* record-or-execute helper */
#define RECORD_OR_EXEC(setup)              \
    do {                                   \
        DLCMD c; memset(&c, 0, sizeof c);  \
        setup;                             \
        if (gRecording) dlAppend(&c);      \
        else dlExec(&c);                   \
    } while (0)

/* =====================================================================
 * Public GL entry points (override the JS library at link time)
 * ===================================================================== */

void pipes_glBegin(GLenum mode)
{
    /* Translate modes the emulation cannot draw. */
    GLenum m = mode;
    if (mode == GL_QUAD_STRIP) m = GL_TRIANGLE_STRIP;
    else if (mode == GL_POLYGON) m = GL_TRIANGLE_FAN;
    RECORD_OR_EXEC({ c.op = OP_BEGIN; c.e1 = m; });
}

void pipes_glEnd(void)
{
    RECORD_OR_EXEC({ c.op = OP_END; });
}

void pipes_glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    RECORD_OR_EXEC({ c.op = OP_VERTEX3F; c.f[0] = x; c.f[1] = y; c.f[2] = z; });
}

void pipes_glVertex3fv(const GLfloat *v)
{
    pipes_glVertex3f(v[0], v[1], v[2]);
}

void pipes_glNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
    RECORD_OR_EXEC({ c.op = OP_NORMAL3F; c.f[0] = x; c.f[1] = y; c.f[2] = z; });
}

void pipes_glNormal3fv(const GLfloat *v)
{
    pipes_glNormal3f(v[0], v[1], v[2]);
}

void pipes_glTexCoord2f(GLfloat s, GLfloat t)
{
    RECORD_OR_EXEC({ c.op = OP_TEXCOORD2F; c.f[0] = s; c.f[1] = t; });
}

void pipes_glPushMatrix(void)
{
    RECORD_OR_EXEC({ c.op = OP_PUSHMATRIX; });
}

void pipes_glPopMatrix(void)
{
    RECORD_OR_EXEC({ c.op = OP_POPMATRIX; });
}

void pipes_glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z)
{
    RECORD_OR_EXEC({ c.op = OP_ROTATEF;
                     c.f[0] = a; c.f[1] = x; c.f[2] = y; c.f[3] = z; });
}

void pipes_glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    RECORD_OR_EXEC({ c.op = OP_SCALEF; c.f[0] = x; c.f[1] = y; c.f[2] = z; });
}

void pipes_glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    RECORD_OR_EXEC({ c.op = OP_TRANSLATEF; c.f[0] = x; c.f[1] = y; c.f[2] = z; });
}

void pipes_glEnable(GLenum cap)
{
    RECORD_OR_EXEC({ c.op = OP_ENABLE; c.e1 = cap; });
}

void pipes_glDisable(GLenum cap)
{
    RECORD_OR_EXEC({ c.op = OP_DISABLE; c.e1 = cap; });
}

void pipes_glFrontFace(GLenum dir)
{
    RECORD_OR_EXEC({ c.op = OP_FRONTFACE; c.e1 = dir; });
}

void pipes_glBindTexture(GLenum target, GLuint tex)
{
    RECORD_OR_EXEC({ c.op = OP_BINDTEXTURE; c.e1 = target;
                     c.i[0] = (GLint)tex; });
}

void pipes_glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    int n = (pname == GL_SHININESS) ? 1 : 4;
    RECORD_OR_EXEC({ c.op = OP_MATERIALFV; c.e1 = face; c.e2 = pname;
                     memcpy(c.f, params, n * sizeof(GLfloat)); });
}

void pipes_glMaterialf(GLenum face, GLenum pname, GLfloat val)
{
    RECORD_OR_EXEC({ c.op = OP_MATERIALF; c.e1 = face; c.e2 = pname;
                     c.f[0] = val; });
}

extern void glFlush(void);

void pipes_glFlush(void)
{
    /* The original renders single-buffered: glFlush made progress visible
     * immediately, which is what animates CLEAR.CXX's digital-dissolve
     * scene wipe (a flush after every scissored rectangle).  A browser
     * composites only when the JS task yields, so during scissored
     * clears we suspend via ASYNCIFY at most every ~10ms — the dissolve
     * becomes visible again, and CalibrateClear's timing loop measures
     * real elapsed time as designed.  The per-pipe flushes of normal
     * drawing (scissor off) never yield, preserving the tick cadence. */
    glFlush();
    if (gScissorOn) {
        static double lastYield = 0.0;
        double now = emscripten_get_now();
        if (now - lastYield >= 10.0) {
            lastYield = now;
            emscripten_sleep(0);
        }
    }
}

extern void glTexParameteri(GLenum target, GLenum pname, GLint param);

void pipes_glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    /* GL 1.1 allowed configuring default texture object 0 (STATE.CXX
     * GLInit -> InitTexParams runs on fresh contexts before any bind);
     * WebGL has no default texture and raises INVALID_OPERATION.  The
     * default object's parameters are never used for rendering here —
     * real texture objects are configured after glGenTextures +
     * glBindTexture in TEXTURE.C — so drop the call when nothing is
     * bound in the current virtual context. */
    if (target == GL_TEXTURE_2D && CUR->boundTex2D == 0)
        return;
    glTexParameteri(target, pname, param);
}

void glMap2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride,
             GLint uorder, GLfloat v1, GLfloat v2, GLint vstride,
             GLint vorder, const GLfloat *points)
{
    if (uorder < 2 || uorder > MAX_EVAL_ORDER ||
        vorder < 2 || vorder > MAX_EVAL_ORDER) {
        fprintf(stderr, "gl11compat: glMap2f order %dx%d unsupported\n",
                uorder, vorder);
        return;
    }
    MAP2 m;
    memset(&m, 0, sizeof m);
    m.defined = 1;
    m.u1 = u1; m.u2 = u2; m.v1 = v1; m.v2 = v2;
    m.uorder = uorder; m.vorder = vorder;
    m.comps = (target == GL_MAP2_VERTEX_3) ? 3 :
              (target == GL_MAP2_TEXTURE_COORD_2) ? 2 : 0;
    if (!m.comps) {
        fprintf(stderr, "gl11compat: glMap2f target 0x%x unsupported\n", target);
        return;
    }
    for (int i = 0; i < uorder; i++)
        for (int j = 0; j < vorder; j++)
            for (int c = 0; c < m.comps; c++)
                m.ctrl[i][j][c] = points[i * ustride + j * vstride + c];

    if (gRecording) {
        DLCMD c; memset(&c, 0, sizeof c);
        c.op = OP_MAP2F; c.e1 = target;
        c.map = (MAP2 *)malloc(sizeof(MAP2));
        *c.map = m;
        dlAppend(&c);
    } else {
        execMap2f(target, &m);
    }
}

void glMapGrid2f(GLint un, GLfloat u1, GLfloat u2,
                 GLint vn, GLfloat v1, GLfloat v2)
{
    RECORD_OR_EXEC({ c.op = OP_MAPGRID2F;
                     c.i[0] = un; c.i[1] = vn;
                     c.f[0] = u1; c.f[1] = u2; c.f[2] = v1; c.f[3] = v2; });
}

void glEvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    RECORD_OR_EXEC({ c.op = OP_EVALMESH2; c.e1 = mode;
                     c.i[0] = i1; c.i[1] = i2; c.i[2] = j1; c.i[3] = j2; });
}

/* =====================================================================
 * glGetIntegerv wrapper
 *
 * mesa GLU's gluScaleImage (used by TEXTURE.C to pad textures to powers
 * of two) queries the full GL 1.x pixel-store state.  WebGL1 lacks the
 * ROW_LENGTH/SKIP/SWAP/LSB pnames, and emscripten's glGetIntegerv leaves
 * the output untouched for them, so GLU would compute strides from
 * uninitialized memory.  Synthesize their GL default values; everything
 * else passes through.
 * ===================================================================== */

extern void emscripten_glGetIntegerv(GLenum pname, GLint *out);

void glGetIntegerv(GLenum pname, GLint *out)
{
    switch (pname) {
    case 0x0CF0:   /* GL_UNPACK_SWAP_BYTES */
    case 0x0CF1:   /* GL_UNPACK_LSB_FIRST */
    case 0x0CF2:   /* GL_UNPACK_ROW_LENGTH */
    case 0x0CF3:   /* GL_UNPACK_SKIP_ROWS */
    case 0x0CF4:   /* GL_UNPACK_SKIP_PIXELS */
    case 0x0D00:   /* GL_PACK_SWAP_BYTES */
    case 0x0D01:   /* GL_PACK_LSB_FIRST */
    case 0x0D02:   /* GL_PACK_ROW_LENGTH */
    case 0x0D03:   /* GL_PACK_SKIP_ROWS */
    case 0x0D04:   /* GL_PACK_SKIP_PIXELS */
        *out = 0;
        return;
    default:
        emscripten_glGetIntegerv(pname, out);
    }
}

/* =====================================================================
 * glTexImage2D wrapper: GL 1.0 accepted a component COUNT (1..4) as
 * internalformat (TEXTURE.C passes pTex->components = 3), which WebGL
 * rejects, leaving the texture incomplete (samples as black).  Remap the
 * legacy counts to the sized-less formats WebGL1 accepts (internalformat
 * must equal format there).
 * ===================================================================== */

extern void emscripten_glTexImage2D(GLenum target, GLint level,
    GLint internalformat, GLsizei width, GLsizei height, GLint border,
    GLenum format, GLenum type, const void *pixels);

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const void *pixels)
{
    switch (internalformat) {
    case 1: internalformat = 0x1909; break;  /* GL_LUMINANCE */
    case 2: internalformat = 0x190A; break;  /* GL_LUMINANCE_ALPHA */
    case 3: internalformat = 0x1907; break;  /* GL_RGB */
    case 4: internalformat = 0x1908; break;  /* GL_RGBA */
    }
    emscripten_glTexImage2D(target, level, internalformat, width, height,
                            border, format, type, pixels);
#ifdef DL_TRACE
    {
        const unsigned char *px = (const unsigned char *)pixels;
        fprintf(stderr, "[tex] upload if=0x%x %dx%d fmt=0x%x type=0x%x px=%p"
                " first=[%d %d %d %d %d %d]\n",
                internalformat, width, height, format, type, pixels,
                px ? px[0] : -1, px ? px[1] : -1, px ? px[2] : -1,
                px ? px[3] : -1, px ? px[4] : -1, px ? px[5] : -1);
    }
#endif
}

/* =====================================================================
 * WGL virtual contexts
 * ===================================================================== */

static void vctxApply(const VCTX *v)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(v->modelview);
    glMaterialfv(GL_FRONT, GL_AMBIENT,  v->mat.ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,  v->mat.diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, v->mat.specular);
    emuMaterialf(GL_FRONT, GL_SHININESS, v->mat.shininess);
    glFrontFace(v->frontFace);
    glBindTexture(GL_TEXTURE_2D, v->boundTex2D);
}

void *wglCreateContext(void *hdc)
{
    (void)hdc;
    for (int i = 1; i < MAX_VCTX; i++) {
        if (!gVctx[i].inUse) {
            vctxInit(&gVctx[i]);
            return (void *)(intptr_t)(i + 1);   /* handle = index+1 */
        }
    }
    fprintf(stderr, "gl11compat: out of virtual contexts\n");
    return 0;
}

int wglDeleteContext(void *hrc)
{
    int idx = (int)(intptr_t)hrc - 1;
    if (idx > 0 && idx < MAX_VCTX)
        gVctx[idx].inUse = 0;
    return 1;
}

void *wglGetCurrentContext(void)
{
    return (void *)(intptr_t)(gCurVctx + 1);
}

void *wglGetCurrentDC(void)
{
    return (void *)(intptr_t)1;
}

int wglMakeCurrent(void *hdc, void *hrc)
{
    (void)hdc;
    int idx = (int)(intptr_t)hrc - 1;
    if (idx < 0 || idx >= MAX_VCTX || !gVctx[idx].inUse)
        return 0;
    if (idx == gCurVctx)
        return 1;
    /* save outgoing modelview (all other state is shadowed on write) */
    glGetFloatv(GL_MODELVIEW_MATRIX, gVctx[gCurVctx].modelview);
    gCurVctx = idx;
    vctxApply(&gVctx[idx]);
    return 1;
}

int wglShareLists(void *h1, void *h2)
{
    (void)h1; (void)h2;
    return 1;   /* all virtual contexts share one real context */
}

int (*wglGetProcAddress(const char *name))(void)
{
    /* Original behavior when an extension is absent: NULL.  TEXTURE.C
     * then runs without GL_EXT_paletted_texture, as on most real
     * hardware of the era. */
    (void)name;
    return 0;
}
