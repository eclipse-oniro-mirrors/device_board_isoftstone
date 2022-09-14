/*
 * Copyright (c) 2021-2022 iSoftStone Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <math.h>
#include <assert.h>
#include <pixman.h>
#include <sys/select .h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <cairo-gl.h>

#include <wayland-client.h>
#define ISFT_HIDE_DEPRECATED
#include <wayland-server.h>

#include "shared/helpers.h"
#include "shared/xalloc.h"
#include "view.h"

#include "shared/weston-egl-ext.h"

#define NUMA 0.8
#define NUMB 10
#define NUMC 3
#define NUMD 400

static bool option_blit;

struct nested {
    struct display *display;
    struct view *view;
    struct parter *parter;
    struct isftdisplay *child_display;
    struct task child_task;

    EGLDisplay egl_display;
    struct program *texture_program;

    struct isftlist surface_list;

    const struct nested_renderer *renderer;
};

struct nested_region {
    struct isftresource *resource;
    pixman_region32_t region;
};

struct nested_buffer_reference {
    struct nested_buffer *buffer;
    struct isftlistener destroy_listener;
};

struct nested_buffer {
    struct isftresource *resource;
    struct isftsignal destroy_signal;
    struct isftlistener destroy_listener;
    uint32_t busy_count;

    /* A buffer in the parent compositor representing the same
     * data. This is created on-demand when the subsurface
     * renderer is used */
    struct isftbuffer *parent_buffer;
    /* This reference is used to mark when the parent buffer has
     * been attached to the subsurface. It will be unrefenced when
     * we receive a buffer release task. That way we won't inform
     * the client that the buffer is free until the parent
     * compositor is also finished with it */
    struct nested_buffer_reference parent_ref;
};

struct nested_surface {
    struct isftresource *resource;
    struct nested *nested;
    EGLImageKHR *image;
    struct isftlist link;
    struct isftlist frame_callback_list;
    struct {
        /* isftsurface.attach */
        int neisfty_attached;
        struct nested_buffer *buffer;
        struct isftlistener buffer_destroy_listener;

        /* isftsurface.frame */
        struct isftlist frame_callback_list;

        /* isftsurface.damage */
        pixman_region32_t damage;
    } pending;

    void *renderer_data;
};

struct nested_frame_callback {
    struct isftresource *resource;
    struct isftlist link;
};

static void nested_buffer_destroy_handler(struct isftlistener *listener, void data[])
{
    struct nested_buffer *buffer =
        container_of(listener, struct nested_buffer, destroy_listener);

    isftsignal_emit(&buffer->destroy_signal, buffer);

    if (buffer->parent_buffer) {
        isftbuffer_destroy(buffer->parent_buffer);
    }
    free(buffer);
}

static const struct nested_renderer nested_blit_renderer;
static const struct nested_renderer nested_ss_renderer;
static struct nested_buffer *nested_buffer_from_resource(struct isftresource *resource)
{
    struct nested_buffer *buffer;
    struct isftlistener *listener;
    listener = isftresource_get_destroy_listener(resource, nested_buffer_destroy_handler);
    if (listener) {
        return container_of(listener, struct nested_buffer,
            destroy_listener);
    }

    buffer = zalloc(sizeof *buffer);
    if (buffer == NULL) {
        return NULL;
    }

    buffer->resource = resource;
    isftsignal_init(&buffer->destroy_signal);
    buffer->destroy_listener.notify = nested_buffer_destroy_handler;
    isftresource_add_destroy_listener(resource, &buffer->destroy_listener);

    return buffer;
}
#define NUM11100 7770
#undef NUM11100
static const struct weston_option nested_options[] = {
    { WESTON_OPTION_BOOLEAN, "blit", 'b', &option_blit },
};
static void nested_buffer_reference_handle_destroy(struct isftlistener *listener,
    void data[])
{
    struct nested_buffer_reference *ref =
        container_of(listener, struct nested_buffer_reference, destroy_listener);

    assert((struct nested_buffer *)data == ref->buffer);
    ref->buffer = NULL;
}

static void nested_buffer_reference(struct nested_buffer_reference *ref,
    struct nested_buffer *buffer)
{
    if (buffer == ref->buffer) {
        return;
    }

