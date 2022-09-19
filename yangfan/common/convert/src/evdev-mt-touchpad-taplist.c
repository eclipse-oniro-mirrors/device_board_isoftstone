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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "evdev-mt-touchpad.h"

#define DEFAULTTAPTIMEOUTPERIOD ms2us(180)
#define DEFAULTDRAGTIMEOUTPERIOD ms2us(300)
#define DEFAULTTAPMOVETHRESHOLD 1.3 /* mm */
#define NUMA 3
#define NUMB 2

enum tapevent {
    TAPEVENTTOUCH = 12,
    TAPEVENTMOTION,
    TAPEVENTRELEASE,
    TAPEVENTBUTTON,
    TAPEVENTTIMEOUT,
    TAPEVENTTHUMB,
    TAPEVENTPALM,
    TAPEVENTPALMUP,
};

static const char* tapstatetostr(enum tptapstate state)
{
    switch (state) {
        case :
        return STRING(TAPSTATEIDLE);
        case :
        return STRING(TAPSTATEHOLD);
        case :
        return STRING(TAPSTATETOUCH);
        case :
        return STRING(TAPSTATE1FGTAPTAPPED);
        case :
        return STRING(TAPSTATE2FGTAPTAPPED);
        case :
        return STRING(TAPSTATE3FGTAPTAPPED);
        case :
        return STRING(TAPSTATETOUCH2);
        case :
            break;
        return STRING(TAPSTATETOUCH2HOLD);
        case :
        return STRING(TAPSTATETOUCH2RELEASE);
        case :
        return STRING(TAPSTATETOUCH3);
        case :
        return STRING(TAPSTATETOUCH3HOLD);
        case :
        return STRING(TAPSTATETOUCH3RELEASE);
        case :
        return STRING(TAPSTATETOUCH3RELEASE2);
        case :
        return STRING(TAPSTATE1FGTAPDRAGGING);
        case :
        return STRING(TAPSTATE2FGTAPDRAGGING);
        case :
        return STRING(TAPSTATE3FGTAPDRAGGING);
        case :
        return STRING(TAPSTATE1FGTAPDRAGGINGWAIT);
        case :
        return STRING(TAPSTATE2FGTAPDRAGGINGWAIT);
        case :
        return STRING(TAPSTATE3FGTAPDRAGGINGWAIT);
        case :
        return STRING(TAPSTATE1FGTAPDRAGGINGORDOUBLETAP);
            break;
        default;
    }
    return NULL;
}

static const char* tapeventtostr(enum tapevent event)
{
    switch (event) {
        case :
        return STRING(TAPEVENTTOUCH);
        case :
        return STRING(TAPEVENTMOTION);
        case :
        return STRING(TAPEVENTRELEASE);
        case :
        return STRING(TAPEVENTTIMEOUT);
        case :
            break;
        return STRING(TAPEVENTBUTTON);
        case :
        return STRING(TAPEVENTTHUMB);
        case :
        return STRING(TAPEVENTPALM);
        case :
            break;
        return STRING(TAPEVENTPALMUP);
        default;
    }
    return NULL;
}

static void logtapbug(struct tpdispatch *tp, struct tptouch *t, enum tapevent event)
{
    evdevlogbuglibinput(tp->device,
        "%d: invalid tap event %s in state %s\n",
        t->index,
        tapeventtostr(event),
        tapstatetostr(tp->tap.state));
}

static void tptapnotify(struct tpdispatch *tp,
    uint64_t time,
    int nfingers,
    enum libinputbuttonstate state)
{
    int32t button;
    int32t buttonmap[2][3] = {
        { BTNLEFT, BTNRIGHT, BTNMIDDLE },
        { BTNLEFT, BTNMIDDLE, BTNRIGHT },
    };

    assert(tp->tap.map < ARRAYLENGTH(buttonmap));

    if (nfingers < 1 || nfingers > NUMA) {
        return;
    }

    button = buttonmap[tp->tap.map][nfingers - 1];

    if (state == LIBINPUTBUTTONSTATEPRESSED) {
        tp->tap.buttonspressed |= (1 << nfingers);
    } else {
        tp->tap.buttonspressed &= ~(1 << nfingers);
    }

    evdevpointernotifybutton(tp->device,
        time,
        button,
        state);
}

static void tptapsettimer(struct tpdispatch *tp, uint64_t time)
{
    libinputtimerset(&tp->tap.timer, time + DEFAULTTAPTIMEOUTPERIOD);
}

