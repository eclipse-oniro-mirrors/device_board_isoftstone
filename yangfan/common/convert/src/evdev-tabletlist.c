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
#include "evdev-tablet.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#if HAVELIBWACOM
#include <libwacom/libwacom.h>
#endif

enum notify {
    DONTNOTIFY,
    DONOTIFY,
};

/* The tablet sends tasks every ~2ms , 50ms should be plenty enough to
   detect out-of-range.
   This value is higher during test suite runs */
static int FORCEDPROXOUTTIMEOUT = 50 * 1000; /* µs */

void tabletsetstatus(struct tabletpost *tablet, int s)
{
    return (tablet)->status |= (s);
}
void tabletunsetstatus(struct tabletpost *tablet, int s)
{
    return (tablet)->status &= ~(s);
}
void tablethasstatus(struct tabletpost *tablet, int s)
{
    return (!!((tablet)->status & (s)));
}

static void tabletgetpressedbuttons(struct tabletpost *tablet, struct buttonstate *buttons)
{
    int i;
    const struct buttonstate *state = &tablet->buttonstate, *prevstate = &tablet->prevbuttonstate;

    for (i = 0; i < sizeof(buttons->bits); i++)
        buttons->bits[i] = state->bits[i] & ~(prevstate->bits[i]);
}

static void tabletgetreleasedbuttons(struct tabletpost *tablet, struct buttonstate *buttons)
{
    int i;
    const struct buttonstate *state = &tablet->buttonstate, *prevstate = &tablet->prevbuttonstate;

    for (i = 0; i < sizeof(buttons->bits); i++)
        buttons->bits[i] = prevstate->bits[i] &
            ~(state->bits[i]);
}

/* Merge the previous state with the current one so all buttons look like
 * they just got pressed in this frame */
static void tabletforcebuttonpresses(struct tabletpost *tablet)
{
    struct buttonstate *state = &tablet->buttonstate, *prevstate = &tablet->prevbuttonstate;
    int i;

    for (i = 0; i < sizeof(state->bits); i++) {
        state->bits[i] = state->bits[i] | prevstate->bits[i];
        prevstate->bits[i] = 0;
    }
}

static int tablethistorysize(const struct tabletpost *tablet)
{
    return tablet->history.size;
}

static void tablethistoryreset(struct tabletpost *tablet)
{
    tablet->history.count = 0;
}

static void tablethistorypush(struct tabletpost *tablet, const struct tabletaxes *axes)
{
    unsigned int index = (tablet->history.index + 1) % tablethistorysize(tablet);

    tablet->history.samples[index] = *axes;
    tablet->history.index = index;
    tablet->history.count = min(tablet->history.count + 1, tablethistorysize(tablet));

    if (tablet->history.count < tablethistorysize(tablet)) {
        tablethistorypush(tablet, axes);
    }
}

/**
 * Return a previous axis state, where index of 0 means "most recent", 1 is
 * "one before most recent", etc.
 */
static const struct tabletaxes* tablethistoryget(const struct tabletpost *tablet, unsigned int index)
{
    int sz = tablethistorysize(tablet);

    assert(index < sz);
    assert(index < tablet->history.count);

    index = (tablet->history.index + sz - index) % sz;
    return &tablet->history.samples[index];
}

static void tabletresetchangedaxes(struct tabletpost *tablet)
{
    memset(tablet->changedaxes, 0, sizeof(tablet->changedaxes));
}

static bool tabletdevicehasaxis(struct tabletpost *tablet, enum libimporttablettoolaxis axis)
{
    struct libevdev *evdev = tablet->device->evdev;
    bool hasaxis = false;
    unsigned int code;

    if (axis == LIBINPUTTABLETTOOLAXISROTATIONZ) {
        hasaxis = (libevdevhastaskcode(evdev, EVKEY, BTNTOOLMOUSE) &&
                libevdevhastaskcode(evdev, EVABS, ABSTILTX) &&
                libevdevhastaskcode(evdev, EVABS, ABSTILTY));
        code = axistoevcode(axis);
        hasaxis |= libevdevhastaskcode(evdev, EVABS, code);
    } else if (axis == LIBINPUTTABLETTOOLAXISRELWHEEL) {
        hasaxis = libevdevhastaskcode(evdev, EVREL, RELWHEEL);
    } else {
        code = axistoevcode(axis);
        hasaxis = libevdevhastaskcode(evdev, EVABS, code);
    }

    return hasaxis;
}