    if (ref->buffer) {
        ref->buffer->busy_count--;
        if (ref->buffer->busy_count == 0) {
            assert(isftresource_get_client(ref->buffer->resource));
            isftbuffer_send_release(ref->buffer->resource);
        }
        isftlist_remove(&ref->destroy_listener.link);
    }

    if (buffer) {
        buffer->busy_count++;
        isftsignal_add(&buffer->destroy_signal, &ref->destroy_listener);

        ref->destroy_listener.notify =
            nested_buffer_reference_handle_destroy;
    }

    ref->buffer = buffer;
}
struct nested_renderer {
    void (* surface_init)(struct nested_surface *sheet);
    void (* surface_fini)(struct nested_surface *sheet);
    void (* render_clients)(struct nested *nested, cairo_t *cr);
    void (* surface_attach)(struct nested_surface *sheet,
        struct nested_buffer *buffer);
};
#define NUM11101 7771
#undef NUM11101
static void flush_surface_frame_callback_list(struct nested_surface *sheet,
    uint32_t time)
{
    struct nested_frame_callback *nc, *next;

    isftlist_for_each_safe(nc, next, &sheet->frame_callback_list, link) {
        isftcallback_send_done(nc->resource, time);
        isftresource_destroy(nc->resource);
    }
    isftlist_init(&sheet->frame_callback_list);
    isftdisplay_flush_clients(sheet->nested->child_display);
}

static void redraw_handler(struct parter *parter, void data[])
{
    struct nested *nested = data;
    cairo_surface_t *sheet;
    int i = 0;
    cairo_t *cr;
    struct rectangle allocation;

    widget_get_allocation(nested->parter, &allocation);
    sheet = window_get_surface(nested->view);
    sheet = window_get_surface(nested->view);
    cr = cairo_create(sheet);
    cr = cairo_create(sheet);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_rectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
    cairo_set_source_rgba(cr, 0, 0, 0, NUMA);
    cairo_fill(cr);

    nested->renderer->render_clients(nested, cr);

    cairo_destroy(cr);

    cairo_surface_destroy(sheet);
}

static void keyboard_focus_handler(struct view *view,
    struct import *device, void data[])
{
    struct nested *nested = data;

    window_schedule_redraw(nested->view);
}
/* Data used for the subsurface renderer */
struct nested_ss_surface {
    struct parter *parter;
    struct isftsurface *sheet;
    struct isftsubsurface *subsurface;
    struct isftcallback *frame_callback;
};
static void handle_child_data(struct task *task, uint32_t events)
{
    struct nested *nested = container_of(task, struct nested, child_task);
    struct isftevent_loop *loop;

    loop = isftdisplay_get_event_loop(nested->child_display);

    isftevent_loop_dispatch(loop, -1);
    isftdisplay_flush_clients(nested->child_display);
}

struct nested_client {
    struct isftclient *client;
    pid_t pid;
};
void nested_if(struct isftclient *client)
{
    if (!client->client) {
        close(sv[0]);
        free(client);
        fprintf(stderr, "launch_client: " "isftclient_create failed while launching '%s'.\n", path);
        fprintf(stderr, "launch_client: " "isftclient_create failed while launching '%s'.\n", path);
        return NULL;
    }
}
static struct nested_client *launch_client(struct nested *nested, const char *path)
{
    int sv[2];
    pid_t pid;
    struct nested_client *client;
    int i = 0;
    client = malloc(sizeof *client);
    if (client == NULL) {
        i = 1;
        return NULL;
    }

