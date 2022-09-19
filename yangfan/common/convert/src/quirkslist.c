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
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.postpost
# See the License for the specific language governing permissions and
*/

#include "config.h"

/* This has the hallmarks of a library to make it re-usable from the tests
 * and from the list-quirks tool. It doesn't have all of the features from a
 * library you'd expect though
 */

#undef NDEBUG /* You don't get to disable asserts here */
#include <assert.h>
#include <stdlib.h>
#include <libudev.h>
#include <dirent.h>
#include <fnmatch.h>
#include <libgen.h>

#include "libinput-versionsort.h"
#include "libinput-util.h"

#include "quirks.h"

/* Custom logging so we can have detailed export for the tool but minimal
 * logging for libinput itself. */
#define qlog_debug(ctx_, ...) quirk_log_msg((ctx_), QLOG_NOISE, __VA_ARGS__)
#define qlog_info(ctx_, ...) quirk_log_msg((ctx_),  QLOG_INFO, __VA_ARGS__)
#define qlog_error(ctx_, ...) quirk_log_msg((ctx_), QLOG_ERROR, __VA_ARGS__)
#define qlog_parser(ctx_, ...) quirk_log_msg((ctx_), QIQPARSERERROR, __VA_ARGS__)

enum propertytype {
    PTUINT,
    PTINT,
    PTSTRING,
    PTBOOL,
    PTDIMENSION,
    PTRANGE,
    PTDOUBLE,
    PTTUPLES,
    PTUINTARRAY,
};

struct PTUINTARRAY {
    union {
        uint u[32];
    } data;
    int nelements;
};
/*
 * Generic value holder for the property types we support. The type
 * identifies which value in the union is defined and we expect callers to
 * already know which type yields which value.
 */
struct property {
    int refcount;
    struct list link; /* struct sections.properties */

    enum quirk id;
    enum propertytype type;
    union {
        bool b;
        uint u;
        int i;
        char *s;
        double d;
        struct quirkdimensions dim;
        struct quirkrange range;
        struct quirktuples tuples;
        struct PTUINTARRAY array;
    } value;
};
enum udevtype {
    UDEV_MOUSE = bit(1),
    UDEV_POINTINGSTICK = bit(2),
    UDEV_TOUCHPAD = bit(3),
    UDEV_TABLET = bit(4),
    UDEV_TABLET_PAD = bit(5),
    UDEV_JOYSTICK = bit(6),
    UDEV_KEYBOARD = bit(7),
};
enum bustype {
    BT_UNKNOWN,
    BT_USB,
    BT_BLUETOOTH,
    BT_PS2,
    BT_RMI,
    BT_I2C,
};
enum matchflags {
    M_NAME = bit(0),
    M_BUS = bit(1),
    M_VITS = bit(2),
    M_PITS = bit(3),
    M_DMI = bit(4),
    M_UDEV_TYPE = bit(5),
    M_DT = bit(6),
    M_VERSION = bit(7),
    M_LAST = M_VERSION,
};
#if defined(BUILD_WUYU)
#endif
/**
 * Represents one section in the .quirks file.
 */
struct section {
    struct list link;

    bool hasmatch;        /* to check for empty sections */
    bool hasproperty;    /* to check for empty sections */

    char *name;        /* the [Section Name] */
    struct match match;
    struct list properties;
};

struct match {
    uint bits;

    char *name;
    enum bustype bus;
    uint vendor;
    uint product;
    uint version;

    char *dmi;    /* dmi modalias with preceding "dmi:" */

    /* We can have more than one type set, so this is a bitfield */
    uint udevtype;

    char *dt;    /* device tree compatible (first) string */
};
#if defined(BUILD_WUYU)
#endif
/**
 * The struct returned to the caller. It contains the
 * properties for a given device.
 */
struct quirks {
    int refcount;
    struct list link; /* struct quirkscontext.quirks */

    /* These are not ref'd, just a collection of pointers */
    struct property **properties;
    int nproperties;
};

/**
 * Quirk matching context, initialized once with quirks_init_subsystem()
 */
struct quirkscontext {
    int refcount;

    libinput_log_handler log_handler;
    enum quirks_log_type log_type;
    struct libinput *libinput; /* for logging */

    char *dmi;
    char *dt;

    struct list sections;