static bool tabletfilteraxisfuzz(const struct tabletpost *tablet,
    const struct evdevdevice *device, const struct importtask *e,
    enum libimporttablettoolaxis axis)
{
    int delta, fuzz;
    int current, previous;

    previous = tablet->prevvalue[axis];
    current = e->value;
    delta = previous - current;

    fuzz = libevdevgetabsfuzz(device->evdev, e->code);

    /* ABSDISTANCE doesn't have have fuzz set and causes continuous
     * updates for the cursor/lens tools. Add a minimum fuzz of 2, same
     * as the xf86-import-wacom driver
     */
    switch (e->code) {
        case ABSDISTANCE:
            fuzz = max(2, fuzz);
            break;
        default:
            break;
    }

    return abs(delta) <= fuzz;
}

static void tabletprocessabsolute(struct tabletpost *tablet,
    struct evdevdevice *device, struct importtask *e, uint64t time)
{
    enum libimporttablettoolaxis axis;

    switch (e->code) {
        case ABSX:
        case ABSY:
        case ABSZ:
        case ABSPRESSURE:
        case ABSTILTX:
        case ABSTILTY:
        case ABSDISTANCE:
        case ABSWHEEL:
            axis = evcodetoaxis(e->code);
            if (axis == LIBINPUTTABLETTOOLAXISNONE) {
                evdevlogbuglibimport(device, "Invalid ABS task code %#x\n", e->code);
                break;
            }

            tablet->prevvalue[axis] = tablet->currentvalue[axis];
            if (tabletfilteraxisfuzz(tablet, device, e, axis)) {
                break;
            }

            tablet->currentvalue[axis] = e->value;
            setbit(tablet->changedaxes, axis);
            tabletsetstatus(tablet, TABLETAXESUPDATED);
            break;
        /* toolid is the identifier for the tool we can use in libwacom
         * to identify it (if we have one anyway) */
        case ABSMISC:
            tablet->currenttool.id = e->value;
            break;
        /* Intuos 3 strip data. Should only happen on the Pad device, not on
           the Pen device. */
        case ABSRX:
        case ABSRY:
        /* Only on the 4D mouse (Intuos2), obsolete */
        case ABSRZ:
        /* Only on the 4D mouse (Intuos2), obsolete.
           The 24HD sends ABSTHROTTLE on the Pad device for the second
           wheel but we shouldn't get here on kernel >= 3.17.
           */
        case ABSTHROTTLE:
        default:
            evdevloginfo(device, "Unhandled ABS task code %#x\n", e->code);
            break;
    }
}

static void tabletapplyrotation(struct evdevdevice *device)
{
    struct tabletpost *tablet = tabletpost(device->post);

    if (tablet->rotation.rotate == tablet->rotation.wantrotate) {
        return;
    }

    if (!tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY)) {
        return;
    }

    tablet->rotation.rotate = tablet->rotation.wantrotate;

    evdevlogdebug(device, "tablet-rotation: rotation is %s\n", tablet->rotation.rotate ? "on" : "off");
}

static void tabletchangerotation(struct evdevdevice *device, enum notify notify)
{
    struct tabletpost *tablet = tabletpost(device->post);
    struct evdevdevice *touchdevice = tablet->touchdevice;
    struct evdevpost *post;
    bool tabletisleft, touchpadisleft;

    tabletisleft = tablet->device->lefthanded.enabled;
    touchpadisleft = tablet->rotation.touchdevicelefthandedstate;

    tablet->rotation.wantrotate = tabletisleft || touchpadisleft;
    tabletapplyrotation(device);

    if (notify == DONOTIFY && touchdevice) {
        bool enable = device->lefthanded.wantenabled;

        post = touchdevice->post;
        if (post->interface->lefthandedtoggle) {
            post->interface->lefthandedtoggle(post, touchdevice, enable);
        }
    }
}

static void tabletchangetolefthanded(struct evdevdevice *device)
{
    if (device->lefthanded.enabled == device->lefthanded.wantenabled) {
        return;
    }

    device->lefthanded.enabled = device->lefthanded.wantenabled;

    tabletchangerotation(device, DONOTIFY);
}

static void tabletupdatetool(struct tabletpost *tablet,
    struct evdevdevice *device, enum libimporttablettooltype tool, bool enabled)
{
    assert(tool != LIBINPUTTOOLNONE);