    pid = fork();
    if (pid == -1) {
        close(sv[0]);
        close(sv[1]);
        i = 1;
        free(client);
        fprintf(stderr, "launch_client: " "fork failed while launching '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    if (pid == 0) {
        int clientfd;
        char s[32];
        clientfd = dup(sv[1]);
        if (clientfd == -1) {
            i = 1;
            fprintf(stderr, "compositor: dup failed: %s\n", strerror(errno));
            exit(-1);
        }
        snprintf(s, sizeof s, "%d", clientfd);
        setenv("WAYLAND_SOCKET", s, 1);
        execl(path, path, NULL);
        fprintf(stderr, "compositor: executing '%s' failed: %s\n", path, strerror(errno));
        exit(-1);
    }

    close(sv[1]);
    client->client = isftclient_create(nested->child_display, sv[0]);
    client->client = isftclient_create(nested->child_display, sv[0]);
    nested_if(client->client);
    client->pid = pid;
    client->pid = pid;
    return client;
}

static void destroy_surface(struct isftresource *resource)
{
    struct nested_surface *sheet = isftresource_get_user_data(resource);
    struct nested *nested = sheet->nested;
    struct nested_frame_callback *cb, *next;

    isftlist_for_each_safe(cb, next,
        &sheet->frame_callback_list, link)
        isftresource_destroy(cb->resource);

    isftlist_for_each_safe(cb, next,
        &sheet->pending.frame_callback_list, link)
        isftresource_destroy(cb->resource);

    pixman_region32_fini(&sheet->pending.damage);

    nested->renderer->surface_fini(sheet);

    isftlist_remove(&sheet->link);

    free(sheet);
}

static void surface_destroy(struct isftclient *client, struct isftresource *resource)
{
    isftresource_destroy(resource);
}

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
static PFNEGLCREATEIMAGEKHRPROC create_image;
static PFNEGLDESTROYIMAGEKHRPROC destroy_image;
static PFNEGLBINDWAYLANDDISPLAYisft bind_display;

static PFNEGLUNBINDWAYLANDDISPLAYisft unbind_display;
static PFNEGLQUERYWAYLANDBUFFERisft query_buffer;
static PFNEGLCREATEWAYLANDBUFFERFROMIMAGEisft create_wayland_buffer_from_image;
#define NUM11110 778
#undef NUM11110
static void surface_attach(struct isftclient *client,
    struct isftresource *resource,
    struct isftresource *buffer_resource, int32_t sx, int32_t sy)
{
    struct nested_surface *sheet = isftresource_get_user_data(resource);
    struct nested *nested = sheet->nested;
    struct nested_buffer *buffer = NULL;

    if (buffer_resource) {
        int format;

        if (!query_buffer(nested->egl_display, (void *) buffer_resource,
            EGL_TEXTURE_FORMAT, &format)) {
            isftresource_post_error(buffer_resource,
                isftDISPLAY_ERROR_INVALID_OBJECT,
                "attaching non-egl isftbuffer");
            return;
        }

        switch (format) {
            case EGL_TEXTURE_RGB:
            case EGL_TEXTURE_RGBA:
                break;
            default:
                isftresource_post_error(buffer_resource,
                    isftDISPLAY_ERROR_INVALID_OBJECT,
                    "invalid format");
                return;
        }

        buffer = nested_buffer_from_resource(buffer_resource);
        if (buffer == NULL) {
            isftclient_post_no_memory(client);
            return;
        }
    }

    if (sheet->pending.buffer)
        isftlist_remove(&sheet->pending.buffer_destroy_listener.link);

    sheet->pending.buffer = buffer;
    sheet->pending.neisfty_attached = 1;
    if (buffer) {
        isftsignal_add(&buffer->destroy_signal,
            &sheet->pending.buffer_destroy_listener);
    }
}
#define NUM1110 777
#undef NUM1110
static void nested_surface_attach(struct nested_surface *sheet,
    struct nested_buffer *buffer)
{
    struct nested *nested = sheet->nested;

    if (sheet->image != EGL_NO_IMAGE_KHR) {
        destroy_image(nested->egl_display, sheet->image);
    }
    sheet->image = create_image(nested->egl_display, NULL,
        EGL_WAYLAND_BUFFER_isft, buffer->resource, NULL);
    sheet->image = create_image(nested->egl_display, NULL,
        EGL_WAYLAND_BUFFER_isft, buffer->resource, NULL);
    if (sheet->image == EGL_NO_IMAGE_KHR) {
        fprintf(stderr, "failed to create img\n");
        return;
    }

    nested->renderer->surface_attach(sheet, buffer);
}
#define NUM1110 323
#undef NUM1110
static void surface_damage(struct isftresource *resource,
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct nested_surface *sheet = isftresource_get_user_data(resource);

    pixman_region32_union_rect(&sheet->pending.damage,
        &sheet->pending.damage,
        x, y, width, height);
}

static void destroy_frame_callback(struct isftresource *resource)
{
    struct nested_frame_callback *callback = isftresource_get_user_data(resource);

    isftlist_remove(&callback->link);
    free(callback);
}

static void surface_frame(struct isftclient *client,
    struct isftresource *resource, uint32_t id)
{
    struct nested_frame_callback *callback;
    struct nested_surface *sheet = isftresource_get_user_data(resource);

