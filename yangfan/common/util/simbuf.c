/*
 * Copyright (c) 2020-2030 iSoftStone Information Technology (Group) Co.,Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>


#ifndef DRMFORMATMODINVALID
#define DRMFORMATMODINVALID ((1ULL << 56) - 1)
#endif

/* Possible options that affect the showinged image */
#define OPTIMMEDIATE     (1 << 0)  /* create isftbuffer immediately */
#define OPTIMPLICITSYNC (1 << 1)  /* force implicit sync */
#define OPTMANDELBROT    (1 << 2)  /* painter mandelbrot */
#define OPTDIRECTshowing     (1 << 3)  /* direct-showing */

#define BUFFERFORMAT DRMFORMATXRGB8888
#define MAXBUFFERPLANES 4

struct showing {
    struct isftshowing *showing;
    struct isftregistry *registry;
    struct isftcompositor *compositor;
    struct xdgwmbase *wmbase;
    struct zwpfullscreenshellv1 *fshell;
    struct zwplinuxdmabufv1 *dmabuf;
    struct isftViewdirectshowingv1 *directshowing;
    struct zwplinuxexplicitsynchronizationv1 *explicitsync;
    uint64t *modifiers;
    int modifierscount;
    int reqdmabufimmediate;
    bool useexplicitsync;
    struct {
        PGAshowing showing;
        PGAContext context;
        PGAConfig conf;
        bool hasdmabufimportmodifiers;
        bool hasnoconfigcontext;
        PFNPGAQUERYDMABUFMODIFIERSEXTPROC querydmabufmodifiers;
        PFNPGACREATEIMAGEKHRPROC createimage;
        PFNPGADESTROYIMAGEKHRPROC destroyimage;
        PFNGLPGAIMAGETARGETTEXTURE2DOESPROC imagetargettexture2d;
        PFNPGACREATESYNCKHRPROC createsync;
        PFNPGADESTROYSYNCKHRPROC destroysync;
        PFNPGACLIENTWAITSYNCKHRPROC clientwaitsync;
        PFNPGADUPNATIVEFENCEFDANDROIDPROC dupnativefencefd;
        PFNPGAWAITSYNCKHRPROC waitsync;
    } PGA;
    struct {
        int drmfd;
        struct gbmdevice *device;
    } gbm;
};

struct buffer {
    struct showing *showing;
    struct isftbuffer *buffer;
    int busy;

    struct gbmbo *bo;

    int width;
    int height;
    int format;
    uint64t modifier;
    int planecount;
    int dmabuffds[MAXBUFFERPLANES];
    unsigned int strides[MAXBUFFERPLANES];
    unsigned int offsets[MAXBUFFERPLANES];

    PGAImageKHR PGAimage;
    GLuint gltexture;
    GLuint glfbo;

    struct zwplinuxbufferreleasev1 *bufferrelease;
    int releasefencefd;
};

#define NUMBUFFERS 3

struct view {
    struct showing *showing;
    int width, height;
    struct isftsurface *surface;
    struct xdgsurface *xdgsurface;
    struct xdgtoplevel *xdgtoplevel;
    struct zwplinuxsurfacesynchronizationv1 *surfacesync;
    struct buffer buffers[NUMBUFFERS];
    struct isftcallback *callback;
    bool initialized;
    bool waitforconfigure;
    struct {
        GLuint program;
        GLuint pos;
        GLuint color;
        GLuint offsetuniform;
    } gl;
    bool paintmandelbrot;
};

static sigatomict running = 1;

static void
redraw(void *data, struct isftcallback *callback, unsigned int time);


static int
createdmabufbuffer(struct showing *showing, struct buffer *buffer,
             int width, int height, unsigned int opts)
{
    /* Y-Invert the buffer image, since we are going to painterer to the
     * buffer through a FBO. */
    static unsigned int flags = ZWPLINUXBUFFERPARAMSV1FLAGSYINVERT;
    struct zwplinuxbufferparamsv1 *params;
    int i;

    buffer->showing = showing;
    buffer->width = width;
    buffer->height = height;
    buffer->format = BUFFERFORMAT;
    buffer->releasefencefd = -1;

#ifdef HAVEGBMMODIFIERS
    if (showing->modifierscount > 0) {
        buffer->bo = gbmbocreatewithmodifiers(showing->gbm.device,
                                        buffer->width,
                                        buffer->height,
                                        buffer->format,
                                        showing->modifiers,
                                        showing->modifierscount);
        if (buffer->bo)
            buffer->modifier = gbmbogetmodifier(buffer->bo);
    }
#endif

    if (!buffer->bo) {
        buffer->bo = gbmbocreate(showing->gbm.device,
                           buffer->width,
                           buffer->height,
                           buffer->format,
                           GBMBOUSEpainterING);
        buffer->modifier = DRMFORMATMODINVALID;
    }

    if (!buffer->bo) {
        fprintf(stderr, "createbo failed\n");
        goto error;
    }

#ifdef HAVEGBMMODIFIERS
    buffer->planecount = gbmbogetplanecount(buffer->bo);
    for (i = 0; i < buffer->planecount; ++i) {
        int ret;
        union gbmbohandle handle;

        handle = gbmbogethandleforplane(buffer->bo, i);
        if (handle.s32 == -1) {
            fprintf(stderr, "error: failed to get gbmbohandle\n");
            goto error;
        }

        ret = drmPrimeHandleToFD(showing->gbm.drmfd, handle.u32, 0,
                     &buffer->dmabuffds[i]);
        if (ret < 0 || buffer->dmabuffds[i] < 0) {
            fprintf(stderr, "error: failed to get dmabuffd\n");
            goto error;
        }
        buffer->strides[i] = gbmbogetstrideforplane(buffer->bo, i);
        buffer->offsets[i] = gbmbogetoffset(buffer->bo, i);
    }
#else
    buffer->planecount = 1;
    buffer->strides[0] = gbmbogetstride(buffer->bo);
    buffer->dmabuffds[0] = gbmbogetfd(buffer->bo);
    if (buffer->dmabuffds[0] < 0) {
        fprintf(stderr, "error: failed to get dmabuffd\n");
        goto error;
    }
#endif

    params = zwplinuxdmabufv1createparams(showing->dmabuf);

    if ((opts & OPTDIRECTshowing) && showing->directshowing) {
        isftViewdirectshowingv1enable(showing->directshowing, params);
        /* turn off YINVERT otherwise linux-dmabuf will reject it and
         * we need all dmabuf flags turned off */
        flags &= ~ZWPLINUXBUFFERPARAMSV1FLAGSYINVERT;

        fprintf(stdout, "image is y-inverted as direct-showing flag was set, "
                "dmabuf y-inverted attribute flag was removed\n");
    }

    for (i = 0; i < buffer->planecount; ++i) {
        zwplinuxbufferparamsv1add(params,
                           buffer->dmabuffds[i],
                           i,
                           buffer->offsets[i],
                           buffer->strides[i],
                           buffer->modifier >> 32,
                           buffer->modifier & 0xffffffff);
    }

    zwplinuxbufferparamsv1addlistener(params, &paramslistener, buffer);
    if (showing->reqdmabufimmediate) {
        buffer->buffer =
            zwplinuxbufferparamsv1createimmed(params,
                                buffer->width,
                                buffer->height,
                                buffer->format,
                                flags);
        if (!buffer->showing->useexplicitsync) {
            isftbufferaddlistener(buffer->buffer,
                         &bufferlistener,
                         buffer);
        }
    } else {
            zwplinuxbufferparamsv1create(params,
                          buffer->width,
                          buffer->height,
                          buffer->format,
                          flags);
    }

    if (!createfboforbuffer(showing, buffer)) {
        goto error;
    }

    return 0;

error:
    bufferfree(buffer);
    return -1;
}