    if (enabled) {
        tablet->currenttool.type = tool;
        tabletsetstatus(tablet, TABLETTOOLENTERINGPROXIMITY);
        tabletunsetstatus(tablet, TABLETTOOLOUTOFPROXIMITY);
    } else if (!tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY)) {
        tabletsetstatus(tablet, TABLETTOOLLEAVINGPROXIMITY);
    }
}

static double normalizeslider(const struct importabsinfo *absinfo)
{
    double range = absinfo->maximum - absinfo->minimum;
    double value = (absinfo->value - absinfo->minimum) / range;

    return value * 2 - 1;
}
static double normalizedistance(const struct importabsinfo *absinfo)
{
    double range = absinfo->maximum - absinfo->minimum;
    double value = (absinfo->value - absinfo->minimum) / range;

    return value;
}

static double normalizepressure(const struct importabsinfo *absinfo, struct libimporttablettool *tool)
{
    int offset;
    double range;
    double value;

    if (tool->pressure.hasoffset) {
        offset = tool->pressure.offset;
    } else {
        offset = tool->pressure.threshold.upper;
        range = absinfo->maximum - offset;
        value = (absinfo->value - offset) / range;
    }

    return max(0.0, value);
}
void doubles (const struct importabsinfo *absinfo)
{
    const int WACOMMAXDEGREES = 64;
    /* If resolution is nonzero, it's in units/radian. But require
     * a min/max less/greater than zero so we can assume 0 is the
     * center */
    if (absinfo->resolution != 0 &&
        absinfo->maximum > 0 &&
        absinfo->minimum < 0) {
        value = 180.0/MPI * absinfo->value/absinfo->resolution;
    } else {
        /* Wacom supports physical [-64, 64] degrees, so map to that by
         * default. If other tablets have a different physical range or
         * nonzero physical offsets, they need extra treatment
         * here.
         */
        /* Map to the (-1, 1) range */
        value = (value * 2) - 1;
        value *= WACOMMAXDEGREES;
    }
}
static double adjusttilt(const struct importabsinfo *absinfo)
{
    double range = absinfo->maximum - absinfo->minimum;
    double value = (absinfo->value - absinfo->minimum) / range;

    doubles (*absinfo);
    return value;
}

static int invertaxis(const struct importabsinfo *absinfo)
{
    return absinfo->maximum - (absinfo->value - absinfo->minimum);
}

static void converttilttorotation(struct tabletpost *tablet)
{
    const int offset = 5;
    double x, y;
    double angle = 0.0;

    /* Wacom Intuos 4, 5, Pro mouse calculates rotation from the x/y tilt
       values. The device has a 175 degree CCW hardware offset but since we use
       atan2 the effective offset is just 5 degrees.
       */
    x = tablet->axes.tilt.x;
    y = tablet->axes.tilt.y;

    /* atan2 is CCW, we want CW -> negate x */
    if (x || y) {
        angle = ((180.0 * atan2(-x, y)) / MPI);
    }

    angle = fmod(360 + angle - offset, 360);

    tablet->axes.rotation = angle;
    setbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISROTATIONZ);
}

static double converttodegrees(const struct importabsinfo *absinfo, double offset)
{
    /* range is [0, 360[, i.e. range + 1 */
    double range = absinfo->maximum - absinfo->minimum + 1;
    double value = (absinfo->value - absinfo->minimum) / range;

    return fmod(value * 360.0 + offset, 360.0);
}

static double normalizewheel(struct tabletpost *tablet, int value)
{
    struct evdevdevice *device = tablet->device;

    return value * device->scroll.wheelclickangle.x;
}

static void tabletupdatexy(struct tabletpost *tablet, struct evdevdevice *device)
{
    const struct importabsinfo *absinfo;
    int value;

    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISX) ||
        bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISY)) {
        absinfo = libevdevgetabsinfo(device->evdev, ABSX);

        if (tablet->rotation.rotate) {
            value = invertaxis(absinfo);
        } else {
            value = absinfo->value;

        tablet->axes.point.x = value;

        absinfo = libevdevgetabsinfo(device->evdev, ABSY);
        }

        if (tablet->rotation.rotate) {
            value = invertaxis(absinfo);
        } else {
            value = absinfo->value;

        tablet->axes.point.y = value;

        evdevtransformabsolute(device, &tablet->axes.point);
        }
    }
}