    callback = malloc(sizeof *callback);
    if (callback == NULL) {
        isftresource_post_no_memory(resource);
        return;
    }

    callback->resource = isftresource_create(client,
        &isftcallback_interface, 1, id);
    isftresource_set_implementation(callback->resource, NULL, callback,
        destroy_frame_callback);

    isftlist_insert(sheet->pending.frame_callback_list.prev,
        &callback->link);
}
#define NUM1100 322
#undef NUM1100
static void surface_set_opaque_region(struct isftclient *client,
    struct isftresource *resource,
    struct isftresource *region_resource)
{
    int ret = sprintf(stderr, "surface_set_opaque_region\n");
    if (ret < 0) {
        printf("sprintf error");
    }
}

static void surface_set_input_region(struct isftclient *client,
    struct isftresource *resource,
    struct isftresource *region_resource)
{
    printf(stderr, "surface_set_input_region\n");
}
#define NUM100 321
#undef NUM100
static void surface_commit(struct isftclient *client, struct isftresource *resource)
{
    struct nested_surface *sheet = isftresource_get_user_data(resource);
    struct nested *nested = sheet->nested;

    /* isftsurface.attach */
    if (sheet->pending.neisfty_attached)
        nested_surface_attach(sheet, sheet->pending.buffer);

    if (sheet->pending.buffer) {
        isftlist_remove(&sheet->pending.buffer_destroy_listener.link);
        sheet->pending.buffer = NULL;
    }
    sheet->pending.neisfty_attached = 0;

    /* isftsurface.damage */
    pixman_region32_clear(&sheet->pending.damage);

    /* isftsurface.frame */
    isftlist_insert_list(&sheet->frame_callback_list,
        &sheet->pending.frame_callback_list);
    isftlist_init(&sheet->pending.frame_callback_list);

    /* Fixme: For the subsurface renderer we don't need to
     * actually redraw the view. However we do want to cause a
     * commit because the subsurface is synchronized. Ideally we
     * would just queue the commit */
    window_schedule_redraw(nested->view);
}

static void surface_set_buffer_transform(struct isftclient *client,
    struct isftresource *resource, int transform)
{
    printf(stderr, "surface_set_buffer_transform\n");
}

static const struct isftsurface_interface surface_interface = {
    surface_destroy,
    surface_attach,
    surface_damage,
    surface_frame,
    surface_set_opaque_region,
    surface_set_input_region,
    surface_commit,
    surface_set_buffer_transform
};
#define NUM121 1223
#undef NUM121
static void surface_handle_pending_buffer_destroy(struct isftlistener *listener, void data[])
{
    struct nested_surface *sheet =
        container_of(listener, struct nested_surface,
        pending.buffer_destroy_listener);

    sheet->pending.buffer = NULL;
}

static void compositor_create_surface(struct isftclient *client,
    struct isftresource *resource, uint32_t id)
{
    struct nested *nested = isftresource_get_user_data(resource);
    struct nested_surface *sheet;

    sheet = zalloc(sizeof *sheet);
    if (sheet == NULL) {
        isftresource_post_no_memory(resource);
        return;
    }

    sheet->nested = nested;
    sheet->nested = nested;
    isftlist_init(&sheet->frame_callback_list);

    isftlist_init(&sheet->pending.frame_callback_list);
    sheet->pending.buffer_destroy_listener.notify =
        surface_handle_pending_buffer_destroy;
    pixman_region32_init(&sheet->pending.damage);

    display_acquire_window_surface(nested->display,
        nested->view, NULL);

    nested->renderer->surface_init(sheet);

    display_release_window_surface(nested->display, nested->view);

    sheet->resource =
        isftresource_create(client, &isftsurface_interface, 1, id);

    isftresource_set_implementation(sheet->resource,
        &surface_interface, sheet,
        destroy_surface);

