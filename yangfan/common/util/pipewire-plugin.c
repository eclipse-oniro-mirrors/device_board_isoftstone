/*
# Copyright (c) 2021-2022 isoftstone Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
 */

#include "pipewire-plugin.h"
#include "backend.h"
#include "libisftView-internal.h"
#include "shared/timespec-util.h"
#include <libisftView/backend-drm.h>
#include <libisftView/isftView-log.h>

#include <sys/mman.h>

#include <errno.h>
#include <unistd.h>

#include <spa/param/format-utils.h>
#include <spa/param/video/format-utils.h>
#include <spa/utils/defs.h>

#include <pipewire/pipewire.h>

#define PROP_RANGE(min, max) 2, (min), (max)

struct type {
    struct spa_type_media_type media_type;
    struct spa_type_media_subtype media_subtype;
    struct spa_type_format_video format_video;
    struct spa_type_video_format video_format;
};

struct isftView_pipewire {
    struct isftView_compositor *compositor;
    struct isftlist export_list;
    struct isftlistener destroy_listener;
    const struct isftView_drm_virtual_export_api *virtual_export_api;

    struct isftView_log_scope *debug;

    struct pw_loop *loop;
    struct isfttask_source *loop_source;

    struct pw_core *core;
    struct pw_type *t;
    struct type type;

    struct pw_remote *remote;
    struct spa_hook remote_listener;
};

struct pipewire_export {
    struct isftView_export *export;
    void (*saved_destroy)(struct isftView_export *export);
    int (*saved_enable)(struct isftView_export *export);
    int (*saved_disable)(struct isftView_export *export);
    int (*saved_start_repaint_loop)(struct isftView_export *export);

    struct isftView_head *head;

    struct isftView_pipewire *pipewire;

    uint32_t seq;
    struct pw_stream *stream;
    struct spa_hook stream_listener;

    struct spa_video_info_raw video_format;

    struct isfttask_source *finish_frame_clock;
    struct isftlist link;
    bool submitted_frame;
    enum dpms_enum dpms;
};

struct pipewire_frame_data {
    struct pipewire_export *export;
    int fd;
    int stride;
    struct drm_fb *drm_buffer;
    int fence_sync_fd;
    struct isfttask_source *fence_sync_task_source;
};

static inline void init_type(struct type *type, struct spa_type_map *map)
{
    spa_type_media_type_map(map, &type->media_type);
    spa_type_media_subtype_map(map, &type->media_subtype);
    spa_type_format_video_map(map, &type->format_video);
    spa_type_video_format_map(map, &type->video_format);
}

static void pipewire_debug_impl(struct isftView_pipewire *pipewire,
    struct pipewire_export *export,
    const char *fmt, va_list ap)
{
    FILE *fp;
    char *logstr;
    int logsize;
    char timestr[128];

    if (!isftView_log_scope_is_enabled(pipewire->debug))
        return;

    fp = open_memstream(&logstr, &logsize);
    if (!fp)
        return;

    isftView_log_scope_timestamp(pipewire->debug, timestr, sizeof timestr);
    fprintf(fp, "%s", timestr);

    if (export)
        fprintf(fp, "[%s]", export->export->name);

    fprintf(fp, " ");
    vfprintf(fp, fmt, ap);
    fprintf(fp, "\n");

    if (fclose(fp) == 0)
        isftView_log_scope_write(pipewire->debug, logstr, logsize);

    free(logstr);
}

static void pipewire_debug(struct isftView_pipewire *pipewire, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pipewire_debug_impl(pipewire, NULL, fmt, ap);
    va_end(ap);
}

static void pipewire_export_debug(struct pipewire_export *export, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pipewire_debug_impl(export->pipewire, export, fmt, ap);
    va_end(ap);
}

static struct isftView_pipewire *isftView_pipewire_get(struct isftView_compositor *compositor);

static struct pipewire_export *lookup_pipewire_export(struct isftView_export *base_export)
{
    struct isftView_compositor *c = base_export->compositor;
    struct isftView_pipewire *pipewire = isftView_pipewire_get(c);
    struct pipewire_export *export;

