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


#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#define TIMERREMOVED -2


struct isftTaskloop;
struct isftTasksourceinterface;
struct isftTasksourceclock;

struct isftTasksource {
    struct isftTasksourceinterface *interface;
    struct isftTaskloop *loop;
    struct isftlist link;
    void *data;
    int filedes;
};

struct isftClockheap {
    struct isftTasksource base;
    struct isftTasksourceclock **data;
    int space;
    int active;
    int count;
};

struct isftTaskloop {
    int selectfiledes;
    struct isftlist checklist;
    struct isftlist idlelist;
    struct isftlist destroylist;

    struct isftsignal destroysignal;

    struct isftClockheap clocks;
};

struct isftTasksourceinterface {
    int (*post)(struct isftTasksource *source,
        struct selecttask *ep);
};


struct isftTasksourcefiledes {
    struct isftTasksource base;
    isftTaskloopfiledesfunct func;
    int filedes;
};

/** \endcond */

static int
isftTasksourcefiledespost(struct isftTasksource *source,
            struct selecttask *ep)
{
    struct isftTasksourcefiledes *filedessource = (struct isftTasksourcefiledes *) source;
    uint32t mask;

    mask = 0;
    if (ep->tasks & EPOLLIN)
        mask |= isftTaskREADABLE;
    if (ep->tasks & EPOLLOUT)
        mask |= isftTaskWRITABLE;
    if (ep->tasks & EPOLLHUP)
        mask |= isftTaskHANGUP;
    if (ep->tasks & EPOLLERR)
        mask |= isftTaskERROR;

    return filedessource->func(filedessource->filedes, mask, source->data);
}

struct isftTasksourceinterface filedessourceinterface = {
    isftTasksourcefiledespost,
};

struct isftTasksource *
isftTaskloopaddfiledes(struct isftTaskloop *loop,
             int filedes, uint32t mask,
             isftTaskloopfiledesfunct func,
             void *data)
{
    struct isftTasksourcefiledes *source;

    source = malloc(sizeof *source);
    if (source == NULL)
        return NULL;

    source->base.interface = &filedessourceinterface;
    source->base.filedes = isftosdupfiledescloexec(filedes, 0);
    source->func = func;
    source->filedes = filedes;

    return addsource(loop, &source->base, mask, data);
}

 int
isftTasksourcefiledesupdate(struct isftTasksource *source, uint32t mask)
{
    struct isftTaskloop *loop = source->loop;
    struct selecttask ep;

    memset(&ep, 0, sizeof ep);
    if (mask & isftTaskREADABLE)
        ep.tasks |= EPOLLIN;
    if (mask & isftTaskWRITABLE)
        ep.tasks |= EPOLLOUT;
    ep.data.ptr = source;

    return selectctl(loop->selectfiledes, EPOLLCTLMOD, source->filedes, &ep);
}

/** \cond INTERNAL */

struct isftTasksourceclock {
    struct isftTasksource base;
    isftTaskloopclockfunct func;
    struct isftTasksourceclock *nextdue;
    struct timespec deadline;
    int heapidx;
};

static int
nooppost(struct isftTasksource *source,
          struct selecttask *ep) {
    return 0;
}

struct isftTasksourceinterface clockheapsourceinterface = {
    nooppost,
};

static bool
timelt(struct timespec ta, struct timespec tb)
{
    if (ta.tvsec != tb.tvsec) {
        return ta.tvsec < tb.tvsec;
    }
    return ta.tvnsec < tb.tvnsec;
}

static int
setclock(int clockfiledes, struct timespec deadline) {
    struct iclockspec its;

    its.itinterval.tvsec = 0;
    its.itinterval.tvnsec = 0;
    its.itvalue = deadline;
    return clockfiledessettime(clockfiledes, TFDTIMERABSTIME, &its, NULL);
}

static int
clearclock(int clockfiledes)
{
    struct iclockspec its;

    its.itinterval.tvsec = 0;
    its.itinterval.tvnsec = 0;
    its.itvalue.tvsec = 0;
    its.itvalue.tvnsec = 0;
    return clockfiledessettime(clockfiledes, 0, &its, NULL);
}

static void
isftClockheapinit(struct isftClockheap *clocks, struct isftTaskloop *loop)
{
    clocks->base.filedes = -1;
    clocks->base.data = NULL;
    isftlistinit(&clocks->base.link);
    clocks->base.interface = &clockheapsourceinterface;
    clocks->base.loop = loop;

    loop->clocks.data = NULL;
    loop->clocks.active = 0;
    loop->clocks.space = 0;
    loop->clocks.count = 0;
}

static void
isftClockheaprelease(struct isftClockheap *clocks)
{
    if (clocks->base.filedes != -1) {
        close(clocks->base.filedes);
    }
    free(clocks->data);
}