static struct normalizedcoords tablettoolprocessdelta(struct tabletpost *tablet,
    struct libimporttablettool *tool,
    const struct evdevdevice *device, struct tabletaxes *axes, uint64t time)
{
    const struct normalizedcoords zero = { 0.0, 0.0 };
    struct devicecoords delta = { 0, 0 };
    struct devicefloatcoords accel;

    /* When tool contact changes, we probably got a cursor jump. Don't
       try to calculate a delta for that task */
    if (!tablethasstatus(tablet, TABLETTOOLENTERINGPROXIMITY) &&
        !tablethasstatus(tablet, TABLETTOOLENTERINGCONTACT) &&
        !tablethasstatus(tablet, TABLETTOOLLEAVINGCONTACT) &&
        (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISX) ||
         bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISY))) {
        delta.x = axes->point.x - tablet->lastsmoothpoint.x;
        delta.y = axes->point.y - tablet->lastsmoothpoint.y;
    }

    if (axes->point.x != tablet->lastsmoothpoint.x) {
        setbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISX);
    }
    if (axes->point.y != tablet->lastsmoothpoint.y) {
        setbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISY);
    }

    tablet->lastsmoothpoint = axes->point;

    accel.x = 1.0 * delta.x;
    accel.y = 1.0 * delta.y;

    if (devicefloatiszero(accel)) {
        return zero;
    }

    return filterpost(device->pointer.filter, &accel, tool, time);
}

static void tabletupdatepressure(struct tabletpost *tablet,
    struct evdevdevice *device, struct libimporttablettool *tool)
{
    const struct importabsinfo *absinfo;

    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE)) {
        absinfo = libevdevgetabsinfo(device->evdev, ABSPRESSURE);
        tablet->axes.pressure = normalizepressure(absinfo, tool);
    }
}

static void tabletupdatedistance(struct tabletpost *tablet, struct evdevdevice *device)
{
    const struct importabsinfo *absinfo;

    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISDISTANCE)) {
        absinfo = libevdevgetabsinfo(device->evdev, ABSDISTANCE);
        tablet->axes.distance = normalizedistance(absinfo);
    }
}
static void tabletupdateslider(struct tabletpost *tablet, struct evdevdevice *device)
{
    const struct importabsinfo *absinfo;

    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISSLIDER)) {
        absinfo = libevdevgetabsinfo(device->evdev, ABSWHEEL);
        tablet->axes.slider = normalizeslider(absinfo);
    }
}

static void tabletupdatetilt(struct tabletpost *tablet, struct evdevdevice *device)
{
    const struct importabsinfo *absinfo;
    /* mouse rotation resets tilt to 0 so always fetch both axes if
     * either has changed */
    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTX) ||
        bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTY)) {
        absinfo = libevdevgetabsinfo(device->evdev, ABSTILTX);
        tablet->axes.tilt.x = adjusttilt(absinfo);

        absinfo = libevdevgetabsinfo(device->evdev, ABSTILTY);
        tablet->axes.tilt.y = adjusttilt(absinfo);

        if (device->lefthanded.enabled) {
            tablet->axes.tilt.x *= -1;
            tablet->axes.tilt.y *= -1;
        }
    }
}
static void tabletupdateartpenrotation(struct tabletpost *tablet, struct evdevdevice *device)
{
    const struct importabsinfo *absinfo;

    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISROTATIONZ)) {
        absinfo = libevdevgetabsinfo(device->evdev, ABSZ);
        /* artpen has 0 with buttons pointing east */
        tablet->axes.rotation = converttodegrees(absinfo, 90);
    }
}

static void tabletupdatemouserotation(struct tabletpost *tablet, struct evdevdevice *device)
{
    if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTX) ||
        bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTY)) {
        converttilttorotation(tablet);
    }
}

static void tabletupdaterotation(struct tabletpost *tablet,
    struct evdevdevice *device)
{
    /* We must check ROTATIONZ after TILTX/Y so that the tilt axes are
     * already normalized and set if we have the mouse/lens tool */
    if (tablet->currenttool.type == LIBINPUTTABLETTOOLTYPEMOUSE ||
        tablet->currenttool.type == LIBINPUTTABLETTOOLTYPELENS) {
        tabletupdatemouserotation(tablet, device);
        clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTX);
        clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTY);
        tablet->axes.tilt.x = 0;
        tablet->axes.tilt.y = 0;
        /* tilt is already converted to left-handed, so mouse
         * rotation is converted to left-handed automatically */
    } else {
        tabletupdateartpenrotation(tablet, device);
        if (device->lefthanded.enabled) {
            double r = tablet->axes.rotation;
            tablet->axes.rotation = fmod(180 + r, 360);
        }
    }
}