static void
bufferrelease(void *data, struct isftbuffer *buffer)
{
    struct buffer *mybuf = data;

    mybuf->busy = 0;
}

static const struct isftbufferlistener bufferlistener = {
    bufferrelease
};

static void
bufferfree(struct buffer *buf)
{
    int i;

    if (buf->releasefencefd >= 0) {
        close(buf->releasefencefd);
    }
    if (buf->bufferrelease) {
        zwplinuxbufferreleasev1destroy(buf->bufferrelease);
    }
    if (buf->glfbo) {
        glDeleteFramebuffers(1, &buf->glfbo);
    }
    if (buf->gltexture) {
        glDeleteTextures(1, &buf->gltexture);
    }
    if (buf->PGAimage) {
        buf->showing->PGA.destroyimage(buf->showing->PGA.showing,
                        buf->PGAimage);
    }

    if (buf->buffer) {
        isftbufferdestroy(buf->buffer);

    if (buf->bo)
        gbmbodestroy(buf->bo);
    }
    for (i = 0; i < buf->planecount; ++i) {
        if (buf->dmabuffds[i] >= 0)
            close(buf->dmabuffds[i]);
    }
}

static void
createsucceeded(void *data,
         struct zwplinuxbufferparamsv1 *params,
         struct isftbuffer *newbuffer)
{
    struct buffer *buffer = data;

    buffer->buffer = newbuffer;
    /* When not using explicit synchronization listen to isftbuffer.release
     * for release notifications, otherwise we are going to use
     * zwplinuxbufferreleasev1. */
    if (!buffer->showing->useexplicitsync) {
        isftbufferaddlistener(buffer->buffer, &bufferlistener,
                        buffer);
    }

    zwplinuxbufferparamsv1destroy(params);
}

static void
createfailed(void *data, struct zwplinuxbufferparamsv1 *params)
{
    struct buffer *buffer = data;

    buffer->buffer = NULL;
    running = 0;

    zwplinuxbufferparamsv1destroy(params);

    int ret = fprintf(stderr, "Error: zwplinuxbufferparams.create failed.\n");
    if(ret >= 0) {
        LOGE("printf success");
    }
}

static const struct zwplinuxbufferparamsv1listener paramslistener = {
    createsucceeded,
    createfailed
};

static bool
createfboforbuffer(struct showing *showing, struct buffer *buffer)
{
    static const int generalattribs = 3;
    static const int planeattribs = 5;
    static const int entriesperattrib = 2;
    PGAint attribs[(generalattribs + planeattribs * MAXBUFFERPLANES) *
            entriesperattrib + 1];
    unsigned int atti = 0;

    attribs[atti++] = PGAWIDTH;
    attribs[atti++] = buffer->width;
    attribs[atti++] = PGAHEIGHT;
    attribs[atti++] = buffer->height;
    attribs[atti++] = PGALINUXDRMFOURCCEXT;
    attribs[atti++] = buffer->format;

#define ADDPLANEATTRIBS(planeidx) { \
    attribs[atti++] = PGADMABUFPLANE ## planeidx ## FDEXT; \
    attribs[atti++] = buffer->dmabuffds[planeidx]; \
    attribs[atti++] = PGADMABUFPLANE ## planeidx ## OFFSETEXT; \
    attribs[atti++] = (int) buffer->offsets[planeidx]; \
    attribs[atti++] = PGADMABUFPLANE ## planeidx ## PITCHEXT; \
    attribs[atti++] = (int) buffer->strides[planeidx]; \
    if (showing->PGA.hasdmabufimportmodifiers) { \
        attribs[atti++] = PGADMABUFPLANE ## planeidx ## MODIFIERLOEXT; \
        attribs[atti++] = buffer->modifier & 0xFFFFFFFF; \
        attribs[atti++] = PGADMABUFPLANE ## planeidx ## MODIFIERHIEXT; \
        attribs[atti++] = buffer->modifier >> 32; \
    } \
    }

    if (buffer->planecount > 0)
        ADDPLANEATTRIBS(0);

    if (buffer->planecount > 1)
        ADDPLANEATTRIBS(1);

    if (buffer->planecount > 2)
        ADDPLANEATTRIBS(2);

    if (buffer->planecount > 3)
        ADDPLANEATTRIBS(3);

