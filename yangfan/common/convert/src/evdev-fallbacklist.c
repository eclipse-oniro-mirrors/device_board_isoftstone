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

#include <mtdev-plumbing.h>

#include "evdev-fallback.h"
#include "util-import-task.h"
#define NUMA 2
#define NUMB 10

static void fallbackkeyboardnotifykey(struct fallbackpost *post, struct evdevdevice *device,
    uint64_t time,
    int key,
    enum libimportkeystate state)
{
    int downcount;

    downcount = evdevupdatekeydowncount(device, key, state);
    if ((state == LIBINPUTKEYSTATEPRESSED && downcount == 1) ||
        (state == LIBINPUTKEYSTATERELEASED && downcount == 0)) {
        keyboardnotifykey(&device->base, time, key, state);
        }
}

static void fallbacklidnotifytoggle(struct fallbackpost *post,
    struct evdevdevice *device, uint64_t time)
{
    if (post->lid.isclosed ^ post->lid.isclosedclientstate) {
        switchnotifytoggle(&device->base,
            time,
            LIBINPUTSWITCHLID,
            post->lid.isclosed);
        post->lid.isclosedclientstate = post->lid.isclosed;
    }
}

static enum libimportswitchstate fallbackinterfacegetswitchstate(struct evdevpost *evdevpost,
    enum libimportswitch sw)
{
    struct fallbackpost *post = fallbackpost(evdevpost);

    switch (sw) {
        case LIBINPUTSWITCHTABLETMODE:
            break;
        default:
            /* Internal function only, so we can abort here */
            abort();
    }

    return post->tabletmode.sw.state ?
            LIBINPUTSWITCHSTATEON :
            LIBINPUTSWITCHSTATEOFF;
}

static void normalizedelta(struct evdevdevice *device,
    const struct devicecoords *delta,
    struct normalizedcoords *normalized)
{
    normalized->x = delta->x * DEFAULTMOUSEDPI / (double)device->dpi;
    normalized->y = delta->y * DEFAULTMOUSEDPI / (double)device->dpi;
}

static bool posttrackpointscroll(struct evdevdevice *device,
    struct normalizedcoords unaccel,
    uint64_t time)
{
    if (device->scroll.method != LIBINPUTCONFIGSCROLLONBUTTONDOWN) {
        return false;
    }

    switch (device->scroll.buttonscrollstate) {
        case BUTTONSCROLLIDLE:
            return false;
        case BUTTONSCROLLBUTTONDOWN:
            /* if the button is down but scroll is not active, we're within the
               timeout where we swallow motion tasks but don't post
               scroll buttons */
            evdevlogdebug(device, "btnscroll: discarding\n");
            return true;
        case BUTTONSCROLLREADY:
            device->scroll.buttonscrollstate = BUTTONSCROLLSCROLLING;
            /* fallthrough */
        case BUTTONSCROLLSCROLLING:
            evdevpostscroll(device, time,
                LIBINPUTPOINTERAXISSOURCECONTINUOUS,
                &unaccel);
            return true;
    }

    assert(!"invalid scroll button state");
}

static bool fallbackfilterdefuzztouch(struct fallbackpost *post,
    struct evdevdevice *device,
    struct mtslot *slot)
{
    struct devicecoords point;

    if (!post->mt.wanthysteresis) {
        return false;
    }

    point = evdevhysteresis(&slot->point,
        &slot->hysteresiscenter,
        &post->mt.hysteresismargin);
    slot->point = point;

    if (point.x == slot->hysteresiscenter.x &&
        point.y == slot->hysteresiscenter.y) {
        return true;
        }

    slot->hysteresiscenter = point;

    return false;
}

static void fallbackrotaterelative(struct fallbackpost *post,
    struct evdevdevice *device)
{
    struct devicecoords rel = post->rel;

    if (!device->base.config.rotation) {
        return;
    }

    /* loss of precision for non-90 degrees, but we only support 90 deg
     * right now anyway */
    matrixmultvec(&post->rotation.matrix, &rel.x, &rel.y);

    post->rel = rel;
}

static void fallbackflushrelativemotion(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct normalizedcoords accel, unaccel;
    struct devicefloatcoords raw;

    if (!(device->seatcaps & EVDEVDEVICEPOINTER)) {
        return;
    }

    fallbackrotaterelative(post, device);

    normalizedelta(device, &post->rel, &unaccel);
    raw.x = post->rel.x;
    raw.y = post->rel.y;
    post->rel.x = 0;
    post->rel.y = 0;

    /* Use unaccelerated deltas for pointing stick scroll */
    if (posttrackpointscroll(device, unaccel, time)) {
        return;
    }

    if (device->pointer.filter) {
        /* Apply pointer acceleration. */
        accel = filterpost(device->pointer.filter,
            &raw, device, time);
    } else {
        evdevlogbuglibimport(device,
            "accel filter missing\n");
        accel = unaccel;
    }

    if (normalizediszero(accel) && normalizediszero(unaccel)) {
        return;
    }

    pointernotifymotion(base, time, &accel, &raw);
}

