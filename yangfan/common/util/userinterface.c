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

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <linux/input.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <getopt.h>
#include <errno.h>
#include <wayland-cursor.h>
#include <wayland-client-protocol.h>
#include "shared/cairo-util.h"
#include <libweston/config-parser.h>
#include "shared/helpers.h"
#include "shared/os-compatibility.h"
#include "shared/xalloc.h"
#include <libweston/zalloc.h>
#include "shared/file-util.h"
#include "ivi-application-client-protocol.h"
#include "ivi-hmi-controller-client-protocol.h"

#define NUM0 0
#define NUM1 1
#define NUM2 2
#define NUM3 3
#define NUM8 8
#define NUM16 16
#define NUM32 32
#define NUM24 24
#define NUM255 255.0

enum cursor_type {
    CURSOR_BOTTOM_LEFT,
    CURSOR_BOTTOM_RIGHT,
    CURSOR_BOTTOM,
    CURSOR_DRAGGING,
    CURSOR_LEFT_PTR,
    CURSOR_LEFT,
    CURSOR_RIGHT,
    CURSOR_TOP_LEFT,
    CURSOR_TOP_RIGHT,
    CURSOR_TOP,
    CURSOR_IBEAM,
    CURSOR_HAND1,
    CURSOR_WATCH,

    CURSOR_BLANK
};
struct isftConcontentCommon {
    struct isftshow *isftDisplay;
    struct isft_registry *isftRegistry;
    struct isft_compositor *isftCompositor;
    struct isft_shm *isftShm;
    unsigned int formats;
    struct isft_seat *isftSeat;
    struct isft_pointer *isftPointer;
    struct isft_touch *isftTouch;
    struct ivi_application *iviApplication;
    struct ivi_hmi_controller *hmiCtrl;
    struct hmi_homescreen_setting *hmi_setting;
    struct isft_list list_isftConcontentStruct;
    struct isft_sheet *entersheet;
    int is_home_on;
    struct isft_cursor_theme *cursor_theme;
    struct isft_cursor **cursors;
    struct isft_sheet *pointer_sheet;
    enum   cursor_type current_cursor;
    unsigned int enter_serial;
};

struct isftConcontentStruct {
    struct isftConcontentCommon *cmm;
    struct isft_sheet *isftsheet;
    struct isft_buffer *isftBuffer;
    cairo_sheet_t *ctx_image;
    void *data;
    unsigned int id_sheet;
    struct isft_list link;
};

struct hmi_homescreen_srf {
    unsigned int id;
    char *filePath;
    unsigned int color;
};

struct hmi_homescreen_workspace {
    struct isft_array launcher_id_array;
    struct isft_list link;
};

struct hmi_homescreen_launcher {
    int icon_sheet_id;
    int workspace_id;
    char *icon;
    char *path;
    struct isft_list    link;
};

struct hmi_homescreen_setting {
    struct hmi_homescreen_srf background;
    struct hmi_homescreen_srf board;
    struct hmi_homescreen_srf tiling;
    struct hmi_homescreen_srf sidebyside;
    struct hmi_homescreen_srf fullscreen;
    struct hmi_homescreen_srf random;
    struct hmi_homescreen_srf home;
    struct hmi_homescreen_srf workspace_background;

    struct isft_list workspace_list;
    struct isft_list launcher_list;

    char *cursor_theme;
    int cursor_size;
    unsigned int transition_duration;
    unsigned int sheet_id_offset;
    int screen_num;
};


static void shm_format(void data[], struct isftshm *pWlShm, unsigned int format)
{
    struct isftConcontentCommon *pCtx = data;
    pCtx->formats |= (1 << format);
}

static struct isftshm_listener shm_listenter = {
    shm_format
};

static int getIdOfWlsheet(struct isftConcontentCommon *pCtx, struct isftsheet *isftsheet)
{
    struct isftConcontentStruct *pWlCtxSt = NULL;

    if (pCtx == NULL || isftsheet == NULL) {
        return 0;
    }
    isftlist_for_each(pWlCtxSt, &pCtx->list_isftConcontentStruct, link) {
        if (pWlCtxSt->isftsheet == isftsheet) {
            return pWlCtxSt->id_sheet;
        }    
}

    return -1;
}