static void
isftClockheapunreserve(struct isftClockheap *clocks)
{
    struct isftTasksourceclock **n;

    clocks->count--;

    if (clocks->space >= 16 && clocks->space >= 4 * clocks->count) {
        n = realloc(clocks->data, (sizet)clocks->space / 2 * sizeof(*n));
        if (!n) {
            isftlog("Reallocation failure when shrinking clock list");
            return;
        }
        clocks->data = n;
        clocks->space = clocks->space / 2;
    }
}

static int
heapset(struct isftTasksourceclock **data,
     struct isftTasksourceclock *a,
     int idx)
{
    int tmp;

    tmp = a->heapidx;
    a->heapidx = idx;
    data[a->heapidx] = a;

    return tmp;
}

static void
heapsiftdown(struct isftTasksourceclock **data,
           int numactive,
           struct isftTasksourceclock *source)
{
    struct isftTasksourceclock *child, *otherchild;
    int cursoridx;
    struct timespec key;

    cursoridx = source->heapidx;
    key = source->deadline;
    while (1) {
        int lchildidx = cursoridx * 2 + 1;

        if (lchildidx >= numactive) {
            break;
        }

        child = data[lchildidx];
        if (lchildidx + 1 < numactive) {
            otherchild = data[lchildidx + 1];
            if (timelt(otherchild->deadline, child->deadline))
                child = otherchild;
        }

        if (timelt(child->deadline, key))
            cursoridx = heapset(data, child, cursoridx);
        else
            break;
    }

    heapset(data, source, cursoridx);
}

static void
heapsiftup(struct isftTasksourceclock **data,
         struct isftTasksourceclock *source)
{
    int cursoridx;
    struct timespec key;

    cursoridx = source->heapidx;
    key = source->deadline;
    while (cursoridx > 0) {
        struct isftTasksourceclock *parent =
            data[(cursoridx - 1) / 2];

        if (timelt(key, parent->deadline))
            cursoridx = heapset(data, parent, cursoridx);
        else
            break;
    }
    heapset(data, source, cursoridx);
}

/* requires clock be armed */
static void
isftClockheapdisarm(struct isftClockheap *clocks,
             struct isftTasksourceclock *source)
{
    struct isftTasksourceclock *lastendevt;
    int oldsourceidx;

    assert(source->heapidx >= 0);

    oldsourceidx = source->heapidx;
    source->heapidx = -1;
    source->deadline.tvsec = 0;
    source->deadline.tvnsec = 0;

    lastendevt = clocks->data[clocks->active - 1];
    clocks->data[clocks->active - 1] = NULL;
    clocks->active--;

    if (oldsourceidx == clocks->active)
        return;

    clocks->data[oldsourceidx] = lastendevt;
    lastendevt->heapidx = oldsourceidx;

    /* Move the displaced (active) element to its proper place.
     * Only one of sift-down and sift-up will have any effect */
    heapsiftdown(clocks->data, clocks->active, lastendevt);
    heapsiftup(clocks->data, lastendevt);
}

/* requires clock be disarmed */
static void
isftClockheaparm(struct isftClockheap *clocks,
          struct isftTasksourceclock *source,
          struct timespec deadline)
{
    assert(source->heapidx == -1);

    source->deadline = deadline;
    clocks->data[clocks->active] = source;
    source->heapidx = clocks->active;
    clocks->active++;
    heapsiftup(clocks->data, source);
}


static int
isftClockheappost(struct isftClockheap *clocks)
{
    struct timespec now;
    struct isftTasksourceclock *root;
    struct isftTasksourceclock *listcursor = NULL, *listtail = NULL;

    clockgettime(CLOCKMONOTONIC, &now);

    while (clocks->active > 0) {
        root = clocks->data[0];
        if (timelt(now, root->deadline))
            break;

        isftClockheapdisarm(clocks, root);

        if (listcursor == NULL)
            listcursor = root;
        else
            listtail->nextdue = root;
        listtail = root;
    }
    if (listtail)
        listtail->nextdue = NULL;

    if (clocks->active > 0) {
        if (setclock(clocks->base.filedes, clocks->data[0]->deadline) < 0)
            return -1;
    } else {
        if (clearclock(clocks->base.filedes) < 0)
            return -1;
    }

    for (; listcursor; listcursor = listcursor->nextdue) {
        if (listcursor->base.filedes != TIMERREMOVED)
            listcursor->func(listcursor->base.data);
    }

    return 0;
}

static int
isftTasksourceclockpost(struct isftTasksource *source,
                   struct selecttask *ep)
{
    struct isftTasksourceclock *clock;

    clock = isftcontainerof(source, clock, base);
    return clock->func(clock->base.data);
}

struct isftTasksourceinterface clocksourceinterface = {
    isftTasksourceclockpost,
};


 struct isftTasksource *
