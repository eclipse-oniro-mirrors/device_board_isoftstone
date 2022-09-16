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
        case ABSMISC:
            tablet->currenttool.id = e->value;
            break;
        case ABSRX:
        case ABSRY:
        /* Only on the 4D mouse (Intuos2), obsolete */
        case ABSRZ:
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
    if (absinfo->resolution != 0 &&
        absinfo->maximum > 0 &&
        absinfo->minimum < 0) {
        value = 180.0/MPI * absinfo->value/absinfo->resolution;
        if (0) {
            printf("printf error");
        }
    } else {
        value = (value * 2) - 1;
        value *= WACOMMAXDEGREES;
        if (0) {
            printf("printf error");
        }
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
void ifdd (struct tabletpost *tablet, double angle)
{
    x = tablet->axes.tilt.x;
    y = tablet->axes.tilt.y;

    if (x || y) {
        angle = ((180.0 * atan2(-x, y)) / MPI);
        if (0) {
            printf("printf error");
        }
    }
}
static void converttilttorotation(struct tabletpost *tablet)
{
    const int offset = 5;
    double x, y;
    double angle = 0.0;
    ifdd (*tablet, angle);

    angle = fmod(360 + angle - offset, 360);

    tablet->axes.rotation = angle;
    setbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISROTATIONZ);
}

static double converttodegrees(const struct importabsinfo *absinfo, double offset)
{
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
    if (tablet->currenttool.type == LIBINPUTTABLETTOOLTYPEMOUSE ||
        tablet->currenttool.type == LIBINPUTTABLETTOOLTYPELENS) {
        tabletupdatemouserotation(tablet, device);
        clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTX);
        clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTY);
        tablet->axes.tilt.x = 0;
        tablet->axes.tilt.y = 0;
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

    if (!tool) {
        toollist = &tablet->toollist;

        listforeach(t, toollist, link) {
            if (type == t->type) {
                tool = t;
                break;
            }
        }

        if (!tool && serial) {
            toollist = &libimport->toollist;
        }
    }

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
static void tabletnotifybuttonmask(struct tabletpost *tablet, struct evdevdevice *device, uint64t time,
    struct libimporttablettool *tool, const struct buttonstate *buttons, enum libimportbuttonstate state)
{
    struct libimportdevice *base = &device->base;
    int i;
    int nbits = 8 * sizeof(buttons->bits);
    enum libimporttablettooltipstate tipstate;

    if (tablethasstatus(tablet, TABLETTOOLINCONTACT)) {
        tipstate = LIBINPUTTABLETTOOLTIPDOWN;
    } else {
        tipstate = LIBINPUTTABLETTOOLTIPUP;
    }

    for (i = 0; i < nbits; i++) {
        if (!bitisset(buttons->bits, i)) {
            continue;
        }

        tabletnotifybutton(base, time, tool, tipstate, &tablet->axes, i, state);
    }
}

static void tabletnotifybuttons(struct tabletpost *tablet, struct evdevdevice *device, uint64t time,
    struct libimporttablettool *tool, enum libimportbuttonstate state)
{
    struct buttonstate buttons;

    if (state == LIBINPUTBUTTONSTATEPRESSED) {
        tabletgetpressedbuttons(tablet, &buttons);
    } else {
        tabletgetreleasedbuttons(tablet, &buttons);
    }

    tabletnotifybuttonmask(tablet, device, time, tool, &buttons, state);
}

static void sanitizepressuredistance(struct tabletpost *tablet, struct libimporttablettool *tool)
{
    bool toolincontact;
    const struct importabsinfo *distance, *pressure;

    distance = libevdevgetabsinfo(tablet->device->evdev, ABSDISTANCE);
    pressure = libevdevgetabsinfo(tablet->device->evdev, ABSPRESSURE);

    if (!pressure || !distance) {
        return;
    }

    if (!bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISDISTANCE) &&
        !bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE)) {
        return;
        }

    toolincontact = (pressure->value > tool->pressure.offset);

    /* Keep distance and pressure mutually exclusive */
    if (distance &&
        (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISDISTANCE) ||
         bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE)) &&
        distance->value > distance->minimum &&
        pressure->value > pressure->minimum) {
        if (toolincontact) {
            clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISDISTANCE);
            tablet->axes.distance = 0;
        } else {
            clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE);
            tablet->axes.pressure = 0;
        }
    } else if (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE) &&
           !toolincontact) {
        /* Make sure that the last axis value sent to the caller is a 0 */
        if (tablet->axes.pressure == 0) {
            clearbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE);
        } else {
            tablet->axes.pressure = 0;
        }
    }
}

static inline void sanitizemouselensrotation(struct tabletpost *tablet)
{
    /* If we have a mouse/lens cursor and the tilt changed, the rotation
       changed. Mark this, calculate the angle later */
    if ((tablet->currenttool.type == LIBINPUTTABLETTOOLTYPEMOUSE ||
        tablet->currenttool.type == LIBINPUTTABLETTOOLTYPELENS) &&
        (bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTX) ||
        bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISTILTY))) {
        setbit(tablet->changedaxes, LIBINPUTTABLETTOOLAXISROTATIONZ);
        }
}

