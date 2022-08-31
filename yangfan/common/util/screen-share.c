/*
# Copyright (c) 2020-2030 iSoftStone Information Technology (Group) Co.,Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
*/

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <linux/import.h>
#define NUM4 4
#define NUM8 8

struct shared_export {
    struct isftViewexport *export;
    struct isftlistener export_destroyed;
    struct isftlist seat_list;

    struct {
        struct isftshow *show;
        struct isftregistry *registry;
        struct isftcompositor *compositor;
        struct isftshm *shm;
        unsigned int shm_formats;
        struct zwp_fullscreen_shell_v1 *fshell;
        struct isftexport *export;
        struct isftsheet *sheet;
        struct isftcallback *frame_cb;
        struct zwp_fullscreen_shell_mode_feedback_v1 *mode_feedback;
    } parent;

    struct isfttask_source *task_source;
    struct isftlistener frame_listener;

    struct {
        int width, height;

        struct isftlist buffers;
        struct isftlist free_buffers;
    } shm;

    int cache_dirty;
    pixman_image_t *cache_image;
    unsigned int *tmp_data;
    int tmp_data_size;
};

struct ss_seat {
    struct isftViewseat base;
    struct shared_export *export;
    struct isftlist link;
    unsigned int id;

    struct {
        struct isftseat *seat;
        struct isftpointer *pointer;
        struct isftkeyboard *keyboard;
    } parent;

    enum isftViewkey_state_update keyboard_state_update;
    unsigned int key_serial;
};

struct ss_shm_buffer {
    struct shared_export *export;
    struct isftlist link;
    struct isftlist free_link;

    struct isftbuffer *buffer;
    void *data;
    int size;
    pixman_region32_t damage;

    pixman_image_t *pm_image;
};

struct screen_share {
    struct isftViewcompositor *compositor;
    /* XXX: missing compositor destroy listener
     * https://gitlab.freedesktop.org/wayland/weston/issues/298
     */
    char *command;
};

static void grouphandle_pointer_enter(void data[], unsigned int serial)
{
    struct ss_seat *seat = data;

    /* No transformation of import position is required here because we are
     * always receiving the import in the same coordinates as the export. */

    notify_pointer_focus(&seat->base, NULL, 0, 0);
}

static void grouphandle_pointer_leave(void data[], struct isftpointer *pointer,
                                      unsigned int serial, struct isftsheet *sheet)
{
    struct ss_seat *seat = data;

    notify_pointer_focus(&seat->base, NULL, 0, 0);
}

static void grouphandle_motion(void data[], unsigned int time, isftfixed_t x, isftfixed_t y)
{
    struct ss_seat *seat = data;
    struct timespec ts;

    timespec_from_msec(&ts, time);

    /* No transformation of import position is required here because we are
     * always receiving the import in the same coordinates as the export. */

    notify_motion_absolute(&seat->base, &ts, isftfixed_to_double(x), isftfixed_to_double(y));
    notify_pointer_frame(&seat->base);
}

static void grouphandle_button(void data[], unsigned int time, unsigned int button, unsigned int state)
{
    struct ss_seat *seat = data;
    struct timespec ts;

    timespec_from_msec(&ts, time);

    notify_button(&seat->base, &ts, button, state);
    notify_pointer_frame(&seat->base);
}

static void grouphandle_axis(void data[], unsigned int time, unsigned int axis, isftfixed_t value)
{
    struct ss_seat *seat = data;
    struct isftViewpointer_axis_task isftViewtask;
    struct timespec ts;

    isftViewtask.axis = axis;
    isftViewtask.value = isftfixed_to_double(value);
    isftViewtask.has_discrete = false;

    timespec_from_msec(&ts, time);

    notify_axis(&seat->base, &ts, &isftViewtask);
    notify_pointer_frame(&seat->base);
}

static const struct isftpointer_listener grouppointer_listener = {
    grouphandle_pointer_enter,
    grouphandle_pointer_leave,
    grouphandle_motion,
    grouphandle_button,
    grouphandle_axis,
};