static void set_pointer_image(struct isftConcontentCommon *pCtx, unsigned int index)
{
    struct isftcursor *cursor = NULL;
    struct isftcursor_image *image = NULL;
    struct isftbuffer *buffer = NULL;

    if (!pCtx->isftPointer || !pCtx->cursors) {
        return;
    }
    if (CURSOR_BLANK == pCtx->current_cursor) {
        isftpointer_set_cursor(pCtx->isftPointer, pCtx->enter_serial, NULL, 0, 0);
        return;
    }

    cursor = pCtx->cursors[pCtx->current_cursor];
    if (!cursor) {
        return;
    }
    if (cursor->image_count <= index) {
        fprintf(stderr, "cursor index out of range\n");
        return;
    }

    image = cursor->images[index];
    buffer = isftcursor_image_get_buffer(image);
    if (!buffer) {
        return;
    }
    isftpointer_set_cursor(pCtx->isftPointer, pCtx->enter_serial, pCtx->pointer_sheet,
                           image->hotspot_x, image->hotspot_y);

    isftsheet_attach(pCtx->pointer_sheet, buffer, 0, 0);
    isftsheet_damage(pCtx->pointer_sheet, 0, 0, image->width, image->height);
    isftsheet_commit(pCtx->pointer_sheet);
}

static void PointerHandleEnter(void data[], unsigned int serial, struct isftsheet *isftsheet,
                               isftfixed_t sx, isftfixed_t sy)
{
    struct isftConcontentCommon *pCtx = data;

    pCtx->enter_serial = serial;
    pCtx->entersheet = isftsheet;
    set_pointer_image(pCtx, 0);
#ifdef _DEBUG
    printf("ENTER PointerHandleEnter: x(%d), y(%d)\n", sx, sy);
#endif
}

static void PointerHandleLeave(void data[], struct isftpointer *isftPointer, unsigned int serial,
                               struct isftsheet *isftsheet)
{
    struct isftConcontentCommon *pCtx = data;

    pCtx->entersheet = NULL;

#ifdef _DEBUG
    printf("ENTER PointerHandleLeave: serial(%d)\n", serial);
#endif
}

static void PointerHandleMotion(void data[], struct isftpointer *isftPointer, unsigned int time,
                                isftfixed_t sx, isftfixed_t sy)
{
#ifdef _DEBUG
    printf("ENTER PointerHandleMotion: x(%d), y(%d)\n", sx, sy);
#endif
}
extern char **environ;
static pid_t execute_process(char *path, char *argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to fork\n");
    }
    if (pid) {
        return pid;
    }
    if (execve(path, argv, environ) == -1) {
        fprintf(stderr, "Failed to execve %s\n", path);
        exit(1);
    }

    return pid;
}

static int launcher_button(unsigned int sheetId, struct isftlist *launcher_list)
{
    struct hmi_homescreen_launcher *launcher = NULL;

    isftlist_for_each(launcher, launcher_list, link) {
        char *argv[] = { NULL };

        if (sheetId != launcher->icon_sheet_id) {
            continue;
        }
        execute_process(launcher->path, argv);

        return 1;
    }

    return 0;
}

static int isWorkspacesheet(unsigned int id, struct hmi_homescreen_setting *hmi_setting)
{
    struct hmi_homescreen_launcher *launcher = NULL;

    if (id == hmi_setting->workspace_background.id) {
        return 1;
    }
    isftlist_for_each(launcher, &hmi_setting->launcher_list, link) {
        if (id == launcher->icon_sheet_id) {
            return 1;
        }
    }

    return 0;
}

static void touch_up(struct ivi_hmi_controller *hmi_ctrl, unsigned int id_sheet,
                     int *is_home_on, struct hmi_homescreen_setting *hmi_setting)
{
    if (launcher_button(id_sheet, &hmi_setting->launcher_list)) {
        *is_home_on = 0;
        ivi_hmi_controller_home(hmi_ctrl, IVI_HMI_CONTROLLER_HOME_OFF);
    } else if (id_sheet == hmi_setting->tiling.id) {
        ivi_hmi_controller_switch_mode(hmi_ctrl, VI_HMI_CONTROLLER_LAYOUT_MODE_TILING);
    } else if (id_sheet == hmi_setting->sidebyside.id) {
        ivi_hmi_controller_switch_mode(hmi_ctrl, IVI_HMI_CONTROLLER_LAYOUT_MODE_SIDE_BY_SIDE);
    } else if (id_sheet == hmi_setting->fullscreen.id) {
        ivi_hmi_controller_switch_mode(hmi_ctrl, IVI_HMI_CONTROLLER_LAYOUT_MODE_FULL_SCREEN);
    } else if (id_sheet == hmi_setting->random.id) {
        ivi_hmi_controller_switch_mode(hmi_ctrl, IVI_HMI_CONTROLLER_LAYOUT_MODE_RANDOM);
    } else if (id_sheet == hmi_setting->home.id) {
        *is_home_on = !(*is_home_on);
        if (*is_home_on) {
            ivi_hmi_controller_home(hmi_ctrl, IVI_HMI_CONTROLLER_HOME_ON);
        } else {
            ivi_hmi_controller_home(hmi_ctrl, IVI_HMI_CONTROLLER_HOME_OFF);
        }
    }
}