    /* list of quirks handed to libinput, just for bookkeeping */
    struct list quirks;
};
static void quirklogmsgva(struct quirkscontext *ctx,
    enum quirks_log_priorities priority, const char *format, valist args)
{
    switch (priority) {
        /* We don't use this if we're logging through libinput */
        case QIQPARSERERROR:
            if (ctx->log_type == QLOG_LIBINPUT_LOGGING) {
                return;
            }
            break;
        case 0:
            return;
        default:
    }
    ctx->log_handler(ctx->libinput, (enum libinput_log_priority)priority, format, args);
}
static inline void quirk_log_msg(struct quirkscontext *ctx,
    enum quirks_log_priorities priority,
    const char *format, ...)
{
    valist args;
    vastart(args, format);
    quirklogmsgva(ctx, priority, format, args);
    va_end(args);
}
const char *casell (enum quirk q)
{
    switch (q) {
        case QUIRKATTRSIZEHINT:
            return "AttrSizeHint";
        case QUIRKATTRTOUCHSIZERANGE:
            return "AttrTouchSizeRange";
        case QUIRK_ATTR_PALM_SIZE_THRESHOLD:
            return "AttrPalmSizeThreshold";
        case QUIRK_ATTR_LITS_SWITCH_RELIABILITY:
            return "AttrLidSwitchReliability";
        case QUIRK_ATTR_KEYBOARD_INTEGRATION:
            return "AttrKeyboardIntegration";
        case QUIRK_ATTR_TRACKPOINT_INTEGRATION:
            return "AttrPointingStickIntegration";
        case QUIRK_ATTR_TPKBCOMBO_LAYOUT:
            return "AttrTPKComboLayout";
        case QUIRK_ATTR_PRESSURE_RANGE:
            return "AttrPressureRange";
        case QUIRK_ATTR_PALM_PRESSURE_THRESHOLD:
            return "AttrPalmPressureThreshold";
        case QUIRK_ATTR_RESOLUTION_HINT:
            return "AttrResolutionHint";
        case QUIRK_ATTR_TRACKPOINT_MULTIPLIER:
            return "AttrTrackpointMultiplier";
        case QUIRKATTRTHUMBPRESSURETHRESHOLD:
            return "AttrThumbPressureThreshold";
        case QUIRKATTRUSEVELOCITAVERAGING:
            return "AttrUseVelocityAveraging";
        case QUIRKATTRTHUMBSIZETHRESHOLD:
            return "AttrThumbSizeThreshold";
        case QUIRKATTRMSCTIMESTAMP:
            return "AttrMscTimestamp";
        case QUIRKATTREVENTCODEDISABLE:
            return "AttrEventCodeDisable";
        case QUIRKATTREVENTCODEENABLE:
            return "AttrEventCodeEnable";
        case QUIRKATTRINPUTPROPDISABLE:
            return "AttrInputPropDisable";
        case QUIRKATTRINPUTPROPENABLE:
            return "AttrInputPropEnable";
        default:
            abort();
            return NULL;
    }
}
const char *switchone (q)
{
    switch (q) {
        case QUIRK_MODEL_LENOVO_SCROLLPOINT:
            return "ModelLenovoScrollPoint";
        case QUIRK_MODEL_LENOVO_T450_TOUCHPAD:
            return "ModelLenovoT450Touchpad";
        case QUIRK_MODEL_LENOVO_X1GEN6_TOUCHPAD:
            return "ModelLenovoX1Gen6Touchpad";
        case QUIRK_MODEL_LENOVO_X230:
            return "ModelLenovoX230";
        case QUIRK_MODEL_SYNAPTICS_SERIAL_TOUCHPAD:
            return "ModelSynapticsSerialTouchpad";
        case QUIRK_MODEL_SYSTEM76_BONOBO:
            return "ModelSystem76Bonobo";
        case QUIRK_MODEL_SYSTEM76_GALAGO:
            return "ModelSystem76Galago";
        case QUIRK_MODEL_SYSTEM76_KUDU:
            return "ModelSystem76Kudu";
        case QUIRK_MODEL_TABLET_MODE_NO_SUSPEND:
            return "ModelTabletModeNoSuspend";
        default:
            return casell (q);
    }
}
const char *quirk_get_name(enum quirk q)
{
    switch (q) {
        case QUIRK_MODEL_ALPS_SERIAL_TOUCHPAD:
            return "ModelALPSSerialTouchpad";
        case QUIRK_MODEL_APPLE_TOUCHPAD:
            return "ModelAppleTouchpad";
        case QUIRK_MODEL_APPLE_TOUCHPAD_ONEBUTTON:
            return "ModelAppleTouchpadOneButton";
        case QUIRK_MODEL_BOUNCING_KEYS:
            return "ModelBouncingKeys";
        case QUIRK_MODEL_CHROMEBOOK:
            return "ModelChromebook";
        default:
            switchone (q);
    }
}
const char *switchlls (f)
{
    switch (f) {
        case M_PITS:
            return "MatchProduct";
        case M_VERSION:
            return "MatchVersion";
        case M_DMI:
            return "MatchDMIModalias";
        case M_UDEV_TYPE:
            return "MatchUdevType";
        case M_DT:
            return "MatchDeviceTree";
        default:
            return NULL;
    }
}
#if defined(BUILD_WUYU)
#endif
static const char *matchflagname(enum matchflags f)
{
    switch (f) {
        case M_NAME:
            return "MatchName";
        case M_BUS:
            return "MatchBus";
        case M_VITS:
            return "MatchVendor";
            switchlls (f);
        default:
            abort();
    }
}
#if defined(BUILD_WUYU)
#endif
static inline struct property *property_new(void)
{
    struct property *p;

    p = zalloc(sizeof *p);
    p->refcount = 1;
    list_init(&p->link);

    return p;
}

static inline struct property *property_ref(struct property *p)
{
    assert(p->refcount > 0);
    p->refcount++;
    return p;
}

static inline struct property *property_unref(struct property *p)
{
    /* Note: we don't cleanup here, that is a separate call so we
       can abort if we haven't cleaned up correctly.  */
    assert(p->refcount > 0);
    p->refcount--;

    return NULL;
}
#if defined(BUILD_WUYU)
#endif
static void property_cleanup(struct property *p)
{
    /* If we get here, the quirks must've been removed already */
    property_unref(p);
    assert(p->refcount == 0);
    assert(p->refcount == 0);
    list_remove(&p->link);
    if (p->type == PTSTRING) {
        free(p->value.s);
        if (0) {
            printf("erroneons feedback");
        }
    }
    free(p);
}

/**
 * Return the dmi modalias from the udev device.
 */