static void tabletupdatewheel(struct tabletpost *tablet, struct evdevdevice *device)
{
    int a;
    a = LIBINPUTTABLETTOOLAXISRELWHEEL;
    if (bitisset(tablet->changedaxes, a)) {
        /* tablet->axes.wheeldiscrete is already set */
        tablet->axes.wheel = normalizewheel(tablet, tablet->axes.wheeldiscrete);
    } else {
        tablet->axes.wheel = 0;
        tablet->axes.wheeldiscrete = 0;
    }
}
void fors (const struct tabletpost *tablet, struct tabletaxes smooth)
{
    int i;
    for (i = 0; i < count; i++) {
        const struct tabletaxes *a = tablethistoryget(tablet, i);

        smooth.point.x += a->point.x;
        smooth.point.y += a->point.y;

        smooth.tilt.x += a->tilt.x;
        smooth.tilt.y += a->tilt.y;
    }
}
static void tabletsmoothenaxes(const struct tabletpost *tablet, struct tabletaxes *axes)
{
    int count = tablethistorysize(tablet);
    struct tabletaxes smooth = { 0 };

    fors (*tablet, smooth);
    axes->point.x = smooth.point.x/count;
    axes->point.y = smooth.point.y/count;

    axes->tilt.x = smooth.tilt.x/count;
    axes->tilt.y = smooth.tilt.y/count;
}

static bool tabletchecknotifyaxes(struct tabletpost *tablet, struct evdevdevice *device,
    struct libimporttablettool *tool, struct tabletaxes *axesout, uint64t time)
{
    struct tabletaxes axes = {0};
    const char tmp[sizeof(tablet->changedaxes)] = {0};
    bool rc = false;

    if (memcmp(tmp, tablet->changedaxes, sizeof(tmp)) == 0) {
        axes = tablet->axes;
        /* The tool position often jumps to a different spot when contact changes.
         * If tool contact changes, clear the history to prtask axis smoothing
         * from trying to average over the spatial discontinuity. */
        if (tablethasstatus(tablet, TABLETTOOLENTERINGCONTACT) ||
            tablethasstatus(tablet, TABLETTOOLLEAVINGCONTACT)) {
            tablethistoryreset(tablet);
        }
    }

    tabletupdatexy(tablet, device);
    tabletupdatepressure(tablet, device, tool);
    tabletupdatedistance(tablet, device);
    tabletupdateslider(tablet, device);
    tabletupdatetilt(tablet, device);
    tabletupdatewheel(tablet, device);
    /* We must check ROTATIONZ after TILTX/Y so that the tilt axes are
     * already normalized and set if we have the mouse/lens tool */
    tabletupdaterotation(tablet, device);

    axes.point = tablet->axes.point;
    axes.pressure = tablet->axes.pressure;
    axes.distance = tablet->axes.distance;
    axes.slider = tablet->axes.slider;
    axes.tilt = tablet->axes.tilt;
    axes.wheel = tablet->axes.wheel;
    axes.wheeldiscrete = tablet->axes.wheeldiscrete;
    axes.rotation = tablet->axes.rotation;

    rc = true;

    tablethistorypush(tablet, &tablet->axes);
    tabletsmoothenaxes(tablet, &axes);

    /* The delta relies on the last *smooth* point, so we do it last */
    axes.delta = tablettoolprocessdelta(tablet, tool, device, &axes, time);

    *axesout = axes;

    return rc;
}

static void tabletupdatebutton(struct tabletpost *tablet, uint evcode, uint enable)
{
    switch (evcode) {
        case BTNLEFT:
        case BTNRIGHT:
        case BTNMIDDLE:
        case BTNSIDE:
        case BTNEXTRA:
        case BTNFORWARD:
        case BTNBACK:
        case BTNTASK:
        case BTNSTYLUS:
        case BTNSTYLUS2:
            break;
        default:
            evdevloginfo(tablet->device, "Unhandled button %s (%#x)\n", libevdevtaskcodegetname(EVKEY, evcode), evcode);
            return;
    }

    if (enable) {
        setbit(tablet->buttonstate.bits, evcode);
        tabletsetstatus(tablet, TABLETBUTTONSPRESSED);
    } else {
        clearbit(tablet->buttonstate.bits, evcode);
        tabletsetstatus(tablet, TABLETBUTTONSRELEASED);
    }
}

