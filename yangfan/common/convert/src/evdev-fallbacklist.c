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

static void
fallbackkeyboardnotifykey(struct fallbackpost *post,
                 struct evdevdevice *device,
                 uint64t time,
                 int key,
                 enum libimportkeystate state)
{
    int downcount;

    downcount = evdevupdatekeydowncount(device, key, state);

    if ((state == LIBINPUTKEYSTATEPRESSED && downcount == 1) ||
        (state == LIBINPUTKEYSTATERELEASED && downcount == 0))
        keyboardnotifykey(&device->base, time, key, state);
}

static void
fallbacklidnotifytoggle(struct fallbackpost *post,
               struct evdevdevice *device,
               uint64t time)
{
    if (post->lid.isclosed ^ post->lid.isclosedclientstate) {
        switchnotifytoggle(&device->base,
                     time,
                     LIBINPUTSWITCHLID,
                     post->lid.isclosed);
        post->lid.isclosedclientstate = post->lid.isclosed;
    }
}

static enum libimportswitchstate
fallbackinterfacegetswitchstate(struct evdevpost *evdevpost,
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

static inline void
normalizedelta(struct evdevdevice *device,
        const struct devicecoords *delta,
        struct normalizedcoords *normalized)
{
    normalized->x = delta->x * DEFAULTMOUSEDPI / (double)device->dpi;
    normalized->y = delta->y * DEFAULTMOUSEDPI / (double)device->dpi;
}

static inline bool
posttrackpointscroll(struct evdevdevice *device,
               struct normalizedcoords unaccel,
               uint64t time)
{
    if (device->scroll.method != LIBINPUTCONFIGSCROLLONBUTTONDOWN)
        return false;

    switch(device->scroll.buttonscrollstate) {
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

static inline bool
fallbackfilterdefuzztouch(struct fallbackpost *post,
                 struct evdevdevice *device,
                 struct mtslot *slot)
{
    struct devicecoords point;

    if (!post->mt.wanthysteresis)
        return false;

    point = evdevhysteresis(&slot->point,
                 &slot->hysteresiscenter,
                 &post->mt.hysteresismargin);
    slot->point = point;

    if (point.x == slot->hysteresiscenter.x &&
        point.y == slot->hysteresiscenter.y)
        return true;

    slot->hysteresiscenter = point;

    return false;
}

static inline void
fallbackrotaterelative(struct fallbackpost *post,
             struct evdevdevice *device)
{
    struct devicecoords rel = post->rel;

    if (!device->base.config.rotation)
        return;

    /* loss of precision for non-90 degrees, but we only support 90 deg
     * right now anyway */
    matrixmultvec(&post->rotation.matrix, &rel.x, &rel.y);

    post->rel = rel;
}

static void
fallbackflushrelativemotion(struct fallbackpost *post,
                   struct evdevdevice *device,
                   uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct normalizedcoords accel, unaccel;
    struct devicefloatcoords raw;

    if (!(device->seatcaps & EVDEVDEVICEPOINTER))
        return;

    fallbackrotaterelative(post, device);

    normalizedelta(device, &post->rel, &unaccel);
    raw.x = post->rel.x;
    raw.y = post->rel.y;
    post->rel.x = 0;
    post->rel.y = 0;

    /* Use unaccelerated deltas for pointing stick scroll */
    if (posttrackpointscroll(device, unaccel, time))
        return;

    if (device->pointer.filter) {
        /* Apply pointer acceleration. */
        accel = filterpost(device->pointer.filter,
                    &raw,
                    device,
                    time);
    } else {
        evdevlogbuglibimport(device,
                       "accel filter missing\n");
        accel = unaccel;
    }

    if (normalizediszero(accel) && normalizediszero(unaccel))
        return;

    pointernotifymotion(base, time, &accel, &raw);
}

static void
fallbackflushwheels(struct fallbackpost *post,
              struct evdevdevice *device,
              uint64t time)
{
    struct normalizedcoords wheeldegrees = { 0.0, 0.0 };
    struct discretecoords discrete = { 0.0, 0.0 };

    if (!(device->seatcaps & EVDEVDEVICEPOINTER))
        return;

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

static void
fallbackflushabsolutemotion(struct fallbackpost *post,
                   struct evdevdevice *device,
                   uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct devicecoords point;

    if (!(device->seatcaps & EVDEVDEVICEPOINTER))
        return;

    point = post->abs.point;
    evdevtransformabsolute(device, &point);

    pointernotifymotionabsolute(base, time, &point);
}

static bool
fallbackflushmtdown(struct fallbackpost *post,
               struct evdevdevice *device,
               int slotidx,
               uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct devicecoords point;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    slot = &post->mt.slots[slotidx];
    if (slot->seatslot != -1) {
        evdevlogbugkernel(device,
                     "driver sent multiple touch down for the same slot");
        return false;
    }

    seatslot = ffs(~seat->slotmap) - 1;
    slot->seatslot = seatslot;

    if (seatslot == -1)
        return false;

    seat->slotmap |= bit(seatslot);
    point = slot->point;
    slot->hysteresiscenter = point;
    evdevtransformabsolute(device, &point);

    touchnotifytouchdown(base, time, slotidx, seatslot,
                &point);

    return true;
}

static bool
fallbackflushmtmotion(struct fallbackpost *post,
             struct evdevdevice *device,
             int slotidx,
             uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct devicecoords point;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    slot = &post->mt.slots[slotidx];
    seatslot = slot->seatslot;
    point = slot->point;

    if (seatslot == -1)
        return false;

    if (fallbackfilterdefuzztouch(post, device, slot))
        return false;

    evdevtransformabsolute(device, &point);
    touchnotifytouchmotion(base, time, slotidx, seatslot,
                  &point);

    return true;
}

static bool
fallbackflushmtup(struct fallbackpost *post,
             struct evdevdevice *device,
             int slotidx,
             uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    slot = &post->mt.slots[slotidx];
    seatslot = slot->seatslot;
    slot->seatslot = -1;

    if (seatslot == -1)
        return false;

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchup(base, time, slotidx, seatslot);

    return true;
}

static bool
fallbackflushmtcancel(struct fallbackpost *post,
             struct evdevdevice *device,
             int slotidx,
             uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct mtslot *slot;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    slot = &post->mt.slots[slotidx];
    seatslot = slot->seatslot;
    slot->seatslot = -1;

    if (seatslot == -1)
        return false;

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchcancel(base, time, slotidx, seatslot);

    return true;
}

static bool
fallbackflushstdown(struct fallbackpost *post,
               struct evdevdevice *device,
               uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    struct devicecoords point;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    if (post->abs.seatslot != -1) {
        evdevlogbugkernel(device,
                     "driver sent multiple touch down for the same slot");
        return false;
    }

    seatslot = ffs(~seat->slotmap) - 1;
    post->abs.seatslot = seatslot;

    if (seatslot == -1)
        return false;

    seat->slotmap |= bit(seatslot);

    point = post->abs.point;
    evdevtransformabsolute(device, &point);

    touchnotifytouchdown(base, time, -1, seatslot, &point);

    return true;
}

static bool
fallbackflushstmotion(struct fallbackpost *post,
             struct evdevdevice *device,
             uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct devicecoords point;
    int seatslot;

    point = post->abs.point;
    evdevtransformabsolute(device, &point);

    seatslot = post->abs.seatslot;

    if (seatslot == -1)
        return false;

    touchnotifytouchmotion(base, time, -1, seatslot, &point);

    return true;
}

static bool
fallbackflushstup(struct fallbackpost *post,
             struct evdevdevice *device,
             uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    seatslot = post->abs.seatslot;
    post->abs.seatslot = -1;

    if (seatslot == -1)
        return false;

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchup(base, time, -1, seatslot);

    return true;
}

static bool
fallbackflushstcancel(struct fallbackpost *post,
             struct evdevdevice *device,
             uint64t time)
{
    struct libimportdevice *base = &device->base;
    struct libimportseat *seat = base->seat;
    int seatslot;

    if (!(device->seatcaps & EVDEVDEVICETOUCH))
        return false;

    seatslot = post->abs.seatslot;
    post->abs.seatslot = -1;

    if (seatslot == -1)
        return false;

    seat->slotmap &= ~bit(seatslot);

    touchnotifytouchcancel(base, time, -1, seatslot);

    return true;
}

static void
fallbackprocesstouchbutton(struct fallbackpost *post,
                  struct evdevdevice *device,
                  uint64t time, int value)
{
    post->pendingtask |= (value) ?
                 EVDEVABSOLUTETOUCHDOWN :
                 EVDEVABSOLUTETOUCHUP;
}

static inline void
fallbackprocesskey(struct fallbackpost *post,
             struct evdevdevice *device,
             struct importtask *e, uint64t time)
{
    enum keytype type;

    /* ignore kernel key repeat */
    if (e->value == 2)
        return;

    if (e->code == BTNTOUCH) {
        if (!device->ismt)
            fallbackprocesstouchbutton(post,
                              device,
                              time,
                              e->value);
        return;
    }

    type = getkeytype(e->code);

    switch (type) {
    case KEYTYPENONE:
        break;
    case KEYTYPEKEY:
    case KEYTYPEBUTTON:
        if ((e->value && hwiskeydown(post, e->code)) ||
            (e->value == 0 && !hwiskeydown(post, e->code)))
            return;

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

static void
fallbackprocesstouch(struct fallbackpost *post,
               struct evdevdevice *device,
               struct importtask *e,
               uint64t time)
{
    struct mtslot *slot = &post->mt.slots[post->mt.slot];

    if (e->code == ABSMTSLOT) {
        if ((sizet)e->value >= post->mt.slotslen) {
            evdevlogbuglibimport(device,
                     "exceeded slot count (%d of max %zd)\n",
                     e->value,
                     post->mt.slotslen);
            e->value = post->mt.slotslen - 1;
        }
        post->mt.slot = e->value;
        return;
    }

    switch (e->code) {
    case ABSMTTRACKINGID:
        if (e->value >= 0) {
            post->pendingtask |= EVDEVABSOLUTEMT;
            slot->state = SLOTSTATEBEGIN;
            if (post->mt.haspalm) {
                int v;
                v = libevdevgetslotvalue(device->evdev,
                                post->mt.slot,
                                ABSMTTOOLTYPE);
                switch (v) {
                case MTTOOLPALM:
                    /* new touch, no cancel needed */
                    slot->palmstate = PALMWASPALM;
                    break;
                default:
                    slot->palmstate = PALMNONE;
                    break;
                }
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
            if (slot->palmstate == PALMNONE)
                slot->palmstate = PALMNEW;
            break;
        default:
            if (slot->palmstate == PALMISPALM)
                slot->palmstate = PALMWASPALM;
            break;
        }
        post->pendingtask |= EVDEVABSOLUTEMT;
        slot->dirty = true;
        break;
    }
}

static inline void
fallbackprocessabsolutemotion(struct fallbackpost *post,
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

static void
fallbacklidkeyboardtask(uint64t time,
                struct libimporttask *task,
                void *data)
{
    struct fallbackpost *post = fallbackpost(data);

    if (!post->lid.isclosed)
        return;

    if (task->type != LIBINPUTEVENTKEYBOARDKEY)
        return;

    if (post->lid.reliability == RELIABILITYWRITEOPEN) {
        int fd = libevdevgetfd(post->device->evdev);
        int rc;
        struct importtask ev[2];

        ev[0] = importtaskinit(0, EVSW, SWLID, 0);
        ev[1] = importtaskinit(0, EVSYN, SYNREPORT, 0);

        rc = write(fd, ev, sizeof(ev));

        if (rc < 0)
            evdevlogerror(post->device,
                    "failed to write SWLID state (%s)",
                    strerror(errno));
    }

    post->lid.isclosed = false;
    fallbacklidnotifytoggle(post, post->device, time);
}

static void
fallbacklidtogglekeyboardlistener(struct fallbackpost *post,
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

static void
fallbacklidtogglekeyboardlisteners(struct fallbackpost *post,
                       bool isclosed)
{
    struct evdevpairedkeyboard *kbd;

    listforeach(kbd, &post->lid.pairedkeyboardlist, link) {
        if (!kbd->device)
            continue;

        fallbacklidtogglekeyboardlistener(post,
                              kbd,
                              isclosed);
    }
}

static inline void
fallbackprocessswitch(struct fallbackpost *post,
            struct evdevdevice *device,
            struct importtask *e,
            uint64t time)
{
    enum libimportswitchstate state;
    bool isclosed;

    switch (e->code) {
    case SWLID:
        isclosed = !!e->value;

        fallbacklidtogglekeyboardlisteners(post, isclosed);

        if (post->lid.isclosed == isclosed)
            return;

        post->lid.isclosed = isclosed;
        fallbacklidnotifytoggle(post, device, time);
        break;
    case SWTABLETMODE:
        if (post->tabletmode.sw.state == e->value)
            return;

        post->tabletmode.sw.state = e->value;
        if (e->value)
            state = LIBINPUTSWITCHSTATEON;
        else
            state = LIBINPUTSWITCHSTATEOFF;
        switchnotifytoggle(&device->base,
                     time,
                     LIBINPUTSWITCHTABLETMODE,
                     state);
        break;
    }
}

static inline bool
fallbackrejectrelative(struct evdevdevice *device,
             const struct importtask *e,
             uint64t time)
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

static inline void
fallbackprocessrelative(struct fallbackpost *post,
              struct evdevdevice *device,
              struct importtask *e, uint64t time)
{
    if (fallbackrejectrelative(device, e, time))
        return;

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

static inline void
fallbackprocessabsolute(struct fallbackpost *post,
              struct evdevdevice *device,
              struct importtask *e,
              uint64t time)
{
    if (device->ismt) {
        fallbackprocesstouch(post, device, e, time);
    } else {
        fallbackprocessabsolutemotion(post, device, e);
    }
}

static inline bool
fallbackanybuttondown(struct fallbackpost *post,
              struct evdevdevice *device)
{
    unsigned int button;

    for (button = BTNLEFT; button < BTNJOYSTICK; button++) {
        if (libevdevhastaskcode(device->evdev, EVKEY, button) &&
            hwiskeydown(post, button))
            return true;
    }
    return false;
}

static inline bool
fallbackarbitratetouch(struct fallbackpost *post,
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

static inline bool
fallbackflushmttasks(struct fallbackpost *post,
             struct evdevdevice *device,
             uint64t time)
{
    bool sent = false;

    for (sizet i = 0; i < post->mt.slotslen; i++) {
        struct mtslot *slot = &post->mt.slots[i];

        if (!slot->dirty)
            continue;

        slot->dirty = false;
        if (slot->palmstate == PALMNEW) {
            if (slot->state != SLOTSTATEBEGIN)
                sent = fallbackflushmtcancel(post,
                                device,
                                i,
                                time);
            slot->palmstate = PALMISPALM;
        } else if (slot->palmstate == PALMNONE) {
            switch (slot->state) {
            case SLOTSTATEBEGIN:
                if (!fallbackarbitratetouch(post,
                                 slot)) {
                    sent = fallbackflushmtdown(post,
                                      device,
                                      i,
                                      time);
                }
                break;
            case SLOTSTATEUPDATE:
                sent = fallbackflushmtmotion(post,
                                device,
                                i,
                                time);
                break;
            case SLOTSTATEEND:
                sent = fallbackflushmtup(post,
                                device,
                                i,
                                time);
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