    isftlist_for_each(export, &pipewire->export_list, link) {
        if (export->export == base_export)
            return export;
    }
    return NULL;
}

static void pipewire_export_handle_frame(struct pipewire_export *export, int fd,
    int stride, struct drm_fb *drm_buffer)
{
    const struct isftView_drm_virtual_export_api *api =
        export->pipewire->virtual_export_api;
    int size = export->export->height * stride;
    struct pw_type *t = export->pipewire->t;
    struct pw_buffer *buffer;
    struct spa_buffer *spa_buffer;
    struct spa_meta_header *h;
    void *ptr;

    if (pw_stream_get_state(export->stream, NULL) !=
        PW_STREAM_STATE_STREAMING)
        goto out;

    buffer = pw_stream_dequeue_buffer(export->stream);
    if (!buffer) {
        isftView_log("Failed to dequeue a pipewire buffer\n");
        goto out;
    }

    spa_buffer = buffer->buffer;

    if ((h = spa_buffer_find_meta(spa_buffer, t->meta.Header))) {
        h->pts = -1;
        h->flags = 0;
        h->seq = export->seq++;
        h->dts_offset = 0;
    }

    ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    memcpy(spa_buffer->datas[0].data, ptr, size);
    munmap(ptr, size);

    spa_buffer->datas[0].chunk->offset = 0;
    spa_buffer->datas[0].chunk->stride = stride;
    spa_buffer->datas[0].chunk->size = spa_buffer->datas[0].maxsize;

    pipewire_export_debug(export, "push frame");
    pw_stream_queue_buffer(export->stream, buffer);

out:
    close(fd);
    export->submitted_frame = true;
    api->buffer_released(drm_buffer);
}

static int pipewire_export_fence_sync_handler(int fd, uint32_t mask, void *data)
{
    struct pipewire_frame_data *frame_data = data;
    struct pipewire_export *export = frame_data->export;

    pipewire_export_handle_frame(export, frame_data->fd, frame_data->stride,
                     frame_data->drm_buffer);

    isfttask_source_remove(frame_data->fence_sync_task_source);
    close(frame_data->fence_sync_fd);
    free(frame_data);

    return 0;
}

static int pipewire_export_submit_frame(struct isftView_export *base_export, int fd,
                 int stride, struct drm_fb *drm_buffer)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);
    struct isftView_pipewire *pipewire = export->pipewire;
    const struct isftView_drm_virtual_export_api *api =
        pipewire->virtual_export_api;
    struct isfttask_loop *loop;
    struct pipewire_frame_data *frame_data;
    int fence_sync_fd;

    pipewire_export_debug(export, "submit frame: fd = %d drm_fb = %p",
                  fd, drm_buffer);

    fence_sync_fd = api->get_fence_sync_fd(export->export);
    if (fence_sync_fd == -1) {
        pipewire_export_handle_frame(export, fd, stride, drm_buffer);
        return 0;
    }

    frame_data = zalloc(sizeof *frame_data);
    if (!frame_data) {
        close(fence_sync_fd);
        pipewire_export_handle_frame(export, fd, stride, drm_buffer);
        return 0;
    }

    loop = isftshow_get_task_loop(pipewire->compositor->isftshow);

    frame_data->export = export;
    frame_data->fd = fd;
    frame_data->stride = stride;
    frame_data->drm_buffer = drm_buffer;
    frame_data->fence_sync_fd = fence_sync_fd;
    frame_data->fence_sync_task_source =
        isfttask_loop_add_fd(loop, frame_data->fence_sync_fd,
                     isfttask_READABLE,
                     pipewire_export_fence_sync_handler,
                     frame_data);

    return 0;
}