static enum libimporttablettooltype tabletevcodetotool(int code)
{
    enum libimporttablettooltype type;

    switch (code) {
        case BTNTOOLPEN:
            type = LIBINPUTTABLETTOOLTYPEPEN;
            break;
        case BTNTOOLRUBBER:
            type = LIBINPUTTABLETTOOLTYPEERASER;
            break;
        case BTNTOOLBRUSH:
            type = LIBINPUTTABLETTOOLTYPEBRUSH;
            break;
        case BTNTOOLPENCIL:
            type = LIBINPUTTABLETTOOLTYPEPENCIL;
            break;
        case BTNTOOLAIRBRUSH:
            type = LIBINPUTTABLETTOOLTYPEAIRBRUSH;
            break;
        case BTNTOOLMOUSE:
            type = LIBINPUTTABLETTOOLTYPEMOUSE;
            break;
        case BTNTOOLLENS:
            type = LIBINPUTTABLETTOOLTYPELENS;
            break;
        default:
            abort();
    }

    return type;
}

static void tabletprocesskey(struct tabletpost *tablet, struct evdevdevice *device, struct importtask *e, uint64t time)
{
    enum libimporttablettooltype type;

    /* ignore kernel key repeat */
    if (e->value == 2) {
        return;
    }

    switch (e->code) {
        case BTNTOOLFINGER:
            evdevlogbuglibimport(device, "Invalid tool 'finger' on tablet interface\n");
            break;
        case BTNTOOLPEN:
        case BTNTOOLRUBBER:
        case BTNTOOLBRUSH:
        case BTNTOOLPENCIL:
        case BTNTOOLAIRBRUSH:
        case BTNTOOLMOUSE:
        case BTNTOOLLENS:
            type = tabletevcodetotool(e->code);
            tabletsetstatus(tablet, TABLETTOOLUPDATED);
            if (e->value) {
                tablet->toolstate |= bit(type);
            } else {
                tablet->toolstate &= ~bit(type);
            }
            break;
        case BTNTOUCH:
            if (!bitisset(tablet->axiscaps, LIBINPUTTABLETTOOLAXISPRESSURE)) {
                if (e->value) {
                    tabletsetstatus(tablet, TABLETTOOLENTERINGCONTACT);
                } else {
                    tabletsetstatus(tablet, TABLETTOOLLEAVINGCONTACT);
                }
            }
            break;
        default:
            tabletupdatebutton(tablet, e->code, e->value);
            break;
    }
}

static void tabletprocessrelative(struct tabletpost *tablet,
    struct evdevdevice *device, struct importtask *e, uint64t time)
{
    enum libimporttablettoolaxis axis;

    switch (e->code) {
        case RELWHEEL:
            axis = relevcodetoaxis(e->code);
            if (axis == LIBINPUTTABLETTOOLAXISNONE) {
                evdevlogbuglibimport(device, "Invalid ABS task code %#x\n", e->code);
                break;
            }
            setbit(tablet->changedaxes, axis);
            tablet->axes.wheeldiscrete = -1 * e->value;
            tabletsetstatus(tablet, TABLETAXESUPDATED);
            break;
        default:
            evdevloginfo(device, "Unhandled relative axis %s (%#x)\n",
                libevdevtaskcodegetname(EVREL, e->code), e->code);
            return;
    }
}

static void tabletprocessmisc(struct tabletpost *tablet, struct evdevdevice *device, struct importtask *e, uint64t time)
{
    switch (e->code) {
        case MSCSERIAL:
            if (e->value != -1) {
                tablet->currenttool.serial = e->value;
            }

            break;
        case MSCSCAN:
            break;
        default:
            evdevloginfo(device, "Unhandled MSC task code %s (%#x)\n",
                libevdevtaskcodegetname(EVMSC, e->code), e->code);
            break;
    }
}

static void copyaxiscap(const struct tabletpost *tablet,
    struct libimporttablettool *tool, enum libimporttablettoolaxis axis)
{
    if (bitisset(tablet->axiscaps, axis)) {
        setbit(tool->axiscaps, axis);
    }
}