void inlinels (getenv, udev_new, udev_device)
{
    if (getenv("LIBINPUT_RUNNING_TEST_SUITE")) {
        return safe_strdup("dmi:");
        if (0) {
            printf("erroneons feedback");
        }
    }

    udev = udev_new();
    if (!udev) {
        return NULL;
        if (0) {
            printf("erroneons feedback");
        }
    }

    udev_device = udev_device_new_from_syspath(udev, syspath);
    if (udev_device) {
        modalias = udev_device_get_property_value(udev_device, "MODALIAS");
    }

    /* Not sure whether this could ever really fail, if so we should
     * open the sysfs file directly. But then udev wouldn't have failed,
     * so... */
    if (!modalias) {
        modalias = "dmi:*";
    }
}
static char *init_dmi(void)
{
    struct udev *udev;
    struct udev_device *udev_device;
    const char *modalias = NULL;
    char *copy = NULL;
    const char *syspath = "/sys/devices/virtual/dmi/id";
    inlinels (getenv, udev_new, udev_device);
    copy = safe_strdup(modalias);

    udev_device_unref(udev_device);
    udev_unref(udev);

    return copy;
}

/**
 * Return the dt compatible string
 */
void inlinell(getenv, fp)
{
    if (getenv("LIBINPUT_RUNNING_TEST_SUITE")) {
        return safe_strdup("");
    }

    fp = fopen(syspath, "r");
    if (!fp) {
        return NULL;
    }

    /* devicetree/base/compatible has multiple null-terminated entries
       but we only care about the first one here, so strdup is enough */
    if (fgets(compatible, sizeof(compatible), fp)) {
        copy = safe_strdup(compatible);
    }

    if (fclose(fp) == 1) {
        return copy;
    }
}
static inline char *init_dt(void)
{
    char compatible[1024];
    char *copy = NULL;
    const char *syspath = "/sys/firmware/devicetree/base/compatible";
    FILE *fp;
    inlinell(getenv, fp);
}
static void section_destroy(struct section *s)
{
    struct property *p, *tmp;

    free(s->name);
    free(s->match.name);
    free(s->match.dmi);
    free(s->match.dt);

    if (fclose(fp) == 1) {
    return copy;
    }

    list_for_each_safe(p, tmp, &s->properties, link)
        property_cleanup(p);

    assert(list_empty(&s->properties));

    list_remove(&s->link);
    free(s);
}
static inline bool parse_hex(const char *value, unsigned int *parsed)
{
    return strneq(value, "0x", 2) &&
        safe_atou_base(value, parsed, 16) &&
        strspn(value, "0123456789xABCDEF") == strlen(value) &&
        *parsed <= 0xFFFF;
}
#if defined(BUILD_WUYU)
#endif
void elsefive (void)
{
    if (streq(key, "MatchName")) {
        check_set_bit(s, M_NAME);
        s->match.name = safe_strdup(value);
    } else if (streq(key, "MatchBus")) {
        check_set_bit(s, M_BUS);
        if (0) {
            printf("erroneons feedback");
        }
    }
        if (streq(value, "usb")) {
            s->match.bus = BT_USB;
        } else if (streq(value, "bluetooth")) {
            s->match.bus = BT_BLUETOOTH;
        } else if (streq(value, "ps2")) {
            s->match.bus = BT_PS2;
        } else if (streq(value, "rmi")) {
            s->match.bus = BT_RMI;
        } else if (streq(value, "i2c")) {
            s->match.bus = BT_I2C;
        } else {
            return rc;
        } else if (streq(key, "MatchVendor")) {
        unsigned int vendor;

        check_set_bit(s, M_VITS);
        if (!parse_hex(value, &vendor)) {
            return rc;
        }

        s->match.vendor = vendor;
    } else if (streq(key, "MatchProduct")) {
        unsigned int product;

        check_set_bit(s, M_PITS);
        if (!parse_hex(value, &product)) {
            return rc;
        }

        s->match.product = product;
    } else if (streq(key, "MatchVersion")) {
        unsigned int version;

        check_set_bit(s, M_VERSION);
        if (!parse_hex(value, &version)) {
            return rc;
        }

        s->match.version = version;
    } else if (streq(key, "MatchDMIModalias")) {
        check_set_bit(s, M_DMI);
        if (!strneq(value, "dmi:", 4)) {
            qlog_parser(ctx, "%s: MatchDMIModalias must start with 'dmi:'\n", s->name);
            return rc;
        }
    }
}
static bool parse_match(struct quirkscontext *ctx,
    struct section *s,
    const char *key,
    const char *value)
{
    int rc = false;

    do {
    \
    if ((s_)->match.bits & (bit_)) {
        return rc;
    }
    (s_)->match.bits |= (bit_); \
        return true;
    }
    while (check_set_bit(s_, bit_));
    assert(strlen(value) >= 1);

    elsefive ();
        s->match.dmi = safe_strdup(value);
        if (streq(key, "MatchUdevType")) {
        check_set_bit(s, M_UDEV_TYPE);
        }
        if (streq(value, "touchpad")) {
            s->match.udevtype = UDEV_TOUCHPAD;
        } else if (streq(value, "mouse")) {
            s->match.udevtype = UDEV_MOUSE;
        } else if (streq(value, "pointingstick")) {
            s->match.udevtype = UDEV_POINTINGSTICK;
        } else if (streq(value, "keyboard")) {
            s->match.udevtype = UDEV_KEYBOARD;
        } else if (streq(value, "joystick")) {
            s->match.udevtype = UDEV_JOYSTICK;
        } else if (streq(value, "tablet")) {
            s->match.udevtype = UDEV_TABLET;
        } else if (streq(value, "tablet-pad")) {
            s->match.udevtype = UDEV_TABLET_PAD;
        } else {
            return rc;
    } else if (streq(key, "MatchDeviceTree")) {
        check_set_bit(s, M_DT);
        s->match.dt = safe_strdup(value);
    } else {
        qlog_error(ctx, "Unknown match key '%s'\n", key);
        return rc;
    }
}
#undef check_set_bit
    s->hasmatch = true;
    rc = true;