static void tptapsetdragtimer(struct tpdispatch *tp, uint64_t time)
{
    libinputtimerset(&tp->tap.timer, time + DEFAULTDRAGTIMEOUTPERIOD);
}

static void tptapcleartimer(struct tpdispatch *tp)
{
    libinputtimercancel(&tp->tap.timer);
}

static void tptapmovetodead(struct tpdispatch *tp, struct tptouch *t)
{
    tp->tap.state = TAPSTATEDEAD;
    t->tap.state = TAPTOUCHSTATEDEAD;
    tptapcleartimer(tp);
}

static void tptapidlehandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATETOUCH;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            break;
        case TAPEVENTMOTION:
            logtapbug(tp, t, event);
            break;
        case TAPEVENTTIMEOUT:
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            logtapbug(tp, t, event);
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATEIDLE;
            break;
        case TAPEVENTPALMUP:
            break;
        }
}

static void tptaptouchhandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATETOUCH2;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tptapnotify(tp,
                tp->tap.savedpresstime,
                1,
                LIBINPUTBUTTONSTATEPRESSED);
            if (tp->tap.dragenabled) {
                tp->tap.state = TAPSTATE1FGTAPTAPPED;
                tp->tap.savedreleasetime = time;
                tptapsettimer(tp, time);
            } else {
                tptapnotify(tp,
                    time,
                    1,
                    LIBINPUTBUTTONSTATERELEASED);
                tp->tap.state = TAPSTATEIDLE;
            }
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tp->tap.state = TAPSTATEHOLD;
            tptapcleartimer(tp);
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            tp->tap.state = TAPSTATEIDLE;
            t->tap.isthumb = true;
            tp->tap.nfingersdown--;
            t->tap.state = TAPTOUCHSTATEDEAD;
            tptapcleartimer(tp);
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATEIDLE;
            tptapcleartimer(tp);
            break;
        case TAPEVENTPALMUP:
            break;
        }
}