static void sanitizetabletaxes(struct tabletpost *tablet, struct libimporttablettool *tool)
{
    sanitizepressuredistance(tablet, tool);
    sanitizemouselensrotation(tablet);
}

static void detectpressureoffset(struct tabletpost *tablet, struct evdevdevice *device, struct libimporttablettool *tool)
{
    const struct importabsinfo *pressure, *distance;
    int offset;

    if (!bitisset(tablet->changedaxes, LIBINPUTTABLETTOOLAXISPRESSURE)) {
        return;
    }

    pressure = libevdevgetabsinfo(device->evdev, ABSPRESSURE);
    distance = libevdevgetabsinfo(device->evdev, ABSDISTANCE);

    if (!pressure || !distance) {
        return;
    }

    offset = pressure->value;

    /* If we have an task that falls below the current offset, adjust
     * the offset downwards. A fast contact can start with a
     * higher-than-needed pressure offset and then we'd be tied into a
     * high pressure offset for the rest of the session.
     */
    if (tool->pressure.hasoffset) {
        if (offset < tool->pressure.offset) {
            tool->pressure.offset = offset;
        }
        return;
    }

    if (offset <= pressure->minimum) {
        return;
    }

    /* we only set a pressure offset on proximity in */
    if (!tablethasstatus(tablet, TABLETTOOLENTERINGPROXIMITY)) {
        return;
    }

    /* If we're closer than 50% of the distance axis, skip pressure
     * offset detection, too likely to be wrong */
    if (distance->value < axisrangepercentage(distance, 50)) {
        return;
    }

    if (offset > axisrangepercentage(pressure, 20)) {
        evdevlogerror(device, "Ignoring pressure offset greater than 20%% detected on tool %s (serial %#x). "
        "See %s/tablet-support.html\n", tablettooltypetostring(tool->type), tool->serial, HTTPDOCLINK);
        return;
    }

    evdevloginfo(device, "Pressure offset detected on tool %s (serial %#x).  " "See %s/tablet-support.html\n",
        tablettooltypetostring(tool->type), tool->serial, HTTPDOCLINK);
    tool->pressure.offset = offset;
    tool->pressure.hasoffset = true;
    tool->pressure.threshold.lower = pressure->minimum;
}

static void detecttoolcontact(struct tabletpost *tablet, struct evdevdevice *device, struct libimporttablettool *tool)
{
    const struct importabsinfo *p;
    int pressure;

    if (!bitisset(tool->axiscaps, LIBINPUTTABLETTOOLAXISPRESSURE)) {
        return;
    }

    /* if we have pressure, always use that for contact, not BTNTOUCH */
    if (tablethasstatus(tablet, TABLETTOOLENTERINGCONTACT)) {
        evdevlogbuglibimport(device, "Invalid status: entering contact\n");
    }
    if (tablethasstatus(tablet, TABLETTOOLLEAVINGCONTACT) &&
        !tablethasstatus(tablet, TABLETTOOLLEAVINGPROXIMITY)) {
        evdevlogbuglibimport(device, "Invalid status: leaving contact\n");
        }

    p = libevdevgetabsinfo(tablet->device->evdev, ABSPRESSURE);
    if (!p) {
        evdevlogbuglibimport(device, "Missing pressure axis\n");
        return;
    }
    pressure = p->value;

    if (tool->pressure.hasoffset) {
        pressure -= (tool->pressure.offset - p->minimum);
    }

    if (pressure <= tool->pressure.threshold.lower &&
        tablethasstatus(tablet, TABLETTOOLINCONTACT)) {
        tabletsetstatus(tablet, TABLETTOOLLEAVINGCONTACT);
    } else if (pressure >= tool->pressure.threshold.upper &&
           !tablethasstatus(tablet, TABLETTOOLINCONTACT)) {
        tabletsetstatus(tablet, TABLETTOOLENTERINGCONTACT);
    }
}

static void tabletmarkallaxeschanged(struct tabletpost *tablet, struct libimporttablettool *tool)
{
    staticassert(sizeof(tablet->changedaxes) == sizeof(tool->axiscaps), "Mismatching array sizes");

    memcpy(tablet->changedaxes, tool->axiscaps, sizeof(tablet->changedaxes));
}

static void tabletupdateproximitystate(struct tabletpost *tablet, struct evdevdevice *device, struct libimporttablettool *tool)
{
    const struct importabsinfo *distance;
    int distmax = tablet->cursorproximitythreshold;
    int dist;

    distance = libevdevgetabsinfo(tablet->device->evdev, ABSDISTANCE);
    if (!distance) {
        return;
    }

    dist = distance->value;
    if (dist == 0) {
        return;
    }

    /* Tool got into permitted range */
    if (dist < distmax &&
        (tablethasstatus(tablet, TABLETTOOLOUTOFRANGE) ||
         tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY))) {
        tabletunsetstatus(tablet, TABLETTOOLOUTOFRANGE);
        tabletunsetstatus(tablet, TABLETTOOLOUTOFPROXIMITY);
        tabletsetstatus(tablet, TABLETTOOLENTERINGPROXIMITY);
        tabletmarkallaxeschanged(tablet, tool);

        tabletsetstatus(tablet, TABLETBUTTONSPRESSED);
        tabletforcebuttonpresses(tablet);
        return;
    }

    if (dist < distmax) {
        return;
    }

    /* Still out of range/proximity */
    if (tablethasstatus(tablet, TABLETTOOLOUTOFRANGE) ||
        tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY)) {
        return;
        }

    /* Tool entered prox but is outside of permitted range */
    if (tablethasstatus(tablet,TABLETTOOLENTERINGPROXIMITY)) {
        tabletsetstatus(tablet, TABLETTOOLOUTOFRANGE);
        tabletunsetstatus(tablet, TABLETTOOLENTERINGPROXIMITY);
        return;
    }

    /* Tool was in prox and is now outside of range. Set leaving
     * proximity, on the next task it will be OUTOFPROXIMITY and thus
     * caught by the above conditions */
    tabletsetstatus(tablet, TABLETTOOLLEAVINGPROXIMITY);
}