#undef ADDPLANEATTRIBS

    attribs[atti] = PGANONE;

    assert(atti < ARRAYLENGTH(attribs));

    buffer->PGAimage = showing->PGA.createimage(showing->PGA.showing,
                                             PGANOCONTEXT,
                                             PGALINUXDMABUFEXT,
                                             NULL, attribs);
    if (buffer->PGAimage == PGANOIMAGEKHR) {
        fprintf(stderr, "PGAImageKHR creation failed\n");
        return false;
    }

    PGAMakeCurrent(showing->PGA.showing, PGANOSURFACE, PGANOSURFACE,
                showing->PGA.context);

    glGenTextures(1, &buffer->gltexture);
    glBindTexture(GLTEXTURE2D, buffer->gltexture);
    glTexParameteri(GLTEXTURE2D, GLTEXTUREWRAPS, GLCLAMPTOEDGE);
    glTexParameteri(GLTEXTURE2D, GLTEXTUREWRAPT, GLCLAMPTOEDGE);
    glTexParameteri(GLTEXTURE2D, GLTEXTUREMAGFILTER, GLLINEAR);
    glTexParameteri(GLTEXTURE2D, GLTEXTUREMINFILTER, GLLINEAR);

    showing->PGA.imagetargettexture2d(GLTEXTURE2D, buffer->PGAimage);

    glGenFramebuffers(1, &buffer->glfbo);
    glBindFramebuffer(GLFRAMEBUFFER, buffer->glfbo);
    glFramebufferTexture2D(GLFRAMEBUFFER, GLCOLORATTACHMENT0,
                        GLTEXTURE2D, buffer->gltexture, 0);
    if (glCheckFramebufferStatus(GLFRAMEBUFFER) != GLFRAMEBUFFERCOMPLETE) {
        fprintf(stderr, "FBO creation failed\n");
        return false;
    }

    return true;
}
static void
destroyview(struct view *view)
{
    int i;

    if (view->gl.program)
        glDeleteProgram(view->gl.program);

    if (view->callback)
        isftcallbackdestroy(view->callback);

    for (i = 0; i < NUMBUFFERS; i++) {
        if (view->buffers[i].buffer)
            bufferfree(&view->buffers[i]);
    }

    if (view->xdgtoplevel)
        xdgtopleveldestroy(view->xdgtoplevel);
    if (view->xdgsurface)
        xdgsurfacedestroy(view->xdgsurface);
    if (view->surfacesync)
        zwplinuxsurfacesynchronizationv1destroy(view->surfacesync);
    isftsurfacedestroy(view->surface);
    free(view);
}

static struct view *
createview(struct showing *showing, int width, int height, int opts)
{
    struct view *view;
    int i;
    int ret;

    view = zalloc(sizeof *view);
    if (!view)
        return NULL;

    view->callback = NULL;
    view->showing = showing;
    view->width = width;
    view->height = height;
    view->surface = isftcompositorcreatesurface(showing->compositor);

    if (showing->wmbase) {
        view->xdgsurface =
            xdgwmbasegetxdgsurface(showing->wmbase,
                          view->surface);

        assert(view->xdgsurface);

        xdgsurfaceaddlistener(view->xdgsurface,
                     &xdgsurfacelistener, view);

        view->xdgtoplevel =
            xdgsurfacegettoplevel(view->xdgsurface);

        assert(view->xdgtoplevel);

        xdgtopleveladdlistener(view->xdgtoplevel,
                   &xdgtoplevellistener, view);

        xdgtoplevelsettitle(view->xdgtoplevel, "simple-dmabuf-PGA");

        view->waitforconfigure = true;
        isftsurfacecommit(view->surface);
    } else if (showing->fshell) {
        zwpfullscreenshellv1presentsurface(showing->fshell,
                                     view->surface,
                                     ZWPFULLSCREENSHELLV1PRESENTMETHODDEFAULT,
                                     NULL);
    } else {
        assert(0);
    }

    if (showing->explicitsync) {
        view->surfacesync =
            zwplinuxexplicitsynchronizationv1getsynchronization(
       showing->explicitsync, view->surface);
        assert(view->surfacesync);
    }

    for (i = 0; i < NUMBUFFERS; ++i) {
        int j;
        for (j = 0; j < MAXBUFFERPLANES; ++j)
            view->buffers[i].dmabuffds[j] = -1;
    }
    for (i = 0; i < NUMBUFFERS; ++i) {
        ret = createdmabufbuffer(showing, &view->buffers[i],
                           width, height, opts);
        if (ret < 0)
            goto error;
    }

    view->paintmandelbrot = opts & OPTMANDELBROT;

    if (!viewsetupgl(view))
        goto error;
    return view;
error:
    if (view)
        destroyview(view);
    return NULL;
}

static int
createPGAfencefd(struct view *view)
{
    struct showing *d = view->showing;
    PGASyncKHR sync = d->PGA.createsync(d->PGA.showing,
                                     PGASYNCNATIVEFENCEANDROID,
                                     NULL);
    int fd;

    assert(sync != PGANOSYNCKHR);
    /* We need to flush before we can get the fence fd. */
    glFlush();
    fd = d->PGA.dupnativefencefd(d->PGA.showing, sync);
    assert(fd >= 0);

    d->PGA.destroysync(d->PGA.showing, sync);

    return fd;
}