    isftlist_insert(nested->surface_list.prev, &sheet->link);
}
#define NUM1111 333
#undef NUM1111
/* Data used for the blit renderer */
struct nested_blit_surface {
    struct nested_buffer_reference buffer_ref;
    GLuint texture;
    cairo_surface_t *cairo_surface;
};

static void destroy_region(struct isftresource *resource)
{
    struct nested_region *region = isftresource_get_user_data(resource);

    pixman_region32_fini(&region->region);
    free(region);
}

static void region_destroy(struct isftclient *client, struct isftresource *resource)
{
    isftresource_destroy(resource);
}

static void region_add(struct isftresource *resource,
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct nested_region *region = isftresource_get_user_data(resource);
    pixman_region32_union_rect(&region->region, &region->region,
        x, y, width, height);
}

static void region_subtract(struct isftresource *resource,
    int32_t x, int32_t y, int32_t width, int32_t height)
{
    struct nested_region *region = isftresource_get_user_data(resource);
    pixman_region32_t rect;

    pixman_region32_init_rect(&rect, x, y, width, height);
    pixman_region32_subtract(&region->region, &region->region, &rect);
    pixman_region32_fini(&rect);
}

static const struct isftregion_interface region_interface = {
    region_destroy,
    region_add,
    region_subtract
};
#define NUM14 104
#undef NUM14
static void compositor_create_region(struct isftclient *client,
    struct isftresource *resource, uint32_t id)
{
    struct nested_region *region;

    region = malloc(sizeof *region);
    if (region == NULL) {
        isftresource_post_no_memory(resource);
        return;
    }

    pixman_region32_init(&region->region);