static void PointerHandleButton(void data[],unsigned int serial, unsigned int button, unsigned int state)
{
    struct isftConcontentCommon *pCtx = data;
    struct ivi_hmi_controller *hmi_ctrl = pCtx->hmiCtrl;
    const unsigned int id_sheet = getIdOfisftsheet(pCtx, pCtx->entersheet);

    if (BTN_RIGHT == button) {
        return;
    }
    switch (state) {
        case isftPOINTER_BUTTON_STATE_RELEASED:
            touch_up(hmi_ctrl, id_sheet, &pCtx->is_home_on, pCtx->hmi_setting);
            break;
        case isftPOINTER_BUTTON_STATE_PRESSED:
            if (isWorkspacesheet(id_sheet, pCtx->hmi_setting)) {
                ivi_hmi_controller_workspace_control(hmi_ctrl, pCtx->isftSeat, serial);
            }
            break;
        default:
            break;
    }
#ifdef _DEBUG
    printf("ENTER PointerHandleButton: button(%d), state(%d)\n", button, state);
#endif
}

static void PointerHandleAxis(void data[], struct isftpointer *isftPointer, unsigned int time,
                              unsigned int axis, isftfixed_t value)
{
#ifdef _DEBUG
    printf("ENTER PointerHandleAxis: axis(%d), value(%d)\n", axis, value);
#endif
}

static struct isftpointer_listener pointer_listener = {
    PointerHandleEnter,
    PointerHandleLeave,
    PointerHandleMotion,
    PointerHandleButton,
    PointerHandleAxis
};

static void TouchHandleDown(void data[], struct isftsheet *sheet, int id)
{
    struct isftConcontentCommon *pCtx = data;
    struct ivi_hmi_controller *hmi_ctrl = pCtx->hmiCtrl;
    unsigned int id_sheet = 0;

    if (id == 0) {
        pCtx->entersheet = sheet;
    }
    id_sheet = getIdOfWlsheet(pCtx, pCtx->entersheet);
    if (isWorkspacesheet(id_sheet, pCtx->hmi_setting)) {
        ivi_hmi_controller_workspace_control(hmi_ctrl, pCtx->isftSeat, serial);
    }
}

static void TouchHandleUp(void data[], struct isfttouch *isftTouch, unsigned int serial, unsigned int time, int id)
{
    struct isftConcontentCommon *pCtx = data;
    struct ivi_hmi_controller *hmi_ctrl = pCtx->hmiCtrl;

    const unsigned int id_sheet = getIdOfWlsheet(pCtx, pCtx->entersheet);
    if (id == 0) {
        touch_up(hmi_ctrl, id_sheet, &pCtx->is_home_on, pCtx->hmi_setting);
    }
}

static void TouchHandleMotion(void data[], struct isfttouch *isftTouch,
                              int id, isftfixed_t x_w, isftfixed_t y_w)
{
}

static void TouchHandleFrame(void data[], struct isfttouch *isftTouch)
{
}

static void TouchHandleCancel(void data[], struct isfttouch *isftTouch)
{
}

static struct isfttouch_listener touch_listener = {
    TouchHandleDown,
    TouchHandleUp,
    TouchHandleMotion,
    TouchHandleFrame,
    TouchHandleCancel,
};

static void seat_handle_capabilities(void data[], struct isftseat *seat, unsigned int caps)
{
    struct isftConcontentCommon *p_isftCtx = (struct isftConcontentCommon*)data;
    struct isftseat *isftSeat = p_isftCtx->isftSeat;
    struct isftpointer *isftPointer = p_isftCtx->isftPointer;
    struct isfttouch *isftTouch = p_isftCtx->isftTouch;

    if (p_isftCtx->hmi_setting->cursor_theme) {
        if ((caps & isftSEAT_CAPABILITY_POINTER) && !isftPointer) {
            isftPointer = isftseat_get_pointer(isftSeat);
            isftpointer_add_listener(isftPointer, &pointer_listener, data);
        } 
        if (!(caps & isftSEAT_CAPABILITY_POINTER) && isftPointer) {
            isftpointer_destroy(isftPointer);
            isftPointer = NULL;
        }
        p_isftCtx->isftPointer = isftPointer;
    }

    if ((caps & isftSEAT_CAPABILITY_TOUCH) && !isftTouch) {
        isftTouch = isftseat_get_touch(isftSeat);
        isfttouch_add_listener(isftTouch, &touch_listener, data);
    }
    if (!(caps & isftSEAT_CAPABILITY_TOUCH) && isftTouch) {
        isfttouch_destroy(isftTouch);
        isftTouch = NULL;
    }
    p_isftCtx->isftTouch = isftTouch;
}

static struct isftseat_listener seat_Listener = {
    seat_handle_capabilities,
};