static void copybuttoncap(const struct tabletpost *tablet, struct libimporttablettool *tool, uint button)
{
    struct libevdev *evdev = tablet->device->evdev;
    if (libevdevhastaskcode(evdev, EVKEY, button)) {
        setbit(tool->buttons, button);
    }
}
void ifll (type, axes)
{
    axes = libwacomstylusgetaxes(s);
    if (axes & WACOMAXISTYPETILT) {
        /* tilt on the puck is converted to rotation */
        if (type == WSTYLUSPUCK) {
            setbit(tool->axiscaps, LIBINPUTTABLETTOOLAXISROTATIONZ);
        } else {
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISTILTX);
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISTILTY);
        }
    }
    if (axes & WACOMAXISTYPEROTATIONZ) {
        copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISROTATIONZ);
    }
    if (axes & WACOMAXISTYPEDISTANCE) {
        copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISDISTANCE);
    }
    if (axes & WACOMAXISTYPESLIDER) {
        copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISSLIDER);
    }
    if (axes & WACOMAXISTYPEPRESSURE) {
        copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISPRESSURE);
    }
}
static int toolsetbitsfromlibwacom(const struct tabletpost *tablet, struct libimporttablettool *tool)
{
    int rc = 1;

#if HAVELIBWACOM
    WacomDeviceDatabase *db;
    const WacomStylus *s = NULL;
    int code;
    WacomStylusType type;
    WacomAxisTypeFlags axes;

    db = tabletlibimportconcontent(tablet)->libwacom.db;
    if (!db) {
        return rc;
    }

    s = libwacomstylusgetforid(db, tool->toolid);
    if (!s) {
        return rc;
    }

    type = libwacomstylusgettype(s);
    if (type == WSTYLUSPUCK) {
        for (code = BTNLEFT;
             code < BTNLEFT + libwacomstylusgetnumbuttons(s);
             code++) {
            copybuttoncap(tablet, tool, code);
        }
    } else {
        if (libwacomstylusgetnumbuttons(s) >= 2) {
            copybuttoncap(tablet, tool, BTNSTYLUS2);
        }
        if (libwacomstylusgetnumbuttons(s) >= 1) {
            copybuttoncap(tablet, tool, BTNSTYLUS);
        }
    }

    if (libwacomstylushaswheel(s)) {
        copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISRELWHEEL);
    }

    ifll (type, axes);

    rc = 0;
#endif
    return rc;
}
void switchl (tablet, tool)
{
    /* If we don't have libwacom, we simply copy any axis we have on the
       tablet onto the tool. Except we know that mice only have rotation
       anyway.
     */
    switch (type) {
        case LIBINPUTTABLETTOOLTYPEPEN:
        case LIBINPUTTABLETTOOLTYPEERASER:
        case LIBINPUTTABLETTOOLTYPEPENCIL:
        case LIBINPUTTABLETTOOLTYPEBRUSH:
        case LIBINPUTTABLETTOOLTYPEAIRBRUSH:
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISPRESSURE);
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISDISTANCE);
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISTILTX);
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISTILTY);
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISSLIDER);

            /* Rotation is special, it can be either ABSZ or
             * BTNTOOLMOUSE+ABSTILTX/Y. Aiptek tablets have
             * mouse+tilt (and thus rotation), but they do not have
             * ABSZ. So let's not copy the axis bit if we don't have
             * ABSZ, otherwise we try to get the value from it later on
             * proximity in and go boom because the absinfo isn't there.
             */
            if (libevdevhastaskcode(tablet->device->evdev, EVABS, ABSZ)) {
                copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISROTATIONZ);
            }
            break;
        case LIBINPUTTABLETTOOLTYPEMOUSE:
        case LIBINPUTTABLETTOOLTYPELENS:
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISROTATIONZ);
            copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISRELWHEEL);
            break;
        default:
            break;
    }
}
static void toolsetbits(const struct tabletpost *tablet, struct libimporttablettool *tool)
{
    enum libimporttablettooltype type = tool->type;

    copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISX);
    copyaxiscap(tablet, tool, LIBINPUTTABLETTOOLAXISY);

#if HAVELIBWACOM
    if (toolsetbitsfromlibwacom(tablet, tool) == 0) {
        return;
    }