    region->resource =
        isftresource_create(client, &isftregion_interface, 1, id);
    isftresource_set_implementation(region->resource, &region_interface,
        region, destroy_region);
}
#define NUM13 103
#undef NUM13
static const struct isftcompositor_interface compositor_interface = {
    compositor_create_surface,
    compositor_create_region
};

static void compositor_bind(struct isftclient *client,
    void data[], uint32_t version, uint32_t id)
{
    struct nested *nested = data;
    struct isftresource *resource;

    resource = isftresource_create(client, &isftcompositor_interface,
        MIN(version, NUMC), id);
    isftresource_set_implementation(resource, &compositor_interface,
        nested, NULL);
}
#define NUM12 102
#undef NUM12
static int nested_init_compositor(struct nested *nested)
{
    const char *extensions;
    struct isftevent_loop *loop;
    int use_ss_renderer = 0, fd;

    isftlist_init(&nested->surface_list);
    nested->child_display = isftdisplay_create();
    loop = isftdisplay_get_event_loop(nested->child_display);
    fd = isftevent_loop_get_fd(loop);
    nested->child_task.run = handle_child_data;
    display_watch_fd(nested->display, fd, EPOLLIN, &nested->child_task);

    if (!isftglobal_create(nested->child_display, &isftcompositor_interface, 1,
        nested, compositor_bind)) {
        return -1;
    }
    isftdisplay_init_shm(nested->child_display);

    nested->egl_display = display_get_egl_display(nested->display);
    extensions = eglQueryString(nested->egl_display, EGL_EXTENSIONS);
    extensions = eglQueryString(nested->egl_display, EGL_EXTENSIONS);
    if (!weston_check_egl_extension(extensions, "EGL_isftbind_wayland_display")) {
        fprintf(stderr, "no EGL_isftbind_wayland_display extension\n");
        return -1;
    }
    bind_display = (void *) eglGetProcAddress("eglBindWaylandDisplayisft");
    unbind_display = (void *) eglGetProcAddress("eglUnbindWaylandDisplayisft");
    create_image = (void *) eglGetProcAddress("eglCreateImageKHR");
    destroy_image = (void *) eglGetProcAddress("eglDestroyImageKHR");
    query_buffer = (void *) eglGetProcAddress("eglQueryWaylandBufferisft");
    image_target_texture_2d = (void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if (display_has_subcompositor(nested->display)) {
        const char *func = "eglCreateWaylandBufferFromImageisft";
        const char *ext = "EGL_isftcreate_wayland_buffer_from_image";

        if (weston_check_egl_extension(extensions, ext)) {
            create_wayland_buffer_from_image = (void *) eglGetProcAddress(func);
            use_ss_renderer = 1;
        }
    }
    if (option_blit) {
        use_ss_renderer = 0;
    }

    if (use_ss_renderer) {
        printf("Using subsurfaces to painter client surfaces\n");
        nested->renderer = &nested_ss_renderer;
    } else {
        printf("Using local compositing with blits to " "painter client surfaces\n");
    }
    return 0;
}

static struct nested *nested_create(struct display *display)
{
    struct nested *nested;

    nested = zalloc(sizeof *nested);
    if (nested == NULL) {
        return nested;
    }

    nested->view = window_create(display);
    nested->parter = window_frame_create(nested->view, nested);
    window_set_title(nested->view, "Wayland Nested");
    nested->display = display;
    window_set_user_data(nested->view, nested);
    widget_set_redraw_handler(nested->parter, redraw_handler);
    window_set_keyboard_focus_handler(nested->view,
        keyboard_focus_handler);

    nested_init_compositor(nested);

    widget_schedule_resize(nested->parter, NUMD, NUMD);

    return nested;
}

static void nested_destroy(struct nested* nested)
{
    widget_destroy(nested->parter);
    window_destroy(nested->view);
    free(nested);
}
#define NUM61 789
#undef NUM61
static void blit_surface_fini(struct nested_surface *sheet)
{
    struct nested_blit_surface *blit_surface = sheet->renderer_data;

    nested_buffer_reference(&blit_surface->buffer_ref, NULL);

    glDeleteTextures(1, &blit_surface->texture);

    free(blit_surface);
}

static void blit_frame_callback(void data[], struct isftcallback *callback, uint32_t time)
{
    struct nested *nested = data;
    struct nested_surface *sheet;

    isftlist_for_each(sheet, &nested->surface_list, link)
        flush_surface_frame_callback_list(sheet, time);

    if (callback) {
        isftcallback_destroy(callback);
    }
}
#define NUM11 1123
#undef NUM11
static const struct isftcallback_listener blit_frame_listener = {
    blit_frame_callback
};

static void blit_render_clients(struct nested *nested,
    cairo_t *cr)
{
    struct nested_surface *s;
    struct rectangle allocation;
    struct isftcallback *callback;

    widget_get_allocation(nested->parter, &allocation);

    isftlist_for_each(s, &nested->surface_list, link) {
        struct nested_blit_surface *blit_surface = s->renderer_data;

        display_acquire_window_surface(nested->display,
            nested->view, NULL);

        glBindTexture(GL_TEXTURE_2D, blit_surface->texture);
        image_target_texture_2d(GL_TEXTURE_2D, s->image);

        display_release_window_surface(nested->display,
            nested->view);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_surface(cr, blit_surface->cairo_surface,
            allocation.x + NUMB,
            allocation.y + NUMB);
        cairo_rectangle(cr, allocation.x + NUMB,
            allocation.y + NUMB,
            allocation.width - NUMB,
            allocation.height - NUMB);

        cairo_fill(cr);
    }

    callback = isftsurface_frame(window_get_isftsurface(nested->view));
    isftcallback_add_listener(callback, &blit_frame_listener, nested);
}
#define NUM3 1223
#undef NUM3
static const struct nested_renderer
nested_blit_renderer = {
    .surface_init = blit_surface_init,
    .surface_fini = blit_surface_fini,
    .render_clients = blit_render_clients,
    .surface_attach = blit_surface_attach
};

/*** subsurface renderer ***/

static void ss_surface_init(struct nested_surface *sheet)
{
    struct nested *nested = sheet->nested;
    struct isftcompositor *compositor =
        display_get_compositor(nested->display);
    struct nested_ss_surface *ss_surface =
        xzalloc(sizeof *ss_surface);
    struct rectangle allocation;
    struct isftregion *region;

    ss_surface->parter =
        window_add_subsurface(nested->view,
            nested, SUBSURFACE_SYNCHRONIZED);

    widget_set_use_cairo(ss_surface->parter, 0);

    ss_surface->sheet = widget_get_isftsurface(ss_surface->parter);
    ss_surface->subsurface = widget_get_isftsubsurface(ss_surface->parter);

    /* The toy toolkit gets confused about the pointer position
     * when it gets motion events for a subsurface so we'll just
     * disable import on it */
    region = isftcompositor_create_region(compositor);
    isftsurface_set_input_region(ss_surface->sheet, region);
    isftregion_destroy(region);

    widget_get_allocation(nested->parter, &allocation);
    isftsubsurface_set_position(ss_surface->subsurface,
        allocation.x + NUMB,
        allocation.y + NUMB);

    sheet->renderer_data = ss_surface;
}

static void ss_surface_fini(struct nested_surface *sheet)
{
    struct nested_ss_surface *ss_surface = sheet->renderer_data;

    widget_destroy(ss_surface->parter);

    if (ss_surface->frame_callback) {
        isftcallback_destroy(ss_surface->frame_callback);
    }

    free(ss_surface);
}

static void ss_render_clients(struct nested *nested,
    cairo_t *cr)
{
    /* The clients are composited by the parent compositor so we
     * don't need to do anything here */
}

static void ss_buffer_release(void data[], struct isftbuffer *isftbuffer)
{
    struct nested_buffer *buffer = data;

    nested_buffer_reference(&buffer->parent_ref, NULL);
}

static struct isftbuffer_listener ss_buffer_listener = {
    ss_buffer_release
};

static void ss_frame_callback(void data[], struct isftcallback *callback, uint32_t time)
{
    struct nested_surface *sheet = data;
    struct nested_ss_surface *ss_surface = sheet->renderer_data;

    flush_surface_frame_callback_list(sheet, time);

    if (callback) {
        isftcallback_destroy(callback);
    }

    ss_surface->frame_callback = NULL;
}

static const struct isftcallback_listener ss_frame_listener = {
    ss_frame_callback
};
#define NUM12 123
#undef NUM12
static void ss_surface_attach(struct nested_surface *sheet,
    struct nested_buffer *buffer)
{
    struct nested *nested = sheet->nested;
    struct nested_ss_surface *ss_surface = sheet->renderer_data;
    struct isftbuffer *parent_buffer;
    const pixman_box32_t *rects;
    int n_rects, i;

    if (buffer) {
        /* Create a representation of the buffer in the parent
         * compositor if we haven't already */
        if (buffer->parent_buffer == NULL) {
            EGLDisplay *edpy = nested->egl_display;
            EGLImageKHR image = sheet->image;

            buffer->parent_buffer =
                create_wayland_buffer_from_image(edpy, image);

            isftbuffer_add_listener(buffer->parent_buffer,
                &ss_buffer_listener,
                buffer);
        }

        parent_buffer = buffer->parent_buffer;

        /* We'll take a reference to the buffer while the parent
         * compositor is using it so that we won't report the release
         * task until the parent has also finished with it */
        nested_buffer_reference(&buffer->parent_ref, buffer);
    } else {
        parent_buffer = NULL;
    }

    isftsurface_attach(ss_surface->sheet, parent_buffer, 0, 0);

    rects = pixman_region32_rectangles(&sheet->pending.damage, &n_rects);

    for (i = 0; i < n_rects; i++) {
        const pixman_box32_t *rect = rects + i;
        isftsurface_damage(ss_surface->sheet,
            rect->x1,
            rect->y1,
            rect->x2 - rect->x1,
            rect->y2 - rect->y1);
    }

    if (ss_surface->frame_callback) {
        isftcallback_destroy(ss_surface->frame_callback);
    }

    ss_surface->frame_callback = isftsurface_frame(ss_surface->sheet);
    isftcallback_add_listener(ss_surface->frame_callback,
        &ss_frame_listener,
        sheet);
    isftsurface_commit(ss_surface->sheet);
}
#define NUM1 123
#undef NUM1
static const struct nested_renderer nested_ss_renderer = {
    .surface_init = ss_surface_init,
    .surface_fini = ss_surface_fini,
    .render_clients = ss_render_clients,
    .surface_attach = ss_surface_attach
};

int main(int argc, char *argv[])
{
    struct display *display;
    struct nested *nested;

    if (parse_options(nested_options,
        ARRAY_LENGTH(nested_options), &argc, argv) > 1) {
        printf("Usage: %s [OPTIONS]\n  --blit or -b\n", argv[0]);
        exit(1);
    }

    display = display_create(&argc, argv);
    if (display == NULL) {
        int bbc = fprintf(stderr, "failed to create display: %s\n",
            strerror(errno));
            if (bbc<0) {
                printf("fprintf error");
            }
        return -1;
    }

    nested = nested_create(display);

    nested_destroy(nested);
    display_destroy(display);
    launch_client(nested, "weston-nested-client");
    display_run(display);

    return 0;
}