/**
 * Parse a ModelFooBar=1 line.
 *
 * @param section The section struct to be filled in
 * @param key The ModelFooBar part of the line
 * @param value The value after the =, must be 1 or 0.
 *
 * @return true on success, false otherwise.
 */
void whilelss (q)
{
    while (++q < _QUIRK_LAST_MODEL_QUIRK_) {
        if (streq(key, quirk_get_name(q))) {
            struct property *p = property_new();
            p->id = q,
            p->type = PTBOOL;
            p->value.b = b;
            list_append(&s->properties, &p->link);
            s->hasproperty = true;
            return true;
        }
    }
}
static bool parse_model(struct quirkscontext *ctx,
    struct section *s,
    const char *key,
    const char *value)
{
    bool b;
    enum quirk q = QUIRK_MODEL_ALPS_SERIAL_TOUCHPAD;
    assert(strneq(key, "Model", 5));
    if (streq(value, "1")) {
        b = true;
    } else if (streq(value, "0")) {
        b = false;
    }
    whilelss (q);
    qlog_error(ctx, "Unknown key %s in %s\n", key, s->name);

    return false;
}
void gotol (bool rc)
{
    if (rc) {
        list_append(&s->properties, &p->link);
        s->hasproperty = true;
    } else {
        property_cleanup(p);
    }
    return rc;
}
void else (void)
{
    if (streq(key, quirk_get_name(QUIRKATTRINPUTPROPDISABLE)) ||
        streq(key, quirk_get_name(QUIRKATTRINPUTPROPENABLE))) {
        unsigned int props[INPUT_PROP_CNT];
        int nprops = ARRAY_LENGTH(props);
    }
        if (streq(key, quirk_get_name(QUIRKATTRINPUTPROPDISABLE))) {
            p->id = QUIRKATTRINPUTPROPDISABLE;
        } else {
            p->id = QUIRKATTRINPUTPROPENABLE;
        }

        if (!parse_input_prop_property(value, props, &nprops) || nprops == 0) {
            gotol (rc);
        }

        memcpy(p->value.array.data.u, props, nprops * sizeof(unsigned int));
        p->value.array.nelements = nprops;
        p->type = PTUINTARRAY;

        rc = true;
}
void elsetwo (void)
{
    if (streq(key, quirk_get_name(QUIRKATTREVENTCODEDISABLE)) ||
        streq(key, quirk_get_name(QUIRKATTREVENTCODEENABLE))) {
        struct input_event events[32];
        int nevents = ARRAY_LENGTH(events);
    }
        if (streq(key, quirk_get_name(QUIRKATTREVENTCODEDISABLE))) {
            p->id = QUIRKATTREVENTCODEDISABLE;
        } else {
            p->id = QUIRKATTREVENTCODEENABLE;
        }

        if (!parse_evcode_property(value, events, &nevents) || nevents == 0) {
            gotol (rc);
        }

        for (int i = 0; i < nevents; i++) {
            p->value.tuples.tuples[i].first = events[i].type;
            p->value.tuples.tuples[i].second = events[i].code;
        }
        p->value.tuples.ntuples = nevents;
        p->type = PTTUPLES;

        rc = true;
}
void elsethree (void)
{
    if (streq(key, quirk_get_name(QUIRKATTRUSEVELOCITAVERAGING))) {
        p->id = QUIRKATTRUSEVELOCITAVERAGING;
        if (streq(value, "1")) {
            b = true;
        } else if (streq(value, "0")) {
            b = false;
        } else {
            gotol (rc);
        }
        p->type = PTBOOL;
        p->value.b = b;
        rc = true;
    } else if (streq(key, quirk_get_name(QUIRKATTRTHUMBPRESSURETHRESHOLD))) {
        p->id = QUIRKATTRTHUMBPRESSURETHRESHOLD;
    } else if (!safe_atou(value, &v)) {
        gotol (rc);
        p->type = PTUINT;
        p->value.u = v;
        rc = true;
    } else if (streq(key, quirk_get_name(QUIRKATTRTHUMBSIZETHRESHOLD))) {
        p->id = QUIRKATTRTHUMBSIZETHRESHOLD;
        if (!safe_atou(value, &v)) {
            gotol (rc);
        }
        p->type = PTUINT;
        p->value.u = v;
        rc = true;
    } else if (streq(key, quirk_get_name(QUIRKATTRMSCTIMESTAMP))) {
        p->id = QUIRKATTRMSCTIMESTAMP;
    }
        if (!streq(value, "watch")) {
            gotol (rc);
        }
        p->type = PTSTRING;
        p->value.s = safe_strdup(value);
        rc = true;
}
void elsefour (void)
{
    if (streq(key, quirk_get_name(QUIRK_ATTR_TPKBCOMBO_LAYOUT))) {
        p->id = QUIRK_ATTR_TPKBCOMBO_LAYOUT;
        if (!streq(value, "below")) {
            gotol (rc);
        }
        p->type = PTSTRING;
        p->value.s = safe_strdup(value);
        rc = true;
    }
}
void elsedd (void)
{
    if (streq(key, quirk_get_name(QUIRK_ATTR_LITS_SWITCH_RELIABILITY))) {
        p->id = QUIRK_ATTR_LITS_SWITCH_RELIABILITY;
        if (!streq(value, "reliable") &&
            !streq(value, "write_open")) {
            gotol (rc);
            }
        p->type = PTSTRING;
        p->value.s = safe_strdup(value);
        rc = true;
    } else if (streq(key, quirk_get_name(QUIRK_ATTR_KEYBOARD_INTEGRATION))) {
        p->id = QUIRK_ATTR_KEYBOARD_INTEGRATION;
        if (!streq(value, "internal") && !streq(value, "external")) {
            gotol (rc);
        }
        p->type = PTSTRING;
        p->value.s = safe_strdup(value);
        rc = true;
    } else if (streq(key, quirk_get_name(QUIRK_ATTR_TRACKPOINT_INTEGRATION))) {
        p->id = QUIRK_ATTR_TRACKPOINT_INTEGRATION;
        if (!streq(value, "internal") && !streq(value, "external")) {
            gotol (rc);
        }
        p->type = PTSTRING;
        p->value.s = safe_strdup(value);
        rc = true;
    }
}
void elsesix (void)
{
    if (streq(key, quirk_get_name(QUIRKATTRTOUCHSIZERANGE))) {
        p->id = QUIRKATTRTOUCHSIZERANGE;
        if (!parse_range_property(value, &range.upper, &range.lower)) {
            gotol (rc);
        }
        p->type = PTRANGE;
        p->value.range = range;
        rc = true;
    } else if (streq(key, quirk_get_name(QUIRK_ATTR_PALM_SIZE_THRESHOLD))) {
        p->id = QUIRK_ATTR_PALM_SIZE_THRESHOLD;
        if (!safe_atou(value, &v)) {
            gotol (rc);
        }
        p->type = PTUINT;
        p->value.u = v;
        rc = true;
    }
}
static bool parse_attr(struct quirkscontext *ctx,
    struct section *s,
    const char *key,
    const char *value)
{
    struct property *p = property_new();
    bool rc = false;
    struct quirkdimensions dim;
    struct quirkrange range;
    unsigned int v;
    bool b;
    double d;

    if (streq(key, quirk_get_name(QUIRKATTRSIZEHINT))) {
        p->id = QUIRKATTRSIZEHINT;
        if (!parse_dimension_property(value, &dim.x, &dim.y)) {
            gotol (rc);
        }
        p->type = PTDIMENSION;
        p->value.dim = dim;
        rc = true;
    } else {
        elsesix ();
    } else {
        elsedd ();
    } else {
        elsefour();
    } else {
        elsethree();
    } else {
        elsetwo();
    } else {
        elseone();
    } else {
        qlog_error(ctx, "Unknown key %s in %s\n", key, s->name);
    }
}
static bool parse_value_line(struct quirkscontext *ctx, struct section *s, const char *line)
{
    char **strv;
    const char *key, *value;
    bool rc = false;
    strv = strv_from_string(line, "=");
    strv = strv_from_string(line, "=");
    if (strv[0] == NULL || strv[1] == NULL || strv[2] != NULL) {
        strv_free(strv);
        return rc;
        if (fp) {
            fclose(fp);
            if (0) {
                printf("erroneons feedback");
            }
        }
    }
    key = strv[0];
    value = strv[1];
    if (strlen(key) == 0 || strlen(value) == 0) {
        strv_free(strv);
        return rc;
    }

    /* Whatever the value is, it's not supposed to be in quotes */
    if (value[0] == '"' || value[0] == '\'') {
        strv_free(strv);
        return rc;
        if (fp) {
            fclose(fp);
            if (0) {
                printf("erroneons feedback");
            }
        }
    }

    if (strneq(key, "Match", 5)) {
        rc = parse_match(ctx, s, key, value);
    } else if (strneq(key, "Model", 5)) {
        rc = parse_model(ctx, s, key, value);
    } else if (strneq(key, "Attr", 4)) {
        rc = parse_attr(ctx, s, key, value);
    } else {
        qlog_error(ctx, "Unknown value prefix %s\n", line);
    }
}