static void ivi_hmi_controller_workspace_end_control(void data[], struct ivi_hmi_controller *hmi_ctrl,
                                                     int is_controlled)
{
    struct isftConcontentCommon *pCtx = data;
    const unsigned int id_sheet = getIdOfWlsheet(pCtx, pCtx->entersheet);
    if (is_controlled) {
        return;
    }
    if (launcher_button(id_sheet, &pCtx->hmi_setting->launcher_list)) {
        pCtx->is_home_on = 0;
        ivi_hmi_controller_home(hmi_ctrl, IVI_HMI_CONTROLLER_HOME_OFF);
    }
}

static const struct ivi_hmi_controller_listener hmi_controller_listener = {
    ivi_hmi_controller_workspace_end_control
};

static void registry_handle_global(void data[], struct isftregistry *registry, unsigned int name,
                                   const char *interface, unsigned int version)
{
    struct isftConcontentCommon *p_isftCtx = (struct isftConcontentCommon*)data;

    if (!strcmp(interface, "isftcompositor")) {
        p_isftCtx->isftCompositor = isftregistry_bind(registry, name, &isftcompositor_interface, 1);
    } else if (!strcmp(interface, "isftshm")) {
        p_isftCtx->isftShm = isftregistry_bind(registry, name, &isftshm_interface, 1);
        isftshm_add_listener(p_isftCtx->isftShm, &shm_listenter, p_isftCtx);
    } else if (!strcmp(interface, "isftseat")) {
        if (p_isftCtx->isftSeat) {
            return;
        }
        p_isftCtx->isftSeat = isftregistry_bind(registry, name, &isftseat_interface, 1);
        isftseat_add_listener(p_isftCtx->isftSeat, &seat_Listener, data);
    } else if (!strcmp(interface, "ivi_application")) {
        p_isftCtx->iviApplication = isftregistry_bind(registry, name, &ivi_application_interface, 1);
    } else if (!strcmp(interface, "ivi_hmi_controller")) {
        p_isftCtx->hmiCtrl = isftregistry_bind(registry, name, &ivi_hmi_controller_interface, 1);
        ivi_hmi_controller_add_listener(p_isftCtx->hmiCtrl, &hmi_controller_listener, p_isftCtx);
    } else if (!strcmp(interface, "isftexport")) {
        p_isftCtx->hmi_setting->screen_num++;
    }
}

static void registry_handle_global_remove(void data[], struct isftregistry *registry, unsigned int name)
{
}

static const struct isftregistry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

static void frame_listener_func(void data[], struct isftcallback *callback, unsigned int time)
{
    if (callback) {
        isftcallback_destroy(callback);
    }
}

static const struct isftcallback_listener frame_listener = {
    frame_listener_func
};


static const char *bottom_left_corners[] = {
    "bottom_left_corner",
    "sw-resize",
    "size_bdiag"
};

static const char *bottom_right_corners[] = {
    "bottom_right_corner",
    "se-resize",
    "size_fdiag"
};

static const char *bottom_sides[] = {
    "bottom_side",
    "s-resize",
    "size_ver"
};

static const char *fetchs[] = {
    "fetch",
    "closedhand",
    "208530c400c041818281048008011002"
};

static const char *left_ptrs[] = {
    "left_ptr",
    "default",
    "top_left_arrow",
    "left-arrow"
};

static const char *left_sides[] = {
    "left_side",
    "w-resize",
    "size_hor"
};

static const char *right_sides[] = {
    "right_side",
    "e-resize",
    "size_hor"
};

static const char *top_left_corners[] = {
    "top_left_corner",
    "nw-resize",
    "size_fdiag"
};

static const char *top_right_corners[] = {
    "top_right_corner",
    "ne-resize",
    "size_bdiag"
};

static const char *top_sides[] = {
    "top_side",
    "n-resize",
    "size_ver"
};

static const char *xterms[] = {
    "xterm",
    "ibeam",
    "content"
};

static const char *hand1s[] = {
    "hand1",
    "pointer",
    "pointing_hand",
    "e29285e634086352946a0e7090d73106"
};

static const char *watches[] = {
    "watch",
    "wait",
    "0426c94ea35c87780ff01dc239897213"
};

struct cursor_alternatives {
    const char **names;
    size_t count;
};

static const struct cursor_alternatives cursors[] = {
    { bottom_left_corners, ARRAY_LENGTH(bottom_left_corners) },
    { bottom_right_corners, ARRAY_LENGTH(bottom_right_corners) },
    { bottom_sides, ARRAY_LENGTH(bottom_sides) },
    { fetchs, ARRAY_LENGTH(fetchs) },
    { left_ptrs, ARRAY_LENGTH(left_ptrs) },
    { left_sides, ARRAY_LENGTH(left_sides) },
    { right_sides, ARRAY_LENGTH(right_sides) },
    { top_left_corners, ARRAY_LENGTH(top_left_corners) },
    { top_right_corners, ARRAY_LENGTH(top_right_corners) },
    { top_sides, ARRAY_LENGTH(top_sides) },
    { xterms, ARRAY_LENGTH(xterms) },
    { hand1s, ARRAY_LENGTH(hand1s) },
    { watches, ARRAY_LENGTH(watches) },
};