static void pipewire_export_clock_update(struct pipewire_export *export)
{
    int64_t msec;
    int32_t refresh;

    if (pw_stream_get_state(export->stream, NULL) ==
        PW_STREAM_STATE_STREAMING)
        refresh = export->export->current_mode->refresh;
    else
        refresh = 1000;

    msec = millihz_to_nsec(refresh) / 1000000;
    isfttask_source_clock_update(export->finish_frame_clock, msec);
}

static int pipewire_export_finish_frame_handler(void *data)
{
    struct pipewire_export *export = data;
    const struct isftView_drm_virtual_export_api *api
        = export->pipewire->virtual_export_api;
    struct timespec now;

    if (export->submitted_frame) {
        struct isftView_compositor *c = export->pipewire->compositor;
        export->submitted_frame = false;
        isftView_compositor_read_presentation_clock(c, &now);
        api->finish_frame(export->export, &now, 0);
    }

    if (export->dpms == isftView_DPMS_ON)
        pipewire_export_clock_update(export);
    else
        isfttask_source_clock_update(export->finish_frame_clock, 0);

    return 0;
}

static void pipewire_export_destroy(struct isftView_export *base_export)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);
    struct isftView_mode *mode, *next;

    isftlist_for_each_safe(mode, next, &base_export->mode_list, link) {
        isftlist_remove(&mode->link);
        free(mode);
    }

    export->saved_destroy(base_export);

    pw_stream_destroy(export->stream);

    isftlist_remove(&export->link);
    isftView_head_release(export->head);
    free(export->head);
    free(export);
}

static int pipewire_export_start_repaint_loop(struct isftView_export *base_export)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);

    pipewire_export_debug(export, "start repaint loop");
    export->saved_start_repaint_loop(base_export);

    pipewire_export_clock_update(export);

    return 0;
}

static void pipewire_set_dpms(struct isftView_export *base_export, enum dpms_enum level)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);

    if (export->dpms == level)
        return;

    export->dpms = level;
    pipewire_export_finish_frame_handler(export);
}

static int pipewire_export_connect(struct pipewire_export *export)
{
    struct isftView_pipewire *pipewire = export->pipewire;
    struct type *type = &pipewire->type;
    uint8_t buffer[1024];
    struct spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];
    struct pw_type *t = pipewire->t;
    int frame_rate = export->export->current_mode->refresh / 1000;
    int width = export->export->width;
    int height = export->export->height;
    int ret;

    params[0] = spa_pod_builder_object(&builder,
        t->param.idEnumFormat, t->spa_format,
        "I", type->media_type.video,
        "I", type->media_subtype.raw,
        ":", type->format_video.format,
        "I", type->video_format.BGRx,
        ":", type->format_video.size,
        "R", &SPA_RECTANGLE(width, height),
        ":", type->format_video.framerate,
        "F", &SPA_FRACTION(0, 1),
        ":", type->format_video.max_framerate,
        "Fru", &SPA_FRACTION(frame_rate, 1),
               PROP_RANGE(&SPA_FRACTION(1, 1),
                  &SPA_FRACTION(frame_rate, 1)));

    ret = pw_stream_connect(export->stream, PW_DIRECTION_export, NULL,
                (PW_STREAM_FLAG_DRIVER |
                 PW_STREAM_FLAG_MAP_BUFFERS),
                params, 1);
    if (ret != 0) {
        isftView_log("Failed to connect pipewire stream: %s",
               spa_strerror(ret));
        return -1;
    }

    return 0;
}

static int pipewire_export_enable(struct isftView_export *base_export)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);
    struct isftView_compositor *c = base_export->compositor;
    const struct isftView_drm_virtual_export_api *api
        = export->pipewire->virtual_export_api;
    struct isfttask_loop *loop;
    int ret;

    api->set_submit_frame_cb(base_export, pipewire_export_submit_frame);

    ret = pipewire_export_connect(export);
    if (ret < 0)
        return ret;

    ret = export->saved_enable(base_export);
    if (ret < 0)
        return ret;

    export->saved_start_repaint_loop = base_export->start_repaint_loop;
    base_export->start_repaint_loop = pipewire_export_start_repaint_loop;
    base_export->set_dpms = pipewire_set_dpms;

    loop = isftshow_get_task_loop(c->isftshow);
    export->finish_frame_clock =
        isfttask_loop_add_clock(loop,
                    pipewire_export_finish_frame_handler,
                    export);
    export->dpms = isftView_DPMS_ON;

    return 0;
}