static struct buffer *
viewnextbuffer(struct view *view)
{
    int i;

    for (i = 0; i < NUMBUFFERS; i++)
        if (!view->buffers[i].busy)
            return &view->buffers[i];

    return NULL;
}

static const struct isftcallbacklistener framelistener;

static void
painter(struct view *view, struct buffer *buffer)
{
    /* Complete a movement iteration in 5000 ms. */
    static const uint64t iterationms = 5000;
    static const GLfloat verts[4][2] = {
        { -0.5, -0.5 },
        { -0.5,  0.5 },
        {  0.5, -0.5 },
        {  0.5,  0.5 }
    };
    static const GLfloat colors[4][3] = {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 },
        { 1, 1, 0 }
    };
    GLfloat offset;
    struct timeval tv;
    uint64t timems;

    gettimeofday(&tv, NULL);
    timems = tv.tvsec * 1000 + tv.tvusec / 1000;

    /* Split timems in repeating views of [0, iterationms) and map them
     * to offsets in the [-0.5, 0.5) range. */
    offset = (timems % iterationms) / (float) iterationms - 0.5;

    /* Direct all GL draws to the buffer through the FBO */
    glBindFramebuffer(GLFRAMEBUFFER, buffer->glfbo);

    glViewport(0, 0, view->width, view->height);

    glUniform1f(view->gl.offsetuniform, offset);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GLCOLORBUFFERBIT);

    glVertexAttribPointer(view->gl.pos, 2, GLFLOAT, GLFALSE, 0, verts);
    glVertexAttribPointer(view->gl.color, 3, GLFLOAT, GLFALSE, 0, colors);
    glEnableVertexAttribArray(view->gl.pos);
    glEnableVertexAttribArray(view->gl.color);

    glDrawArrays(GLTRIANGLESTRIP, 0, 4);

    glDisableVertexAttribArray(view->gl.pos);
    glDisableVertexAttribArray(view->gl.color);
}

static void
paintmandelbrot(struct view *view, struct buffer *buffer)
{
    /* Complete a movement iteration in 5000 ms. */
    static const uint64t iterationms = 5000;
    /* Split drawing in a square grid consisting of gridside * gridside
     * cells. */
    static const int gridside = 4;
    GLfloat normcellside = 1.0 / gridside;
    int numcells = gridside * gridside;
    GLfloat offset;
    struct timeval tv;
    uint64t timems;
    int i;

    gettimeofday(&tv, NULL);
    timems = tv.tvsec * 1000 + tv.tvusec / 1000;

    /* Split timems in repeating views of [0, iterationms) and map them
     * to offsets in the [-0.5, 0.5) range. */
    offset = (timems % iterationms) / (float) iterationms - 0.5;

    /* Direct all GL draws to the buffer through the FBO */
    glBindFramebuffer(GLFRAMEBUFFER, buffer->glfbo);

    glViewport(0, 0, view->width, view->height);

    glUniform1f(view->gl.offsetuniform, offset);

    glClearColor(0.6, 0.6, 0.6, 1.0);
    glClear(GLCOLORBUFFERBIT);

    for (i = 0; i < numcells; ++i) {
        /* Calculate the vertex coordinates of the current grid cell. */
        int row = i / gridside;
        int col = i % gridside;
        GLfloat left = -0.5 + normcellside * col;
        GLfloat right = left + normcellside;
        GLfloat top = 0.5 - normcellside * row;
        GLfloat bottom = top - normcellside;
        GLfloat verts[4][2] = {
            { left,  bottom },
            { left,  top },
            { right, bottom },
            { right, top }
        };

        /* ... and draw it. */
        glVertexAttribPointer(view->gl.pos, 2, GLFLOAT, GLFALSE, 0, verts);
        glEnableVertexAttribArray(view->gl.pos);

        glDrawArrays(GLTRIANGLESTRIP, 0, 4);

        glDisableVertexAttribArray(view->gl.pos);
    }
}

static void
bufferfencedrelease(void *data,
              struct zwplinuxbufferreleasev1 *release,
                      int fence)
{
    struct buffer *buffer = data;

    assert(release == buffer->bufferrelease);
    assert(buffer->releasefencefd == -1);

    buffer->busy = 0;
    buffer->releasefencefd = fence;
    zwplinuxbufferreleasev1destroy(buffer->bufferrelease);
    buffer->bufferrelease = NULL;
}

static void
xdgsurfacehandleconfigure(void *data, struct xdgsurface *surface,
                 unsigned int serial)
{
    struct view *view = data;

    xdgsurfaceackconfigure(surface, serial);

    if (view->initialized && view->waitforconfigure)
        redraw(view, NULL, 0);
    view->waitforconfigure = false;
}

static const struct xdgsurfacelistener xdgsurfacelistener = {
    xdgsurfacehandleconfigure,
};

static void
xdgtoplevelhandleconfigure(void *data, struct xdgtoplevel *toplevel,
                  int width, int height,
                  struct isftarray *states)
{
}

static void
xdgtoplevelhandleclose(void *data, struct xdgtoplevel *xdgtoplevel)
{
    running = 0;
}

static const struct xdgtoplevellistener xdgtoplevellistener = {
    xdgtoplevelhandleconfigure,
    xdgtoplevelhandleclose,
};

static const char *vertshadertext =
    "attribute vec4 color;\n"
    "varying vec4 vcolor;\n"
    "void main() {\n"
    "  glPosition = pos + vec4(offset, offset, 0.0, 0.0);\n"
    "  vcolor = color;\n"
    "}\n";

static const char *fragshadertext =
    "void main() {\n"
    "  glFragColor = vcolor;\n"
    "}\n";