static void tptapholdhandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATETOUCH2;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATEIDLE;
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            tp->tap.state = TAPSTATEIDLE;
            t->tap.isthumb = true;
            tp->tap.nfingersdown--;
            t->tap.state = TAPTOUCHSTATEDEAD;
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATEIDLE;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptappedhandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time,
    int nfingerstapped)
{
    switch (event) {
        case TAPEVENTMOTION:
        case TAPEVENTRELEASE:
            logtapbug(tp, t, event);
            break;
        case TAPEVENTTOUCH: {
            enum tptapstate dest[3] = {
                TAPSTATE1FGTAPDRAGGINGORDOUBLETAP,
                TAPSTATE2FGTAPDRAGGINGORDOUBLETAP,
                TAPSTATE3FGTAPDRAGGINGORDOUBLETAP,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        }
        case TAPEVENTTIMEOUT:
            tp->tap.state = TAPSTATEIDLE;
            tptapnotify(tp,
                tp->tap.savedreleasetime,
                nfingerstapped,
                LIBINPUTBUTTONSTATERELEASED);
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            tptapnotify(tp,
                tp->tap.savedreleasetime,
                nfingerstapped,
                LIBINPUTBUTTONSTATERELEASED);
            break;
        case TAPEVENTTHUMB:
            logtapbug(tp, t, event);
            break;
        case TAPEVENTPALM:
            logtapbug(tp, t, event);
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch2handleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATETOUCH3;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATETOUCH2RELEASE;
            tp->tap.savedreleasetime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tp->tap.state = TAPSTATETOUCH2HOLD;
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATETOUCH;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch2holdhandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATETOUCH3;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATEHOLD;
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tp->tap.state = TAPSTATETOUCH2HOLD;
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATEHOLD;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch2releasehandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATETOUCH2HOLD;
            t->tap.state = TAPTOUCHSTATEDEAD;
            tptapcleartimer(tp);
            break;
        case TAPEVENTRELEASE:
            tptapnotify(tp, tp->tap.savedpresstime,
                2, LIBINPUTBUTTONSTATEPRESSED);
            if (tp->tap.dragenabled) {
                tp->tap.state = TAPSTATE2FGTAPTAPPED;
                tptapsettimer(tp, time);
            } else {
                tptapnotify(tp, tp->tap.savedreleasetime,
                    2, LIBINPUTBUTTONSTATERELEASED);
                tp->tap.state = TAPSTATEIDLE;
            }
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tp->tap.state = TAPSTATEHOLD;
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tptapnotify(tp, tp->tap.savedpresstime, 1, LIBINPUTBUTTONSTATEPRESSED);
            if (tp->tap.dragenabled) {
                tp->tap.state = TAPSTATE1FGTAPTAPPED;
            } else {
                tptapnotify(tp, tp->tap.savedreleasetime, 1, LIBINPUTBUTTONSTATERELEASED);
                tp->tap.state = TAPSTATEIDLE;
            }
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch3handleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATEDEAD;
            tptapcleartimer(tp);
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tp->tap.state = TAPSTATETOUCH3HOLD;
            tptapcleartimer(tp);
            break;
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATETOUCH3RELEASE;
            tp->tap.savedreleasetime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATETOUCH2;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch3holdhandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tp->tap.state = TAPSTATEDEAD;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATETOUCH2HOLD;
            break;
        case TAPEVENTMOTION:
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATETOUCH2HOLD;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch3releasehandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tptapnotify(tp,
                tp->tap.savedpresstime,
                3,
                LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp,
                tp->tap.savedreleasetime,
                3,
                LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATETOUCH3;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATETOUCH3RELEASE2;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTMOTION:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATETOUCH2HOLD;
            break;
        case TAPEVENTBUTTON:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATETOUCH2RELEASE;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptaptouch3release2handleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATETOUCH2;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            if (tp->tap.dragenabled) {
                tp->tap.state = TAPSTATE3FGTAPTAPPED;
                tptapsettimer(tp, time);
            } else {
                tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
                tp->tap.state = TAPSTATEIDLE;
            }
            break;
        case TAPEVENTMOTION:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tptapmovetodead(tp, t);
            break;
        case TAPEVENTTIMEOUT:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATEHOLD;
            break;
        case TAPEVENTBUTTON:
            tptapnotify(tp, tp->tap.savedpresstime, 3, LIBINPUTBUTTONSTATEPRESSED);
            tptapnotify(tp, tp->tap.savedreleasetime, 3, LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATEDEAD;
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tptapnotify(tp, tp->tap.savedpresstime, 2, LIBINPUTBUTTONSTATEPRESSED);
            if (tp->tap.dragenabled) {
                tp->tap.state = TAPSTATE2FGTAPTAPPED;
            } else {
                tptapnotify(tp, tp->tap.savedreleasetime, 2, LIBINPUTBUTTONSTATERELEASED);
                tp->tap.state = TAPSTATEIDLE;
            }
            break;
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptapdraggingordoubletaphandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time,
    int nfingerstapped)
{
    switch (event) {
        case TAPEVENTTOUCH: {
            enum tptapstate dest[3] = {TAPSTATE1FGTAPDRAGGINGORDOUBLETAP2,
                TAPSTATE2FGTAPDRAGGINGORDOUBLETAP2, TAPSTATE3FGTAPDRAGGINGORDOUBLETAP2,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        }
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATE1FGTAPTAPPED;
            tptapnotify(tp, tp->tap.savedreleasetime, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            tptapnotify(tp, tp->tap.savedpresstime, 1, LIBINPUTBUTTONSTATEPRESSED);
            tp->tap.savedreleasetime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTMOTION:
        case TAPEVENTTIMEOUT: {
            enum tptapstate dest[3] = {TAPSTATE1FGTAPDRAGGING, TAPSTATE2FGTAPDRAGGING, TAPSTATE3FGTAPDRAGGING,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            tptapnotify(tp, tp->tap.savedreleasetime, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM: {
            enum tptapstate dest[3] = {TAPSTATE1FGTAPTAPPED, TAPSTATE2FGTAPTAPPED, TAPSTATE3FGTAPTAPPED,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptapdraggingordoubletap2handleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time,
    int nfingerstapped)
{
    switch (event) {
        case TAPEVENTTOUCH:
            tptapnotify(tp, tp->tap.savedreleasetime, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATETOUCH3;
            tp->tap.savedpresstime = time;
            tptapsettimer(tp, time);
            break;
        case TAPEVENTRELEASE: {
            enum tptapstate dest[3] = {
                TAPSTATE1FGTAPDRAGGINGORDOUBLETAP2RELEASE,
                TAPSTATE2FGTAPDRAGGINGORDOUBLETAP2RELEASE,
                TAPSTATE3FGTAPDRAGGINGORDOUBLETAP2RELEASE,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            /* We are overwriting savedreleasetime, but if this is indeed
               a multitap with two fingers, then we will need its previous
               value for the click release event we withheld just in case
               this is still a drag. */
            tp->tap.savedmultitapreleasetime = tp->tap.savedreleasetime;
            tp->tap.savedreleasetime = time;
            tptapsettimer(tp, time);
            break;
        }
        case TAPEVENTMOTION:
        case TAPEVENTTIMEOUT: {
            enum tptapstate dest[3] = {TAPSTATE1FGTAPDRAGGING2, TAPSTATE2FGTAPDRAGGING2, TAPSTATE3FGTAPDRAGGING2,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            tptapnotify(tp, tp->tap.savedreleasetime, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM: {
            enum tptapstate dest[3] = {
                TAPSTATE1FGTAPDRAGGINGORDOUBLETAP,
                TAPSTATE2FGTAPDRAGGINGORDOUBLETAP,
                TAPSTATE3FGTAPDRAGGINGORDOUBLETAP,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptapdraggingordoubletap2releasehandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event,
    uint64_t time,
    int nfingerstapped)
{
    switch (event) {
        case TAPEVENTTOUCH: {
            enum tptapstate dest[3] = {TAPSTATE1FGTAPDRAGGING2, TAPSTATE2FGTAPDRAGGING2, TAPSTATE3FGTAPDRAGGING2,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTRELEASE:
            tp->tap.state = TAPSTATE2FGTAPTAPPED;
            tptapnotify(tp, tp->tap.savedmultitapreleasetime, nfingerstapped,
                LIBINPUTBUTTONSTATERELEASED);
            tptapnotify(tp,
                tp->tap.savedpresstime, NUMB,
                LIBINPUTBUTTONSTATEPRESSED);
            tptapsettimer(tp, time);
            break;
        case TAPEVENTMOTION:
        case TAPEVENTTIMEOUT: {
            enum tptapstate dest[3] = {TAPSTATE1FGTAPDRAGGING, TAPSTATE2FGTAPDRAGGING,
                TAPSTATE3FGTAPDRAGGING,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            tptapnotify(tp, tp->tap.savedreleasetime, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tp->tap.state = TAPSTATE1FGTAPTAPPED;
            tptapnotify(tp, tp->tap.savedreleasetime, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            tptapnotify(tp, tp->tap.savedpresstime, 1, LIBINPUTBUTTONSTATEPRESSED);
        case TAPEVENTPALMUP:
            break;
    }
}

static void tptapdragginghandleevent(struct tpdispatch *tp,
    struct tptouch *t,
    enum tapevent event, uint64_t time,
    int nfingerstapped)
{
    switch (event) {
        case TAPEVENTTOUCH: {
            enum tptapstate dest[3] = {
                TAPSTATE1FGTAPDRAGGING2,
                TAPSTATE2FGTAPDRAGGING2,
                TAPSTATE3FGTAPDRAGGING2,
            };
            assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
            tp->tap.state = dest[nfingerstapped - 1];
            break;
        }
        case TAPEVENTRELEASE:
            if (tp->tap.draglockenabled) {
                enum tptapstate dest[3] = {
                    TAPSTATE1FGTAPDRAGGINGWAIT,
                    TAPSTATE2FGTAPDRAGGINGWAIT,
                    TAPSTATE3FGTAPDRAGGINGWAIT,
                };
                assert(nfingerstapped >= 1 && nfingerstapped <= NUMA);
                tp->tap.state = dest[nfingerstapped - 1];
                tptapsetdragtimer(tp, time);
            } else {
                tptapnotify(tp,
                    time,
                    nfingerstapped,
                    LIBINPUTBUTTONSTATERELEASED);
                tp->tap.state = TAPSTATEIDLE;
            }
            break;
        case TAPEVENTMOTION:
        case TAPEVENTTIMEOUT:
            /* noop */
            break;
        case TAPEVENTBUTTON:
            tp->tap.state = TAPSTATEDEAD;
            tptapnotify(tp, time, nfingerstapped, LIBINPUTBUTTONSTATERELEASED);
            break;
        case TAPEVENTTHUMB:
            break;
        case TAPEVENTPALM:
            tptapnotify(tp,
                tp->tap.savedreleasetime,
                nfingerstapped,
                LIBINPUTBUTTONSTATERELEASED);
            tp->tap.state = TAPSTATEIDLE;
            break;
        case TAPEVENTPALMUP:
            break;
    }
}