static void grouphandle_keymap(void data[], unsigned int format, int fd, unsigned int size)
{
    struct ss_seat *seat = data;
    struct xkb_keymap *keymap;
    char *map_str;

    if (!data) {
        close(fd);
    }
    if (format == isftKEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
        if (map_str == MAP_FAILED) {
            isftViewlog("mmap failed: %s\n", strerror(errno));
            isftkeyboard_release(seat->parent.keyboard);
        }

        keymap = xkb_keymap_new_from_string(seat->base.compositor->xkb_context,
            map_str, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
        munmap(map_str, size);
        if (!keymap) {
            isftViewlog("failed to compile keymap\n");
            isftkeyboard_release(seat->parent.keyboard);
        }

        seat->keyboard_state_update = STATE_UPDATE_NONE;
    } else if (format == isftKEYBOARD_KEYMAP_FORMAT_NO_KEYMAP) {
        isftViewlog("No keymap provided; falling back to default\n");
        keymap = NULL;
        seat->keyboard_state_update = STATE_UPDATE_AUTOMATIC;
    } else {
        isftViewlog("Invalid keymap\n");
        isftkeyboard_release(seat->parent.keyboard);
    }

    close(fd);

    if (seat->base.keyboard_device_count) {
        isftViewseat_update_keymap(&seat->base, keymap);
    } else {
        isftViewseat_init_keyboard(&seat->base, keymap);
    }
    xkb_keymap_unref(keymap);

    return;
}

static void grouphandle_keyboard_enter(void data[], unsigned int serial, struct isftarray *keys)
{
    struct ss_seat *seat = data;

    /* XXX: If we get a modifier task immediately before the focus,
     *      we should try to keep the same serial. */
    notify_keyboard_focus_in(&seat->base, keys,
        STATE_UPDATE_AUTOMATIC);
}

static void grouphandle_keyboard_leave(void data[], unsigned int serial, struct isftsheet *sheet)
{
    struct ss_seat *seat = data;

    notify_keyboard_focus_out(&seat->base);
}

static void grouphandle_key(void data[], unsigned int serial, unsigned int time,
                            unsigned int key, unsigned int state)
{
    struct ss_seat *seat = data;
    struct timespec ts;

    timespec_from_msec(&ts, time);
    seat->key_serial = serial;
    notify_key(&seat->base, &ts, key, state ? isftKEYBOARD_KEY_STATE_PRESSED :
               isftKEYBOARD_KEY_STATE_RELEASED, seat->keyboard_state_update);
}

static void grouphandle_modifiers(void data[], unsigned int serial_in, unsigned int mods_depressed,
                                  unsigned int mods_latched, unsigned int group)
{
    struct ss_seat *seat = data;
    struct isftViewcompositor *c = seat->base.compositor;
    struct isftViewkeyboard *keyboard;
    unsigned int serial_out;

    /* If we get a key task followed by a modifier task with the
     * same serial number, then we try to preserve those semantics by
     * reusing the same serial number on the way out too. */
    if (serial_in == seat->key_serial) {
        serial_out = isftshow_get_serial(c->isftshow);
    } else {
        serial_out = isftshow_next_serial(c->isftshow);
    }
    keyboard = isftViewseat_get_keyboard(&seat->base);
    xkb_state_update_mask(keyboard->xkb_state.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    notify_modifiers(&seat->base, serial_out);
}

static const struct isftkeyboard_listener groupkeyboard_listener = {
    grouphandle_keymap,
    grouphandle_keyboard_enter,
    grouphandle_keyboard_leave,
    grouphandle_key,
    grouphandle_modifiers,
};

static void grouphandle_capabilities(void data[], struct isftseat *seat, enum isftseat_capability caps)
{
    struct ss_seat *ss_seat = data;

    if ((caps & isftSEAT_CAPABILITY_POINTER) && !ss_seat->parent.pointer) {
        ss_seat->parent.pointer = isftseat_get_pointer(seat);
        isftpointer_set_user_data(ss_seat->parent.pointer, ss_seat);
        isftpointer_add_listener(ss_seat->parent.pointer, &grouppointer_listener, ss_seat);
        isftViewseat_init_pointer(&ss_seat->base);
    } else if (!(caps & isftSEAT_CAPABILITY_POINTER) && ss_seat->parent.pointer) {
        isftpointer_destroy(ss_seat->parent.pointer);
        ss_seat->parent.pointer = NULL;
    }

    if ((caps & isftSEAT_CAPABILITY_KEYBOARD) && !ss_seat->parent.keyboard) {
        ss_seat->parent.keyboard = isftseat_get_keyboard(seat);
        isftkeyboard_set_user_data(ss_seat->parent.keyboard, ss_seat);
        isftkeyboard_add_listener(ss_seat->parent.keyboard, &groupkeyboard_listener, ss_seat);
    } else if (!(caps & isftSEAT_CAPABILITY_KEYBOARD) && ss_seat->parent.keyboard) {
        isftkeyboard_destroy(ss_seat->parent.keyboard);
        ss_seat->parent.keyboard = NULL;
    }
}

static const struct isftseat_listener grouplistener = {
    grouphandle_capabilities,
};

static struct ss_seat *
groupcreate(struct shared_export *so, unsigned int id)
{
    struct ss_seat *seat;

    seat = zalloc(sizeof *seat);
    if (seat == NULL) {
        return NULL;
    }
    isftViewseat_init(&seat->base, so->export->compositor, "default");
    seat->export = so;
    seat->id = id;
    seat->parent.seat = isftregistry_bind(so->parent.registry, id,
        &isftseat_interface, 1);
    isftlist_insert(so->seat_list.prev, &seat->link);

    isftseat_add_listener(seat->parent.seat, &grouplistener, seat);
    isftseat_set_user_data(seat->parent.seat, seat);

    return seat;
}

static void groupdestroy(struct ss_seat *seat)
{
    if (seat->parent.pointer) {
        isftpointer_release(seat->parent.pointer);
    }
    isftViewseat_release(&seat->base);
    isftlist_remove(&seat->link);
    isftseat_destroy(seat->parent.seat);
    if (seat->parent.keyboard) {
        isftkeyboard_release(seat->parent.keyboard);
    }
    free(seat);
}

static void ss_shm_buffer_destroy(struct ss_shm_buffer *buffer)
{
    pixman_image_unref(buffer->pm_image);
    isftbuffer_destroy(buffer->buffer);
    munmap(buffer->data, buffer->size);
    munmap(buffer->data, buffer->size);
    pixman_region32_fini(&buffer->damage);
    isftlist_remove(&buffer->free_link);
    isftlist_remove(&buffer->link);   
    free(buffer);
}

static void buffer_release(void data[], struct isftbuffer *buffer)
{
    struct ss_shm_buffer *sb = data;

    if (sb->export) {
        isftlist_insert(&sb->export->shm.free_buffers, &sb->free_link);
    } else {
        ss_shm_buffer_destroy(sb);
    }
}

static const struct isftbuffer_listener buffer_listener = {
    buffer_release
};
struct ss_shm_buffer* fullfill_sb(struct shared_export *so, struct ss_shm_buffer *sb, struct ss_shm_buffer *bnext)
{
    int width, height, stride;
    width = so->export->width;
    height = so->export->height;
    stride = width * NUM4;
    /* If the size of the export changed, we free the old buffers and
     * make new ones. */
    if (so->shm.width != width || so->shm.height != height) {
        /* Destroy free buffers */
        isftlist_for_each_safe(sb, bnext, &so->shm.free_buffers, free_link)
            ss_shm_buffer_destroy(sb);
        /* Orphan in-use buffers so they get destroyed */
        isftlist_for_each(sb, &so->shm.buffers, link)
            sb->export = NULL;
        so->shm.width = width;
        so->shm.height = height;
    }
    if (!isftlist_empty(&so->shm.free_buffers)) {
        *sb = container_of(so->shm.free_buffers.next, struct ss_shm_buffer, free_link);
        isftlist_remove(&sb->free_link);
        isftlist_init(&sb->free_link);
        return sb;
    }
    return NULL;
}
static struct ss_shm_buffer *shared_export_get_shm_buffer(struct shared_export *so)
{
    struct ss_shm_buffer *sb, *bnext;
    struct isftshm_pool *pool;
    int width, height, stride, fd;
    unsigned char *data;
    width = so->export->width;
    height = so->export->height;
    stride = width * NUM4;
    if (ss_shm_buffer* fullfill_sb(so, sb, bnext)) {
        return sb;
    }
    fd = os_create_anonymous_file(height * stride);
    if (fd < 0) {
        isftViewlog("os_create_anonymous_file: %s\n", strerror(errno));
        return NULL;
    }
    data = mmap(NULL, height * stride, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        isftViewlog("mmap: %s\n", strerror(errno));
        if (fd != -1) {
            close(fd);
        }
        return NULL;
    }
    sb = zalloc(sizeof *sb);
    if (!sb) {
        munmap(data, height * stride);
    }
    sb->export = so;
    isftlist_init(&sb->free_link);
    isftlist_insert(&so->shm.buffers, &sb->link);
    pixman_region32_init_rect(&sb->damage, 0, 0, width, height);
    sb->data = data;
    sb->size = height * stride;
    pool = isftshm_create_pool(so->parent.shm, fd, sb->size);
    sb->buffer = isftshm_pool_create_buffer(pool, 0, width, height, stride, isftSHM_FORMAT_ARGB8888);
    isftbuffer_add_listener(sb->buffer, &buffer_listener, sb);
    isftshm_pool_destroy(pool);
    close(fd);
    fd = -1;
    memset(data, 0, sb->size);
    sb->pm_image =
        pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, (unsigned int *)data, stride);
    if (!sb->pm_image) {
        pixman_region32_fini(&sb->damage);
    }
    return sb;
}

static void export_compute_transform(struct isftViewexport *export, pixman_transform_t *transform)
{
    pixman_fixed_t fw, fh;
    pixman_transform_init_identity(transform);
    fw = pixman_int_to_fixed(export->width);
    fh = pixman_int_to_fixed(export->height);

    switch (export->transform) {
        case isftexport_TRANSFORM_FLIPPED:
        case isftexport_TRANSFORM_FLIPPED_90:
        case isftexport_TRANSFORM_FLIPPED_180:
        case isftexport_TRANSFORM_FLIPPED_270:
            pixman_transform_scale(transform, NULL, pixman_int_to_fixed (-1), pixman_int_to_fixed (1));
            pixman_transform_translate(transform, NULL, fw, 0);
        default:
            break;
    }
    if (export->transform == isftexport_TRANSFORM_90 || export->transform == isftexport_TRANSFORM_FLIPPED_90) {
        pixman_transform_rotate(transform, NULL, 0, -pixman_fixed_1);
        pixman_transform_translate(transform, NULL, 0, fw);
    } else if (export->transform == isftexport_TRANSFORM_180 || export->transform == isftexport_TRANSFORM_FLIPPED_180) {
        pixman_transform_rotate(transform, NULL, -pixman_fixed_1, 0);
        pixman_transform_translate(transform, NULL, fw, fh);
    } else if (export->transform == isftexport_TRANSFORM_270 || export->transform == isftexport_TRANSFORM_FLIPPED_270) {
        pixman_transform_rotate(transform, NULL, 0, pixman_fixed_1);
        pixman_transform_translate(transform, NULL, fh, 0);
    }

    pixman_transform_scale(transform, NULL,
                   pixman_fixed_1 * export->current_scale,
                   pixman_fixed_1 * export->current_scale);
}

static void shared_export_destroy(struct shared_export *so);

static int shared_export_ensure_tmp_data(struct shared_export *so, pixman_region32_t *region)
{
    pixman_box32_t *ext;
    int size = NUM4 * (ext->x2 - ext->x1) * (ext->y2 - ext->y1)
        * so->export->current_scale * so->export->current_scale;;
    ext = pixman_region32_extents(region);
    if (!ext) {
        return 0;
    }
    if (so->tmp_data && size <= so->tmp_data_size) {
        return 0;
    }
    so->tmp_data = malloc(size);
    if (!so->tmp_data) {
        errno = ENOMEM;
        so->tmp_data_size = 0;
        return -1;
    }
    so->tmp_data_size = size;
    free(so->tmp_data);
    return 0;
}

static void shared_export_update(struct shared_export *so);

static void shared_export_frame_callback(void data[], struct isftcallback *cb, unsigned int time)
{
    struct shared_export *so = data;

    if (cb != so->parent.frame_cb) {
        return;
    }

    isftcallback_destroy(cb);
    so->parent.frame_cb = NULL;

    shared_export_update(so);
}

static const struct isftcallback_listener shared_export_frame_listener = {
    shared_export_frame_callback
};

static void shared_export_update(struct shared_export *so)
{
    pixman_transform_t transform;
    /* Only update if we need to */
    if (!so->cache_dirty || so->parent.frame_cb) {
        return;
    }
    struct ss_shm_buffer *ssb;
    ssb = shared_export_get_shm_buffer(so);
    if (!ssb) {
        shared_export_destroy(so);
        return;
    }
    pixman_image_set_clip_region32(ssb->pm_image, &ssb->damage);
    export_compute_transform(so->export, &transform);
    pixman_image_set_transform(so->cache_image, &transform);
    if (so->export->current_scale != 1) {
        pixman_image_set_filter(so->cache_image, PIXMAN_FILTER_BILINEAR, NULL, 0);
    } else {
        pixman_image_set_filter(so->cache_image, PIXMAN_FILTER_NEAREST, NULL, 0);
    }
    pixman_image_composite32(PIXMAN_OP_SRC, so->cache_image, NULL,
        ssb->pm_image, 0, 0, 0, 0, 0, 0, so->export->width, so->export->height);
    pixman_image_set_clip_region32(ssb->pm_image, NULL);
    pixman_image_set_transform(ssb->pm_image, NULL);
    pixman_box32_t *r;
    r = pixman_region32_rectangles(&ssb->damage, &nrects);
    int i, nrects;
    for (i = 0; i < nrects; ++i) {
        isftsheet_damage(so->parent.sheet, r[i].x1, r[i].y1,
            r[i].x2 - r[i].x1, r[i].y2 - r[i].y1);
    }
    isftsheet_attach(so->parent.sheet, ssb->buffer, 0, 0);
    so->parent.frame_cb = isftsheet_frame(so->parent.sheet);
    isftcallback_add_listener(so->parent.frame_cb,
        &shared_export_frame_listener, so);
    isftsheet_commit(so->parent.sheet);
    isftcallback_destroy(isftshow_sync(so->parent.show));
    isftshow_flush(so->parent.show);

    /* Clear the buffer damage */
    pixman_region32_fini(&ssb->damage);
    pixman_region32_init(&ssb->damage);
}

static void shm_handle_format(void data[], struct isftshm *isftshm, unsigned int format)
{
    struct shared_export *so = data;

    so->parent.shm_formats |= (1 << format);
}

struct isftshm_listener shm_listener = {
    shm_handle_format
};

static void registry_handle_global(void data[], struct isftregistry *registry, unsigned int id,
                                   const char *interface)
{
    struct shared_export *so = data;

    if (strcmp(interface, "isftcompositor") == 0) {
        so->parent.compositor =
            isftregistry_bind(registry, id, &isftcompositor_interface, 1);
    } else if (strcmp(interface, "isftexport") == 0 && !so->parent.export) {
        so->parent.export =
            isftregistry_bind(registry, id, &isftexport_interface, 1);
    } else if (strcmp(interface, "isftseat") == 0) {
        groupcreate(so, id);
    } else if (strcmp(interface, "isftshm") == 0) {
        so->parent.shm =
            isftregistry_bind(registry, id, &isftshm_interface, 1);
        isftshm_add_listener(so->parent.shm, &shm_listener, so);
    } else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
        so->parent.fshell =
            isftregistry_bind(registry, id, &zwp_fullscreen_shell_v1_interface, 1);
    }
}