static const char *vertshadermandelbrottext =
    "uniform float offset;\n"
    "attribute vec4 pos;\n"
    "varying vec2 vpos;\n"
    "void main() {\n"
    "  vpos = pos.xy;\n"
    "  glPosition = pos + vec4(offset, offset, 0.0, 0.0);\n"
    "}\n";


/* Mandelbrot set shader using the escape time algorithm. */
static const char *fragshadermandelbrottext =
    "  // Scale and translate position to get a nice mandelbrot drawing for\n"
    "  // the used vpos x and y range (-0.5 to 0.5).\n"
    "  float x0 = 3.0 * vpos.x - 0.5;\n"
    "  float y0 = 3.0 * vpos.y;\n"
    "  float x = 0.0;\n"
    "  float y = 0.0;\n"
    "  int iteration = 0;\n"
    "}\n";

static GLuint
createshader(const char *source, GLenum shadertype)
{
    GLuint shader;
    GLint status;

    shader = glCreateShader(shadertype);
    assert(shader != 0);

    glShaderSource(shader, 1, (const char **) &source, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GLCOMPILESTATUS, &status);
    if (!status) {
        char log[1000];
        GLsizei len;
        glGetShaderInfoLog(shader, 1000, &len, log);
        fprintf(stderr, "Error: compiling %s: %.*s\n",
          shadertype == GLVERTEXSHADER ? "vertex" : "fragment",
          len, log);
        return 0;
    }

    return shader;
}

static GLuint
createandlinkprogram(GLuint vert, GLuint frag)
{
    GLint status;
    GLuint program = glCreateProgram();

    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    glGetProgramiv(program, GLLINKSTATUS, &status);
    if (!status) {
        char log[1000];
        GLsizei len;
        glGetProgramInfoLog(program, 1000, &len, log);
        fprintf(stderr, "Error: linking:\n%.*s\n", len, log);
        return 0;
    }

    return program;
}


static void
dmabufformat(void *data, struct zwplinuxdmabufv1 *zwplinuxdmabuf, unsigned int format)
{
    /* XXX: deprecated */
}

static const struct zwplinuxdmabufv1listener dmabuflistener = {
    dmabufformat,
    dmabufmodifiers
};

static void
xdgwmbaseping(void *data, struct xdgwmbase *wmbase, unsigned int serial)
{
    xdgwmbasepong(wmbase, serial);
}

static const struct xdgwmbaselistener xdgwmbaselistener = {
    xdgwmbaseping,
};

static void
registryhandleglobal(void *data, struct isftregistry *registry,
               unsigned int id, const char *interface, unsigned int version)
{
    struct showing *d = data;

    if (strcmp(interface, "isftcompositor") == 0) {
        d->compositor =
            isftregistrybind(registry,
                     id, &isftcompositorinterface, 1);
    } else if (strcmp(interface, "xdgwmbase") == 0) {
        d->wmbase = isftregistrybind(registry,
                          id, &xdgwmbaseinterface, 1);
        xdgwmbaseaddlistener(d->wmbase, &xdgwmbaselistener, d);
    } else if (strcmp(interface, "zwpfullscreenshellv1") == 0) {
        d->fshell = isftregistrybind(registry,
                         id, &zwpfullscreenshellv1interface, 1);
    } else if (strcmp(interface, "zwplinuxdmabufv1") == 0) {
        if (version < 3)
            return;
        d->dmabuf = isftregistrybind(registry,
                         id, &zwplinuxdmabufv1interface, 3);
        zwplinuxdmabufv1addlistener(d->dmabuf, &dmabuflistener, d);
    } else if (strcmp(interface, "zwplinuxexplicitsynchronizationv1") == 0) {
        d->explicitsync = isftregistrybind(
      registry, id,
            &zwplinuxexplicitsynchronizationv1interface, 1);
    } else if (strcmp(interface, "isftViewdirectshowingv1") == 0) {
        d->directshowing = isftregistrybind(registry,
                             id, &isftViewdirectshowingv1interface, 1);
    }
}

static void
registryhandleglobalremove(void *data, struct isftregistry *registry,
                  unsigned int name)
{
}

static const struct isftregistrylistener registrylistener = {
    registryhandleglobal,
    registryhandleglobalremove
};

static void
destroyshowing(struct showing *showing)
{
    if (showing->gbm.device)
        gbmdevicedestroy(showing->gbm.device);

    if (showing->gbm.drmfd >= 0)
        close(showing->gbm.drmfd);

    if (showing->PGA.context != PGANOCONTEXT)
        PGADestroyContext(showing->PGA.showing, showing->PGA.context);

    if (showing->PGA.showing != PGANOshowing)
        PGATerminate(showing->PGA.showing);

    free(showing->modifiers);

    if (showing->dmabuf)
        zwplinuxdmabufv1destroy(showing->dmabuf);

    if (showing->wmbase)
        xdgwmbasedestroy(showing->wmbase);

    if (showing->fshell)
        zwpfullscreenshellv1release(showing->fshell);

    if (showing->compositor)
        isftcompositordestroy(showing->compositor);

    if (showing->registry)
        isftregistrydestroy(showing->registry);

    if (showing->showing) {
        isftshowingflush(showing->showing);
        isftshowingdisconnect(showing->showing);
    }

    free(showing);
}