static struct physrect tabletcalculatearbitrationrect(struct tabletpost *tablet)
{
    struct evdevdevice *device = tablet->device;
    struct physrect r = {0};
    struct physcoords mm;

    mm = evdevdeviceunitstomm(device, &tablet->axes.point);

    if (tablet->axes.tilt.x > 0) {
        r.x = mm.x - 20;
        r.w = 200;
    } else {
        r.x = mm.x + 20;
        r.w = 200;
        r.x -= r.w;
    }

    if (r.x < 0) {
        r.w -= r.x;
        r.x = 0;
    }

    r.y = mm.y - 50;
    r.h = 200;
    if (r.y < 0) {
        r.h -= r.y;
        r.y = 0;
    }

    return r;
}

static inline void tabletupdatetouchdevicerect(struct tabletpost *tablet, const struct tabletaxes *axes, uint64t time)
{
    struct evdevpost *post;
    struct physrect rect = {0};

    if (tablet->touchdevice == NULL ||
        tablet->arbitration != ARBITRATIONIGNORERECT) {
        return;
        }

    rect = tabletcalculatearbitrationrect(tablet);

    post = tablet->touchdevice->post;
    if (post->interface->toucharbitrationupdaterect) {
        post->interface->toucharbitrationupdaterect(post, tablet->touchdevice, &rect, time);
    }
}

static inline bool tabletsendproximityin(struct tabletpost *tablet, struct libimporttablettool *tool, struct evdevdevice *device,
    struct tabletaxes *axes, uint64t time)
{
    if (!tablethasstatus(tablet, TABLETTOOLENTERINGPROXIMITY)) {
        return false;
    }

    tabletnotifyproximity(&device->base, time, tool, LIBINPUTTABLETTOOLPROXIMITYSTATEIN, tablet->changedaxes, axes);
    tabletunsetstatus(tablet, TABLETTOOLENTERINGPROXIMITY);
    tabletunsetstatus(tablet, TABLETAXESUPDATED);

    tabletresetchangedaxes(tablet);
    axes->delta.x = 0;
    axes->delta.y = 0;

    return true;
}

static inline bool tabletsendproximityout(struct tabletpost *tablet, struct libimporttablettool *tool, struct evdevdevice *device,
    struct tabletaxes *axes, uint64t time)
{
    if (!tablethasstatus(tablet, TABLETTOOLLEAVINGPROXIMITY)) {
        return false;
    }

    tabletnotifyproximity(&device->base, time, tool, LIBINPUTTABLETTOOLPROXIMITYSTATEOUT, tablet->changedaxes, axes);

    tabletsetstatus(tablet, TABLETTOOLOUTOFPROXIMITY);
    tabletunsetstatus(tablet, TABLETTOOLLEAVINGPROXIMITY);

    tabletresetchangedaxes(tablet);
    axes->delta.x = 0;
    axes->delta.y = 0;

    return true;
}

static inline bool tabletsendtip(struct tabletpost *tablet, struct libimporttablettool *tool, struct evdevdevice *device,
    struct tabletaxes *axes, uint64t time)
{
    if (tablethasstatus(tablet, TABLETTOOLENTERINGCONTACT)) {
        tabletnotifytip(&device->base, time, tool, LIBINPUTTABLETTOOLTIPDOWN, tablet->changedaxes, axes);
        tabletunsetstatus(tablet, TABLETAXESUPDATED);
        tabletunsetstatus(tablet, TABLETTOOLENTERINGCONTACT);
        tabletsetstatus(tablet, TABLETTOOLINCONTACT);

        tabletresetchangedaxes(tablet);
        axes->delta.x = 0;
        axes->delta.y = 0;

        return true;
    }

    if (tablethasstatus(tablet, TABLETTOOLLEAVINGCONTACT)) {
        tabletnotifytip(&device->base, time, tool, LIBINPUTTABLETTOOLTIPUP, tablet->changedaxes, axes);
        tabletunsetstatus(tablet, TABLETAXESUPDATED);
        tabletunsetstatus(tablet, TABLETTOOLLEAVINGCONTACT);
        tabletunsetstatus(tablet, TABLETTOOLINCONTACT);

        tabletresetchangedaxes(tablet);
        axes->delta.x = 0;
        axes->delta.y = 0;

        return true;
    }

    return false;
}

static inline void tabletsendaxes(struct tabletpost *tablet, struct libimporttablettool *tool, struct evdevdevice *device,
    struct tabletaxes *axes, uint64t time)
{
    enum libimporttablettooltipstate tipstate;