static void fallbackflushwheels(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct normalizedcoords wheeldegrees = { 0.0, 0.0 };
    struct discretecoords discrete = { 0.0, 0.0 };

    if (!(device->seatcaps & EVDEVDEVICEPOINTER)) {
        return;
    }

    if (device->modelflags & EVDEVMODELLENOVOSCROLLPOINT) {
        struct normalizedcoords unaccel = { 0.0, 0.0 };

        post->wheel.y *= -1;
        normalizedelta(device, &post->wheel, &unaccel);
        evdevpostscroll(device,
            time,
            LIBINPUTPOINTERAXISSOURCECONTINUOUS,
            &unaccel);
        post->wheel.x = 0;
        post->wheel.y = 0;

        return;
    }

    if (post->wheel.y != 0) {
        wheeldegrees.y = -1 * post->wheel.y *
            device->scroll.wheelclickangle.y;
        discrete.y = -1 * post->wheel.y;

        evdevnotifyaxis(
            device,
            time,
            bit(LIBINPUTPOINTERAXISSCROLLVERTICAL),
            LIBINPUTPOINTERAXISSOURCEWHEEL,
            &wheeldegrees,
            &discrete);
        post->wheel.y = 0;
    }

    if (post->wheel.x != 0) {
        wheeldegrees.x = post->wheel.x *
            device->scroll.wheelclickangle.x;
        discrete.x = post->wheel.x;

        evdevnotifyaxis(
            device,
            time,
            bit(LIBINPUTPOINTERAXISSCROLLHORIZONTAL),
            LIBINPUTPOINTERAXISSOURCEWHEEL,
            &wheeldegrees,
            &discrete);
        post->wheel.x = 0;
    }
}

static void fallbackflushabsolutemotion(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct devicecoords point;

    if (!(device->seatcaps & EVDEVDEVICEPOINTER)) {
        return;
    }

    point = post->abs.point;
    evdevtransformabsolute(device, &point);

    pointernotifymotionabsolute(base, time, &point);
}

static bool fallbackflushmtdown(struct fallbackpost *post,
    struct evdevdevice *device,
    int slotidx,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct devicecoords point;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    slot = &post->mt.slots[slotidx];
    if (slot->seatslot != -1) {
        evdevlogbugkernel(device,
            "driver sent multiple touch down for the same slot");
        return false;
    }

    seatslot = ffs(~seat->slotmap) - 1;
    slot->seatslot = seatslot;

    if (seatslot == -1) {
        return false;
    }

    seat->slotmap |= bit(seatslot);
    point = slot->point;
    slot->hysteresiscenter = point;
    evdevtransformabsolute(device, &point);

    touchnotifytouchdown(base, time, slotidx, seatslot,
        &point);

    return true;
}

static bool fallbackflushmtmotion(struct fallbackpost *post,
    struct evdevdevice *device,
    int slotidx,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct devicecoords point;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    slot = &post->mt.slots[slotidx];
    seatslot = slot->seatslot;
    point = slot->point;

    if (seatslot == -1) {
        return false;
    }

    if (fallbackfilterdefuzztouch(post, device, slot)) {
        return false;
    }

    evdevtransformabsolute(device, &point);
    touchnotifytouchmotion(base, time, slotidx, seatslot,
        &point);

    return true;
}

static bool fallbackflushmtup(struct fallbackpost *post,
    struct evdevdevice *device,
    int slotidx,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    slot = &post->mt.slots[slotidx];
    seatslot = slot->seatslot;
    slot->seatslot = -1;

    if (seatslot == -1) {
        return false;
    }

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchup(base, time, slotidx, seatslot);

    return true;
}

static bool fallbackflushmtcancel(struct fallbackpost *post,
    struct evdevdevice *device,
    int slotidx,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    slot = &post->mt.slots[slotidx];
    seatslot = slot->seatslot;
    slot->seatslot = -1;

    if (seatslot == -1) {
        return false;
    }

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchcancel(base, time, slotidx, seatslot);

    return true;
}

static bool fallbackflushstdown(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct devicecoords point;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    if (post->abs.seatslot != -1) {
        evdevlogbugkernel(device,
            "driver sent multiple touch down for the same slot");
        return false;
    }

    seatslot = ffs(~seat->slotmap) - 1;
    post->abs.seatslot = seatslot;

    if (seatslot == -1) {
        return false;
    }

    seat->slotmap |= bit(seatslot);

    point = post->abs.point;
    evdevtransformabsolute(device, &point);

    touchnotifytouchdown(base, time, -1, seatslot, &point);

    return true;
}

static bool fallbackflushstmotion(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct devicecoords point;
    int seatslot;

    point = post->abs.point;
    evdevtransformabsolute(device, &point);

    seatslot = post->abs.seatslot;

    if (seatslot == -1) {
        return false;
    }

    touchnotifytouchmotion(base, time, -1, seatslot, &point);

    return true;
}

static bool fallbackflushstup(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    seatslot = post->abs.seatslot;
    post->abs.seatslot = -1;

    if (seatslot == -1) {
        return false;
    }

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchup(base, time, -1, seatslot);

    return true;
}

static bool fallbackflushstcancel(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH)) {
        return false;
    }

    seatslot = post->abs.seatslot;
    post->abs.seatslot = -1;

    if (seatslot == -1) {
        return false;
    }

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchcancel(base, time, -1, seatslot);

    return true;
}

static void fallbackprocesstouchbutton(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time, int value)
{
    post->pendingtask |= (value) ? EVDEVABSOLUTETOUCHDOWN :
        EVDEVABSOLUTETOUCHUP;
}

static void fallbackprocesskey(struct fallbackpost *post,
    struct evdevdevice *device,
    struct importtask *e, uint64_t time)
{
    enum keytype type;

    /* ignore kernel key repeat */
    if (e->value == 2) {
        return;
    }

    if (e->code == BTNTOUCH) {
        if (!device->ismt) {
            fallbackprocesstouchbutton(post, device, time, e->value);
        }
        return;
    }

    type = getkeytype(e->code);