static int pipewire_export_disable(struct isftView_export *base_export)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);

    isfttask_source_remove(export->finish_frame_clock);

    pw_stream_disconnect(export->stream);

    return export->saved_disable(base_export);
}

static void pipewire_export_stream_state_changed(void *data, enum pw_stream_state old,
                     enum pw_stream_state state,
                     const char *error_message)
{
    struct pipewire_export *export = data;

    pipewire_export_debug(export, "state changed %s -> %s",
                  pw_stream_state_as_string(old),
                  pw_stream_state_as_string(state));

    switch (state) {
    case PW_STREAM_STATE_STREAMING:
        isftView_export_schedule_repaint(export->export);
        break;
    default:
        break;
    }
}

static void pipewire_export_stream_format_changed(void *data, const struct spa_pod *format)
{
    struct pipewire_export *export = data;
    struct isftView_pipewire *pipewire = export->pipewire;
    uint8_t buffer[1024];
    struct spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[2];
    struct pw_type *t = pipewire->t;
    int32_t width, height, stride, size;
    const int bpp = 4;

    if (!format) {
        pipewire_export_debug(export, "format = None");
        pw_stream_finish_format(export->stream, 0, NULL, 0);
        return;
    }

    spa_format_video_raw_parse(format, &export->video_format,
                   &pipewire->type.format_video);

    width = export->video_format.size.width;
    height = export->video_format.size.height;
    stride = SPA_ROUND_UP_N(width * bpp, 4);
    size = height * stride;

    pipewire_export_debug(export, "format = %dx%d", width, height);

    params[0] = spa_pod_builder_object(&builder,
        t->param.idBuffers, t->param_buffers.Buffers,
        ":", t->param_buffers.size,
        "i", size,
        ":", t->param_buffers.stride,
        "i", stride,
        ":", t->param_buffers.buffers,
        "iru", 4, PROP_RANGE(2, 8),
        ":", t->param_buffers.align,
        "i", 16);

    params[1] = spa_pod_builder_object(&builder,
        t->param.idMeta, t->param_meta.Meta,
        ":", t->param_meta.type, "I", t->meta.Header,
        ":", t->param_meta.size, "i", sizeof(struct spa_meta_header));

    pw_stream_finish_format(export->stream, 0, params, 2);
}

static const struct pw_stream_tasks stream_tasks = {
    PW_VERSION_STREAM_taskS,
    .state_changed = pipewire_export_stream_state_changed,
    .format_changed = pipewire_export_stream_format_changed,
};

static struct isftView_export *pipewire_export_create(struct isftView_compositor *c, char *name)
{
    struct isftView_pipewire *pipewire = isftView_pipewire_get(c);
    struct pipewire_export *export;
    struct isftView_head *head;
    const struct isftView_drm_virtual_export_api *api;
    const char *make = "isftView";
    const char *model = "Virtual show";
    const char *serial_number = "unknown";
    const char *connector_name = "pipewire";

    if (!name || !strlen(name))
        return NULL;

    api = pipewire->virtual_export_api;

    export = zalloc(sizeof *export);
    if (!export)
        return NULL;

    head = zalloc(sizeof *head);
    if (!head)
        goto err;

    export->stream = pw_stream_new(pipewire->remote, name, NULL);
    if (!export->stream) {
        isftView_log("Cannot initialize pipewire stream\n");
        goto err;
    }

    pw_stream_add_listener(export->stream, &export->stream_listener,
                   &stream_tasks, export);

    export->export = api->create_export(c, name);
    if (!export->export) {
        isftView_log("Cannot create virtual export\n");
        goto err;
    }