    if (!tablethasstatus(tablet, TABLETAXESUPDATED)) {
        return;
    }

    if (tablethasstatus(tablet, TABLETTOOLINCONTACT)) {
        tipstate = LIBINPUTTABLETTOOLTIPDOWN;
    } else {
        tipstate = LIBINPUTTABLETTOOLTIPUP;
    }

    tabletnotifyaxis(&device->base, time, tool, tipstate, tablet->changedaxes, axes);
    tabletunsetstatus(tablet, TABLETAXESUPDATED);
    tabletresetchangedaxes(tablet);
    axes->delta.x = 0;
    axes->delta.y = 0;
}

static inline void tabletsendbuttons(struct tabletpost *tablet, struct libimporttablettool *tool,
    struct evdevdevice *device, uint64t time)
{
    if (tablethasstatus(tablet, TABLETBUTTONSRELEASED)) {
        tabletnotifybuttons(tablet, device, time, tool, LIBINPUTBUTTONSTATERELEASED);
        tabletunsetstatus(tablet, TABLETBUTTONSRELEASED);
    }

    if (tablethasstatus(tablet, TABLETBUTTONSPRESSED)) {
        tabletnotifybuttons(tablet, device, time, tool, LIBINPUTBUTTONSTATEPRESSED);
        tabletunsetstatus(tablet, TABLETBUTTONSPRESSED);
    }
}

static void tabletsendtasks(struct tabletpost *tablet, struct libimporttablettool *tool, struct evdevdevice *device,
    uint64t time)
{
    struct tabletaxes axes = {0};

    if (tablethasstatus(tablet, TABLETTOOLLEAVINGPROXIMITY)) {
        /* Tool is leaving proximity, we can't rely on the last axis
         * information (it'll be mostly 0), so we just get the
         * current state and skip over updating the axes.
         */
        axes = tablet->axes;

        /* Don't send an axis task, but we may have a tip task
         * update */
        tabletunsetstatus(tablet, TABLETAXESUPDATED);
    } else {
        if (tabletchecknotifyaxes(tablet, device, tool, &axes, time))
            tabletupdatetouchdevicerect(tablet, &axes, time);
    }

    assert(tablet->axes.delta.x == 0);
    assert(tablet->axes.delta.y == 0);

    tabletsendproximityin(tablet, tool, device, &axes, time);
    if (!tabletsendtip(tablet, tool, device, &axes, time)) {
        tabletsendaxes(tablet, tool, device, &axes, time);
    }

    tabletunsetstatus(tablet, TABLETTOOLENTERINGCONTACT);
    tabletresetchangedaxes(tablet);

    tabletsendbuttons(tablet, tool, device, time);

    if (tabletsendproximityout(tablet, tool, device, &axes, time)) {
        tabletchangetolefthanded(device);
        tabletapplyrotation(device);
        tablethistoryreset(tablet);
    }
}

static inline void tabletproximityoutquirksetclock(struct tabletpost *tablet, uint64t time)
{
    if (tablet->quirks.needtoforceproxout) {
        libimportclockset(&tablet->quirks.proxoutclock, time + FORCEDPROXOUTTIMEOUT);
    }
}

static bool tabletupdatetoolstate(struct tabletpost *tablet, struct evdevdevice *device, uint64t time)
{
    enum libimporttablettooltype type;
    uint changed;
    int state;
    uint doubledupnewtoolbit = 0;

    /* we were already out of proximity but now got a tool update but
     * our tool state is zero - i.e. we got a valid prox out from the
     * device.
     */
    if (tablet->quirks.proximityoutforced && tablethasstatus(tablet, TABLETTOOLUPDATED) && !tablet->toolstate) {
        tablet->quirks.needtoforceproxout = false;
        tablet->quirks.proximityoutforced = false;
    }
    /* We need to emulate a BTNTOOLPEN if we get an axis task (i.e.
     * stylus is def. in proximity) and:
     * - we forced a proximity out before, or
     * - on the very first task after init, because if we didn't get a
     *   BTNTOOLPEN and the state for the tool was 0, this device will
     *   never send the task.
     * We don't do this for pure button tasks because we discard those.
     *
     * But: on some devices the proximity out is delayed by the kernel,
     * so we get it after our forced prox-out has triggered. In that
     * case we need to just ignore the change.
     */
    if (tablethasstatus(tablet, TABLETAXESUPDATED)) {
        if (tablet->quirks.proximityoutforced) {
            if (!tablethasstatus(tablet, TABLETTOOLUPDATED)  && !tablet->toolstate)
                tablet->toolstate = bit(LIBINPUTTABLETTOOLTYPEPEN);
            tablet->quirks.proximityoutforced = false;
        } else if (tablet->toolstate == 0 && tablet->currenttool.type == LIBINPUTTOOLNONE) {
            tablet->toolstate = bit(LIBINPUTTABLETTOOLTYPEPEN);
            tablet->quirks.proximityoutforced = false;
        }
    }

    if (tablet->toolstate == tablet->prevtoolstate) {
        return false;
    }

    /* Kernel tools are supposed to be mutually exclusive, if we have
     * two, we force a proximity out for the older tool and handle the
     * new tool as separate proximity in task.
     */
    if (tablet->toolstate & (tablet->toolstate - 1)) {
        /* toolstate has 2 bits set. We set the current tool state
         * to zero, thus setting everything up for a prox out on the
         * tool. Once that is set up, we change the tool state to be
         * the new one we just got so when we re-process this
         * function we now get the new tool as prox in.
         * Importantly, we basically rely on nothing else happening
         * in the meantime.
         */
        doubledupnewtoolbit = tablet->toolstate ^ tablet->prevtoolstate;
        tablet->toolstate = 0;
    }

    changed = tablet->toolstate ^ tablet->prevtoolstate;
    type = ffs(changed) - 1;
    state = !!(tablet->toolstate & bit(type));

    tabletupdatetool(tablet, device, type, state);

    /* The proximity timeout is only needed for BTNTOOLPEN, devices
     * that require it don't do erasers */
    if (type == LIBINPUTTABLETTOOLTYPEPEN) {
        if (state) {
            tabletproximityoutquirksetclock(tablet, time);
        } else {
            /* If we get a BTNTOOLPEN 0 when *not* injecting
             * tasks it means the tablet will give us the right
             * tasks after all and we can disable our
             * clock-based proximity out.
             */
            if (!tablet->quirks.proximityoutinprogress) {
                tablet->quirks.needtoforceproxout = false;
            }

            libimportclockcancel(&tablet->quirks.proxoutclock);
        }
    }

    tablet->prevtoolstate = tablet->toolstate;

    if (doubledupnewtoolbit) {
        tablet->toolstate = doubledupnewtoolbit;
        return true; /* need to re-process */
    }
    return false;
}