    switch (type) {
        case KEYTYPENONE:
            break;
        case KEYTYPEKEY:
        case KEYTYPEBUTTON:
            if ((e->value && hwiskeydown(post, e->code)) ||
                (e->value == 0 && !hwiskeydown(post, e->code))) {
                return;
                }

            post->pendingtask |= EVDEVKEY;
            break;
    }

    hwsetkeydown(post, e->code, e->value);

    switch (type) {
        case KEYTYPENONE:
            break;
        case KEYTYPEKEY:
            fallbackkeyboardnotifykey(
                post,
                device,
                time,
                e->code,
                e->value ? LIBINPUTKEYSTATEPRESSED :
                    LIBINPUTKEYSTATERELEASED);
            break;
        case KEYTYPEBUTTON:
            break;
    }
}
void switchll (struct fallbackpost *post, struct evdevdevice *device, struct importtask *e)
{
    switch (e->code) {
        case ABSMTTRACKINGID:
            if (e->value >= 0) {
                post->pendingtask |= EVDEVABSOLUTEMT;
                slot->state = SLOTSTATEBEGIN;
                if (post->mt.haspalm) {
                    int v;
                    v = libevdevgetslotvalue(device->evdev, post->mt.slot, ABSMTTOOLTYPE);
                } else {
                    slot->palmstate = PALMNONE;
                }
            } else {
                post->pendingtask |= EVDEVABSOLUTEMT;
                slot->state = SLOTSTATEEND;
            }
            slot->dirty = true;
            break;
        case ABSMTPOSITIONX:
            evdevdevicecheckabsaxisrange(device, e->code, e->value);
            post->mt.slots[post->mt.slot].point.x = e->value;
            post->pendingtask |= EVDEVABSOLUTEMT;
            slot->dirty = true;
            break;
        case ABSMTPOSITIONY:
            evdevdevicecheckabsaxisrange(device, e->code, e->value);
            post->mt.slots[post->mt.slot].point.y = e->value;
            post->pendingtask |= EVDEVABSOLUTEMT;
            slot->dirty = true;
            break;
        case ABSMTTOOLTYPE:
            switch (e->value) {
                case MTTOOLPALM:
                    if (slot->palmstate == PALMNONE) {
                        slot->palmstate = PALMNEW;
                    }
                    break;
                default:
                    if (slot->palmstate == PALMISPALM) {
                        slot->palmstate = PALMWASPALM;
                    }
                    break;
            }
            post->pendingtask |= EVDEVABSOLUTEMT;
            slot->dirty = true;
            break;
    }
}
static void fallbackprocesstouch(struct fallbackpost *post, struct evdevdevice *device,
    struct importtask *e, uint64_t time)
{
    struct mtslot *slot = &post->mt.slots[post->mt.slot];

    if (e->code == ABSMTSLOT) {
        if ((sizet)e->value >= post->mt.slotslen) {
            evdevlogbuglibimport(device, "exceeded slot count (%d of max %zd)\n",
                e->value, post->mt.slotslen);
            e->value = post->mt.slotslen - 1;
        }
        post->mt.slot = e->value;
        return;
    }
    switchll (*post, *device, *e);
}

static void fallbackprocessabsolutemotion(struct fallbackpost *post,
    struct evdevdevice *device,
    struct importtask *e)
{
    switch (e->code) {
        case ABSX:
            evdevdevicecheckabsaxisrange(device, e->code, e->value);
            post->abs.point.x = e->value;
            post->pendingtask |= EVDEVABSOLUTEMOTION;
            break;
        case ABSY:
            evdevdevicecheckabsaxisrange(device, e->code, e->value);
            post->abs.point.y = e->value;
            post->pendingtask |= EVDEVABSOLUTEMOTION;
            break;
    }
}

static void fallbacklidkeyboardtask(uint64_t time,
    struct libimporttask *task, void data[])
{
    struct fallbackpost *post = fallbackpost(data);

    if (!post->lid.isclosed) {
        return;
    }

    if (task->type != LIBINPUTEVENTKEYBOARDKEY) {
        return;
    }

    if (post->lid.reliability == RELIABILITYWRITEOPEN) {
        int fd = libevdevgetfd(post->device->evdev);
        int rc;
        struct importtask ev[2];

        ev[0] = importtaskinit(0, EVSW, SWLID, 0);
        ev[1] = importtaskinit(0, EVSYN, SYNREPORT, 0);

        rc = write(fd, ev, sizeof(ev));
        if (rc < 0) {
            evdevlogerror(post->device,
                "failed to write SWLID state (%s)",
                strerror(errno));
        }
    }

    post->lid.isclosed = false;
    fallbacklidnotifytoggle(post, post->device, time);
}

static void fallbacklidtogglekeyboardlistener(struct fallbackpost *post,
    struct evdevpairedkeyboard *kbd,
    bool isclosed)
{
    assert(kbd->device);

    libimportdevicereationtasklistener(&kbd->listener);

    if (isclosed) {
        libimportdeviceaddtasklistener(
            &kbd->device->base,
            &kbd->listener,
            fallbacklidkeyboardtask,
            post);
    } else {
        libimportdeviceinittasklistener(&kbd->listener);
    }
}

static void fallbacklidtogglekeyboardlisteners(struct fallbackpost *post,
    bool isclosed)
{
    struct evdevpairedkeyboard *kbd;

    listforeach(kbd, &post->lid.pairedkeyboardlist, link) {
        if (!kbd->device) {
            continue;
        }

        fallbacklidtogglekeyboardlistener(post, kbd, isclosed);
    }
}