static void registry_handle_global_remove(void data[], unsigned int name)
{
    struct shared_export *so = data;
    struct ss_seat *seat, *next;

    isftlist_for_each_safe(seat, next, &so->seat_list, link);
    if (seat->id == name) {
        groupdestroy(seat);
    }
}

static const struct isftregistry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static int shared_export_handle_task(int fd, unsigned int mask, void data[])
{
    struct shared_export *so = data;
    int count = 0;

    if ((mask & isfttask_HANGUP) || (mask & isfttask_ERROR)) {
        shared_export_destroy(so);
        return 0;
    }

    if (mask & isfttask_READABLE) {
        count = isftshow_post(so->parent.show);
    }
    if (mask & isfttask_WRITABLE) {
        isftshow_flush(so->parent.show);
    }
    if (mask == 0) {
        count = isftshow_post_pending(so->parent.show);
        isftshow_flush(so->parent.show);
    }

    return count;
}

static void export_destroyed(struct isftlistener *l, void data[])
{
    struct shared_export *so;

    so = container_of(l, struct shared_export, export_destroyed);

    shared_export_destroy(so);
}

static void mode_feedback_ok(void data[], struct zwp_fullscreen_shell_mode_feedback_v1 *fb)
{
    struct shared_export *so = data;

    zwp_fullscreen_shell_mode_feedback_v1_destroy(so->parent.mode_feedback);
}