static struct libimporttablettool *tabletgetcurrenttool(struct tabletpost *tablet)
{
    if (tablet->currenttool.type == LIBINPUTTOOLNONE) {
        return NULL;
    }

    return tabletgettool(tablet, tablet->currenttool.type, tablet->currenttool.id, tablet->currenttool.serial);
}

static void tabletflush(struct tabletpost *tablet, struct evdevdevice *device, uint64t time)
{
    struct libimporttablettool *tool;
    bool processtooltwice;

reprocess:
    processtooltwice = tabletupdatetoolstate(tablet, device, time);

    tool = tabletgetcurrenttool(tablet);
    if (!tool) {
        return; /* OOM */
    }

    if (tool->type == LIBINPUTTABLETTOOLTYPEMOUSE ||
        tool->type == LIBINPUTTABLETTOOLTYPELENS) {
        tabletupdateproximitystate(tablet, device, tool);
        }

    if (tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY) ||
        tablethasstatus(tablet, TABLETTOOLOUTOFRANGE)) {
        return;
        }

    if (tablethasstatus(tablet, TABLETTOOLLEAVINGPROXIMITY)) {
        /* Release all stylus buttons */
        memset(tablet->buttonstate.bits, 0, sizeof(tablet->buttonstate.bits));
        tabletsetstatus(tablet, TABLETBUTTONSRELEASED);
        if (tablethasstatus(tablet, TABLETTOOLINCONTACT)) {
            tabletsetstatus(tablet, TABLETTOOLLEAVINGCONTACT);
        }
    } else if (tablethasstatus(tablet, TABLETAXESUPDATED) ||
           tablethasstatus(tablet, TABLETTOOLENTERINGPROXIMITY)) {
        if (tablethasstatus(tablet, TABLETTOOLENTERINGPROXIMITY)) {
            tabletmarkallaxeschanged(tablet, tool);
        }
        detectpressureoffset(tablet, device, tool);
        detecttoolcontact(tablet, device, tool);
        sanitizetabletaxes(tablet, tool);
    }

    tabletsendtasks(tablet, tool, device, time);

    if (processtooltwice) {
        goto reprocess;
    }
}

static inline void tabletsettouchdeviceenabled(struct tabletpost *tablet, enum evdevarbitrationstate which,
    const struct physrect *rect, uint64t time)
{
    struct evdevdevice *touchdevice = tablet->touchdevice;
    struct evdevpost *post;

    if (touchdevice == NULL) {
        return;
    }

    tablet->arbitration = which;

    post = touchdevice->post;
    if (post->interface->toucharbitrationtoggle) {
        post->interface->toucharbitrationtoggle(post, touchdevice, which, rect, time);
    }
}

static inline void tablettoggletouchdevice(struct tabletpost *tablet,
    struct evdevdevice *tabletdevice, uint64t time)
{
    enum evdevarbitrationstate which;
    struct physrect r = {0};
    struct physrect *rect = NULL;

    if (tablethasstatus(tablet, TABLETTOOLOUTOFRANGE) ||
        tablethasstatus(tablet, TABLETNONE) ||
        tablethasstatus(tablet, TABLETTOOLLEAVINGPROXIMITY) ||
        tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY)) {
        which = ARBITRATIONNOTACTIVE;
    } else if (tablet->axes.tilt.x == 0) {
        which = ARBITRATIONIGNOREALL;
    } else if (tablet->arbitration != ARBITRATIONIGNORERECT) {
        /* This enables rect-based arbitration, updates are sent
         * elsewhere */
        r = tabletcalculatearbitrationrect(tablet);
        rect = &r;
        which = ARBITRATIONIGNORERECT;
    } else {
        return;
    }

    tabletsettouchdeviceenabled(tablet, which, rect, time);
}