    export->saved_destroy = export->export->destroy;
    export->export->destroy = pipewire_export_destroy;
    export->saved_enable = export->export->enable;
    export->export->enable = pipewire_export_enable;
    export->saved_disable = export->export->disable;
    export->export->disable = pipewire_export_disable;
    export->pipewire = pipewire;
    isftlist_insert(pipewire->export_list.prev, &export->link);

    isftView_head_init(head, connector_name);
    isftView_head_set_subpixel(head, isftexport_SUBPIXEL_NONE);
    isftView_head_set_monitor_strings(head, make, model, serial_number);
    head->compositor = c;
    export->head = head;

    isftView_export_attach_head(export->export, head);

    pipewire_export_debug(export, "created");

    return export->export;
err:
    if (export->stream)
        pw_stream_destroy(export->stream);
    if (head)
        free(head);
    free(export);
    return NULL;
}

static bool pipewire_export_is_pipewire(struct isftView_export *export)
{
    return lookup_pipewire_export(export) != NULL;
}

static int pipewire_export_set_mode(struct isftView_export *base_export, const char *modeline)
{
    struct pipewire_export *export = lookup_pipewire_export(base_export);
    const struct isftView_drm_virtual_export_api *api =
        export->pipewire->virtual_export_api;
    struct isftView_mode *mode;
    int n, width, height, refresh = 0;

    if (export == NULL) {
        isftView_log("export is not pipewire.\n");
        return -1;
    }

    if (!modeline)
        return -1;

    n = sscanf(modeline, "%dx%d@%d", &width, &height, &refresh);
    if (n != 2 && n != 3)
        return -1;

    if (pw_stream_get_state(export->stream, NULL) !=
        PW_STREAM_STATE_UNCONNECTED) {
        return -1;
    }

    mode = zalloc(sizeof *mode);
    if (!mode)
        return -1;

    pipewire_export_debug(export, "mode = %dx%d@%d", width, height, refresh);

    mode->flags = isftexport_MODE_CURRENT;
    mode->width = width;
    mode->height = height;
    mode->refresh = (refresh ? refresh : 60) * 1000LL;

    isftlist_insert(base_export->mode_list.prev, &mode->link);

    base_export->current_mode = mode;

    api->set_gbm_format(base_export, "XRGB8888");

    return 0;
}

static void pipewire_export_set_seat(struct isftView_export *export, const char *seat)
{
}

static void isftView_pipewire_destroy(struct isftlistener *l, void *data)
{
    struct isftView_pipewire *pipewire =
        isftcontainer_of(l, pipewire, destroy_listener);

    isftView_log_scope_destroy(pipewire->debug);
    pipewire->debug = NULL;

    isfttask_source_remove(pipewire->loop_source);
    pw_loop_leave(pipewire->loop);
    pw_loop_destroy(pipewire->loop);
}

static struct isftView_pipewire *isftView_pipewire_get(struct isftView_compositor *compositor)
{
    struct isftlistener *listener;
    struct isftView_pipewire *pipewire;

    listener = isftsignal_get(&compositor->destroy_signal,
                 isftView_pipewire_destroy);
    if (!listener)
        return NULL;

    pipewire = isftcontainer_of(listener, pipewire, destroy_listener);
    return pipewire;
}

static int isftView_pipewire_loop_handler(int fd, uint32_t mask, void *data)
{
    struct isftView_pipewire *pipewire = data;
    int ret;

    ret = pw_loop_iterate(pipewire->loop, 0);
    if (ret < 0)
        isftView_log("pipewire_loop_iterate failed: %s",
               spa_strerror(ret));

    return 0;
}

static void isftView_pipewire_state_changed(void *data, enum pw_remote_state old,
                  enum pw_remote_state state, const char *error)
{
    struct isftView_pipewire *pipewire = data;

    pipewire_debug(pipewire, "[remote] state changed %s -> %s",
               pw_remote_state_as_string(old),
               pw_remote_state_as_string(state));