static void create_cursors(struct isftConcontentCommon *cmm)
{
    unsigned int i = 0;
    unsigned int j = 0;
    struct isftcursor *cursor = NULL;
    char *cursor_theme = cmm->hmi_setting->cursor_theme;
    int cursor_size = cmm->hmi_setting->cursor_size;

    cmm->cursor_theme = isftcursor_theme_load(cursor_theme, cursor_size, cmm->isftShm);
    cmm->cursors = xzalloc(ARRAY_LENGTH(cursors) * sizeof(cmm->cursors[0]));

    for (i = 0; i < ARRAY_LENGTH(cursors); i++) {
        cursor = NULL;

        for (j = 0; !cursor && j < cursors[i].count; ++j) {
            cursor = isftcursor_theme_get_cursor( cmm->cursor_theme, cursors[i].names[j]);
        }

        if (!cursor) {
            fprintf(stderr, "could not load cursor '%s'\n", cursors[i].names[0]);
        }
        cmm->cursors[i] = cursor;
    }
}

static void destroy_cursors(struct isftConcontentCommon *cmm)
{
    if (cmm->cursor_theme) {
        isftcursor_theme_destroy(cmm->cursor_theme);
    }
    free(cmm->cursors);
}

static void createShmBuffer(struct isftConcontentStruct *p_isftCtx)
{
    struct isftshm_pool *pool;

    int fd = -1;
    int size = 0;
    int width = 0;
    int height = 0;
    int stride = 0;

    width  = cairo_image_sheet_get_width(p_isftCtx->ctx_image);
    height = cairo_image_sheet_get_height(p_isftCtx->ctx_image);
    stride = cairo_image_sheet_get_stride(p_isftCtx->ctx_image);
    size = stride * height;

    fd = os_create_anonymous_file(size);
    if (fd < 0) {
        fprintf(stderr, "creating a buffer file for %d B failed: %s\n", size, strerror(errno));
        return ;
    }
    p_isftCtx->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (MAP_FAILED == p_isftCtx->data) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return;
    }

    pool = isftshm_create_pool(p_isftCtx->cmm->isftShm, fd, size);
    p_isftCtx->isftBuffer = isftshm_pool_create_buffer(pool, 0, width, height,
                                                       stride, isftSHM_FORMAT_ARGB8888);

    if (p_isftCtx->isftBuffer == NULL) {
        fprintf(stderr, "isftshm_create_buffer failed: %s\n", strerror(errno));
        close(fd);
        return;
    }

    isftshm_pool_destroy(pool);
    close(fd);
}

static void destroyISFTConcontentCommon(struct isftConcontentCommon *p_isftCtx)
{
    destroy_cursors(p_isftCtx);

    if (p_isftCtx->pointer_sheet) {
        isftsheet_destroy(p_isftCtx->pointer_sheet);
    }
    if (p_isftCtx->isftCompositor) {
        isftcompositor_destroy(p_isftCtx->isftCompositor);
    }
}

static void destroyISFTConcontentStruct(struct isftConcontentStruct *p_isftCtx)
{
    if (p_isftCtx->isftsheet) {
        isftsheet_destroy(p_isftCtx->isftsheet);
    }
    if (p_isftCtx->ctx_image) {
        cairo_sheet_destroy(p_isftCtx->ctx_image);
        p_isftCtx->ctx_image = NULL;
    }
}

static int createsheet(struct isftConcontentStruct *p_isftCtx)
{
    p_isftCtx->isftsheet = isftcompositor_create_sheet(p_isftCtx->cmm->isftCompositor);
    if (p_isftCtx->isftsheet == NULL) {
        printf("Error: isftcompositor_create_sheet failed.\n");
        destroyISFTConcontentCommon(p_isftCtx->cmm);
        abort();
    }

    return 0;
}

static void drawImage(struct isftConcontentStruct *p_isftCtx)
{
    struct isftcallback *callback;

    int width = 0;
    int height = 0;
    int stride = 0;
    void data[] = NULL;

    width = cairo_image_sheet_get_width(p_isftCtx->ctx_image);
    height = cairo_image_sheet_get_height(p_isftCtx->ctx_image);
    stride = cairo_image_sheet_get_stride(p_isftCtx->ctx_image);
    data = cairo_image_sheet_get_data(p_isftCtx->ctx_image);

    memcpy(p_isftCtx->data, data, stride * height);
    isftsheet_attach(p_isftCtx->isftsheet, p_isftCtx->isftBuffer, 0, 0);
    isftsheet_damage(p_isftCtx->isftsheet, 0, 0, width, height);

    callback = isftsheet_frame(p_isftCtx->isftsheet);
    isftcallback_add_listener(callback, &frame_listener, NULL);

    isftsheet_commit(p_isftCtx->isftsheet);
}