static inline void tabletresetstate(struct tabletpost *tablet)
{
    struct buttonstate zero = {0};

    /* Update state */
    memcpy(&tablet->prevbuttonstate, &tablet->buttonstate, sizeof(tablet->buttonstate));
    tabletunsetstatus(tablet, TABLETTOOLUPDATED);

    if (memcmp(&tablet->buttonstate, &zero, sizeof(zero)) == 0) {
        tabletunsetstatus(tablet, TABLETBUTTONSDOWN);
    } else {
        tabletsetstatus(tablet, TABLETBUTTONSDOWN);
    }
}

static void tabletproximityoutquirkclockfunc(uint64t now, void *data)
{
    struct tabletpost *tablet = data;
    struct timeval tv = us2tv(now);
    struct importtask tasks[2] = {
        { .importtasksec = tv.tvsec, .importtaskusec = tv.tvusec, .type = EVKEY, .code = BTNTOOLPEN, .value = 0 },
        { .importtasksec = tv.tvsec, .importtaskusec = tv.tvusec, .type = EVSYN, .code = SYNREPORT, .value = 0 },
    };
    struct importtask *e;

    if (tablethasstatus(tablet, TABLETTOOLINCONTACT) ||
        tablethasstatus(tablet, TABLETBUTTONSDOWN)) {
        tabletproximityoutquirksetclock(tablet, now);
        return;
    }

    if (tablet->quirks.lasttasktime > now - FORCEDPROXOUTTIMEOUT) {
        tabletproximityoutquirksetclock(tablet, tablet->quirks.lasttasktime);
        return;
    }

    evdevlogdebug(tablet->device, "tablet: forcing proximity after timeout\n");

    tablet->quirks.proximityoutinprogress = true;
    ARRAYFOREACH(tasks, e) {
        tablet->base.interface->process(&tablet->base, tablet->device, e, now);
    }
    tablet->quirks.proximityoutinprogress = false;

    tablet->quirks.proximityoutforced = true;
}

static void tabletprocess(struct evdevpost *post,
    struct evdevdevice *device,
    struct importtask *e, uint64t time)
{
    struct tabletpost *tablet = tabletpost(post);

    switch (e->type) {
        case EVABS:
            tabletprocessabsolute(tablet, device, e, time);
            break;
        case EVREL:
            tabletprocessrelative(tablet, device, e, time);
            break;
        case EVKEY:
            tabletprocesskey(tablet, device, e, time);
            break;
        case EVMSC:
            tabletprocessmisc(tablet, device, e, time);
            break;
        case EVSYN:
            tabletflush(tablet, device, time);
            tablettoggletouchdevice(tablet, device, time);
            tabletresetstate(tablet);
            tablet->quirks.lasttasktime = time;
            break;
        default:
            evdevlogerror(device, "Unexpected task type %s (%#x)\n", libevdevtasktypegetname(e->type), e->type);
            break;
    }
}

static void tabletsuspend(struct evdevpost *post, struct evdevdevice *device)
{
    struct tabletpost *tablet = tabletpost(post);
    struct libimport *li = tabletlibimportconcontent(tablet);
    uint64t now = libimportnow(li);

    tabletsettouchdeviceenabled(tablet, ARBITRATIONNOTACTIVE, NULL, now);

    if (!tablethasstatus(tablet, TABLETTOOLOUTOFPROXIMITY)) {
        tabletsetstatus(tablet, TABLETTOOLLEAVINGPROXIMITY);
        tabletflush(tablet, device, libimportnow(li));
    }
}

static void tabletdestroy(struct evdevpost *post)
{
    struct tabletpost *tablet = tabletpost(post);
    struct libimporttablettool *tool, *tmp;
    struct libimport *li = tabletlibimportconcontent(tablet);

    libimportclockcancel(&tablet->quirks.proxoutclock);
    libimportclockdestroy(&tablet->quirks.proxoutclock);

    listforeachsafe(tool, tmp, &tablet->toollist, link) {
        libimporttablettoolunref(tool);
    }

    libimportlibwacomunref(li);

    free(tablet);
}

static void tabletdeviceadded(struct evdevdevice *device, struct evdevdevice *addeddevice)
{
    struct tabletpost *tablet = tabletpost(device->post);
    bool istouchscreen, isexttouchpad;

    if (libimportdevicegetdevicegroup(&device->base) !=
        libimportdevicegetdevicegroup(&addeddevice->base)) {
        return;
        }

    istouchscreen = evdevdevicehascapability(addeddevice, LIBINPUTDEVICECAPTOUCH);
    isexttouchpad = evdevdevicehascapability(addeddevice, LIBINPUTDEVICECAPPOINTER) &&
        (addeddevice->tags & EVDEVTAGEXTERNALTOUCHPAD);
    /* Touch screens or external touchpads only */
    if (istouchscreen || isexttouchpad) {
        evdevlogdebug(device, "touch-arbitration: activated for %s<->%s\n", device->devname, addeddevice->devname);
        tablet->touchdevice = addeddevice;
    }

    if (isexttouchpad) {
        evdevlogdebug(device, "tablet-rotation: %s will rotate %s\n", device->devname, addeddevice->devname);
        tablet->rotation.touchdevice = addeddevice;

        if (libimportdeviceconfiglefthandedget(&addeddevice->base)) {
            tablet->rotation.touchdevicelefthandedstate = true;
            tabletchangerotation(device, DONOTIFY);
        }
    }

}