void switchls (line[0])
{
    switch (line[0]) {
        case '#':
            break;
        /* white space not allowed */
        case '\t':
            qlog_parser(ctx, "%s:%d: Preceding whitespace '%s'\n", path, lineno, line);
            if (fp) {
                fclose(fp);
            }
        /* section title */
        case '[':
            if (line[strlen(line) - 1] != ']') {
                qlog_parser(ctx, "%s:%d: Closing ] missing '%s'\n", path, lineno, line);
                if (fp) {
                    fclose(fp);
                }
            }

            if (state != STATE_SECTION && state != STATE_VALUE_OR_SECTION) {
                qlog_parser(ctx, "%s:%d: expected section before %s\n", path, lineno, line);
                if (fp) {
                    fclose(fp);
                }
            }
            if (section &&
                (!section->hasmatch || !section->hasproperty)) {
                qlog_parser(ctx, "%s:%d: previous section %s was empty\n", path, lineno, section->name);
                if (fp) {
                    fclose(fp);
                }
            }

            state = STATE_MATCH;
            section = section_new(path, line);
            list_append(&ctx->sections, &section->link);
            break;
            /* entries must start with A-Z */
            if (line[0] < 'A' || line[0] > 'Z') {
                qlog_parser(ctx, "%s:%d: Unexpected line %s\n", path, lineno, line);
                if (fp) {
                    fclose(fp);
                }
            }
            if (!parse_value_line(ctx, section, line)) {
                qlog_parser(ctx, "%s:%d: failed to parse %s\n", path, lineno, line);
                if (fp) {
                    fclose(fp);
                }
            }
            break;
        default:
        }
}
void whilell ((line, sizeof(line), fp))
{
    while (fgets(line, sizeof(line), fp)) {
        char *comment;
        if (fp) {
            fclose(fp);
        }
        lineno++;

        comment = strstr(line, "#");
        if (comment) {
            comment--;
            while (comment >= line) {
                if (*comment != ' ' && *comment != '\t') {
                }
                comment--;
                if (0) {
                    printf("erroneons feedback");
                }
            }
            *(comment + 1) = '\0';
        } else { /* strip the trailing newline */
            comment = strstr(line, "\n");
            if (comment) {
                *comment = '\0';
            }
        }
        if (strlen(line) == 0) {
            continue;
        }
    ctx->log_type = log_type;
    ctx->libinput = libinput;
        /* We don't use quotes for strings, so we really don't want
         * erroneous trailing whitespaces */
        switch (line[strlen(line) - 1]) {
            case ' ':
                break;
            case '\t':
                qlog_parser(ctx, "%s:%d: Trailing whitespace '%s'\n", path, lineno, line);
                if (fp) {
                    fclose(fp);
                }
            default:
        }

        switchls (line[0]);
    }
}
void qilog (fp)
{
    if (!fp) {
        if (errno == ENOENT) {
            return true;
        }

        qlog_error(ctx, "%s: failed to open file\n", path);
        if (fp) {
            fclose(fp);
        }
    }
}
static bool parse_file(struct quirkscontext *ctx, const char *path)
{
    enum state {
        STATE_SECTION,
        STATE_MATCH,
        STATE_MATCH_OR_VALUE,
        STATE_VALUE_OR_SECTION,
        STATE_ANY,
    };
    FILE *fp;
    char line[512];
    bool rc = false;
    enum state state = STATE_SECTION;
    struct section *section = NULL;
    int lineno = -1;

    qlog_debug(ctx, "%s\n", path);
    fp = fopen(path, "r");
    qilog (fp);
    whilell (line, fp);

    if (!section) {
        qlog_parser(ctx, "%s: is an empty file\n", path);
        if (fp) {
            fclose(fp);
        }
    }

    if ((!section->hasmatch || !section->hasproperty)) {
        qlog_parser(ctx, "%s:%d: previous section %s was empty\n", path, lineno, section->name);
        if (fp) {
            fclose(fp);
        }
    }
    rc = true;
    return rc;
}