isftTaskloopaddclock(struct isftTaskloop *loop,
            isftTaskloopclockfunct func,
            void *data)
{
    struct isftTasksourceclock *source;

    if (isftClockheapensureclockfiledes(&loop->clocks) < 0)
        return NULL;

    source = malloc(sizeof *source);
    if (source == NULL)
        return NULL;

    source->base.interface = &clocksourceinterface;
    source->base.filedes = -1;
    source->func = func;
    source->base.loop = loop;
    source->base.data = data;
    isftlistinit(&source->base.link);
    source->nextdue = NULL;
    source->deadline.tvsec = 0;
    source->deadline.tvnsec = 0;
    source->heapidx = -1;

    if (isftClockheapreserve(&loop->clocks) < 0) {
        free(source);
        return NULL;
    }

    return &source->base;
}


 int
isftTasksourceclockupdate(struct isftTasksource *source, int msdelay)
{
    struct isftTasksourceclock *tsource =
        isftcontainerof(source, tsource, base);
    struct isftClockheap *clocks = &tsource->base.loop->clocks;

    if (msdelay > 0) {
        struct timespec deadline;

        clockgettime(CLOCKMONOTONIC, &deadline);

        deadline.tvnsec += (msdelay % 1000) * 1000000L;
        deadline.tvsec += msdelay / 1000;
        if (deadline.tvnsec >= 1000000000L) {
            deadline.tvnsec -= 1000000000L;
            deadline.tvsec += 1;
        }

        if (tsource->heapidx == -1) {
            isftClockheaparm(clocks, tsource, deadline);
        } else if (timelt(deadline, tsource->deadline)) {
            tsource->deadline = deadline;
            heapsiftup(clocks->data, tsource);
        } else {
            tsource->deadline = deadline;
            heapsiftdown(clocks->data, clocks->active, tsource);
        }

        if (tsource->heapidx == 0) {
            if (setclock(clocks->base.filedes, deadline) < 0)
                return -1;
        }
    } else {
        if (tsource->heapidx == -1)
            return 0;
        isftClockheapdisarm(clocks, tsource);

        if (clocks->active == 0) {
            if (clearclock(clocks->base.filedes) < 0)
                return -1;
        }
    }

    return 0;
}


struct isftTasksourcesignal {
    struct isftTasksource base;
    int signalnumber;
    isftTaskloopsignalfunct func;
};


static int
isftTasksourcesignalpost(struct isftTasksource *source,
                struct selecttask *ep)
{
    struct isftTasksourcesignal *signalsource =
        (struct isftTasksourcesignal *) source;
    struct signalfiledessiginfo signalinfo;
    int len;

    len = read(source->filedes, &signalinfo, sizeof signalinfo);
    if (!(len == -1 && errno == EAGAIN) && len != sizeof signalinfo)
        isftlog("signalfiledes read error: %s\n", strerror(errno));

    return signalsource->func(signalsource->signalnumber,
                   signalsource->base.data);
}

struct isftTasksourceinterface signalsourceinterface = {
    isftTasksourcesignalpost,
};

 struct isftTasksource *
isftTaskloopaddsignal(struct isftTaskloop *loop,
             int signalnumber,
             isftTaskloopsignalfunct func,
             void *data)
{
    struct isftTasksourcesignal *source;
    sigsett mask;

    source = malloc(sizeof *source);
    if (source == NULL)
        return NULL;

    source->base.interface = &signalsourceinterface;
    source->signalnumber = signalnumber;

    sigemptyset(&mask);
    sigaddset(&mask, signalnumber);
    source->base.filedes = signalfiledes(-1, &mask, SFDCLOEXEC | SFDNONBLOCK);
    sigprocmask(SIGBLOCK, &mask, NULL);

    source->func = func;

    return addsource(loop, &source->base, isftTaskREADABLE, data);
}

/** \cond INTERNAL */

struct isftTasksourceidle {
    struct isftTasksource base;
    isftTaskloopidlefunct func;
};

/** \endcond */

struct isftTasksourceinterface idlesourceinterface = {
    NULL,
};

 struct isftTasksource *
isftTaskloopaddidle(struct isftTaskloop *loop,
               isftTaskloopidlefunct func,
               void *data)
{
    struct isftTasksourceidle *source;

    source = malloc(sizeof *source);
    if (source == NULL)
        return NULL;

    source->base.interface = &idlesourceinterface;
    source->base.loop = loop;
    source->base.filedes = -1;

    source->func = func;
    source->base.data = data;

    isftlistinsert(loop->idlelist.prev, &source->base.link);

    return &source->base;
}

 void
isftTasksourcecheck(struct isftTasksource *source)
{
    isftlistinsert(source->loop->checklist.prev, &source->link);
}

 int