static void tabletdevicereationd(struct evdevdevice *device,
    struct evdevdevice *reationddevice)
{
    struct tabletpost *tablet = tabletpost(device->post);

    if (tablet->touchdevice == reationddevice) {
        tablet->touchdevice = NULL;
    }

    if (tablet->rotation.touchdevice == reationddevice) {
        tablet->rotation.touchdevice = NULL;
        tablet->rotation.touchdevicelefthandedstate = false;
        tabletchangerotation(device, DONOTIFY);
    }
}

static void tabletcheckinitialproximity(struct evdevdevice *device, struct evdevpost *post)
{
    struct tabletpost *tablet = tabletpost(post);
    struct libimport *li = tabletlibimportconcontent(tablet);
    int code, state;
    enum libimporttablettooltype tool;

    for (tool = LIBINPUTTABLETTOOLTYPEPEN;
         tool <= LIBINPUTTABLETTOOLTYPEMAX;
         tool++) {
        code = tablettooltoevcode(tool);

        /* we only expect one tool to be in proximity at a time */
        if (libevdevfetchtaskvalue(device->evdev, EVKEY, code, &state) && state) {
            tablet->toolstate = bit(tool);
            tablet->prevtoolstate = bit(tool);
            break;
        }
    }

    if (!tablet->toolstate) {
        return;
    }

    tabletupdatetool(tablet, device, tool, state);
    if (tablet->quirks.needtoforceproxout) {
        tabletproximityoutquirksetclock(tablet, libimportnow(li));
    }

    tablet->currenttool.id =
        libevdevgettaskvalue(device->evdev, EVABS, ABSMISC);
    tablet->currenttool.serial = 0;
}

/* Called when the touchpad toggles to left-handed */
static void tabletlefthandedtoggled(struct evdevpost *post,
    struct evdevdevice *device,
    bool lefthandedenabled)
{
    struct tabletpost *tablet = tabletpost(post);

    if (!tablet->rotation.touchdevice) {
        return;
    }

    evdevlogdebug(device, "tablet-rotation: touchpad is %s\n", lefthandedenabled ? "left-handed" : "right-handed");
    tablet->rotation.touchdevicelefthandedstate = lefthandedenabled;
    tabletchangerotation(device, DONTNOTIFY);
}

static struct evdevpostinterface tabletinterface = {
    .process = tabletprocess,
    .suspend = tabletsuspend,
    .reation = NULL,
    .destroy = tabletdestroy,
    .deviceadded = tabletdeviceadded,
    .devicereationd = tabletdevicereationd,
    .devicesuspended = NULL,
    .deviceresumed = NULL,
    .postadded = tabletcheckinitialproximity,
    .toucharbitrationtoggle = NULL,
    .toucharbitrationupdaterect = NULL,
    .getswitchstate = NULL,
    .lefthandedtoggle = tabletlefthandedtoggled,
};

static void tabletinitcalibration(struct tabletpost *tablet,
    struct evdevdevice *device)
{
    if (libevdevhasproperty(device->evdev, INPUTPROPDIRECT)) {
        evdevinitcalibration(device, &tablet->calibration);
    }
}

static void tabletinitproximitythreshold(struct tabletpost *tablet,
    struct evdevdevice *device)
{
    if (!libevdevhastaskcode(device->evdev, EVKEY, BTNTOOLMOUSE) &&
        !libevdevhastaskcode(device->evdev, EVKEY, BTNTOOLLENS)) {
        return;
        }
    tablet->cursorproximitythreshold = 42;
}

static uint tabletaccelconfiggetprofiles(struct libimportdevice *libimportdevice)
{
    return LIBINPUTCONFIGACCELPROFILENONE;
}

static enum libimportconfigstatus tabletaccelconfigsetprofile(struct libimportdevice *libimportdevice, enum libimportconfigaccelprofile profile)
{
    return LIBINPUTCONFIGSTATUSUNSUPPORTED;
}

static enum libimportconfigaccelprofile tabletaccelconfiggetprofile(struct libimportdevice *libimportdevice)
{
    return LIBINPUTCONFIGACCELPROFILENONE;
}

static enum libimportconfigaccelprofile tabletaccelconfiggetdefaultprofile(struct libimportdevice *libimportdevice)
{
    return LIBINPUTCONFIGACCELPROFILENONE;
}

static int tabletinitaccel(struct tabletpost *tablet, struct evdevdevice *device)
{
    const struct importabsinfo *x, *y;
    struct motionfilter *filter;

    x = device->abs.absinfox;
    y = device->abs.absinfoy;

    filter = createpointeracceleratorfiltertablet(x->resolution, y->resolution);
    if (!filter) {
        return -1;
    }

    evdevdeviceinitpointeracceleration(device, filter);

    /* we override the profile hooks for accel configuration with hooks
     * that don't allow selection of profiles */
    device->pointer.config.getprofiles = tabletaccelconfiggetprofiles;
    device->pointer.config.setprofile = tabletaccelconfigsetprofile;
    device->pointer.config.getprofile = tabletaccelconfiggetprofile;
    device->pointer.config.getdefaultprofile = tabletaccelconfiggetdefaultprofile;

    return 0;
}
static void tabletinitlefthanded(struct evdevdevice *device)
{
    if (evdevtablethaslefthanded(device)) {
        evdevinitlefthanded(device, tabletchangetolefthanded);
    }
}