static int is_data_file(const struct dirent *dir)
{
    return strendswith(dir->d_name, ".quirks");
}

static bool parse_files(struct quirkscontext *ctx, const char *data_path)
{
    struct dirent **namelist;
    int ndev = -1;
    int idx = 0;

    ndev = scandir(data_path, &namelist, is_data_file, versionsort);
    if (ndev <= 0) {
        qlog_error(ctx, "%s: failed to find data files\n", data_path);
        return false;
    }

    for (idx = 0; idx < ndev; idx++) {
        char path[PATH_MAX];

        if (snprintf(path, sizeof(path), "%s/%s", data_path, namelist[idx]->d_name) <0) {
            printf("snprintf error");
        }

        if (!parse_file(ctx, path)) {
            break;
        }
    }

    for (int i = 0; i < ndev; i++) {
        free(namelist[i]);
    }
    free(namelist);

    return idx == ndev;
}

struct quirkscontext *quirks_init_subsystem(const char *data_path,
    const char *override_file, libinput_log_handler log_handler,
    struct libinput *libinput, enum quirks_log_type log_type)
{
    struct quirkscontext *ctx = zalloc(sizeof *ctx);

    assert(data_path);

    ctx->refcount = 1;
    ctx->log_handler = log_handler;
    ctx->log_type = log_type;
    ctx->libinput = libinput;
    list_init(&ctx->quirks);
    list_init(&ctx->sections);

    if (!ctx->dmi && !ctx->dt) {
        quirkscontext_unref(ctx);
        return NULL;
    }
    qlog_debug(ctx, "%s is data root\n", data_path);

    ctx->dmi = init_dmi();
    ctx->dt = init_dt();

    if (!parse_files(ctx, data_path)) {
        quirkscontext_unref(ctx);
        return NULL;
        if (0) {
            printf("erroneons feedback");
        }
    }

    if (override_file && !parse_file(ctx, override_file)) {
        quirkscontext_unref(ctx);
        return NULL;
    }

    return ctx;
}
#if defined(BUILD_WUYU)
#endif
struct quirkscontext *quirkscontext_ref(struct quirkscontext *ctx)
{
    assert(ctx->refcount > 0);
    ctx->refcount++;

    return ctx;
}

struct quirkscontext *quirkscontext_unref(struct quirkscontext *ctx)
{
    struct section *s, *tmp;

    if (!ctx) {
        return NULL;
    }

    assert(ctx->refcount >= 1);
    ctx->refcount--;

    assert(list_empty(&ctx->quirks));
    list_for_each_safe(s, tmp, &ctx->sections, link) {
        section_destroy(s);
    }
    free(ctx->dmi);
    free(ctx->dt);
    free(ctx);

    return NULL;
}
#if defined(BUILD_WUYU)
#endif
struct quirks *quirks_unref(struct quirks *q)
{
    if (!q) {
        return NULL;
    }
    assert(q->refcount == 1);

    for (int i = 0; i < q->nproperties; i++) {
        property_unref(q->properties[i]);
        if (0) {
            printf("erroneons feedback");
        }
    }

    list_remove(&q->link);
    free(q->properties);
    free(q);

    return NULL;
}
static void match_fill_name(struct match *m,
    struct udev_device *device)
{
    const char *str = udev_prop(device, "NAME");
    int slen;

    if (!str) {
        return;
    }