    switch (state) {
    case PW_REMOTE_STATE_ERROR:
        isftView_log("pipewire remote error: %s\n", error);
        break;
    case PW_REMOTE_STATE_CONNECTED:
        isftView_log("connected to pipewire daemon\n");
        break;
    default:
        break;
    }
}


static const struct pw_remote_tasks remote_tasks = {
    PW_VERSION_REMOTE_taskS,
    .state_changed = isftView_pipewire_state_changed,
};

static int isftView_pipewire_init(struct isftView_pipewire *pipewire)
{
    struct isfttask_loop *loop;

    pw_init(NULL, NULL);

    pipewire->loop = pw_loop_new(NULL);
    if (!pipewire->loop)
        return -1;

    pw_loop_enter(pipewire->loop);

    pipewire->core = pw_core_new(pipewire->loop, NULL);
    pipewire->t = pw_core_get_type(pipewire->core);
    init_type(&pipewire->type, pipewire->t->map);

    pipewire->remote = pw_remote_new(pipewire->core, NULL, 0);
    pw_remote_add_listener(pipewire->remote,
        &pipewire->remote_listener,
        &remote_tasks, pipewire);

    pw_remote_connect(pipewire->remote);

    while (true) {
        enum pw_remote_state state;
        const char *error = NULL;
        int ret;

        state = pw_remote_get_state(pipewire->remote, &error);
        if (state == PW_REMOTE_STATE_CONNECTED)
            break;

        if (state == PW_REMOTE_STATE_ERROR) {
            isftView_log("pipewire error: %s\n", error);
            goto err;
        }

        ret = pw_loop_iterate(pipewire->loop, -1);
        if (ret < 0) {
            isftView_log("pipewire_loop_iterate failed: %s",
                   spa_strerror(ret));
            goto err;
        }
    }

    loop = isftshow_get_task_loop(pipewire->compositor->isftshow);
    pipewire->loop_source =
        isfttask_loop_add_fd(loop, pw_loop_get_fd(pipewire->loop),
                     isfttask_READABLE,
                     isftView_pipewire_loop_handler,
                     pipewire);

    return 0;
err:
    if (pipewire->remote)
        pw_remote_destroy(pipewire->remote);
    pw_loop_leave(pipewire->loop);
    pw_loop_destroy(pipewire->loop);
    return -1;
}

static const struct isftView_pipewire_api pipewire_api = {
    pipewire_export_create,
    pipewire_export_is_pipewire,
    pipewire_export_set_mode,
    pipewire_export_set_seat,
};

isftEXPORT int isftView_module_init(struct isftView_compositor *compositor)
{
    int ret;
    struct isftView_pipewire *pipewire;
    const struct isftView_drm_virtual_export_api *api =
        isftView_drm_virtual_export_get_api(compositor);

    if (!api)
        return -1;

    pipewire = zalloc(sizeof *pipewire);
    if (!pipewire)
        return -1;

    if (!isftView_compositor_add_destroy_listener_once(compositor,
                             &pipewire->destroy_listener,
                             isftView_pipewire_destroy)) {
        free(pipewire);
        return 0;
    }

    pipewire->virtual_export_api = api;
    pipewire->compositor = compositor;
    isftlist_init(&pipewire->export_list);

    ret = isftView_plugin_api_register(compositor, isftView_PIPEWIRE_API_NAME,
                     &pipewire_api, sizeof(pipewire_api));

    if (ret < 0) {
        isftView_log("Failed to register pipewire API.\n");
        isftlist_remove(&pipewire->destroy_listener.link);
        free(pipewire);
        return -1;
    }

    ret = isftView_pipewire_init(pipewire);
    if (ret < 0) {
        isftView_log("Failed to initialize pipewire.\n");
        isftlist_remove(&pipewire->destroy_listener.link);
        free(pipewire);
        return -1;
    }

    pipewire->debug =
        isftView_compositor_add_log_scope(compositor, "pipewire",
                        "Debug messages from pipewire plugin\n",
                        NULL, NULL, NULL);

    return 0;

}