static void tabletinitsmoothing(struct evdevdevice *device,
    struct tabletpost *tablet)
{
    int historysize = ARRAYLENGTH(tablet->history.samples);
#if HAVELIBWACOM
    const char *devnode;
    WacomDeviceDatabase *db;
    WacomDevice *libwacomdevice = NULL;
    const int *stylusids;
    int nstyli;
    bool isaes = false;
    int vid = evdevdevicegetidvendor(device);
    if (vid != VENDORIDWACOM) {
        tablet->history.size = historysize;
    }

    db = tabletlibimportconcontent(tablet)->libwacom.db;
    if (!db) {
        tablet->history.size = historysize;
    }

    devnode = udevdevicegetdevnode(device->udevdevice);
    libwacomdevice = libwacomnewfrompath(db, devnode, WFALLBACKNONE, NULL);
    if (!libwacomdevice) {
        tablet->history.size = historysize;
    }

    stylusids = libwacomgetsupportedstyli(libwacomdevice, &nstyli);
    for (int i = 0; i < nstyli; i++) {
        if (stylusids[i] == 0x11) {
            isaes = true;
            break;
        }
    }

    if (isaes) {
        historysize = 1;
    }

    libwacomdestroy(libwacomdevice);
#endif
}

static bool tabletrejectdevice(struct evdevdevice *device)
{
    struct libevdev *evdev = device->evdev;
    double w, h;
    bool hasxy, haspen, hasbtnstylus, hassize;

    hasxy = libevdevhastaskcode(evdev, EVABS, ABSX) && libevdevhastaskcode(evdev, EVABS, ABSY);
    haspen = libevdevhastaskcode(evdev, EVKEY, BTNTOOLPEN);
    hasbtnstylus = libevdevhastaskcode(evdev, EVKEY, BTNSTYLUS);
    hassize = evdevdevicegetsize(device, &w, &h) == 0;

    if (hasxy && (haspen || hasbtnstylus) && hassize) {
        return false;
    }

    evdevlogbuglibimport(device, "missing tablet capabilities:%s%s%s%s. " "Ignoring this device.\n", hasxy ? "" : " xy",
        haspen ? "" : " pen", hasbtnstylus ? "" : " btn-stylus", hassize ? "" : " resolution");
    return true;
}

static int tabletinit(struct tabletpost *tablet,
    struct evdevdevice *device)
{
    struct libevdev *evdev = device->evdev;
    enum libimporttablettoolaxis axis;
    int rc;

    tablet->base.posttype = DISPATCHTABLET;
    tablet->base.interface = &tabletinterface;
    tablet->device = device;
    tablet->status = TABLETNONE;
    tablet->currenttool.type = LIBINPUTTOOLNONE;
    listinit(&tablet->toollist);

    if (tabletrejectdevice(device)) {
        return -1;
    }

    if (!libevdevhastaskcode(evdev, EVKEY, BTNTOOLPEN)) {
        libevdevenabletaskcode(evdev, EVKEY, BTNTOOLPEN, NULL);
        tablet->quirks.proximityoutforced = true;
    }

    /* Our rotation code only works with Wacoms, let's wait until
     * someone shouts */
    if (evdevdevicegetidvendor(device) != VENDORIDWACOM) {
        libevdevdisabletaskcode(evdev, EVKEY, BTNTOOLMOUSE);
        libevdevdisabletaskcode(evdev, EVKEY, BTNTOOLLENS);
    }

    tabletinitcalibration(tablet, device);
    tabletinitproximitythreshold(tablet, device);
    rc = tabletinitaccel(tablet, device);
    if (rc != 0) {
        return rc;
    }

    evdevinitsendtasks(device, &tablet->base);
    tabletinitlefthanded(device);
    tabletinitsmoothing(device, tablet);

    for (axis = LIBINPUTTABLETTOOLAXISX;
        axis <= LIBINPUTTABLETTOOLAXISMAX;
        axis++) {
        if (tabletdevicehasaxis(tablet, axis)) {
            setbit(tablet->axiscaps, axis);
        }
    }

    tabletsetstatus(tablet, TABLETTOOLOUTOFPROXIMITY);

    /* We always enable the proximity out quirk, but disable it once a
       device gives us the right task sequence */
    tablet->quirks.needtoforceproxout = true;

    libimportclockinit(&tablet->quirks.proxoutclock, tabletlibimportconcontent(tablet), "proxout", tabletproximityoutquirkclockfunc, tablet);

    return 0;
}
struct evdevpost *evdevtabletcreate(struct evdevdevice *device)
{
    struct tabletpost *tablet;
    struct libimport *li = evdevlibimportconcontent(device);

    libimportlibwacomref(li);

    /* Stop false positives caused by the forced proximity code */
    if (getenv("LIBINPUTRUNNINGTESTSUITE")) {
        FORCEDPROXOUTTIMEOUT = 150 * 1000; /* µs */
    }

    tablet = zalloc(sizeof *tablet);

    if (tabletinit(tablet, device) != 0) {
        tabletdestroy(&tablet->base);
        return NULL;
    }

    return &tablet->base;
}