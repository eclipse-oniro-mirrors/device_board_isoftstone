/*
* Copyright (c) 2021-2022 isoftstone Device Co., Ltd.
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

#include "pipewire-plugin.h"
#include <libisftView/backend-drm.h>
#include <libisftView/isftView-log.h>
#include <sys/mman.h>
#include "backend.h"
#include "libisftView-internal.h"
#include "shared/timespec-util.h"

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

struct isftViewpipewire {
    struct isftViewcompositor *compositor;
    struct isftlist export_list;
    struct isftlistener destroylistener;
    const struct isftViewdrmvirtualexportapi *virtualexportapi;

    struct isftView_log_scope *debug;

    struct pw_loop *loop;
    struct isfttask_source *loopsource;

    struct pw_core *core;
    struct pw_type *t;
    struct type type;

    struct pw_remote *remote;
    struct spa_hook remote_listener;
};

struct pipewireexport {
    struct isftViewexport *export;
    void (*saved_destroy)(struct isftViewexport *export);
    int (*saved_enable)(struct isftViewexport *export);
    int (*saved_disable)(struct isftViewexport *export);
    int (*saved_start_repaint_loop)(struct isftViewexport *export);

    struct isftView_head *head;

    struct isftViewpipewire *pipewire;

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
    struct pipewireexport *export;
    int fd;
    int stride;
    struct drm_fb *drm_buffer;
    int fence_sync_fd;
    struct isfttask_source *fence_sync_task_source;
};

static inline void inittype(struct type *type, struct spa_type_map *map)
{
    spa_type_media_type_map(map, &type->media_type);
    spa_type_media_subtype_map(map, &type->media_subtype);
    spa_type_format_video_map(map, &type->format_video);
    spa_type_video_format_map(map, &type->video_format);
}

static void pipewire_debug_impl(struct isftViewpipewire *pipewire,
    struct pipewireexport *export,
    const char *fmt, va_list ap)
{
    FILE *fp;
    char *logstr;
    int logsize;
    char timestr[128];

    if (!isftView_log_scope_is_enabled(pipewire->debug)) {
        return;
    }
    fp = open_memstream(&logstr, &logsize);
    if (!fp) {
        return;
    }

    isftView_log_scope_timestamp(pipewire->debug, timestr, sizeof timestr);
    int ret = fprintf(fp, "%s", timestr);
        if (ret < 0) {
            printf("sprintf error");

    if (export) {
        fprintf(fp, "[%s]", export->export->name);
    }

    int ret = fprintf(fp, " ");
        if (ret < 0) {
            printf("sprintf error");
    vfprintf(fp, fmt, ap);
    int ret = fprintf(fp, "\n");
        if (ret < 0) {
            printf("sprintf error");

    if (fclose(fp) == 0) {
        isftView_log_scope_write(pipewire->debug, logstr, logsize);
    }

    free(logstr);
}

static void pipewiredebug(struct isftViewpipewire *pipewire, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pipewire_debug_impl(pipewire, NULL, fmt, ap);
    va_end(ap);
}

static void PipewireExportDebug(struct pipewireexport *export, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    pipewire_debug_impl(export->pipewire, export, fmt, ap);
    va_end(ap);
}

static struct isftViewpipewire *isftView_pipewire_get(struct isftViewcompositor *compositor);

static struct pipewireexport *lookup_pipewire_export(struct isftViewexport *base_export)
{
    struct isftViewcompositor *c = base_export->compositor;
    struct isftViewpipewire *pipewire = isftView_pipewire_get(c);
    struct pipewireexport *export;

    isftlist_for_each(export, &pipewire->export_list, link) {
        if (export->export == base_export) {
            return export;
        }
    }
    return NULL;
}

static void pipewire_export_handle_frame(struct pipewireexport *export, int fd,
    int stride, struct drm_fb *drm_buffer)
{
    const struct isftViewdrmvirtualexportapi *api =
        export->pipewire->virtualexportapi;
    int size = export->export->height * stride;
    struct pw_type *t = export->pipewire->t;
    struct pw_buffer *buffer;
    struct spa_buffer *spa_buffer;
    struct spa_meta_header *h;
    void *ptr;

    if (pw_stream_get_state(export->stream, NULL) !=
        PW_STREAM_STATE_STREAMING) {
        close(fd);
        export->submitted_frame = true;
        api->buffer_released(drm_buffer);
        }

    buffer = pw_stream_dequeue_buffer(export->stream);
    if (!buffer) {
        isftViewlog("Failed to dequeue a pipewire buffer\n");
    }
        close(fd);
        export->submitted_frame = true;
        api->buffer_released(drm_buffer);
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

    PipewireExportDebug(export, "push frame");
    pw_stream_queue_buffer(export->stream, buffer);
}

static int pipewire_export_fence_sync_handler(int fd, uint32_t mask, void data[])
{
    struct pipewire_frame_data *frame_data = data;
    struct pipewireexport *export = frame_data->export;

    pipewire_export_handle_frame(export, frame_data->fd, frame_data->stride,
                     frame_data->drm_buffer);

    isfttask_source_remove(frame_data->fence_sync_task_source);
    close(frame_data->fence_sync_fd);
    free(frame_data);

    return 0;
}

static int pipewire_export_submit_frame(struct isftViewexport *base_export, int fd,
    int stride, struct drm_fb *drm_buffer)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);
    struct isftViewpipewire *pipewire = export->pipewire;
    const struct isftViewdrmvirtualexportapi *api =
        pipewire->virtualexportapi;
    struct isfttask_loop *loop;
    struct pipewire_frame_data *frame_data;
    int fence_sync_fd;

    PipewireExportDebug(export, "submit frame: fd = %d drm_fb = %p",
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

    loop = isftshowgettaskloop(pipewire->compositor->isftshow);

    frame_data->export = export;
    frame_data->fd = fd;
    frame_data->stride = stride;
    frame_data->drm_buffer = drm_buffer;
    frame_data->fence_sync_fd = fence_sync_fd;
    frame_data->fence_sync_task_source =
        isfttaskloopaddfd(loop, frame_data->fence_sync_fd,
                     isfttask_READABLE,
                     pipewire_export_fence_sync_handler,
                     frame_data);

    return 0;
}

#define NUMA 1000
#define NUMB 1000000
static void pipewire_export_clock_update(struct pipewireexport *export)
{
    int64_t msec;
    int32_t refresh;

    if (pw_stream_get_state(export->stream, NULL) ==
        PW_STREAM_STATE_STREAMING)
        refresh = export->export->current_mode->refresh;
    else
        refresh = NUMA;

    msec = millihz_to_nsec(refresh) / NUMB;
    isfttask_source_clock_update(export->finish_frame_clock, msec);
}

static int pipewire_export_finish_frame_handler(void data[])
{
    struct pipewireexport *export = data;
    const struct isftViewdrmvirtualexportapi *api
        = export->pipewire->virtualexportapi;
    struct timespec now;

    if (export->submitted_frame) {
        struct isftViewcompositor *c = export->pipewire->compositor;
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

static void pipewire_export_destroy(struct isftViewexport *base_export)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);
    struct isftView_mode *mode, *next;

    isftlist_for_each_safe(mode, next, &base_export->mode_list, link) {
        isftlistremove(&mode->link);
        free(mode);
    }

    export->saved_destroy(base_export);

    pw_stream_destroy(export->stream);

    isftlistremove(&export->link);
    isftView_head_release(export->head);
    free(export->head);
    free(export);
}

static int pipewire_export_start_repaint_loop(struct isftViewexport *base_export)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);

    PipewireExportDebug(export, "start repaint loop");
    export->saved_start_repaint_loop(base_export);

    pipewire_export_clock_update(export);

    return 0;
}

static void pipewire_set_dpms(struct isftViewexport *base_export, enum dpms_enum level)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);

    if (export->dpms == level)
        return;

    export->dpms = level;
    pipewire_export_finish_frame_handler(export);
}

static int pipewire_export_connect(struct pipewireexport *export)
{
    struct isftViewpipewire *pipewire = export->pipewire;
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
        isftViewlog("Failed to connect pipewire stream: %s",
            spa_strerror(ret));
        return -1;
    }

    return 0;
}

static int pipewire_export_enable(struct isftViewexport *base_export)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);
    struct isftViewcompositor *c = base_export->compositor;
    const struct isftViewdrmvirtualexportapi *api
        = export->pipewire->virtualexportapi;
    struct isfttask_loop *loop;
    int ret;

    api->set_submit_frame_cb(base_export, pipewire_export_submit_frame);

    ret = pipewire_export_connect(export);
    if (ret < 0) {
        return ret;
    }
    ret = export->saved_enable(base_export);
    if (ret < 0) {
        return ret;
    }

    export->saved_start_repaint_loop = base_export->start_repaint_loop;
    base_export->start_repaint_loop = pipewire_export_start_repaint_loop;
    base_export->set_dpms = pipewire_set_dpms;

    loop = isftshowgettaskloop(c->isftshow);
    export->finish_frame_clock =
        isfttask_loop_add_clock(loop,
                    pipewire_export_finish_frame_handler,
                    export);
    export->dpms = isftView_DPMS_ON;

    return 0;
}

static int pipewire_export_disable(struct isftViewexport *base_export)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);

    isfttask_source_remove(export->finish_frame_clock);

    pw_stream_disconnect(export->stream);

    return export->saved_disable(base_export);
}

static void pipewire_export_stream_state_changed(void data[], enum pw_stream_state old,
    enum pw_stream_state state,
    const char *error_message)
{
    struct pipewireexport *export = data;

    PipewireExportDebug(export, "state changed %s -> %s",
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

static void pipewireexportstreamformatchanged(void data[], const struct spa_pod *format)
{
    struct pipewireexport *export = data;
    struct isftViewpipewire *pipewire = export->pipewire;
    uint8_t buffer[1024];
    struct spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[2];
    struct pw_type *t = pipewire->t;
    int32_t width, height, stride, size;
    const int bpp = 4;

    if (!format) {
        PipewireExportDebug(export, "format = None");
        pw_stream_finish_format(export->stream, 0, NULL, 0);
        return;
    }

    spa_format_video_raw_parse(format, &export->video_format,
        &pipewire->type.format_video);

    width = export->video_format.size.width;
    height = export->video_format.size.height;
    stride = SPA_ROUND_UP_N(width * bpp, 4);
    size = height * stride;

    PipewireExportDebug(export, "format = %dx%d", width, height);

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
    .format_changed = pipewireexportstreamformatchanged,
};

static struct isftViewexport *pipewire_export_create(struct isftViewcompositor *c, char *name)
{
    struct isftViewpipewire *pipewire = isftView_pipewire_get(c);
    struct pipewireexport *export;
    struct isftView_head *head;
    const struct isftViewdrmvirtualexportapi *api;
    const char *make = "isftView";
    const char *model = "Virtual show";
    const char *serial_number = "unknown";
    const char *connector_name = "pipewire";

    if (!name || !strlen(name)) {
        return NULL;
    }

    api = pipewire->virtualexportapi;

    export = zalloc(sizeof *export);
    if (!export) {
        return NULL;
    }

    head = zalloc(sizeof *head);
    if (!head) {
        if (export->stream) {
            pw_stream_destroy(export->stream);
        }
        if (head) {
            free(head);
        }
        free(export);
        return NULL;
    }

    export->stream = pw_stream_new(pipewire->remote, name, NULL);
    if (!export->stream) {
        isftViewlog("Cannot initialize pipewire stream\n");
        if (export->stream) {
            pw_stream_destroy(export->stream);
        }
        if (head) {
            free(head);
        }
        free(export);
        return NULL;
    }

    pw_stream_add_listener(export->stream, &export->stream_listener,
        &stream_tasks, export);

    export->export = api->create_export(c, name);
    if (!export->export) {
        isftViewlog("Cannot create virtual export\n");
        if (export->stream) {
            pw_stream_destroy(export->stream);
        }
        if (head) {
            free(head);
        }
        free(export);
        return NULL;
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

    PipewireExportDebug(export, "created");

    return export->export;
}
static bool pipewireexportispipewire(struct isftViewexport *export)
{
    return lookup_pipewire_export(export) != NULL;
}

#define NUMC 2
#define NUMD 3
#define NUME 60
static int pipewireexportsetmode(struct isftViewexport *base_export, const char *modeline)
{
    struct pipewireexport *export = lookup_pipewire_export(base_export);
    const struct isftViewdrmvirtualexportapi *api =
        export->pipewire->virtualexportapi;
    struct isftView_mode *mode;
    int n, width, height, refresh = 0;

    if (export == NULL) {
        isftViewlog("export is not pipewire.\n");
        return -1;
    }

    if (!modeline) {
        return -1;
    }

    n = sscanf(modeline, "%dx%d@%d", &width, &height, &refresh);
    if (n != NUMC && n != NUMD) {
        return -1;
    }

    if (pw_stream_get_state(export->stream, NULL) !=
        PW_STREAM_STATE_UNCONNECTED) {
        return -1;
    }

    mode = zalloc(sizeof *mode);
    if (!mode) {
        return -1;
    }
    PipewireExportDebug(export, "mode = %dx%d@%d", width, height, refresh);

    mode->flags = isftexport_MODE_CURRENT;
    mode->width = width;
    mode->height = height;
    mode->refresh = (refresh ? refresh : NUME) * 1000LL;

    isftlist_insert(base_export->mode_list.prev, &mode->link);

    base_export->current_mode = mode;

    api->set_gbm_format(base_export, "XRGB8888");

    return 0;
}

static void pipewireexportsetseat(struct isftViewexport *export, const char *seat)
{
}

static void isftViewpipewiredestroy(struct isftlistener *l, void data[])
{
    struct isftViewpipewire *pipewire =
        isftcontainer_of(l, pipewire, destroylistener);

    isftView_log_scope_destroy(pipewire->debug);
    pipewire->debug = NULL;

    isfttask_source_remove(pipewire->loopsource);
    pwloopleave(pipewire->loop);
    pwloopdestroy(pipewire->loop);
}

static struct isftViewpipewire *isftView_pipewire_get(struct isftViewcompositor *compositor)
{
    struct isftlistener *listener;
    struct isftViewpipewire *pipewire;

    listener = isftsignal_get(&compositor->destroy_signal,
        isftViewpipewiredestroy);
    if (!listener) {
        return NULL;
    }

    pipewire = isftcontainer_of(listener, pipewire, destroylistener);
    return pipewire;
}

static int isftViewpipewireloophandler(int fd, uint32_t mask, void data[])
{
    struct isftViewpipewire *pipewire = data;
    int ret;

    ret = pwloopiterate(pipewire->loop, 0);
    if (ret < 0) {
        isftViewlog("pipewire_loop_iterate failed: %s", spa_strerror(ret));
    }

    return 0;
}

static void isftViewpipewirestatechanged(void data[], enum pw_remote_state old,
    enum pwremotestate state, const char *error)
{
    struct isftViewpipewire *pipewire = data;

    pipewiredebug(pipewire, "[remote] state changed %s -> %s",
        pw_remote_state_as_string(old),
        pw_remote_state_as_string(state));

    switch (state) {
        case PW_REMOTE_STATE_ERROR:
            isftViewlog("pipewire remote error: %s\n", error);
            break;
        case PW_REMOTE_STATE_CONNECTED:
            isftViewlog("connected to pipewire daemon\n");
            break;
        default:
            break;
    }
}

static const struct pw_remote_tasks remote_tasks = {
    PW_VERSION_REMOTE_taskS,
    .state_changed = isftViewpipewirestatechanged,
};

static int isftViewpipewireinit(struct isftViewpipewire *pipewire)
{
    struct isfttask_loop *loop;

    pwinit(NULL, NULL);

    pipewire->loop = pw_loop_new(NULL);
    if (!pipewire->loop) {
        return -1;
    }

    pwloopenter(pipewire->loop);

    pipewire->core = pw_core_new(pipewire->loop, NULL);
    pipewire->t = pw_core_get_type(pipewire->core);
    inittype(&pipewire->type, pipewire->t->map);

    pipewire->remote = pwremotenew(pipewire->core, NULL, 0);
    pwremoteaddlistener(pipewire->remote, &pipewire->remote_listener, &remote_tasks, pipewire);

    pwremoteconnect(pipewire->remote);

    while (true) {
        enum pwremotestate state;
        const char *error = NULL;
        int ret;

        state = pwremotegetstate(pipewire->remote, &error);
        if (state == PW_REMOTE_STATE_CONNECTED)
            break;

        if (state == PW_REMOTE_STATE_ERROR) {
            isftViewlog("pipewire error: %s\n", error);
            if (pipewire->remote) {
                pw_remote_destroy(pipewire->remote);
            }
        }

        ret = pwloopiterate(pipewire->loop, -1);
        if (ret < 0) {
            isftViewlog("pipewire_loop_iterate failed: %s",
                spa_strerror(ret));
            if (pipewire->remote) {
                pw_remote_destroy(pipewire->remote);
            }
        }
    }

    loop = isftshowgettaskloop(pipewire->compositor->isftshow);
    pipewire->loopsource = isfttaskloopaddfd(loop, pw_loop_get_fd(pipewire->loop),
        isfttask_READABLE, isftViewpipewireloophandler, pipewire);

    return 0;
    pwloopleave(pipewire->loop);
    pwloopdestroy(pipewire->loop);
    return -1;
}

static const struct isftViewpipewireapi pipewireapi = {
    pipewire_export_create,
    pipewireexportispipewire,
    pipewireexportsetmode,
    pipewireexportsetseat,
};

isftEXPORT int isftViewmoduleinit(struct isftViewcompositor *compositor)
{
    int ret;
    struct isftViewpipewire *pipewire;
    const struct isftViewdrmvirtualexportapi *api =
        isftViewdrmvirtualexportgetapi(compositor);

    if (!api) {
        return -1;
    }

    pipewire = zalloc(sizeof *pipewire);
    if (!pipewire) {
        return -1;
    }

    if (!isftViewcompositoradddestroylisteneronce(compositor,
        &pipewire->destroylistener, isftViewpipewiredestroy)) {
        free(pipewire);
        return 0;
    }

    pipewire->virtualexportapi = api;
    pipewire->compositor = compositor;
    isftlistinit(&pipewire->export_list);

    ret = isftView_plugin_api_register(compositor, isftView_PIPEWIRE_API_NAME,
        &pipewireapi, sizeof(pipewireapi));

    if (ret < 0) {
        isftViewlog("Failed to register pipewire API.\n");
        isftlistremove(&pipewire->destroylistener.link);
        free(pipewire);
        return -1;
    }

    ret = isftViewpipewireinit(pipewire);
    if (ret < 0) {
        isftViewlog("Failed to initialize pipewire.\n");
        isftlistremove(&pipewire->destroylistener.link);
        free(pipewire);
        return -1;
    }

    pipewire->debug = isftViewcompositoraddlogscope(compositor, "pipewire",
        "Debug messages from pipewire plugin\n", NULL, NULL, NULL);

    return 0;
}