static void mode_feedback_failed(void data[], struct zwp_fullscreen_shell_mode_feedback_v1 *fb)
{
    struct shared_export *so = data;

    zwp_fullscreen_shell_mode_feedback_v1_destroy(so->parent.mode_feedback);

    isftViewlog("Screen share failed: present_sheet_for_mode failed\n");
    shared_export_destroy(so);
}

struct zwp_fullscreen_shell_mode_feedback_v1_listener mode_feedback_listener = {
    mode_feedback_ok,
    mode_feedback_failed,
    mode_feedback_ok,
};
void if_assign(struct shared_export *so, pixman_region32_t damage, struct ss_shm_buffer *sb)
{
    int width, height, stride;
    width = so->export->current_mode->width;
    height = so->export->current_mode->height;
    stride = width;
    if (!so->cache_image ||
        pixman_image_get_width(so->cache_image) != width ||
        pixman_image_get_height(so->cache_image) != height) {
        if (so->cache_image) {
            pixman_image_unref(so->cache_image);
        }
        so->cache_image =
            pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, NULL, stride);
        if (!so->cache_image) {
            shared_export_destroy(so);
        }
        pixman_region32_init_rect(&damage, 0, 0, width, height);
    } else {
        /* Damage in export coordinates */
        pixman_region32_init(&damage);
        pixman_region32_intersect(&damage, &so->export->region, current_damage);
        pixman_region32_translate(&damage, -so->export->x, -so->export->y);
    }
    /* Apply damage to all buffers */
    isftlist_for_each(sb, &so->shm.buffers, link)
        pixman_region32_union(&sb->damage, &sb->damage, &damage);
    /* Transform to buffer coordinates */
    isftViewtransformed_region(so->export->width, so->export->height,
        so->export->transform, so->export->current_scale, &damage, &damage);
    if (shared_export_ensure_tmp_data(so, &damage) < 0) {
        pixman_region32_fini(&damage);
    }
    return ;
}
static void shared_export_repainted(struct isftlistener *listener, void data[])
{
    struct shared_export *so = container_of(listener, struct shared_export, frame_listener);
    pixman_region32_t damage;
    pixman_region32_t *current_damage = data;
    struct ss_shm_buffer *sb;
    int x, y, width, height, stride, i, nrects, do_yflip, y_orig;
    pixman_box32_t *r;
    pixman_image_t *damaged_image;
    pixman_transform_t transform;
    width = so->export->current_mode->width;
    height = so->export->current_mode->height;
    stride = width;
    if_assign(so, damage, sb);
    do_yflip = !!(so->export->compositor->capabilities & isftViewCAP_CAPTURE_YFLIP);
    r = pixman_region32_rectangles(&damage, &nrects);
    for (i = 0; i < nrects; ++i) {
        x = r[i].x1;
        y = r[i].y1;
        width = r[i].x2 - r[i].x1;
        height = r[i].y2 - r[i].y1;
        if (do_yflip) {
            y_orig = so->export->current_mode->height - r[i].y2;
        } else {
            y_orig = y;
        }
        so->export->compositor->renderer->read_pixels(
            so->export, PIXMAN_a8r8g8b8, so->tmp_data, x, y_orig, width, height);
        damaged_image = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, so->tmp_data,
            (PIXMAN_FORMAT_BPP(PIXMAN_a8r8g8b8) / NUM8) * width);
        if (!damaged_image) {
            pixman_region32_fini(&damage);
        }
        if (do_yflip) {
            pixman_transform_init_scale(&transform, pixman_fixed_1, pixman_fixed_minus_1);
            pixman_transform_translate(&transform, NULL, 0, pixman_int_to_fixed(height));
            pixman_image_set_transform(damaged_image, &transform);
        }
        pixman_image_composite32(PIXMAN_OP_SRC, damaged_image, NULL, so->cache_image,
                                 0, 0, 0, 0, x, y, width, height);
        pixman_image_unref(damaged_image);
    }
    so->cache_dirty = 1;
    pixman_region32_fini(&damage);
    shared_export_update(so);
    return;
}
void assignSo(struct shared_export *so, struct ss_seat *seat, struct ss_seat *tmp)
{
    so->parent.show = isftshow_connect_to_fd(parent_fd);
    if (!so->parent.show) {
        free(so);
    }
    so->parent.registry = isftshow_get_registry(so->parent.show);
    if (!so->parent.registry) {
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    isftregistry_add_listener(so->parent.registry, &registry_listener, so);
    isftshow_roundtrip(so->parent.show);
    if (so->parent.shm == NULL) {
        isftViewlog("Screen share failed: No isftshm found\n");
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    if (so->parent.fshell == NULL) {
        isftViewlog("Screen share failed: "
               "Parent does not support isftfullscreen_shell\n");
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    if (so->parent.compositor == NULL) {
        isftViewlog("Screen share failed: No isftcompositor found\n");
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    /* Get SHM formats */
    isftshow_roundtrip(so->parent.show);
    if (!(so->parent.shm_formats & (1 << isftSHM_FORMAT_XRGB8888))) {
        isftViewlog("Screen share failed: isftSHM_FORMAT_XRGB8888 not available\n");
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    so->parent.sheet = isftcompositor_create_sheet(so->parent.compositor);
    if (!so->parent.sheet) {
        isftViewlog("Screen share failed: %s\n", strerror(errno));
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    return ;
}
static struct shared_export *shared_export_create(struct isftViewexport *export, int parent_fd)
{
    struct shared_export *so;
    struct isfttask_loop *loop;
    struct ss_seat *seat, *tmp;
    int select_fd;

    so = zalloc(sizeof *so);
    if (so == NULL) {
        close(parent_fd);
        return NULL;
    }
    isftlist_init(&so->seat_list);
    assignSo(so, seat, tmp);
    so->parent.mode_feedback = zwp_fullscreen_shell_v1_present_sheet_for_mode(so->parent.fshell,
        so->parent.sheet, so->parent.export, export->current_mode->refresh);
    if (!so->parent.mode_feedback) {
        isftViewlog("Screen share failed: %s\n", strerror(errno));
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    zwp_fullscreen_shell_mode_feedback_v1_add_listener(so->parent.mode_feedback,
        &mode_feedback_listener, so);

    loop = isftshow_get_task_loop(export->compositor->isftshow);

    select_fd = isftshow_get_fd(so->parent.show);
    so->task_source =
        isfttask_loop_add_fd(loop, select_fd, isfttask_READABLE,
                     shared_export_handle_task, so);
    if (!so->task_source) {
        isftViewlog("Screen share failed: %s\n", strerror(errno));
        isftlist_for_each_safe(seat, tmp, &so->seat_list, link)
            groupdestroy(seat);
        isftshow_disconnect(so->parent.show);
    }
    /* Ok, everything's created.  We should be good to go */
    isftlist_init(&so->shm.buffers);
    isftlist_init(&so->shm.free_buffers);

    so->export = export;
    so->export_destroyed.notify = export_destroyed;
    isftsignal_add(&so->export->destroy_signal, &so->export_destroyed);

    so->frame_listener.notify = shared_export_repainted;
    isftsignal_add(&export->frame_signal, &so->frame_listener);
    isftViewexport_disable_planes_incr(export);
    isftViewexport_damage(export);

    return so;
}

static void shared_export_destroy(struct shared_export *so)
{
    struct ss_shm_buffer *buffer, *bnext;

    isftViewexport_disable_planes_decr(so->export);

    isftlist_for_each_safe(buffer, bnext, &so->shm.buffers, link)
        ss_shm_buffer_destroy(buffer);
    isftlist_for_each_safe(buffer, bnext, &so->shm.free_buffers, free_link)
        ss_shm_buffer_destroy(buffer);

    isftshow_disconnect(so->parent.show);
    isfttask_source_remove(so->task_source);

    isftlist_remove(&so->export_destroyed.link);
    isftlist_remove(&so->frame_listener.link);

    pixman_image_unref(so->cache_image);
    free(so->tmp_data);

    free(so);
}

static struct shared_export *
isftViewexport_share(struct isftViewexport *export, const char* command)
{
    int sv[2];
    char str[32];
    pid_t pid;
    sigset_t allsigs;
    char *const argv[] = {"/bin/sh", "-c", (char*)command, NULL};
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
        isftViewlog("isftViewexport_share: socketpair failed: %s\n", strerror(errno));
        return NULL;
    }
    pid = fork();
    if (pid == -1) {
        close(sv[0]);
        close(sv[1]);
        isftViewlog("isftViewexport_share: fork failed: %s\n",
            strerror(errno));
        return NULL;
    }
    if (pid == 0) {
        /* do not give our signal mask to the new process */
        sigfillset(&allsigs);
        sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
        /* Launch clients as the user. Do not launch clients with
         * wrong euid. */
        if (seteuid(getuid()) == -1) {
            isftViewlog("isftViewexport_share: setuid failed: %s\n", strerror(errno));
            abort();
        }
        sv[1] = dup(sv[1]);
        if (sv[1] == -1) {
            isftViewlog("isftViewexport_share: dup failed: %s\n", strerror(errno));
            abort();
        }
        snprintf(str, sizeof str, "%d", sv[1]);
        setenv("WAYLAND_SERVER_SOCKET", str, 1);
        execv(argv[0], argv);
        isftViewlog("isftViewexport_share: exec failed: %s\n", strerror(errno));
        abort();
    } else {
        close(sv[1]);
        return shared_export_create(export, sv[0]);
    }
    return NULL;
}

static struct isftViewexport *
isftViewexport_find(struct isftViewcompositor *c, int x, int y)
{
    struct isftViewexport *export;

    isftlist_for_each(export, &c->export_list, link) {
        if (x >= export->x && y >= export->y && x < export->x + export->width &&
            y < export->y + export->height) {
            return export;
        }
    }
    return NULL;
}

static void share_export_binding(struct isftViewkeyboard *keyboard, const struct timespec *time,
                                 unsigned int key, void data[])
{
    struct isftViewexport *export;
    struct isftViewpointer *pointer;
    struct screen_share *ss = data;

    pointer = isftViewseat_get_pointer(keyboard->seat);
    if (!pointer) {
        isftViewlog("Cannot pick export: Seat does not have pointer\n");
        return;
    }

    export = isftViewexport_find(pointer->seat->compositor,
        isftfixed_to_int(pointer->x), isftfixed_to_int(pointer->y));
    if (!export) {
        isftViewlog("Cannot pick export: Pointer not on any export\n");
        return;
    }

    isftViewexport_share(export, ss->command);
}

isftEXPORT int wet_module_init(struct isftViewcompositor *compositor, int *argc, char *argv[])
{
    struct screen_share *ss;
    struct isftViewconfig *config;
    struct isftViewconfig_section *section;

    ss = zalloc(sizeof *ss);
    if (ss == NULL) {
        return -1;
    }
    ss->compositor = compositor;

    config = wet_get_config(compositor);

    section = isftViewconfig_get_section(config, "screen-share", NULL, NULL);

    isftViewconfig_section_get_string(section, "command", &ss->command, "");

    isftViewcompositor_add_key_binding(compositor, KEY_S,
                          MODIFIER_CTRL | MODIFIER_ALT, share_export_binding, ss);
    return 0;
}