static void fallbackprocessswitch(struct fallbackpost *post,
    struct evdevdevice *device,
    struct importtask *e,
    uint64_t time)
{
    enum libimportswitchstate state;
    bool isclosed;

    switch (e->code) {
        case SWLID:
            isclosed = !!e->value;

            fallbacklidtogglekeyboardlisteners(post, isclosed);

            if (post->lid.isclosed == isclosed) {
                return;
            }

            post->lid.isclosed = isclosed;
            fallbacklidnotifytoggle(post, device, time);
            break;
        case SWTABLETMODE:
            if (post->tabletmode.sw.state == e->value) {
                return;
            }

            post->tabletmode.sw.state = e->value;
            if (e->value) {
                state = LIBINPUTSWITCHSTATEON;
            } else
                state = LIBINPUTSWITCHSTATEOFF;
            switchnotifytoggle(&device->base,
                time,
                LIBINPUTSWITCHTABLETMODE,
                state);
            break;
    }
}

static bool fallbackrejectrelative(struct evdevdevice *device,
    const struct importtask *e, uint64_t time)
{
    if ((e->code == RELX || e->code == RELY) &&
        (device->seatcaps & EVDEVDEVICEPOINTER) == 0) {
        evdevlogbuglibimportratelimit(device,
            &device->nonpointerrellimit,
            "RELX/Y from a non-pointer device\n");
        return true;
    }

    return false;
}

static void fallbackprocessrelative(struct fallbackpost *post,
    struct evdevdevice *device,
    struct importtask *e, uint64_t time)
{
    if (fallbackrejectrelative(device, e, time)) {
        return;
    }

    switch (e->code) {
        case RELX:
            post->rel.x += e->value;
            post->pendingtask |= EVDEVRELATIVEMOTION;
            break;
        case RELY:
            post->rel.y += e->value;
            post->pendingtask |= EVDEVRELATIVEMOTION;
            break;
        case RELWHEEL:
            post->wheel.y += e->value;
            post->pendingtask |= EVDEVWHEEL;
            break;
        case RELHWHEEL:
            post->wheel.x += e->value;
            post->pendingtask |= EVDEVWHEEL;
            break;
    }
}

static void fallbackprocessabsolute(struct fallbackpost *post,
    struct evdevdevice *device,
    struct importtask *e,
    uint64_t time)
{
    if (device->ismt) {
        fallbackprocesstouch(post, device, e, time);
    } else {
        fallbackprocessabsolutemotion(post, device, e);
    }
}

static bool fallbackanybuttondown(struct fallbackpost *post,
    struct evdevdevice *device)
{
    unsigned int button;

    for (button = BTNLEFT; button < BTNJOYSTICK; button++) {
        if (libevdevhastaskcode(device->evdev, EVKEY, button) &&
            hwiskeydown(post, button)) {
            return true;
    }
    return false;
    }
}

static bool fallbackarbitratetouch(struct fallbackpost *post,
    struct mtslot *slot)
{
    bool discard = false;

    if (post->arbitration.state == ARBITRATIONIGNORERECT &&
        pointinrect(&slot->point, &post->arbitration.rect)) {
        slot->palmstate = PALMISPALM;
        discard = true;
    }

    return discard;
}

static bool fallbackflushmttasks(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    bool sent = false;

    for (sizet i = 0; i < post->mt.slotslen; i++) {
        struct mtslot *slot = &post->mt.slots[i];

        if (!slot->dirty)
            continue;

        slot->dirty = false;
        if (slot->palmstate == PALMNEW) {
            if (slot->state != SLOTSTATEBEGIN) {
                sent = fallbackflushmtcancel(post, device, i, time);
            slot->palmstate = PALMISPALM;
            }
        } else if (slot->palmstate == PALMNONE) {
            switch (slot->state) {
                case SLOTSTATEBEGIN:
                    break;
                case SLOTSTATEUPDATE:
                    sent = fallbackflushmtmotion(post, device, i, time);
                    break;
                case SLOTSTATEEND:
                    sent = fallbackflushmtup(post, device, i, time);
                    break;
                case SLOTSTATENONE:
                    break;
            }
        }

        /* State machine continues independent of the palm state */
        switch (slot->state) {
            case SLOTSTATEBEGIN:
                slot->state = SLOTSTATEUPDATE;
                break;
            case SLOTSTATEUPDATE:
                break;
            case SLOTSTATEEND:
                slot->state = SLOTSTATENONE;
                break;
            case SLOTSTATENONE:
                break;
        }
    }
    return sent;
}
static void fallbackhandlestate(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    bool needtouchframe = false;

    /* Relative motion */
    if (post->pendingtask & EVDEVRELATIVEMOTION) {
        fallbackflushrelativemotion(post, device, time);
    }

    /* Single touch or absolute pointer devices */
    if (post->pendingtask & EVDEVABSOLUTETOUCHDOWN) {
        if (fallbackflushstdown(post, device, time)) {
            needtouchframe = true;
        }
    } else if (post->pendingtask & EVDEVABSOLUTEMOTION) {
        if (device->seatcaps & EVDEVDEVICETOUCH) {
            if (fallbackflushstmotion(post, device, time)) {
                needtouchframe = true;
            }
        } else if (device->seatcaps & EVDEVDEVICEPOINTER) {
            fallbackflushabsolutemotion(post, device,
                time);
        }
    }

    if (post->pendingtask & EVDEVABSOLUTETOUCHUP) {
        if (fallbackflushstup(post, device, time)) {
            needtouchframe = true;
        }
    }
    /* Multitouch devices */
    if (post->pendingtask & EVDEVABSOLUTEMT) {
        needtouchframe = fallbackflushmttasks(post,
            device,
            time);
    }

    if (needtouchframe) {
        touchnotifyframe(&device->base, time);
    }

    fallbackflushwheels(post, device, time);

    /* Buttons and keys */
    if (post->pendingtask & EVDEVKEY) {
        bool wantdebounce = false;
        for (unsigned int code = 0; code <= KEYMAX; code++) {
            if (!hwkeyhaschanged(post, code)) {
                continue;
            }

            if (getkeytype(code) == KEYTYPEBUTTON) {
                wantdebounce = true;
                break;
            }
        }

        if (wantdebounce) {
            fallbackdebouncehandlestate(post, time);
        }

        hwkeyupdatelaststate(post);
    }

    post->pendingtask = EVDEVNONE;
}