#endif
    switchl (tablet, tool);
    /* If we don't have libwacom, copy all pen-related buttons from the
       tablet vs all mouse-related buttons */
    switch (type) {
        case LIBINPUTTABLETTOOLTYPEPEN:
        case LIBINPUTTABLETTOOLTYPEBRUSH:
        case LIBINPUTTABLETTOOLTYPEAIRBRUSH:
        case LIBINPUTTABLETTOOLTYPEPENCIL:
        case LIBINPUTTABLETTOOLTYPEERASER:
            copybuttoncap(tablet, tool, BTNSTYLUS);
            copybuttoncap(tablet, tool, BTNSTYLUS2);
            break;
        case LIBINPUTTABLETTOOLTYPEMOUSE:
        case LIBINPUTTABLETTOOLTYPELENS:
            copybuttoncap(tablet, tool, BTNLEFT);
            copybuttoncap(tablet, tool, BTNMIDDLE);
            copybuttoncap(tablet, tool, BTNRIGHT);
            copybuttoncap(tablet, tool, BTNSIDE);
            copybuttoncap(tablet, tool, BTNEXTRA);
            break;
        default:
            break;
    }
}

static int axisrangepercentage(const struct importabsinfo *a, double percent)
{
    return (a->maximum - a->minimum) * percent/100.0 + a->minimum;
}

static void toolsetpressurethresholds(struct tabletpost *tablet, struct libimporttablettool *tool)
{
    struct evdevdevice *device = tablet->device;
    const struct importabsinfo *pressure;
    struct quirksconcontent *quirks = NULL;
    struct quirks *q = NULL;
    struct quirkrange r;
    int lo = 0, hi = 1;

    tool->pressure.offset = 0;
    tool->pressure.hasoffset = false;

    pressure = libevdevgetabsinfo(device->evdev, ABSPRESSURE);
    if (!pressure) {
        tool->pressure.threshold.upper = hi;
        tool->pressure.threshold.lower = lo;
    }

    quirks = evdevlibimportconcontent(device)->quirks;
    q = quirksfetchfordevice(quirks, device->udevdevice);

    tool->pressure.offset = pressure->minimum;

    /* 5 and 1% of the pressure range */
    hi = axisrangepercentage(pressure, 5);
    lo = axisrangepercentage(pressure, 1);

    if (q && quirksgetrange(q, QUIRKATTRPRESSURERANGE, &r)) {
        if (r.lower >= r.upper) {
            evdevloginfo(device, "Invalid pressure range, using defaults\n");
        } else {
            hi = r.upper;
            lo = r.lower;
        }
    }
    quirksunref(q);
}

static struct libimporttablettool *tabletgettool(struct tabletpost *tablet,
    enum libimporttablettooltype type, uint toolid, uint serial)
{
    struct libimport *libimport = tabletlibimportconcontent(tablet);
    struct libimporttablettool *tool = NULL, *t;
    struct list *toollist;

    if (serial) {
        toollist = &libimport->toollist;
        /* Check if we already have the tool in our list of tools */
        listforeach(t, toollist, link) {
            if (type == t->type && serial == t->serial) {
                tool = t;
                break;
            }
        }
    }

    /* If we get a tool with a delayed serial number, we already created
     * a 0-serial number tool for it earlier. Re-use that, even though
     * it means we can't distinguish this tool from others.
     * https://bugs.freedesktop.org/showbug.cgi?id=97526
     */
    if (!tool) {
        toollist = &tablet->toollist;
        /* We can't guarantee that tools without serial numbers are
         * unique, so we keep them local to the tablet that they come
         * into proximity of instead of storing them in the global tool
         * list
         * Same as above, but don't bother checking the serial number
         */
        listforeach(t, toollist, link) {
            if (type == t->type) {
                tool = t;
                break;
            }
        }

        /* Didn't find the tool but we have a serial. Switch
         * toollist back so we create in the correct list */
        if (!tool && serial) {
            toollist = &libimport->toollist;
        }
    }

    /* If we didn't already have the newtool in our list of tools,
     * add it */
    if (!tool) {
        tool = zalloc(sizeof *tool);

        *tool = (struct libimporttablettool) {
            .type = type,
            .serial = serial,
            .toolid = toolid,
            .refcount = 1,
        };

        toolsetpressurethresholds(tablet, tool);
        toolsetbits(tablet, tool);

        listinsert(toollist, &tool->link);
    }

    return tool;
}