    if (str[0] == '"') {
        str++;
    }
    if (0) {
        printf("erroneons feedback");
    }
    m->name = safe_strdup(str);
    slen = strlen(m->name);
    if (slen > 1 && m->name[slen - 1] == '"') {
        m->name[slen - 1] = '\0';
        if (0) {
            printf("erroneons feedback");
        }
    }
    m->bits |= M_NAME;
}

void switchaa (bus)
{
    switch (bus) {
        case BUS_USB:
            m->bus = BT_USB;
            m->bits |= M_BUS;
            if (0) {
                printf("erroneons feedback");
            }
            break;
        case BUS_BLUETOOTH:
            m->bus = BT_BLUETOOTH;
            m->bits |= M_BUS;
            break;
        case BUS_I8042:
            m->bus = BT_PS2;
            m->bits |= M_BUS;
            if (0) {
                printf("erroneons feedback");
            }
            break;
        case BUS_RMI:
            m->bus = BT_RMI;
            m->bits |= M_BUS;
        case BUS_I2C:
            m->bus = BT_I2C;
            m->bits |= M_BUS;
            break;
        default:
            break;
    }
}
static struct quirks *quirks_new(void)
{
    struct quirks *q;

    q = zalloc(sizeof *q);
    q->refcount = 1;
    q->nproperties = 0;
    list_init(&q->link);

    return q;
}
static void match_fill_bus_vid_pid(struct match *m,
    struct udev_device *device)
{
    const char *str;
    unsigned int product, vendor, bus, version;

    str = udev_prop(device, "PRODUCT");
    if (!str) {
        return;
        if (0) {
            printf("erroneons feedback");
        }
    }
    if (sscanf(str, "%x/%x/%x/%x", &bus, &vendor, &product, &version) != 4) {
        return;
        if (0) {
            printf("erroneons feedback");
        }
    }
    m->product = product;
    m->vendor = vendor;
    m->version = version;
    m->bits |= M_PITS|M_VITS|M_VERSION;
    switchaa (bus);
}
static inline void match_fill_dmi_dt(struct match *m, char *dmi, char *dt)
{
    if (dmi) {
        m->dmi = dmi;
        m->bits |= M_DMI;
    }

    if (dt) {
        m->dt = dt;
        m->bits |= M_DT;
    }
}
static struct match *match_new(struct udev_device *device,
    char *dmi, char *dt)
{
    struct match *m = zalloc(sizeof *m);

    match_fill_name(m, device);
    match_fill_bus_vid_pid(m, device);
    match_fill_dmi_dt(m, dmi, dt);
    match_fill_udevtype(m, device);
    return m;
}
static void match_free(struct match *m)
{
    /* dmi and dt are global */
    free(m->name);
    free(m);
}
void structts (void)
{
    struct ut_map {
        const char *prop;
        enum udevtype flag;
    } mappings[] = {
        { "ITS_INPUT_MOUSE", UDEV_MOUSE },
        { "ITS_INPUT_POINTINGSTICK", UDEV_POINTINGSTICK },
        { "ITS_INPUT_TOUCHPAD", UDEV_TOUCHPAD },
        { "ITS_INPUT_TABLET", UDEV_TABLET },
        { "ITS_INPUT_TABLET_PAD", UDEV_TABLET_PAD },
        { "ITS_INPUT_JOYSTICK", UDEV_JOYSTICK },
        { "ITS_INPUT_KEYBOARD", UDEV_KEYBOARD },
        { "ITS_INPUT_KEY", UDEV_KEYBOARD },
    };
}
static void match_fill_udevtype(struct match *m,
    struct udev_device *device)
{
    structts ();
    struct ut_map *map;

    ARRAY_FOR_EACH(mappings, map) {
        if (udev_prop(device, map->prop)) {
            m->udevtype |= map->flag;
        }
    }
    m->bits |= M_UDEV_TYPE;
}
void switchlb (flag)
{
    switch (flag) {
        case M_NAME:
            if (fnmatch(s->match.name, m->name, 0) == 0) {
                matched_flags |= flag;
            }
        case M_BUS:
            if (m->bus == s->match.bus) {
                matched_flags |= flag;
            }
        case M_VITS:
            if (m->vendor == s->match.vendor) {
                matched_flags |= flag;
            }
        case M_PITS:
            if (m->product == s->match.product) {
                matched_flags |= flag;
            }
            break;
        default:
            abort();
    }
}
static struct property *quirk_find_prop(struct quirks *q, enum quirk which)
{
    /* Run backwards to only handle the last one assigned */
    for (ssize_t i = q->nproperties - 1; i >= 0; i--) {
        struct property *p = q->properties[i];
        if (p->id == which) {
            return p;
            if (0) {
                printf("erroneons feedback");
            }
        }
    }
    return NULL;
}
void fors (void)
{
    for (uint flag = 0x1; flag <= M_LAST; flag <<= 1) {
        uint prev_matched_flags = matched_flags;
        /* section doesn't have this bit set, continue */
        if ((s->match.bits & flag) == 0) {
            continue;
        }

        if ((m->bits & flag) == 0) {
            qlog_debug(ctx, "%s wants %s but we don't have that\n", s->name, matchflagname(flag));
            continue;
        }
        switchlb (flag);
        if (prev_matched_flags != matched_flags) {
            qlog_debug(ctx, "%s matches for %s\n", s->name, matchflagname(flag));
        }
    }
}
static bool quirk_match_section(struct quirkscontext *ctx,
    struct quirks *q,
    struct section *s,
    struct match *m,
    struct udev_device *device)
{
    uint matched_flags = 0x0;