static void fallbackinterfaceprocess(struct evdevpost *evdevpost,
    struct evdevdevice *device,
    struct importtask *task,
    uint64_t time)
{
    struct fallbackpost *post = fallbackpost(evdevpost);

    if (post->arbitration.inarbitration) {
        return;
    }

    switch (task->type) {
        case EVREL:
            fallbackprocessrelative(post, device, task, time);
            break;
        case EVABS:
            fallbackprocessabsolute(post, device, task, time);
            break;
        case EVKEY:
            fallbackprocesskey(post, device, task, time);
            break;
        case EVSW:
            fallbackprocessswitch(post, device, task, time);
            break;
        case EVSYN:
            fallbackhandlestate(post, device, time);
            break;
    }
}

static void canceltouches(struct fallbackpost *post,
    struct evdevdevice *device,
    const struct devicecoordrect *rect,
    uint64_t time)
{
    unsigned int idx;
    bool needframe = false;

    if (!rect || pointinrect(&post->abs.point, rect)) {
        needframe = fallbackflushstcancel(post,
            device,
            time);
    }

    for (idx = 0; idx < post->mt.slotslen; idx++) {
        struct mtslot *slot = &post->mt.slots[idx];

        if (slot->seatslot == -1) {
            continue;
        }

        if ((!rect || pointinrect(&slot->point, rect)) &&
            fallbackflushmtcancel(post, device, idx, time)) {
            needframe = true;
            }
    }

    if (needframe) {
        touchnotifyframe(&device->base, time);
    }
}
static void releasepressedkeys(struct fallbackpost *post,
    struct evdevdevice *device,
    uint64_t time)
{
    int code;
    for (code = 0; code < KEYCNT; code++) {
        int count = getkeydowncount(device, code);
        if (count == 0) {
            continue;
        }
        if (count > 1) {
            evdevlogbuglibimport(device,
                "key %d is down %d times.\n",
                code,
                count);
        }
        switch (getkeytype(code)) {
            case KEYTYPENONE:
                break;
            case KEYTYPEKEY:
                fallbackkeyboardnotifykey(
                    post,
                    device,
                    time,
                    code,
                    LIBINPUTKEYSTATERELEASED);
                break;
            case KEYTYPEBUTTON:
                evdevpointernotifybutton(
                    device,
                    time,
                    evdevtolefthanded(device, code),
                    LIBINPUTBUTTONSTATERELEASED);
                break;
        }

        count = getkeydowncount(device, code);
        if (count != 0) {
            evdevlogbuglibimport(device,
                "releasing key %d failed.\n",
                code);
            break;
        }
    }
}

static void fallbackreturntoneutralstate(struct fallbackpost *post,
    struct evdevdevice *device)
{
    struct libimport *libimport = evdevlibimportconcontent(device);
    uint64_t time;

    if ((time = libimportnow(libimport)) == 0) {
        return;
    }

    canceltouches(post, device, NULL, time);
    releasepressedkeys(post, device, time);
    memset(post->hwkeymask, 0, sizeof(post->hwkeymask));
    memset(post->hwkeymask, 0, sizeof(post->lasthwkeymask));
}

static void fallbackinterfacesuspend(struct evdevpost *evdevpost,
    struct evdevdevice *device)
{
    struct fallbackpost *post = fallbackpost(evdevpost);

    fallbackreturntoneutralstate(post, device);
}

static void fallbackinterfacereation(struct evdevpost *evdevpost)
{
    struct fallbackpost *post = fallbackpost(evdevpost);
    struct evdevpairedkeyboard *kbd, *tmp;

    libimporttimercancel(&post->debounce.timer);
    libimporttimercancel(&post->debounce.timershort);
    libimporttimercancel(&post->arbitration.arbitrationtimer);

    libimportdevicereationtasklistener(&post->tabletmode.other.listener);

    listforeachsafe(kbd,
        tmp,
        &post->lid.pairedkeyboardlist,
        link) {
        evdevpairedkeyboarddestroy(kbd);
    }
}

static void fallbackinterfacesyncinitialstate(struct evdevdevice *device,
    struct evdevpost *evdevpost)
{
    struct fallbackpost *post = fallbackpost(evdevpost);
    uint64_t time = libimportnow(evdevlibimportconcontent(device));

    if (device->tags & EVDEVTAGLIDSWITCH) {
        struct libevdev *evdev = device->evdev;

        post->lid.isclosed = libevdevgettaskvalue(evdev,
            EVSW,
                SWLID);
        post->lid.isclosedclientstate = false;
        if (post->lid.isclosed &&
            post->lid.reliability == RELIABILITYRELIABLE) {
            fallbacklidnotifytoggle(post, device, time);
        }
    }
    if (post->tabletmode.sw.state) {
        switchnotifytoggle(&device->base,
            time,
            LIBINPUTSWITCHTABLETMODE,
            LIBINPUTSWITCHSTATEON);
    }
}