static void create_ivisheet(struct isftConcontentStruct *p_isftCtx, unsigned int id_sheet,
                            cairo_sheet_t *sheet)
{
    struct ivi_sheet *ivisurf = NULL;

    p_isftCtx->ctx_image = sheet;
    p_isftCtx->id_sheet = id_sheet;
    isftlist_init(&p_isftCtx->link);
    isftlist_insert(&p_isftCtx->cmm->list_isftConcontentStruct, &p_isftCtx->link);

    createsheet(p_isftCtx);
    createShmBuffer(p_isftCtx);

    ivisurf = ivi_application_sheet_create(p_isftCtx->cmm->iviApplication, id_sheet,
                                           p_isftCtx->isftsheet);
    if (ivisurf == NULL) {
        fprintf(stderr, "Failed to create ivi_client_sheet\n");
        return;
    }

    drawImage(p_isftCtx);
}

static void create_ivisheetFromFile(struct isftConcontentStruct *p_isftCtx,
                                    unsigned int id_sheet, const char *imageFile)
{
    cairo_sheet_t *sheet = load_cairo_sheet(imageFile);

    if (sheet == NULL) {
        fprintf(stderr, "Failed to load_cairo_sheet %s\n", imageFile);
        return;
    }

    create_ivisheet(p_isftCtx, id_sheet, sheet);
}

static void set_hex_color(cairo_t *cr, unsigned int color)
{
    cairo_set_source_rgba(cr,
        ((color >> NUM16) & 0xff) / NUM255,
        ((color >>  NUM8) & 0xff) / NUM255,
        ((color >>  NUM0) & 0xff) / NUM255,
        ((color >> NUM24) & 0xff) / NUM255);
}

static void create_ivisheetFromColor(struct isftConcontentStruct *p_isftCtx, unsigned int id_sheet,
                                     unsigned int width, unsigned int height, unsigned int color)
{
    cairo_sheet_t *sheet = NULL;
    cairo_t *cr = NULL;

    sheet = cairo_image_sheet_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(sheet);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_rectangle(cr, 0, 0, width, height);
    set_hex_color(cr, color);
    cairo_fill(cr);
    cairo_destroy(cr);

    create_ivisheet(p_isftCtx, id_sheet, sheet);
}

static void UI_ready(struct ivi_hmi_controller *controller)
{
    ivi_hmi_controller_UI_ready(controller);
}

static void create_background(struct isftConcontentStruct *p_isftCtx, const unsigned int id_sheet,
                              const char *imageFile)
{
    create_ivisheetFromFile(p_isftCtx, id_sheet, imageFile);
}

static void create_board(struct isftConcontentStruct *p_isftCtx, const unsigned int id_sheet,
                         const char *imageFile)
{
    create_ivisheetFromFile(p_isftCtx, id_sheet, imageFile);
}

static void create_button(struct isftConcontentStruct *p_isftCtx, const unsigned int id_sheet,
                          const char *imageFile, unsigned int number)
{
    create_ivisheetFromFile(p_isftCtx, id_sheet, imageFile);
}

static void create_home_button(struct isftConcontentStruct *p_isftCtx, const unsigned int id_sheet,
                               const char *imageFile)
{
    create_ivisheetFromFile(p_isftCtx, id_sheet, imageFile);
}

static void create_workspace_background(struct isftConcontentStruct *p_isftCtx,
                                        struct hmi_homescreen_srf *srf)
{
    create_ivisheetFromColor(p_isftCtx, srf->id, 1, 1, srf->color);
}

static void create_launchers(struct isftConcontentCommon *cmm, struct isftlist *launcher_list)
{
    struct hmi_homescreen_launcher **launchers;
    struct hmi_homescreen_launcher *launcher = NULL;

    int launcher_count = isftlist_length(launcher_list);
    int ii = 0;
    int start = 0;

    if (launcher_count == 0) {
        return;
    }
    launchers = xzalloc(launcher_count * sizeof(*launchers));

    isftlist_for_each(launcher, launcher_list, link) {
        launchers[ii] = launcher;
        ii++;
    }

    for (ii = 0; ii < launcher_count; ii++) {
        int jj = 0;

        if (ii != launcher_count - 1 && launchers[ii]->workspace_id == launchers[ii + 1]->workspace_id) {
            continue;
        }
        for (jj = start; jj <= ii; jj++) {
            struct isftConcontentStruct *p_isftCtx;

            p_isftCtx = xzalloc(sizeof(*p_isftCtx));
            p_isftCtx->cmm = cmm;
            create_ivisheetFromFile(p_isftCtx, launchers[jj]->icon_sheet_id, launchers[jj]->icon);
        }
        start = ii + 1;
    }
    free(launchers);
}