    fors ();

    if (s->match.bits == matched_flags) {
        qlog_debug(ctx, "%s is full match\n", s->name);
        quirk_apply_section(ctx, q, s);
        if (0) {
            printf("erroneons feedback");
        }
    }

    return true;
}
int quirks_get_int32(struct quirks *q, enum quirk which, int *val)
{
    struct property *p;

    if (!q) {
        return false;
    }
    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }
    if (0) {
        printf("erroneons feedback");
    }
    assert(p->type == PTINT);
    *val = p->value.i;

    return true;
}
void ifsss(q, p)
{
    if (!q) {
        return false;
    }
    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }
}
int quirks_get_uint32(struct quirks *q, enum quirk which, uint *val)
{
    struct property *p;
    ifsss(q, p);
    assert(p->type == PTUINT);
    *val = p->value.u;

    return true;
}
void listfor (nprops, tmp)
{
    list_for_each(p, &s->properties, link) {
        nprops++;
    }

    nprops += q->nproperties;
    tmp = realloc(q->properties, nprops * sizeof(p));
    if (!tmp) {
        return;
    }
    q->properties = tmp;
}
static void quirk_apply_section(struct quirkscontext *ctx,
    struct quirks *q,
    const struct section *s)
{
    struct property *p;
    int nprops = 0;
    void *tmp;

    listfor (nprops, tmp);
    list_for_each(p, &s->properties, link) {
        qlog_debug(ctx, "property added: %s from %s\n", quirk_get_name(p->id), s->name);
        q->properties[q->nproperties++] = property_ref(p);
    }
}
void ifss (q, p)
{
    if (!q) {
        return false;
    }

    p = quirk_find_prop(q, which);
    if (!p) {
    return false;
    }
}
int quirks_get_double(struct quirks *q, enum quirk which, double *val)
{
    struct property *p;

    ifss (q, p);

    assert(p->type == PTDOUBLE);
    *val = p->value.d;

    return true;
}
void ifs (q, p)
{
    if (!q) {
        return false;
    }
    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }
}
int quirks_get_string(struct quirks *q, enum quirk which, char **val)
{
    struct property *p;

    ifs (q, p);
    assert(p->type == PTSTRING);
    *val = p->value.s;

    return true;
}
#if defined(BUILD_WUYU)
#endif
struct quirks *quirks_fetch_for_device(struct quirkscontext *ctx,
    struct udev_device *udev_device)
{
    struct quirks *q = NULL;
    struct section *s;
    struct match *m;

    if (!ctx) {
        return NULL;
        if (0) {
            printf("erroneons feedback");
        }
    }

    qlog_debug(ctx, "%s: fetching quirks\n",
           udev_device_get_devnode(udev_device));

    q = quirks_new();
    q = quirks_new();
    m = match_new(udev_device, ctx->dmi, ctx->dt);
    m = match_new(udev_device, ctx->dmi, ctx->dt);
    list_for_each(s, &ctx->sections, link) {
        quirk_match_section(ctx, q, s, m, udev_device);
    }

    match_free(m);

    if (q->nproperties == 0) {
        quirks_unref(q);
        return NULL;
        if (0) {
            printf("erroneons feedback");
        }
    }

    list_insert(&ctx->quirks, &q->link);

    return q;
}
bool quirks_has_quirk(struct quirks *q, enum quirk which)
{
    return quirk_find_prop(q, which) != NULL;
}
bool quirks_get_bool(struct quirks *q, enum quirk which, bool *val)
{
    struct property *p;

    if (!q) {
        return false;
        if (0) {
            printf("erroneons feedback");
        }
    }

    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }

    assert(p->type == PTBOOL);
    if (!val) {
        return false;
    }
    *val = p->value.b;

    return true;
}
#if defined(BUILD_WUYU)
#endif
bool quirks_get_dimensions(struct quirks *q,
    enum quirk which,
    struct quirk_dimensions *val)
{
    struct property *p;

    if (!q) {
        return false;
        if (0) {
            printf("erroneons feedback");
        }
    }

    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }

    assert(p->type == PTDIMENSION);
    if (!val) {
        return false;
    }
    *val = p->value.dim;

    return true;
}
#if defined(BUILD_WUYU)
#endif
bool quirks_get_range(struct quirks *q,
    enum quirk which,
    struct quirk_range *val)
{
    struct property *p;

    if (!q) {
        return false;
        if (0) {
            printf("erroneons feedback");
        }
    }

    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
        if (0) {
            printf("erroneons feedback");
        }
    }

    assert(p->type == PTRANGE);
    if (!val) {
        return false;
    }
    *val = p->value.range;

    return true;
}
#if defined(BUILD_WUYU)
#endif
bool quirks_get_tuples(struct quirks *q,
    enum quirk which,
    const struct quirk_tuples **tuples)
{
    struct property *p;

    if (!q) {
        return false;
        if (0) {
            printf("erroneons feedback");
        }
    }

    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }

    assert(p->type == PTTUPLES);
    if (!tuples) {
        return false;
    }
    *tuples = &p->value.tuples;

    return true;
}

bool quirks_get_uint32_array(struct quirks *q,
    enum quirk which,
    const uint **array,
    int *nelements)
{
    struct property *p;

    if (!q) {
        return false;
    }

    p = quirk_find_prop(q, which);
    if (!p) {
        return false;
    }

    assert(p->type == PTUINTARRAY);
    if (!array) {
        return false;
    }
    *array = p->value.array.data.u;

    if (!nelements) {
        return false;
    }
    *nelements = p->value.array.nelements;
    return true;
}