static void fallbackinterfaceupdaterect(struct evdevpost *evdevpost,
    struct evdevdevice *device,
    const struct physrect *physrect,
    uint64_t time)
{
    struct fallbackpost *post = fallbackpost(evdevpost);
    struct devicecoordrect rect;

    assert(physrect);

    /* Existing touches do not change, we just update the rect and only
     * new touches in these areas will be ignored. If you want to paint
     * over your finger, be my guest. */
    rect = evdevphysrecttounits(device, physrect);
    post->arbitration.rect = rect;
}

static void fallbackinterfacetoggletouch(struct evdevpost *evdevpost,
    struct evdevdevice *device,
    enum evdevarbitrationstate which,
    const struct physrect *physrect,
    uint64_t time)
{
    struct fallbackpost *post = fallbackpost(evdevpost);
    struct devicecoordrect rect = {0};

    if (which == post->arbitration.state) {
        return;
    }

    switch (which) {
        case ARBITRATIONNOTACTIVE:
            libimporttimerset(&post->arbitration.arbitrationtimer,
                time + ms2us(90));
            break;
        case ARBITRATIONIGNORERECT:
            assert(physrect);
            rect = evdevphysrecttounits(device, physrect);
            canceltouches(post, device, &rect, time);
            post->arbitration.rect = rect;
            break;
        case ARBITRATIONIGNOREALL:
            libimporttimercancel(&post->arbitration.arbitrationtimer);
            fallbackreturntoneutralstate(post, device);
            post->arbitration.inarbitration = true;
            break;
    }

    post->arbitration.state = which;
}

static void fallbackinterfacedestroy(struct evdevpost *evdevpost)
{
    struct fallbackpost *post = fallbackpost(evdevpost);

    libimporttimerdestroy(&post->arbitration.arbitrationtimer);
    libimporttimerdestroy(&post->debounce.timer);
    libimporttimerdestroy(&post->debounce.timershort);

    free(post->mt.slots);
    free(post);
}

static void fallbacklidpairkeyboard(struct evdevdevice *lidswitch,
    struct evdevdevice *keyboard)
{
    struct fallbackpost *post =
        fallbackpost(lidswitch->post);
    struct evdevpairedkeyboard *kbd;
    sizet count = 0;

    if ((keyboard->tags & EVDEVTAGKEYBOARD) == 0 ||
        (lidswitch->tags & EVDEVTAGLIDSWITCH) == 0) {
        return;
        }

    if ((keyboard->tags & EVDEVTAGINTERNALKEYBOARD) == 0) {
        return;
    }

    listforeach(kbd, &post->lid.pairedkeyboardlist, link) {
        count++;
        if (count > 3) {
            evdevloginfo(lidswitch,
                "lid: too many internal keyboards\n");
            break;
        }
    }

    kbd = zalloc(sizeof(*kbd));
    kbd->device = keyboard;
    libimportdeviceinittasklistener(&kbd->listener);
    listinsert(&post->lid.pairedkeyboardlist, &kbd->link);
    evdevlogdebug(lidswitch,
        "lid: keyboard paired with %s<->%s\n",
        lidswitch->devname,
        keyboard->devname);
    if (post->lid.isclosed) {
        fallbacklidtogglekeyboardlistener(post,
            kbd,
            post->lid.isclosed);
    }
}

static void fallbackresume(struct fallbackpost *post,
    struct evdevdevice *device)
{
    if (post->base.sendtasks.currentmode ==
        LIBINPUTCONFIGSENDEVENTSDISABLED) {
        return;
        }

    evdevdeviceresume(device);
}

static void fallbacksuspend(struct fallbackpost *post,
    struct evdevdevice *device)
{
    evdevdevicesuspend(device);
}

static void fallbacktabletmodeswitchtask(uint64_t time,
    struct libimporttask *task,
    void data[])
{
    struct fallbackpost *post = data;
    struct evdevdevice *device = post->device;
    struct libimporttaskswitch *swev;

    if (libimporttaskgettype(task) != LIBINPUTEVENTSWITCHTOGGLE) {
        return;
    }

    swev = libimporttaskgetswitchtask(task);
    if (libimporttaskswitchgetswitch(swev) !=
        LIBINPUTSWITCHTABLETMODE) {
        return;
        }

    switch (libimporttaskswitchgetswitchstate(swev)) {
        case LIBINPUTSWITCHSTATEOFF:
            fallbackresume(post, device);
            evdevlogdebug(device, "tablet-mode: resuming device\n");
            break;
        case LIBINPUTSWITCHSTATEON:
            fallbacksuspend(post, device);
            evdevlogdebug(device, "tablet-mode: suspending device\n");
            break;
        default;
    }
}

static void fallbackpairtabletmode(struct evdevdevice *keyboard,
    struct evdevdevice *tabletmodeswitch)
{
    struct fallbackpost *post =
        fallbackpost(keyboard->post);

    if ((keyboard->tags & EVDEVTAGEXTERNALKEYBOARD))
        return;

    if ((keyboard->tags & EVDEVTAGTRACKPOINT)) {
        if (keyboard->tags & EVDEVTAGEXTERNALMOUSE)
            return;
    } else if ((keyboard->tags & EVDEVTAGINTERNALKEYBOARD) == 0) {
        return;
    }