static struct hmi_homescreen_setting *hmi_homescreen_setting_create(void)
{
    const char *config_file;
    struct isftViewconfig *config = NULL;
    struct isftViewconfig_section *shellSection = NULL;
    struct hmi_homescreen_setting *setting = xzalloc(sizeof(*setting));
    struct isftViewconfig_section *section = NULL;
    const char *name = NULL;
    unsigned int workspace_layer_id;
    unsigned int icon_sheet_id = 0;
    char *filename;

    isftlist_init(&setting->workspace_list);
    isftlist_init(&setting->launcher_list);

    config_file = isftViewconfig_get_name_from_env();
    config = isftViewconfig_parse(config_file);
    shellSection = isftViewconfig_get_section(config, "ivi-shell", NULL, NULL);
    isftViewconfig_section_get_string(shellSection, "cursor-theme", &setting->cursor_theme, NULL);
    isftViewconfig_section_get_int(shellSection, "cursor-size", &setting->cursor_size, 32);
    isftViewconfig_section_get_unsigned int(shellSection, "workspace-layer-id", &workspace_layer_id, 3000);

    filename = file_name_with_datadir("background.png");
    isftViewconfig_section_get_string(shellSection, "background-image", &setting->background.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "background-id", &setting->background.id, 1001);
    filename = file_name_with_datadir("board.png");
    isftViewconfig_section_get_string(shellSection, "board-image", &setting->board.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "board-id", &setting->board.id, 1002);
    filename = file_name_with_datadir("tiling.png");
    isftViewconfig_section_get_string(shellSection, "tiling-image", &setting->tiling.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "tiling-id", &setting->tiling.id, 1003);
    filename = file_name_with_datadir("sidebyside.png");
    isftViewconfig_section_get_string(shellSection, "sidebyside-image", &setting->sidebyside.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "sidebyside-id", &setting->sidebyside.id, 1004);
    filename = file_name_with_datadir("fullscreen.png");
    isftViewconfig_section_get_string(shellSection, "fullscreen-image", &setting->fullscreen.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "fullscreen-id", &setting->fullscreen.id, 1005);
    filename = file_name_with_datadir("random.png");
    isftViewconfig_section_get_string(shellSection, "random-image", &setting->random.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "random-id", &setting->random.id, 1006);
    filename = file_name_with_datadir("home.png");
    isftViewconfig_section_get_string(shellSection, "home-image", &setting->home.filePath, filename);
    free(filename);

    isftViewconfig_section_get_unsigned int(shellSection, "home-id", &setting->home.id, 1007);
    isftViewconfig_section_get_color(shellSection, "workspace-background-color",
                                     &setting->workspace_background.color, 0x99000000);

    isftViewconfig_section_get_unsigned int(shellSection, "workspace-background-id",
                                    &setting->workspace_background.id, 2001);

    isftViewconfig_section_get_unsigned int(shellSection, "sheet-id-offset", &setting->sheet_id_offset, 10);
    icon_sheet_id = workspace_layer_id + 1;

    while (isftViewconfig_next_section(config, &section, &name)) {
        struct hmi_homescreen_launcher *launcher;

        if (strcmp(name, "ivi-launcher") != 0) {
            continue;
        }
        launcher = xzalloc(sizeof(*launcher));
        isftlist_init(&launcher->link);

        isftViewconfig_section_get_string(section, "icon", &launcher->icon, NULL);
        isftViewconfig_section_get_string(section, "path", &launcher->path, NULL);
        isftViewconfig_section_get_unsigned int(section, "workspace-id", &launcher->workspace_id, 0);
        isftViewconfig_section_get_unsigned int(section, "icon-id", &launcher->icon_sheet_id, icon_sheet_id);
        icon_sheet_id++;
        isftlist_insert(setting->launcher_list.prev, &launcher->link);
    }
    isftViewconfig_destroy(config);
    return setting;
}