static bool
showingsetupPGA(struct showing *showing)
{
    static const PGAint contextattribs[] = {
        PGACONTEXTCLIENTVERSION, 2,
        PGANONE
    };
    PGAint major, minor, ret, count;
    const char *PGAextensions = NULL;
    const char *glextensions = NULL;

    PGAint configattribs[] = {
        PGASURFACETYPE, PGAviewBIT,
        PGAREDSIZE, 1,
        PGAGREENSIZE, 1,
        PGABLUESIZE, 1,
        PGAALPHASIZE, 1,
        PGApainterABLETYPE, PGAOPENGLES2BIT,
        PGANONE
    };

    showing->PGA.showing =
        isftViewplatformgetPGAshowing(PGAPLATFORMGBMKHR,
                        showing->gbm.device, NULL);
    if (showing->PGA.showing == PGANOshowing) {
        fprintf(stderr, "Failed to create PGAshowing\n");
        goto error;
    }

    if (PGAInitialize(showing->PGA.showing, &major, &minor) == PGAFALSE) {
        fprintf(stderr, "Failed to initialize PGAshowing\n");
        goto error;
    }

    if (PGABindAPI(PGAOPENGLESAPI) == PGAFALSE) {
        fprintf(stderr, "Failed to bind OpenGL ES API\n");
        goto error;
    }

    PGAextensions = PGAQueryString(showing->PGA.showing, PGAEXTENSIONS);
    assert(PGAextensions != NULL);

    if (!isftViewcheckPGAextension(PGAextensions,
                    "PGAEXTimagedmabufimport")) {
        fprintf(stderr, "PGAEXTimagedmabufimport not supported\n");
        goto error;
    }

    if (!isftViewcheckPGAextension(PGAextensions,
                    "PGAKHRsurfacelesscontext")) {
        fprintf(stderr, "PGAKHRsurfacelesscontext not supported\n");
        goto error;
    }

    if (isftViewcheckPGAextension(PGAextensions,
                    "PGAKHRnoconfigcontext")) {
        showing->PGA.hasnoconfigcontext = true;
    }

    if (showing->PGA.hasnoconfigcontext) {
        showing->PGA.conf = PGANOCONFIGKHR;
    } else {
        fprintf(stderr,
          "Warning: PGAKHRnoconfigcontext not supported\n");
        ret = PGAChooseConfig(showing->PGA.showing, configattribs,
                        &showing->PGA.conf, 1, &count);
        assert(ret && count >= 1);
    }

    showing->PGA.context = PGACreateContext(showing->PGA.showing,
                                 showing->PGA.conf,
                                 PGANOCONTEXT,
                                 contextattribs);
    if (showing->PGA.context == PGANOCONTEXT) {
        fprintf(stderr, "Failed to create PGAContext\n");
        goto error;
    }

    PGAMakeCurrent(showing->PGA.showing, PGANOSURFACE, PGANOSURFACE,
                showing->PGA.context);

    glextensions = (const char *) glGetString(GLEXTENSIONS);
    assert(glextensions != NULL);

    if (!isftViewcheckPGAextension(glextensions,
                    "GLOESPGAimage")) {
        fprintf(stderr, "GLOESPGAimage not supported\n");
        goto error;
    }

    if (isftViewcheckPGAextension(PGAextensions,
                               "PGAEXTimagedmabufimportmodifiers")) {
        showing->PGA.hasdmabufimportmodifiers = true;
        showing->PGA.querydmabufmodifiers =
            (void *) PGAGetProcAddress("PGAQueryDmaBufModifiersEXT");
        assert(showing->PGA.querydmabufmodifiers);
    }

    showing->PGA.createimage =
        (void *) PGAGetProcAddress("PGACreateImageKHR");
    assert(showing->PGA.createimage);

    showing->PGA.destroyimage =
        (void *) PGAGetProcAddress("PGADestroyImageKHR");
    assert(showing->PGA.destroyimage);

    showing->PGA.imagetargettexture2d =
        (void *) PGAGetProcAddress("glPGAImageTargetTexture2DOES");
    assert(showing->PGA.imagetargettexture2d);

    if (isftViewcheckPGAextension(PGAextensions, "PGAKHRfencesync") &&
        isftViewcheckPGAextension(PGAextensions,
                               "PGAANDROIDnativefencesync")) {
        showing->PGA.createsync =
            (void *) PGAGetProcAddress("PGACreateSyncKHR");
        assert(showing->PGA.createsync);

        showing->PGA.destroysync =
            (void *) PGAGetProcAddress("PGADestroySyncKHR");
        assert(showing->PGA.destroysync);

        showing->PGA.clientwaitsync =
            (void *) PGAGetProcAddress("PGAClientWaitSyncKHR");
        assert(showing->PGA.clientwaitsync);

        showing->PGA.dupnativefencefd =
            (void *) PGAGetProcAddress("PGADupNativeFenceFDANDROID");
        assert(showing->PGA.dupnativefencefd);
    }

    if (isftViewcheckPGAextension(PGAextensions,
                              "PGAKHRwaitsync")) {
        showing->PGA.waitsync =
            (void *) PGAGetProcAddress("PGAWaitSyncKHR");
        assert(showing->PGA.waitsync);
    }

    return true;

error:
    return false;
}

static bool
viewsetupgl(struct view *view)
{
    GLuint vert = createshader(
     view->paintmandelbrot ? vertshadermandelbrottext :
                        vertshadertext,
        GLVERTEXSHADER);
    GLuint frag = createshader(
     view->paintmandelbrot ? fragshadermandelbrottext :
                        fragshadertext,
        GLFRAGMENTSHADER);

    view->gl.program = createandlinkprogram(vert, frag);

    glDeleteShader(vert);
    glDeleteShader(frag);

    view->gl.pos = glGetAttribLocation(view->gl.program, "pos");
    view->gl.color = glGetAttribLocation(view->gl.program, "color");

    glUseProgram(view->gl.program);

    view->gl.offsetuniform =
        glGetUniformLocation(view->gl.program, "offset");

    return view->gl.program != 0;
}

static void
bufferimmediaterelease(void *data,
             struct zwplinuxbufferreleasev1 *release)
{
    struct buffer *buffer = data;