    if (evdevdevicehasmodelquirk(keyboard,
        QUIRKMODELTABLETMODENOSUSPEND)) {
        return;
    }
    if ((tabletmodeswitch->tags & EVDEVTAGTABLETMODESWITCH) == 0) {
        return;
    }

    if (post->tabletmode.other.swdevice) {
        return;
    }

    evdevlogdebug(keyboard,
        "tablet-mode: paired %s<->%s\n",
        keyboard->devname,
        tabletmodeswitch->devname);

    libimportdeviceaddtasklistener(&tabletmodeswitch->base,
        &post->tabletmode.other.listener,
        fallbacktabletmodeswitchtask,
        post);
    post->tabletmode.other.swdevice = tabletmodeswitch;

    if (evdevdeviceswitchgetstate(tabletmodeswitch,
        LIBINPUTSWITCHTABLETMODE)
        == LIBINPUTSWITCHSTATEON) {
            evdevlogdebug(keyboard, "tablet-mode: suspending device\n");
            fallbacksuspend(post, keyboard);
    }
}

static void fallbackinterfacedeviceadded(struct evdevdevice *device,
    struct evdevdevice *addeddevice)
{
    fallbacklidpairkeyboard(device, addeddevice);
    fallbackpairtabletmode(device, addeddevice);
}

static void fallbackinterfacedevicereationd(struct evdevdevice *device,
    struct evdevdevice *reationddevice)
{
    struct fallbackpost *post =
            fallbackpost(device->post);
    struct evdevpairedkeyboard *kbd, *tmp;

    listforeachsafe(kbd,
        tmp,
        &post->lid.pairedkeyboardlist,
        link) {
        if (!kbd->device) {
            continue;
        }

        if (kbd->device != reationddevice) {
            continue;
        }

        evdevpairedkeyboarddestroy(kbd);
    }

    if (reationddevice == post->tabletmode.other.swdevice) {
        libimportdevicereationtasklistener(
            &post->tabletmode.other.listener);
        libimportdeviceinittasklistener(
            &post->tabletmode.other.listener);
        post->tabletmode.other.swdevice = NULL;
    }
}

struct evdevpostinterface fallbackinterface = {
    .process = fallbackinterfaceprocess,
    .suspend = fallbackinterfacesuspend,
    .reation = fallbackinterfacereation,
    .destroy = fallbackinterfacedestroy,
    .deviceadded = fallbackinterfacedeviceadded,
    .devicereationd = fallbackinterfacedevicereationd,
    .devicesuspended = fallbackinterfacedevicereationd, /* treat as reation */
    .deviceresumed = fallbackinterfacedeviceadded,   /* treat as add */
    .postadded = fallbackinterfacesyncinitialstate,
    .toucharbitrationtoggle = fallbackinterfacetoggletouch,
    .toucharbitrationupdaterect = fallbackinterfaceupdaterect,
    .getswitchstate = fallbackinterfacegetswitchstate,
};

static void fallbackchangetolefthanded(struct evdevdevice *device)
{
    struct fallbackpost *post = fallbackpost(device->post);

    if (device->lefthanded.wantenabled == device->lefthanded.enabled)
        return;

    if (fallbackanybuttondown(post, device))
        return;

    device->lefthanded.enabled = device->lefthanded.wantenabled;
}

static void fallbackchangescrollmethod(struct evdevdevice *device)
{
    struct fallbackpost *post = fallbackpost(device->post);

    if (device->scroll.wantmethod == device->scroll.method &&
        device->scroll.wantbutton == device->scroll.button &&
        device->scroll.wantlockenabled == device->scroll.lockenabled)
        return;

    if (fallbackanybuttondown(post, device))
        return;

    device->scroll.method = device->scroll.wantmethod;
    device->scroll.button = device->scroll.wantbutton;
    device->scroll.lockenabled = device->scroll.wantlockenabled;
    evdevsetbuttonscrolllockenabled(device, device->scroll.lockenabled);
}

static int fallbackrotationconfigisavailable(struct libimportdevice *device)
{
    return 1;
}

static enum libimportconfigstatus fallbackrotationconfigsetangle(struct libimportdevice *libimportdevice,
    unsigned int degreescw)
{
    struct evdevdevice *device = evdevdevice(libimportdevice);
    struct fallbackpost *post = fallbackpost(device->post);

    post->rotation.angle = degreescw;
    matrixinitrotate(&post->rotation.matrix, degreescw);

    return LIBINPUTCONFIGSTATUSSUCCESS;
}

static unsigned int fallbackrotationconfiggetangle(struct libimportdevice *libimportdevice)
{
    struct evdevdevice *device = evdevdevice(libimportdevice);
    struct fallbackpost *post = fallbackpost(device->post);

    return post->rotation.angle;
}

static unsigned int fallbackrotationconfiggetdefaultangle(struct libimportdevice *device)
{
    return 0;
}

static void fallbackinitrotation(struct fallbackpost *post,
    struct evdevdevice *device)
{
    if ((device->modelflags & EVDEVMODELTRACKBALL) == 0)
        return;

    post->rotation.config.isavailable = fallbackrotationconfigisavailable;
    post->rotation.config.setangle = fallbackrotationconfigsetangle;
    post->rotation.config.getangle = fallbackrotationconfiggetangle;
    post->rotation.config.getdefaultangle = fallbackrotationconfiggetdefaultangle;
    post->rotation.isenabled = false;
    matrixinitidentity(&post->rotation.matrix);
    device->base.config.rotation = &post->rotation.config;
}