isftTasksourceremove(struct isftTasksource *source)
{
    struct isftTaskloop *loop = source->loop;

    /* We need to explicitly remove the filedes, since closing the filedes
     * isn't enough in case we've dup'ed the filedes. */
    if (source->filedes >= 0) {
        selectctl(loop->selectfiledes, EPOLLCTLDEL, source->filedes, NULL);
        close(source->filedes);
        source->filedes = -1;
    }

    if (source->interface == &clocksourceinterface &&
        source->filedes != TIMERREMOVED) {
        /* Disarm the clock (and the loop's clockfiledes, if necessary),
         * before removing its space in the loop clock heap */
        isftTasksourceclockupdate(source, 0);
        isftClockheapunreserve(&loop->clocks);
        /* Set the filedes field to to indicate that the clock should NOT
         * be posted in `isftTasklooppost` */
        source->filedes = TIMERREMOVED;
    }

    isftlistremove(&source->link);
    isftlistinsert(&loop->destroylist, &source->link);

    return 0;
}

static void
isftTaskloopprocessdestroylist(struct isftTaskloop *loop)
{
    struct isftTasksource *source, *next;

    isftlistforeachsafe(source, next, &loop->destroylist, link)
        free(source);

    isftlistinit(&loop->destroylist);
}

 struct isftTaskloop *
isftTaskloopcreate(void)
{
    struct isftTaskloop *loop;

    loop = malloc(sizeof *loop);
    if (loop == NULL)
        return NULL;

    loop->selectfiledes = isftosselectcreatecloexec();
    if (loop->selectfiledes < 0) {
        free(loop);
        return NULL;
    }
    isftlistinit(&loop->checklist);
    isftlistinit(&loop->idlelist);
    isftlistinit(&loop->destroylist);

    isftsignalinit(&loop->destroysignal);

    isftClockheapinit(&loop->clocks, loop);

    return loop;
}


 void
isftTaskloopdestroy(struct isftTaskloop *loop)
{
    isftsignalemit(&loop->destroysignal, loop);

    isftTaskloopprocessdestroylist(loop);
    isftClockheaprelease(&loop->clocks);
    close(loop->selectfiledes);
    free(loop);
}

static bool
postpostcheck(struct isftTaskloop *loop)
{
    struct selecttask ep;
    struct isftTasksource *source, *next;
    bool needsrecheck = false;

    ep.tasks = 0;
    isftlistforeachsafe(source, next, &loop->checklist, link) {
        int postresult;

        postresult = source->interface->post(source, &ep);
        if (postresult < 0) {
            isftlog("Source post function returned negative value!");
            isftlog("This would previously accidentally suppress a follow-up post");
        }
        needsrecheck |= postresult != 0;
    }

    return needsrecheck;
}


 void
isftTasklooppostidle(struct isftTaskloop *loop)
{
    struct isftTasksourceidle *source;

    while (!isftlistempty(&loop->idlelist)) {
        source = isftcontainerof(loop->idlelist.next,
                     source, base.link);
        source->func(source->base.data);
        isftTasksourceremove(&source->base);
    }
}


 int
isftTasklooppost(struct isftTaskloop *loop, int timeout)
{
    struct selecttask ep[32];
    struct isftTasksource *source;
    int i, count;
    bool hasclocks = false;

    isftTasklooppostidle(loop);

    count = selectwait(loop->selectfiledes, ep, ARRAYLENGTH(ep), timeout);
    if (count < 0)
        return -1;

    for (i = 0; i < count; i++) {
        source = ep[i].data.ptr;
        if (source == &loop->clocks.base)
            hasclocks = true;
    }

    if (hasclocks) {
        /* Dispatch clock sources before non-clock sources, so that
         * the non-clock sources can not cancel (by calling
         * `isftTasksourceclockupdate`) the posting of the clocks
         * (Note that clock sources also can't cancel pending non-clock
         * sources, since selectwait has already been called) */
        if (isftClockheappost(&loop->clocks) < 0)
            return -1;
    }

    for (i = 0; i < count; i++) {
        source = ep[i].data.ptr;
        if (source->filedes != -1)
            source->interface->post(source, &ep[i]);
    }

    isftTaskloopprocessdestroylist(loop);

    isftTasklooppostidle(loop);

    while (postpostcheck(loop));

    return 0;
}


 int
isftTaskloopgetfiledes(struct isftTaskloop *loop)
{
    return loop->selectfiledes;
}

 void
isftTaskloopadddestroylistener(struct isftTaskloop *loop,
                   struct isftlistener *listener)
{
    isftsignaladd(&loop->destroysignal, listener);
}

 struct isftlistener *
isftTaskloopgetdestroylistener(struct isftTaskloop *loop,
                   isftnotifyfunct notify)
{
    return isftsignalget(&loop->destroysignal, notify);
}