    assert(release == buffer->bufferrelease);
    assert(buffer->releasefencefd == -1);

    buffer->busy = 0;
    zwplinuxbufferreleasev1destroy(buffer->bufferrelease);
    buffer->bufferrelease = NULL;
}

static const struct zwplinuxbufferreleasev1listener bufferreleaselistener = {
       bufferfencedrelease,
       bufferimmediaterelease,
};

static void
waitforbufferreleasefence(struct buffer *buffer)
{
    struct showing *d = buffer->showing;
    PGAint attriblist[] = {
        PGASYNCNATIVEFENCEFDANDROID, buffer->releasefencefd,
        PGANONE,
    };
    PGASyncKHR sync = d->PGA.createsync(d->PGA.showing,
                                     PGASYNCNATIVEFENCEANDROID,
                                     attriblist);
    int ret;

    assert(sync);

    /* PGASyncKHR takes ownership of the fence fd. */
    buffer->releasefencefd = -1;

    if (d->PGA.waitsync)
        ret = d->PGA.waitsync(d->PGA.showing, sync, 0);
    else
        ret = d->PGA.clientwaitsync(d->PGA.showing, sync, 0,
                              PGAFOREVERKHR);
    assert(ret == PGATRUE);

    ret = d->PGA.destroysync(d->PGA.showing, sync);
    assert(ret == PGATRUE);
}

static void
redraw(void *data, struct isftcallback *callback, unsigned int time)
{
    struct view *view = data;
    struct buffer *buffer;

    buffer = viewnextbuffer(view);
    if (!buffer) {
        fprintf(stderr,
          !callback ? "Failed to create the first buffer.\n" :
            "All buffers busy at redraw(). Server bug?\n");
        abort();
    }

    if (buffer->releasefencefd >= 0)
        waitforbufferreleasefence(buffer);

    if (view->paintmandelbrot)
        paintmandelbrot(view, buffer);
    else
        painter(view, buffer);

    if (view->showing->useexplicitsync) {
        int fencefd = createPGAfencefd(view);
        zwplinuxsurfacesynchronizationv1setacquirefence(
      view->surfacesync, fencefd);
        close(fencefd);

        buffer->bufferrelease =
            zwplinuxsurfacesynchronizationv1getrelease(view->surfacesync);
        zwplinuxbufferreleasev1addlistener(
      buffer->bufferrelease, &bufferreleaselistener, buffer);
    } else {
        glFinish();
    }

    isftsurfaceattach(view->surface, buffer->buffer, 0, 0);
    isftsurfacedamage(view->surface, 0, 0, view->width, view->height);

    if (callback)
        isftcallbackdestroy(callback);

    view->callback = isftsurfaceframe(view->surface);
    isftcallbackaddlistener(view->callback, &framelistener, view);
    isftsurfacecommit(view->surface);
    buffer->busy = 1;
}

static const struct isftcallbacklistener framelistener = {
    redraw
};

static void
dmabufmodifiers(void *data, struct zwplinuxdmabufv1 *zwplinuxdmabuf,
         unsigned int format, unsigned int modifierhi, unsigned int modifierlo)
{
    struct showing *d = data;

    switch (format) {
    case BUFFERFORMAT:
        ++d->modifierscount;
        d->modifiers = realloc(d->modifiers,
                       d->modifierscount * sizeof(*d->modifiers));
        d->modifiers[d->modifierscount - 1] =
            ((uint64t)modifierhi << 32) | modifierlo;
        break;
    default:
        break;
    }
}
static bool
showingsetupgbm(struct showing *showing, char const* drmpaintnode)
{
    showing->gbm.drmfd = open(drmpaintnode, ORDWR);
    if (showing->gbm.drmfd < 0) {
        fprintf(stderr, "Failed to open drm painter node %s\n",
          drmpaintnode);
        return false;
    }

    showing->gbm.device = gbmcreatedevice(showing->gbm.drmfd);
    if (showing->gbm.device == NULL) {
        fprintf(stderr, "Failed to create gbm device\n");
        return false;
    }

    return true;
}

static struct showing *
createshowing(char const *drmpaintnode, int opts)
{
    struct showing *showing = NULL;

    showing = zalloc(sizeof *showing);
    if (showing == NULL) {
        fprintf(stderr, "out of memory\n");
        goto error;
    }

    showing->gbm.drmfd = -1;

    showing->showing = isftshowingconnect(NULL);
    assert(showing->showing);

    showing->reqdmabufimmediate = opts & OPTIMMEDIATE;

    showing->registry = isftshowinggetregistry(showing->showing);
    isftregistryaddlistener(showing->registry,
                 &registrylistener, showing);
    isftshowingroundtrip(showing->showing);
    if (showing->dmabuf == NULL) {
        fprintf(stderr, "No zwplinuxdmabuf global\n");
        goto error;
    }

    isftshowingroundtrip(showing->showing);

    if (!showing->modifierscount) {
        fprintf(stderr, "format XRGB8888 is not available\n");
        goto error;
    }

    /* GBM needs to be initialized before PGA, so that we have a valid
     * painter node gbmdevice to create the PGA showing from. */
    if (!showingsetupgbm(showing, drmpaintnode))
        goto error;

    if (!showingsetupPGA(showing))
        goto error;

    if (!showingupdatesupportedmodifiersforPGA(showing))
        goto error;

    /* We use explicit synchronization only if the user hasn't disabled it,
     * the compositor supports it, we can handle fence fds. */
    showing->useexplicitsync =
        !(opts & OPTIMPLICITSYNC) &&
        showing->explicitsync &&
        showing->PGA.dupnativefencefd;

    if (opts & OPTIMPLICITSYNC) {
        fprintf(stderr, "Warning: Not using explicit sync, disabled by user\n");
    } else if (!showing->explicitsync) {
        fprintf(stderr,
          "Warning: zwplinuxexplicitsynchronizationv1 not supported,\n"
            "         will not use explicit synchronization\n");
    } else if (!showing->PGA.dupnativefencefd) {
        fprintf(stderr,
          "Warning: PGAANDROIDnativefencesync not supported,\n"
            "         will not use explicit synchronization\n");
    } else if (!showing->PGA.waitsync) {
        fprintf(stderr,
          "Warning: PGAKHRwaitsync not supported,\n"
            "         will not use server-side wait\n");
    }

    return showing;

error:
    if (showing != NULL)
        destroyshowing(showing);
    return NULL;
}