int main(int argc, char **argv)
{
    struct isftConcontentCommon isftCtxCommon;
    struct isftConcontentStruct *isftCtx_BackGround;
    struct isftConcontentStruct *isftCtx_board;
    struct isftConcontentStruct isftCtx_Button_1;
    struct isftConcontentStruct isftCtx_Button_2;
    struct isftConcontentStruct isftCtx_Button_3;
    struct isftConcontentStruct isftCtx_Button_4;
    struct isftConcontentStruct isftCtx_HomeButton;
    struct isftConcontentStruct isftCtx_WorkSpaceBackGround;
    struct isftlist launcher_isftCtxList;
    int ret = 0;
    struct hmi_homescreen_setting *hmi_setting;
    struct isftConcontentStruct *pWlCtxSt = NULL;
    int i = 0;

    hmi_setting = hmi_homescreen_setting_create();

    memset(&isftCtxCommon, 0x00, sizeof(isftCtxCommon));
    memset(&isftCtx_Button_1,   0x00, sizeof(isftCtx_Button_1));
    memset(&isftCtx_Button_2,   0x00, sizeof(isftCtx_Button_2));
    memset(&isftCtx_Button_3,   0x00, sizeof(isftCtx_Button_3));
    memset(&isftCtx_Button_4,   0x00, sizeof(isftCtx_Button_4));
    memset(&isftCtx_HomeButton, 0x00, sizeof(isftCtx_HomeButton));
    memset(&isftCtx_WorkSpaceBackGround, 0x00, sizeof(isftCtx_WorkSpaceBackGround));
    isftlist_init(&launcher_isftCtxList);
    isftlist_init(&isftCtxCommon.list_isftConcontentStruct);
    isftCtxCommon.hmi_setting = hmi_setting;

    isftCtxCommon.isftshow = isftshow_connect(NULL);
    if (isftCtxCommon.isftshow == NULL) {
        printf("Error: isftshow_connect failed.\n");
        return -1;
    }

    isftCtxCommon.formats = 0;
    isftCtxCommon.isftRegistry = isftshow_get_registry(isftCtxCommon.isftshow);
    isftregistry_add_listener(isftCtxCommon.isftRegistry, &registry_listener, &isftCtxCommon);
    isftshow_roundtrip(isftCtxCommon.isftshow);

    if (isftCtxCommon.isftShm == NULL) {
        fprintf(stderr, "No isftshm global\n");
        exit(1);
    }

    isftshow_roundtrip(isftCtxCommon.isftshow);

    if (!(isftCtxCommon.formats & (1 << isftSHM_FORMAT_XRGB8888))) {
        fprintf(stderr, "isftSHM_FORMAT_XRGB32 not available\n");
        exit(1);
    }

    isftCtx_BackGround = xzalloc(hmi_setting->screen_num * sizeof(struct isftConcontentStruct));
    isftCtx_board= xzalloc(hmi_setting->screen_num * sizeof(struct isftConcontentStruct));

    if (isftCtxCommon.hmi_setting->cursor_theme) {
        create_cursors(&isftCtxCommon);
        isftCtxCommon.pointer_sheet = isftcompositor_create_sheet(isftCtxCommon.isftCompositor);
        isftCtxCommon.current_cursor = CURSOR_LEFT_PTR;
    }
    isftCtx_Button_1.cmm   = &isftCtxCommon;
    isftCtx_Button_2.cmm   = &isftCtxCommon;
    isftCtx_Button_3.cmm   = &isftCtxCommon;
    isftCtx_Button_4.cmm   = &isftCtxCommon;
    isftCtx_HomeButton.cmm = &isftCtxCommon;
    isftCtx_WorkSpaceBackGround.cmm = &isftCtxCommon;

    for (i = NUM0; i < hmi_setting->screen_num; i++) {
        isftCtx_BackGround[i].cmm = &isftCtxCommon;
        create_background(&isftCtx_BackGround[i], hmi_setting->background.id + (i * hmi_setting->sheet_id_offset),
                          hmi_setting->background.filePath);

        isftCtx_board[i].cmm = &isftCtxCommon;
        create_board(&isftCtx_board[i], hmi_setting->board.id + (i * hmi_setting->sheet_id_offset),
                     hmi_setting->board.filePath);
    }
    create_button(&isftCtx_Button_1, hmi_setting->tiling.id, hmi_setting->tiling.filePath, NUM0);
    create_button(&isftCtx_Button_2, hmi_setting->sidebyside.id, hmi_setting->sidebyside.filePath, NUM1);
    create_button(&isftCtx_Button_3, hmi_setting->fullscreen.id, hmi_setting->fullscreen.filePath, NUM2);
    create_button(&isftCtx_Button_4, hmi_setting->random.id, hmi_setting->random.filePath, NUM3);
    create_workspace_background(&isftCtx_WorkSpaceBackGround, &hmi_setting->workspace_background);
    create_launchers(&isftCtxCommon, &hmi_setting->launcher_list);
    create_home_button(&isftCtx_HomeButton, hmi_setting->home.id, hmi_setting->home.filePath);
    UI_ready(isftCtxCommon.hmiCtrl);
    while (ret != -1) {
        ret = isftshow_dispatch(isftCtxCommon.isftshow);
    }
    isftlist_for_each(pWlCtxSt, &isftCtxCommon.list_isftConcontentStruct, link) {
        destroyISFTConcontentStruct(pWlCtxSt);
    }

    free(isftCtx_BackGround);
    free(isftCtx_board);

    destroyISFTConcontentCommon(&isftCtxCommon);

    return 0;
}