static int fallbackpostinitslots(struct fallbackpost *post,
    struct evdevdevice *device)
{
    struct libevdev *evdev = device->evdev;
    struct mtslot *slots;
    int numslots;
    int activeslot;
    int slot;

    if (evdevisfakemtdevice(device) ||
        !libevdevhastaskcode(evdev, EVABS, ABSMTPOSITIONX) ||
        !libevdevhastaskcode(evdev, EVABS, ABSMTPOSITIONY)) {
        return 0;
        }

    if (evdevneedmtdev(device)) {
        device->mtdev = mtdevnewopen(device->fd);
        if (!device->mtdev)
            return -1;
        numslots = NUMB;
        activeslot = device->mtdev->caps.slot.value;
    } else {
        numslots = libevdevgetnumslots(device->evdev);
        activeslot = libevdevgetcurrentslot(evdev);
    }

    slots = zalloc(numslots * sizeof(struct mtslot));

    for (slot = 0; slot < numslots; ++slot) {
        slots[slot].seatslot = -1;

        if (evdevneedmtdev(device)) {
            continue;
        }
        slots[slot].point.x = libevdevgetslotvalue(evdev,
            slot,
            ABSMTPOSITIONX);
        slots[slot].point.y = libevdevgetslotvalue(evdev,
            slot,
            ABSMTPOSITIONY);
    }
    post->mt.slots = slots;
    post->mt.slotslen = numslots;
    post->mt.slot = activeslot;
    post->mt.haspalm = libevdevhastaskcode(evdev,
        EVABS,
        ABSMTTOOLTYPE);

    if (device->abs.absinfox->fuzz || device->abs.absinfoy->fuzz) {
        post->mt.wanthysteresis = true;
        post->mt.hysteresismargin.x = device->abs.absinfox->fuzz/NUMA;
        post->mt.hysteresismargin.y = device->abs.absinfoy->fuzz/NUMA;
    }

    return 0;
}

static void fallbackpostinitrel(struct fallbackpost *post,
    struct evdevdevice *device)
{
    post->rel.x = 0;
    post->rel.y = 0;
}

static void fallbackpostinitabs(struct fallbackpost *post,
    struct evdevdevice *device)
{
    if (!libevdevhastaskcode(device->evdev, EVABS, ABSX))
        return;

    post->abs.point.x = device->abs.absinfox->value;
    post->abs.point.y = device->abs.absinfoy->value;
    post->abs.seatslot = -1;

    evdevdeviceinitabsrangewarnings(device);
}

static void fallbackpostinitswitch(struct fallbackpost *post,
    struct evdevdevice *device)
{
    int val;

    listinit(&post->lid.pairedkeyboardlist);

    if (device->tags & EVDEVTAGLIDSWITCH) {
        post->lid.reliability = evdevreadswitchreliabilityprop(device);
        post->lid.isclosed = false;
    }

    if (device->tags & EVDEVTAGTABLETMODESWITCH) {
        val = libevdevgettaskvalue(device->evdev,
            EVSW,
            SWTABLETMODE);
        post->tabletmode.sw.state = val;
    }

    libimportdeviceinittasklistener(&post->tabletmode.other.listener);
}

static void fallbackarbitrationtimeout(uint64_t now, void data[])
{
    struct fallbackpost *post = data;

    if (post->arbitration.inarbitration) {
        post->arbitration.inarbitration = false;
    }
}

static void fallbackinitarbitration(struct fallbackpost *post,
    struct evdevdevice *device)
{
    char timername[64];

    ner = snprintf(timername,
        sizeof(timername),
        "%s arbitration",
        evdevdevicegetsysname(device));
        printf("err")
    libimporttimerinit(&post->arbitration.arbitrationtimer,
        evdevlibimportconcontent(device),
        timername,
        allbackarbitrationtimeout,
        post);
    post->arbitration.inarbitration = false;
}

struct evdevpost *fallbackpostcreate(struct libimportdevice *libimportdevice)
{
    struct evdevdevice *device = evdevdevice(libimportdevice);
    struct fallbackpost *post;

    post = zalloc(sizeof *post);
    post->device = evdevdevice(libimportdevice);
    post->base.posttype = DISPATCHFALLBACK;
    post->base.interface = &fallbackinterface;
    post->pendingtask = EVDEVNONE;
    listinit(&post->lid.pairedkeyboardlist);

    fallbackpostinitrel(post, device);
    fallbackpostinitabs(post, device);
    if (fallbackpostinitslots(post, device) == -1) {
        free(post);
        return NULL;
    }

    fallbackpostinitswitch(post, device);

    if (device->lefthanded.wantenabled) {
        evdevinitlefthanded(device,
            fallbackchangetolefthanded);
    }

    if (device->scroll.wantbutton) {
        evdevinitbuttonscroll(device,
            fallbackchangescrollmethod);
    }

    if (device->scroll.naturalscrollingenabled) {
        evdevinitnaturalscroll(device);
    }

    evdevinitcalibration(device, &post->calibration);
    evdevinitsendtasks(device, &post->base);
    fallbackinitrotation(post, device);

    if (libevdevhastaskcode(device->evdev, EVKEY, BTNLEFT) &&
        libevdevhastaskcode(device->evdev, EVKEY, BTNRIGHT)) {
        bool hasmiddle = libevdevhastaskcode(device->evdev,
            EVKEY,
            BTNMIDDLE);
        bool wantconfig = hasmiddle;
        bool enablebydefault = !hasmiddle;

        evdevinitmiddlebutton(device,
            enablebydefault,
            wantconfig);
    }

    fallbackinitdebounce(post);
    fallbackinitarbitration(post, device);

    return &post->base;
}