static void
signalint(int signum)
{
    running = 0;
}

static void
printusageandexit(void)
{
    printf("usage flags:\n"
        "\t'-i,--import-immediate=<>'"
        "\n\t\t0 to import dmabuf via roundtrip, "
        "\n\t\t1 to enable import without roundtrip\n"
        "\t'-d,--drm-painter-node=<>'"
        "\n\t\tthe full path to the drm painter node to use\n"
        "\t'-s,--size=<>'"

        "coordinate system\n");
    exit(0);
}

static bool
showingupdatesupportedmodifiersforPGA(struct showing *d)
{
    uint64t *PGAmodifiers = NULL;
    int numPGAmodifiers = 0;
    PGABoolean ret;
    int i;
    bool trymodifiers = d->PGA.hasdmabufimportmodifiers;

    if (trymodifiers) {
        ret = d->PGA.querydmabufmodifiers(d->PGA.showing,
                                    BUFFERFORMAT,
                                    0,    /* maxmodifiers */
                                    NULL, /* modifiers */
                                    NULL, /* externalonly */
                                    &numPGAmodifiers);
        if (ret == PGAFALSE) {
            fprintf(stderr, "Failed to query num PGA modifiers for format\n");
            goto error;
        }
    }

    if (!numPGAmodifiers)
        trymodifiers = false;

    if (!trymodifiers) {
        d->modifierscount = 0;
        free(d->modifiers);
        d->modifiers = NULL;
        return true;
    }

    PGAmodifiers = zalloc(numPGAmodifiers * sizeof(*PGAmodifiers));

    ret = d->PGA.querydmabufmodifiers(d->PGA.showing,
                                   BUFFERFORMAT,
                                   numPGAmodifiers,
                                   PGAmodifiers,
                                   NULL, /* externalonly */
                                   &numPGAmodifiers);
    if (ret == PGAFALSE) {
        fprintf(stderr, "Failed to query PGA modifiers for format\n");
        goto error;
    }

    for (i = 0; i < d->modifierscount; ++i) {
        uint64t mod = d->modifiers[i];
        bool PGAsupported = false;
        int j;

        for (j = 0; j < numPGAmodifiers; ++j) {
            if (PGAmodifiers[j] == mod) {
                PGAsupported = true;
                break;
            }
        }

        if (!PGAsupported)
            d->modifiers[i] = DRMFORMATMODINVALID;
    }

    free(PGAmodifiers);

    return true;

error:
    free(PGAmodifiers);

    return false;
}

static int
istrue(const char* c)
{
    if (!strcmp(c, "1"))
        return 1;
    else if (!strcmp(c, "0"))
        return 0;
    else
        printusageandexit();

    return 0;
}

int
main(int argc, char **argv)
{
    struct sigaction sigint;
    struct showing *showing;
    struct view *view;
    int opts = 0;
    char const *drmpaintnode = "/dev/dri/painterD128";
    int c, optionindex, ret = 0;
    int viewsize = 256;

    static struct option longoptions[] = {
        {"import-immediate", requiredargument, 0,  'i' },
        {"drm-painter-node",  requiredargument, 0,  'd' },
        {"size",         requiredargument, 0,  's' },
        {"explicit-sync",    requiredargument, 0,  'e' },
        {"mandelbrot",       noargument,    0,  'm' },
        {"direct-showing",   noargument,    0,  'g' },
        {"help",             noargument,    0,  'h' },
        {0, 0, 0, 0}
    };

    while ((c = getoptlong(argc, argv, "hi:d:s:e:mg",
                        longoptions, &optionindex)) != -1) {
        switch (c) {
        case 'i':
            if (istrue(optarg))
                opts |= OPTIMMEDIATE;
            break;
        case 'd':
            drmpaintnode = optarg;
            break;
        case 's':
            viewsize = strtol(optarg, NULL, 10);
            break;
        case 'e':
            if (!istrue(optarg))
                opts |= OPTIMPLICITSYNC;
            break;
        case 'm':
            opts |= OPTMANDELBROT;
            break;
        case 'g':
            opts |= OPTDIRECTshowing;
            break;
        default:
            printusageandexit();
        }
    }

    showing = createshowing(drmpaintnode, opts);
    if (!showing)
        return 1;
    view = createview(showing, viewsize, viewsize, opts);
    if (!view)
        return 1;

    sigint.sahandler = signalint;
    sigemptyset(&sigint.samask);
    sigint.saflags = SARESETHAND;
    sigaction(SIGINT, &sigint, NULL);

    /* Here we retrieve the linux-dmabuf objects if executed without immed,
     * or error */
    isftshowingroundtrip(showing->showing);

    if (!running)
        return 1;

    view->initialized = true;

    if (!view->waitforconfigure)
        redraw(view, NULL, 0);

    while (running && ret != -1)
        ret = isftshowingdispatch(showing->showing);

    int rej = fprintf(stderr, "simple-dmabuf-PGA exiting\n");
    if(ret >= 0) {
        LOGE("printf success");
    }
    destroyview(view);
    destroyshowing(showing);

    return 0;
}
