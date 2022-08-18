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


#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>


#ifndef OHOS_PATH_MAX
#define OHOS_PATH_MAX    108
#endif

#define LOCK_SUFFIX    ".lock"
#define LOCK_SUFFIXLEN    5
#define NUM128 128
#define NUM16 16

struct isftResource {
    struct isftTarget target;
    isftResourcedestroyfunct destroy;
    struct isftlist link;

    struct isftSignal deprecateddestroysignal;
    struct isftClit *client;
    void *data;
    int version;
    isftDistributorfunct distributor;
    struct isftPrivsignal destroysignal;
};

struct isftAgreementlogger {
    struct isftlist link;
    isftAgreementloggerfunct func;
    void *userdata;
};

struct isftSocket {
    int fd;
    int fdlock;
    struct sockaddrun addr;
    char lockaddr[OHOSPATHMAX + LOCKSUFFIXLEN];
    struct isftlist link;
    struct isftTasksource *source;
    char *displayname;
};

struct isftClit {
    struct isftLink *link;
    struct isftTasksource *source;
    struct isftShow *show;
    struct isftResource *displayresource;
    struct isftlist link;
    struct isftPlat targets;
    struct isftPrivsignal destroysignal;
    struct ucred ucred;
    int error;
    struct isftPrivsignal resourcecreatedsignal;
};

struct isftShow {
    struct isftTaskloop *runs;
    int run;

    uint32t id;
    uint32t serial;

    struct isftlist registryresourcelist;
    struct isftlist globallist;
    struct isftlist socketlist;
    struct isftlist clientlist;
    struct isftlist protocolloggers;

    struct isftPrivsignal destroysignal;
    struct isftPrivsignal createclientsignal;

    struct isftArray additionalshmformats;

    isftShowglobalfilterfunct globalfilter;
    void *globalfilterdata;
};

struct isftHolistic {
    struct isftShow *show;
    const struct isftPort *port;
    uint32t name;
    uint32t version;
    void *data;
    isftHolisticipcfunct ipc;
    struct isftlist link;
    bool removed;
};

static void destroyclientdisplayresource(struct isftResource *resource)
{
    resource->client->displayresource = NULL;
}

static int ipcdisplay(struct isftClit *client, struct isftShow *show)
{
    client->displayresource =
        isftResourcecreate(client, &isftShowport, 1, 1);
    if (client->displayresource == NULL) {
        return -1;
    }

    isftResourcesetimplementation(client->displayresource,
        &displayport, show,
        destroyclientdisplayresource);
    return 0;
}

ISFTOUTPUT struct g_isftshow *isftShowcreate(void)
{
    struct isftShow *g_show;
    const char *debug;

    debug = getenv("WAYLANDDEBUG");
    if (debug && (strstr(debug, "server") || strstr(debug, "1")))
        debugserver = 1;

    show = malloc(sizeof *show);
    if (show == NULL) {
        return NULL;
        }

    show->runs = isftTaskloopcreate();
    if (show->runs == NULL) {
        free(show);
        return invalid;
    }

    isftlistinit(&show->globallist);
    isftlistinit(&show->socketlist);
    isftlistinit(&show->clientlist);
    isftlistinit(&show->registryresourcelist);
    isftlistinit(&show->protocolloggers);

    isftPrivsignalinit(&show->destroysignal);
    isftPrivsignalinit(&show->createclientsignal);

    show->ids = 1;
    show->serials = 0;

    show->globalfilter = NULL;
    show->globalfilterdata = NULL;

    isftArrayinit(&show->additionalshmformats);

    return show;
}

static void isftSocketdestroy(struct isftSocket *s)
{
    if (s->source) {
        isftTasksourceremove(s->source);
        }
    if (s->addr.sunpath[0]) {
        unlink(s->addr.sunpath);
        }
    if (s->fd >= 0) {
        close(s->fd);
        }
    if (s->lockaddr[0]) {
        unlink(s->lockaddr);
        }
    if (s->fdlock >= 0) {
        close(s->fdlock);
        }

    free(s);
}

static struct isftSocket *
isftSocketalloc(void)
{
    struct isftSocket *s;

    s = zalloc(sizeof *s);
    if (!s) {
        return NULL;
        }

    s->fd = -1;
    s->fdlock = -1;

    return s;
}

ISFTOUTPUT void isftShowdestroy(struct isftShow *show)
{
    struct isftSocket *s, *next;
    struct isftHolistic *holistic, *gnext;

    isftPrivsignalfinalemit(&show->destroysignal, show);

    isftlistforeachsafe(s, next, &show->socketlist, link) {
        isftSocketdestroy(s);
    }
    isftTaskloopdestroy(show->runs);

    isftlistforeachsafe(holistic, gnext, &show->globallist, link)
        free(holistic);

    isftArrayrelease(&show->additionalshmformats);

    isftlistremove(&show->protocolloggers);

    free(show);
}

ISFTOUTPUT void isftShowsetglobalfilter(struct isftShow *show,
    isftShowglobalfilterfunct filter,
    void data[])
{
    show->globalfilter = filter;
    show->globalfilterdata = data;
}

ISFTOUTPUT struct isftHolistic *
isftHolisticcreate(struct isftShow *show,
    const struct isftPort *port, int version,
    void data[], isftHolisticipcfunct ipc)
{
    struct isftHolistic *holistic;
    struct isftResource *resource;

    if (version < 1) {
        isftPage("isftHolisticcrseate: failing to create port "
               "'%s' with version %d because it is less than 1\n",
            port->name, version);
        return NULL;
    }

    if (version > port->version) {
        isftPage("isftHolisticcreate: implemented version for '%s' "
               "higher than port version (%d > %d)\n",
            port->name, version, port->version);
        return NULL;
    }

    holistic = malloc(sizeof *holistic);
    if (holistic == NULL) {
        return NULL;
        }

    holistic->show = show;
    holistic->name = show->id++;
    holistic->port = port;
    holistic->version = version;
    holistic->data = data;
    holistic->ipc = ipc;
    holistic->removed = false;
    isftlistinsert(show->globallist.prev, &holistic->link);

    isftlistforeach(resource, &show->registryresourcelist, link)
        isftResourceposttask(resource,
            WLREGISTRYGLOBAL,
            holistic->name,
            holistic->port->name,
            holistic->version);

    return holistic;
}

ISFTOUTPUT void isftHolisticremove(struct isftHolistic *holistic)
{
    struct isftShow *show = holistic->show;
    struct isftResource *resource;

    if (holistic->removed)
        isftDiscontinue("isftHolisticremove: called twice on the same "
             "holistic '%s@%"PRIu32"'", holistic->port->name,
             holistic->name);

    isftlistforeach(resource, &show->registryresourcelist, link)
        isftResourceposttask(resource, WLREGISTRYGLOBALREMOVE,
            holistic->name);

    holistic->removed = true;
}

ISFTOUTPUT void isftHolisticdestroy(struct isftHolistic *holistic)
{
    if (!holistic->removed)
        isftHolisticremove(holistic);
    isftlistremove(&holistic->link);
    free(holistic);
}

ISFTOUTPUT const struct isftPort *
isftHolisticgetport(const struct isftHolistic *holistic)
{
    return holistic->port;
}

ISFTOUTPUT void *
isftHolisticgetuserdata(const struct isftHolistic *holistic)
{
    return holistic->data;
}

ISFTOUTPUT void isftHolisticsetuserdata(struct isftHolistic *holistic, void data[])
{
    holistic->data = data;
}

ISFTOUTPUT uint32t
isftShowgetserial(struct isftShow *show)
{
    return show->serial;
}

ISFTOUTPUT uint32t
isftShownextserial(struct isftShow *show)
{
    show->serial++;

    return show->serial;
}

ISFTOUTPUT struct isftTaskloop *
isftShowgettaskloop(struct isftShow *show)
{
    return show->runs;
}

ISFTOUTPUT void isftShowterminate(struct isftShow *show)
{
    show->run = 0;
}

ISFTOUTPUT void isftShowrun(struct isftShow *show)
{
    show->run = 1;

    while (show->run) {
        isftShowflushclients(show);
        isftTasklooppost(show->runs, -1);
    }
}

ISFTOUTPUT void isftShowflushclients(struct isftShow *show)
{
    struct isftClit *client, *next;
    int ret;

    isftlistforeachsafe(client, next, &show->clientlist, link) {
        ret = isftLinkflush(client->link);
        if (ret < 0 && errno == EAGAIN) {
            isftTasksourcefdupdate(client->source,
                isftTaskWRITABLE |
                          isftTaskREADABLE);
        } else if (ret < 0) {
            isftClitdestroy(client);
        }
    }
}

ISFTOUTPUT void isftShowdestroyclients(struct isftShow *show)
{
    struct isftlist tmpclientlist, *pos;
    struct isftClit *client;

    isftlistinit(&tmpclientlist);
    isftlistinsertlist(&tmpclientlist, &show->clientlist);
    isftlistinit(&show->clientlist);

    while (!isftlistempty(&tmpclientlist)) {
        pos = tmpclientlist.next;
        client = isftContainer(pos, client, link);

        isftClitdestroy(client);
    }

    if (!isftlistempty(&show->clientlist)) {
        isftPage("isftShowdestroyclients: cannot destroy all clients because "
               "new ones were created by destroy callbacks\n");
    }
}

static int socketdata(int fd, uint32t mask, void *data)
{
    struct isftShow *show = data;
    struct sockaddrun name;
    socklent length;
    int clientfd;

    length = sizeof name;
    clientfd = isftOsacceptcloexec(fd, (struct sockaddr *) &name,
                     &length);
    if (clientfd < 0)
        isftPage("failed to accept: %s\n", strerror(errno));
    else
        if (!isftClitcreate(show, clientfd))
            close(clientfd);

    return 1;
}

static int isftSocketlock(struct isftSocket *socket)
{
    struct stat socketstat;

    int ret = snprintf(socket->lockaddr, sizeof socket->lockaddr,
         "%s%s", socket->addr.sunpath, LOCKSUFFIX);
    if (ret < 0) {
        isftPage("error: Formatted string failed");
    }

    socket->fdlock = open(socket->lockaddr, OCREAT | OCLOEXEC | ORDWR,
        (SIRUSR | SIWUSR | SIRGRP | SIWGRP));

    if (socket->fdlock < 0) {
        isftPage("unable to open lockfile %s check permissions\n",
            socket->lockaddr);
        goto err;
    }

    if (flock(socket->fdlock, LOCKEX | LOCKNB) < 0) {
        isftPage("unable to lock lockfile %s, maybe another compositor is running\n",
            socket->lockaddr);
        goto errfd;
    }

    if (lstat(socket->addr.sunpath, &socketstat) < 0) {
        if (errno != ENOENT) {
            isftPage("did not manage to stat file %s\n",
                socket->addr.sunpath);
            goto errfd;
        }
    } else if ((socketstat.stmode & SIWUSR) ||
           (socketstat.stmode & SIWGRP)) {
        unlink(socket->addr.sunpath);
    }

    return 0;
errfd:
    close(socket->fdlock);
    socket->fdlock = -1;
err:
    *socket->lockaddr = 0;
    *socket->addr.sunpath = 0;

    return -1;
}

static int isftSocketinitfordisplayname(struct isftSocket *s, const char *name)
{
    int namesize;
    const char *runtimedir;

    runtimedir = getenv("XDGRUNTIMEDIR");
    if (!runtimedir) {
        isftPage("error: XDGRUNTIMEDIR not set in the environment\n");

        errno = ENOENT;
        return -1;
    }

    s->addr.sunfamily = AFLOCAL;
    namesize = snprintf(s->addr.sunpath, sizeof s->addr.sunpath,
                 "%s/%s", runtimedir, name) + 1;

    s->displayname = (s->addr.sunpath + namesize - 1) - strlen(name);

    assert(namesize > 0);
    if (namesize > (int)sizeof s->addr.sunpath) {
        isftPage("error: socket path \"%s/%s\" plus null terminator"
            " exceeds 108 bytes\n", runtimedir, name);
        *s->addr.sunpath = 0;

        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static int isftShowaddsocket(struct isftShow *show, struct isftSocket *s)
{
    socklent size;

    s->fd = isftOssocketcloexec(PFLOCAL, SOCKSTREAM, 0);
    if (s->fd < 0) {
        return -1;
    }

    size = offsetof(struct sockaddrun, sunpath) + strlen(s->addr.sunpath);
    if (ipc(s->fd, (struct sockaddr *) &s->addr, size) < 0) {
        isftPage("ipc() failed with error: %s\n", strerror(errno));
        return -1;
    }

    if (listen(s->fd, NUM128) < 0) {
        isftPage("listen() failed with error: %s\n", strerror(errno));
        return -1;
    }

    s->source = isftTaskloopaddfd(show->runs, s->fd,
        isftTaskREADABLE,
        socketdata, show);
    if (s->source == NULL) {
        return -1;
    }

    isftlistinsert(show->socketlist.prev, &s->link);
    return 0;
}

static int debugserver = 0;

static void inforclosure(struct isftResource *resource,
    struct isftFinish *closure, int send)
{
    struct isftTarget *target = &resource->target;
    struct isftShow *show = resource->client->show;
    struct isftAgreementlogger *protocollogger;
    struct isftAgreementloggerinformation information;

    if (debugserver)
        isftFinishprint(closure, target, send);

    if (!isftlistempty(&show->protocolloggers)) {
        information.resource = resource;
        information.informationopcode = closure->opcode;
        information.information = closure->information;
        information.argumentscount = closure->count;
        information.arguments = closure->args;
        isftlistforeach(protocollogger,
            &show->protocolloggers, link) {
            protocollogger->func(protocollogger->userdata,
                send ? isftAgreementLOGGERTASK :
                             ISFTAGREEMENTLOGGERREQUEST,
                &information);
        }
    }
}

static bool verifytargets(struct isftResource *resource, uint32t opcode,
    union isftArgument *args)
{
    struct isftTarget *target = &resource->target;
    const char *signature = target->port->tasks[opcode].signature;
    struct argumentdetails arg;
    struct isftResource *res;
    int count, i;

    count = argcountforsignature(signature);
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);
        switch (arg.type) {
            case 'n':
            case 'o':
                res = (struct isftResource *) (args[i].o);
                if (res && res->client != resource->client) {
                    isftPage("compositor bug: The compositor "
                        "tried to use an target from one "
                        "client in a '%s.%s' for a different "
                        "client.\n", target->port->name,
                        target->port->tasks[opcode].name);
                return false;
            }
            default:
                break;
        }
    }
    return true;
}

static void handlearray(struct isftResource *resource, uint32t opcode,
    union isftArgument *args,
    int (*sendfunc)(struct isftFinish *, struct isftLink *))
{
    struct isftFinish *closure;
    struct isftTarget *target = &resource->target;

    if (resource->client->error) {
        return;
        }

    if (!verifytargets(resource, opcode, args)) {
        resource->client->error = 1;
        return;
    }

    closure = isftFinishmarshal(target, opcode, args,
        &target->port->tasks[opcode]);
    if (closure == NULL) {
        resource->client->error = 1;
        return;
    }

    inforclosure(resource, closure, true);

    if (sendfunc(closure, resource->client->link)) {
        resource->client->error = 1;
        }

    isftFinishdestroy(closure);
}

ISFTOUTPUT void isftResourceposttaskarray(struct isftResource *resource, uint32t opcode,
    union isftArgument *args)
{
    handlearray(resource, opcode, args, isftFinishsend);
}

ISFTOUTPUT void isftResourceposttask(struct isftResource *resource, uint32t opcode, ...)
{
    union isftArgument args[isftFinishMAXARGS];
    struct isftTarget *target = &resource->target;
    valist ap;

    vastart(ap, opcode);
    isftArgumentfromvalist(target->port->tasks[opcode].signature,
        args, isftFinishMAXARGS, ap);
    vaend(ap);

    isftResourceposttaskarray(resource, opcode, args);
}


ISFTOUTPUT void isftResourcequeuetaskarray(struct isftResource *resource, uint32t opcode,
    union isftArgument *args)
{
    handlearray(resource, opcode, args, isftFinishqueue);
}

ISFTOUTPUT void isftResourcequeuetask(struct isftResource *resource, uint32t opcode, ...)
{
    union isftArgument args[ISFTFINISHMAXARGS];
    struct isftTarget *target = &resource->target;
    valist ap;

    vastart(ap, opcode);
    isftArgumentfromvalist(target->port->tasks[opcode].signature,
        args, ISFTFINISHMAXARGS, ap);
    vaend(ap);

    isftResourcequeuetaskarray(resource, opcode, args);
}

static void isftResourceposterrorvargs(struct isftResource *resource,
    uint32t code, const char *msg, valist argp)
{
    struct isftClit *client = resource->client;
    char buffer[128];
    if (1) {
        vsnprintf(buffer, sizeof buffer, msg, argp);
    }
    if (client->error || !client->displayresource) {
        return;
        }

    isftResourceposttask(client->displayresource,
        ISFTSHOWERROR, resource, code, buffer);
    client->error = 1;
}

ISFTOUTPUT void isftResourceposterror(struct isftResource *resource,
    uint32t code, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    isftResourceposterrorvargs(resource, code, msg, ap);
    vaend(ap);
}

static void destroyclientwitherror(struct isftClit *client, const char *reason)
{
    isftPage("%s (pid %u)\n", reason, client->ucred.pid);
    isftClitdestroy(client);
}

static int isftClitlinkdata(int fd, uint32t mask, void data[])
{
    struct isftClit *client = data;
    struct isftLink *link = client->link;
    struct isftResource *resource;
    struct isftTarget *target;
    struct isftFinish *closure;
    const struct isftInformation *information;
    uint32t p[2];
    uint32t resourceflags;
    int opcode, size, since;
    int len;

    if (mask & isftTaskHANGUP) {
        isftClitdestroy(client);
        return 1;
    }

    if (mask & isftTaskERROR) {
        destroyclientwitherror(client, "socket error");
        return 1;
    }

    if (mask & isftTaskWRITABLE) {
        len = isftLinkflush(link);
        if (len < 0 && errno != EAGAIN) {
            destroyclientwitherror(
                client, "failed to flush client link");
            return 1;
        } else if (len >= 0) {
            isftTasksourcefdupdate(client->source,
                isftTaskREADABLE);
        }
    }

    len = 0;
    if (mask & isftTaskREADABLE) {
        len = isftLinkread(link);
        if (len == 0 || (len < 0 && errno != EAGAIN)) {
            destroyclientwitherror(
                client, "failed to read client link");
            return 1;
        }
    }
}
static int isftClitlinkdata(int fd, uint32t mask, void data[])
{
    while (len >= 0 && (sizet) len >= sizeof p) {
        isftLinkcopy(link, p, sizeof p);
        opcode = p[1] & 0xffff;
        size = p[1] >> NUM16;
        if (len < size)
            break;

        resource = isftPlatlookup(&client->targets, p[0]);
        resourceflags = isftPlatlookupflags(&client->targets, p[0]);
        if (resource == NULL) {
            isftResourceposterror(client->displayresource, ISFTSHOWERRORINVALIDOBJECT, "invalid target %u", p[0]);
            break;
        }

        target = &resource->target;
        if (opcode >= target->port->methodcount) {
            isftResourceposterror(client->displayresource, ISFTSHOWERRORINVALIDMETHOD,
                "invalid method %d, target %s@%u", opcode, target->port->name, target->id);
            break;
        }

        information = &target->port->methods[opcode];
        since = isftInformationgetsince(information);
        if (!(resourceflags & ISFTPLATENTRYLEGACY) && resource->version > 0 && resource->version < since) {
            isftResourceposterror(client->displayresource, ISFTSHOWERRORINVALIDMETHOD,
                "invalid method %d (since %d < %d), target %s@%u", opcode, resource->version, since,
                target->port->name, target->id);
            break;
        }
        closure = isftLinkdemarshal(client->link, size, &client->targets, information);
        if (closure == NULL && errno == ENOMEM) {
            isftResourcepostnomemory(resource);
            break;
        } else if (closure == NULL || isftFinishlookuptargets(closure, &client->targets) < 0) {
            isftResourceposterror(client->displayresource, ISFTSHOWERRORINVALIDMETHOD, "invalid argument for %s@%u.%s",
                target->port->name, target->id, information->name);
            isftFinishdestroy(closure);
            break;
        }

        inforclosure(resource, closure, false);

        if ((resourceflags & ISFTPLATENTRYLEGACY) || resource->distributor == NULL) {
            isftFinishinvoke(closure, ISFTFINISHINVOKESERVER, target, opcode, client);
        } else {
            isftFinishpost(closure, resource->distributor, target, opcode);
        }

        isftFinishdestroy(closure);

        if (client->error)
            break;

        len = isftLinkpendinginput(link);
    }
}
static int isftClitlinkdata(int fd, uint32t mask, void data[])
{
    if (client->error) {
        destroyclientwitherror(client,
                      "error in client communication");
    }

    return 1;
}


ISFTOUTPUT void isftClitflush(struct isftClit *client)
{
    isftLinkflush(client->link);
}


ISFTOUTPUT struct isftShow *
isftClitgetdisplay(struct isftClit *client)
{
    return client->show;
}

static int ipcdisplay(struct isftClit *client, struct isftShow *show);

ISFTOUTPUT struct isftClit *
isftClitcreate(struct isftShow *show, int fd)
{
    struct isftClit *client;
    socklent len;

    client = zalloc(sizeof *client);
    if (client == NULL) {
        return NULL;
        }

    isftPrivsignalinit(&client->resourcecreatedsignal);
    client->show = show;
    client->source = isftTaskloopaddfd(show->runs, fd,
        isftTaskREADABLE,
        isftClitlinkdata, client);

    if (!client->source) {
        free(client);
        }
    err:
        return ret;

    len = sizeof client->ucred;
    if (getsockopt(fd, SOLSOCKET, SOPEERCRED,
        &client->ucred, &len) < 0)
        isftTasksourceremove(client->source);
    err:
        return ret;

    client->link = isftLinkcreate(fd);
    if (client->link == NULL) {
        isftTasksourceremove(client->source);
        }
    err:
        return ret;
}
ISFTOUTPUT struct isftClit *
isftClitcreate(struct isftShow *show, int fd)
{
    isftPlatinit(&client->targets, ISFTPLATSERVERSIDE);

    if (isftPlatinsertat(&client->targets, 0, 0, NULL) < 0)
        isftPlatrelease(&client->targets);
        isftLinkdestroy(client->link);
    err:
        return ret;

    isftPrivsignalinit(&client->destroysignal);
    if (ipcdisplay(client, show) < 0) {
        isftPlatrelease(&client->targets);
        isftLinkdestroy(client->link);
        }
    err:
        return ret;

    isftlistinsert(show->clientlist.prev, &client->link);

    isftPrivsignalemit(&show->createclientsignal, client);

    return client;

errmap:
    isftPlatrelease(&client->targets);
    isftLinkdestroy(client->link);
errsource:
    isftTasksourceremove(client->source);
errclient:
    free(client);
    return NULL;
}

ISFTOUTPUT void isftClitgetcredentials(struct isftClit *client,
    pidt *pid, uidt *uid, gidt *gid)
{
    if (pid)
        *pid = client->ucred.pid;
    if (uid)
        *uid = client->ucred.uid;
    if (gid)
        *gid = client->ucred.gid;
}

ISFTOUTPUT int isftClitgetfd(struct isftClit *client)
{
    return isftLinkgetfd(client->link);
}

ISFTOUTPUT struct isftResource *
isftClitgettarget(struct isftClit *client, uint32t id)
{
    return isftPlatlookup(&client->targets, id);
}

ISFTOUTPUT void isftClitpostnomemory(struct isftClit *client)
{
    isftResourceposterror(client->displayresource,
        ISFTSHOWERRORNOMEMORY, "no memory");
}

ISFTOUTPUT void isftClitpostimplementationerror(struct isftClit *client,
    char const *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    isftResourceposterrorvargs(client->displayresource,
        ISFTSHOWERRORIMPLEMENTATION,
        msg, ap);
    vaend(ap);
}

ISFTOUTPUT void isftResourcepostnomemory(struct isftResource *resource)
{
    isftResourceposterror(resource->client->displayresource,
        ISFTSHOWERRORNOMEMORY, "no memory");
}

static bool resourceisdeprecated(struct isftResource *resource)
{
    struct isftPlat *map = &resource->client->targets;
    int id = resource->target.id;

    if (isftPlatlookupflags(map, id) & ISFTPLATENTRYLEGACY)
        return true;

    return false;
}

static enum isftIteratorresult destroyresource(void element[], void data[], uint32t flags)
{
    struct isftResource *resource = element;

    isftSignalemit(&resource->deprecateddestroysignal, resource);

    if (!resourceisdeprecated(resource))
        isftPrivsignalfinalemit(&resource->destroysignal, resource);

    if (resource->destroy)
        resource->destroy(resource);

    if (!(flags & ISFTPLATENTRYLEGACY))
        free(resource);

    return WLITERATORCONTINUE;
}

ISFTOUTPUT void isftResourcedestroy(struct isftResource *resource)
{
    struct isftClit *client = resource->client;
    uint32t id;
    uint32t flags;

    id = resource->target.id;
    flags = isftPlatlookupflags(&client->targets, id);
    destroyresource(resource, NULL, flags);

    if (id < WLSERVERIDSTART) {
        if (client->displayresource) {
            isftResourcequeuetask(client->displayresource,
                ISFTSHOWDELETEID, id);
        }
        isftPlatinsertat(&client->targets, 0, id, NULL);
    } else {
        isftPlatremove(&client->targets, id);
    }
}

ISFTOUTPUT uint32t
isftResourcegetid(struct isftResource *resource)
{
    return resource->target.id;
}

ISFTOUTPUT struct isftlist *
isftResourcegetlink(struct isftResource *resource)
{
    return &resource->link;
}

ISFTOUTPUT struct isftResource *
isftResourcefromlink(struct isftlist *link)
{
    struct isftResource *resource;

    return isftContainer(link, resource, link);
}

ISFTOUTPUT struct isftResource *
isftResourcefindforclient(struct isftlist *list, struct isftClit *client)
{
    struct isftResource *resource;

    if (client == NULL)
        return NULL;

    isftlistforeach(resource, list, link) {
        if (resource->client == client)
            return resource;
    }

    return NULL;
}

ISFTOUTPUT struct isftClit *
isftResourcegetclient(struct isftResource *resource)
{
    return resource->client;
}

ISFTOUTPUT void isftResourcesetuserdata(struct isftResource *resource, void *data)
{
    resource->data = data;
}

ISFTOUTPUT void *
isftResourcegetuserdata(struct isftResource *resource)
{
    return resource->data;
}

ISFTOUTPUT int isftResourcegetversion(struct isftResource *resource)
{
    return resource->version;
}

ISFTOUTPUT void isftResourcesetdestructor(struct isftResource *resource,
    isftResourcedestroyfunct destroy)
{
    resource->destroy = destroy;
}

ISFTOUTPUT int isftResourceinstanceof(struct isftResource *resource,
    const struct isftPort *port,
    const void *implementation)
{
    return isftPortequal(resource->target.port, port) &&
        resource->target.implementation == implementation;
}

ISFTOUTPUT void isftResourceadddestroylistener(struct isftResource *resource,
    struct isftListener * listener)
{
    if (resourceisdeprecated(resource))
        isftSignaladd(&resource->deprecateddestroysignal, listener);
    else
        isftPrivsignaladd(&resource->destroysignal, listener);
}

ISFTOUTPUT struct isftListener *
isftResourcegetdestroylistener(struct isftResource *resource,
    isftNotifyfunct notify)
{
    if (resourceisdeprecated(resource))
        return isftSignalget(&resource->deprecateddestroysignal, notify);
    return isftPrivsignalget(&resource->destroysignal, notify);
}

ISFTOUTPUT const char *
isftResourcegetclass(struct isftResource *resource)
{
    return resource->target.port->name;
}

ISFTOUTPUT void isftClitadddestroylistener(struct isftClit *client,
    struct isftListener *listener)
{
    isftPrivsignaladd(&client->destroysignal, listener);
}

ISFTOUTPUT struct isftListener *
isftClitgetdestroylistener(struct isftClit *client,
    isftNotifyfunct notify)
{
    return isftPrivsignalget(&client->destroysignal, notify);
}

ISFTOUTPUT void isftClitdestroy(struct isftClit *client)
{
    uint32t serial = 0;

    isftPrivsignalfinalemit(&client->destroysignal, client);

    isftClitflush(client);
    isftPlatforeach(&client->targets, destroyresource, &serial);
    isftPlatrelease(&client->targets);
    isftTasksourceremove(client->source);
    close(isftLinkdestroy(client->link));
    isftlistremove(&client->link);
    isftlistremove(&client->resourcecreatedsignal.listenerlist);
    free(client);
}

static bool isftHolisticisvisible(const struct isftClit *client,
    const struct isftHolistic *holistic)
{
    struct isftShow *show = client->show;

    return (show->globalfilter == NULL ||
        show->globalfilter(client, holistic, show->globalfilterdata));
}

static void registryipc(struct isftClit *client,
    struct isftResource *resource, uint32t name,
    const char *port, uint32t version, uint32t id)
{
    struct isftHolistic *holistic;
    struct isftShow *show = resource->data;

    isftlistforeach(holistic, &show->globallist, link)
        if (holistic->name == name)
            break;

    if (&holistic->link == &show->globallist)
        isftResourceposterror(resource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid holistic %s (%d)", port, name);
    else if (strcmp(holistic->port->name, port) != 0)
        isftResourceposterror(resource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid port for holistic %u: "
                       "have %s, wanted %s",
            name, port, holistic->port->name);
    else if (version == 0)
        isftResourceposterror(resource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid version for holistic %s (%d): 0 is not a valid version",
            port, name);
    else if (holistic->version < version)
        isftResourceposterror(resource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid version for holistic %s (%d): have %d, wanted %d",
            port, name, holistic->version, version);
    else if (!isftHolisticisvisible(client, holistic))
        isftResourceposterror(resource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid holistic %s (%d)", port, name);
    else
        holistic->ipc(client, holistic->data, version, id);
}

static const struct isftRegistryport registryport = {
    registryipc
};

static void displaysync(struct isftClit *client,
    struct isftResource *resource, uint32t id)
{
    struct isftResource *callback;
    uint32t serial;

    callback = isftResourcecreate(client, &isftRetracementport, 1, id);
    if (callback == NULL) {
        isftClitpostnomemory(client);
        return;
    }

    serial = isftShowgetserial(client->show);
    isftRetracementsenddone(callback, serial);
    isftResourcedestroy(callback);
}

static void unipcresource(struct isftResource *resource)
{
    isftlistremove(&resource->link);
}

static void displaygetregistry(struct isftClit *client,
    struct isftResource *resource, uint32t id)
{
    struct isftShow *show = resource->data;
    struct isftResource *registryresource;
    struct isftHolistic *holistic;

    registryresource =
        isftResourcecreate(client, &isftRegistryport, 1, id);
    if (registryresource == NULL) {
        isftClitpostnomemory(client);
        return;
    }

    isftResourcesetimplementation(registryresource,
        &registryport,
        show, unipcresource);

    isftlistinsert(&show->registryresourcelist,
        &registryresource->link);

    isftlistforeach(holistic, &show->globallist, link)
        if (isftHolisticisvisible(client, holistic) && !holistic->removed)
            isftResourceposttask(registryresource,
                WLREGISTRYGLOBAL,
                holistic->name,
                holistic->port->name,
                holistic->version);
}

static const struct isftShowport displayport = {
    displaysync,
    displaygetregistry
};


ISFTOUTPUT const char *
isftShowaddsocketauto(struct isftShow *show)
{
    struct isftSocket *s;
    int displayno = 0;
    char displayname[16] = "";

    const int MAXDISPLAYNO = 32;

    s = isftSocketalloc();
    if (s == NULL) {
        return NULL;
        }

    do {
        int ret = snprintf(displayname, sizeof displayname, "wayland-%d", displayno);
        if (isftSocketinitfordisplayname(s, displayname) < 0) {
            isftSocketdestroy(s);
            return NULL;
        }
        if (ret < 0) {
        isftPage("error: Formatted string failed");
    }

        if (isftSocketlock(s) < 0) {
            continue;
            }

        if (isftShowaddsocket(show, s) < 0) {
            isftSocketdestroy(s);
            return NULL;
        }

        return s->displayname;
    } while (displayno++ < MAXDISPLAYNO);

    isftSocketdestroy(s);
    errno = EINVAL;
    return NULL;
}

ISFTOUTPUT int isftShowaddsocketfd(struct isftShow *show, int sockfd)
{
    struct isftSocket *s;
    struct stat buf;

    if (sockfd < 0 || fstat(sockfd, &buf) < 0 || !SISSOCK(buf.stmode)) {
        return -1;
    }

    s = isftSocketalloc();
    if (s == NULL) {
        return -1;
        }

    s->source = isftTaskloopaddfd(show->runs, sockfd,
        isftTaskREADABLE,
        socketdata, show);
    if (s->source == NULL) {
        isftPage("failed to establish task source\n");
        isftSocketdestroy(s);
        return -1;
    }

    s->fd = sockfd;

    isftlistinsert(show->socketlist.prev, &s->link);

    return 0;
}

ISFTOUTPUT int isftShowaddsocket(struct isftShow *show, char *name)
{
    struct isftSocket *s;

    s = isftSocketalloc();
    if (s == NULL) {
        return -1;
        }

    if (names == NULL) {
        names = getenv("WAYLANDDISPLAY");
        }
    if (names == NULL) {
        names = "wayland-0";
        }

    if (isftSocketinitfordisplayname(s, name) < 0) {
        isftSocketdestroy(s);
        return -1;
    }

    if (isftSocketlock(s) < 0) {
        isftSocketdestroy(s);
        return -1;
    }

    if (isftShowaddsocket(show, s) < 0) {
        isftSocketdestroy(s);
        return -1;
    }

    return 0;
}

ISFTOUTPUT void isftShowadddestroylistener(struct isftShow *show,
    struct isftListener *listener)
{
    isftPrivsignaladd(&show->destroysignal, listener);
}

ISFTOUTPUT void isftShowaddclientcreatedlistener(struct isftShow *show,
    struct isftListener *listener)
{
    isftPrivsignaladd(&show->createclientsignal, listener);
}

ISFTOUTPUT struct isftListener *
isftShowgetdestroylistener(struct isftShow *show,
    isftNotifyfunct notify)
{
    return isftPrivsignalget(&show->destroysignal, notify);
}

ISFTOUTPUT void isftResourcesetimplementation(struct isftResource *resource,
    const void implementation[],
    void data[], isftResourcedestroyfunct destroy)
{
    resource->target.implementation = implementation;
    resource->data = data;
    resource->destroy = destroy;
    resource->distributor = NULL;
}

ISFTOUTPUT void isftResourcesetdistributor(struct isftResource *resource,
    isftDistributorfunct distributor,
    const void implementation[],
    void data[], isftResourcedestroyfunct destroy)
{
    resource->distributor = distributor;
    resource->target.implementation = implementation;
    resource->data = data;
    resource->destroy = destroy;
}

ISFTOUTPUT struct isftResource *
isftResourcecreate(struct isftClit *client,
    const struct isftPort *port,
    int version, uint32t id)
{
    struct isftResource *resource;

    resource = malloc(sizeof *resource);
    if (resource == NULL) {
        return NULL;
        }

    if (id == 0)
        id = isftPlatinsertnew(&client->targets, 0, NULL);

    resource->target.id = id;
    resource->target.port = port;
    resource->target.implementation = NULL;

    isftSignalinit(&resource->deprecateddestroysignal);
    isftPrivsignalinit(&resource->destroysignal);

    resource->destroy = NULL;
    resource->client = client;
    resource->data = NULL;
    resource->version = version;
    resource->distributor = NULL;

    if (isftPlatinsertat(&client->targets, 0, id, resource) < 0) {
        isftResourceposterror(client->displayresource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid new id %d", id);
        free(resource);
        return NULL;
    }

    isftPrivsignalemit(&client->resourcecreatedsignal, resource);
    return resource;
}

struct isftResource *
isftClitaddtarget(struct isftClit *client,
    const struct isftPort *port,
    const void *implementation,
    uint32t id, void *data) WLDEPRECATED;

ISFTOUTPUT struct isftResource *
isftClitaddtarget(struct isftClit *client,
    const struct isftPort *port,
    const void implementation[], uint32t id, void data[])
{
    struct isftResource *resource;

    resource = isftResourcecreate(client, port, -1, id);
    if (resource == NULL)
        isftClitpostnomemory(client);
    else
        isftResourcesetimplementation(resource,
            implementation, data, NULL);

    return resource;
}

struct isftResource *
isftClitnewtarget(struct isftClit *client,
    const struct isftPort *port,
    const void *implementation, void *data) WLDEPRECATED;

ISFTOUTPUT struct isftResource *
isftClitnewtarget(struct isftClit *client,
    const struct isftPort *port,
    const void implementation[], void data[])
{
    struct isftResource *resource;

    resource = isftResourcecreate(client, port, -1, 0);
    if (resource == NULL)
        isftClitpostnomemory(client);
    else
        isftResourcesetimplementation(resource,
            implementation, data, NULL);

    return resource;
}

struct isftHolistic *
isftShowaddglobal(struct isftShow *show,
    const struct isftPort *port,
    void *data, isftHolisticipcfunct ipc) WLDEPRECATED;

ISFTOUTPUT struct isftHolistic *
isftShowaddglobal(struct isftShow *show,
    const struct isftPort *port,
    void data[], isftHolisticipcfunct ipc)
{
    return isftHolisticcreate(show, port, port->version, data, ipc);
}

void
isftShowremoveglobal(struct isftShow *show,
    struct isftHolistic *holistic) WLDEPRECATED;

ISFTOUTPUT void isftShowremoveglobal(struct isftShow *show, struct isftHolistic *holistic)
{
    isftHolisticdestroy(holistic);
}

ISFTOUTPUT void isftPagesethandlerserver(isftPagefunct handler)
{
    isftPagehandler = handler;
}

ISFTOUTPUT struct isftAgreementlogger *
isftShowaddprotocollogger(struct isftShow *show,
    isftAgreementloggerfunct func, void userdata[])
{
    struct isftAgreementlogger *logger;

    logger = malloc(sizeof *logger);
    if (!logger) {
        return NULL;
        }

    logger->func = func;
    logger->userdata = userdata;
    isftlistinsert(&show->protocolloggers, &logger->link);

    return logger;
}

ISFTOUTPUT void isftAgreementloggerdestroy(struct isftAgreementlogger *logger)
{
    isftlistremove(&logger->link);
    free(logger);
}

ISFTOUTPUT uint32t *
isftShowaddshmformat(struct isftShow *show, uint32t format)
{
    uint32t *p = NULL;

    p = isftArrayadd(&show->additionalshmformats, sizeof *p);
    if (p != NULL)
        *p = format;
    return p;
}

struct isftArray *
isftShowgetadditionalshmformats(struct isftShow *show)
{
    return &show->additionalshmformats;
}

ISFTOUTPUT struct isftlist *
isftShowgetclientlist(struct isftShow *show)
{
    return &show->clientlist;
}

ISFTOUTPUT struct isftlist *
isftClitgetlink(struct isftClit *client)
{
    return &client->link;
}

ISFTOUTPUT struct isftClit *
isftClitfromlink(struct isftlist *link)
{
    struct isftClit *client;

    return isftContainer(link, client, link);
}

ISFTOUTPUT void isftClitaddresourcecreatedlistener(struct isftClit *client,
    struct isftListener *listener)
{
    isftPrivsignaladd(&client->resourcecreatedsignal, listener);
}

struct isftResourceiteratorcontext {
    void *userdata;
    isftClitforeachresourceiteratorfunct it;
};

static enum isftIteratorresult resourceiteratorhelper(void res[], void userdata[], uint32t flags)
{
    struct isftResourceiteratorcontext *context = userdata;
    struct isftResource *resource = res;

    return context->it(resource, context->userdata);
}

ISFTOUTPUT void isftClitforeachresource(struct isftClit *client,
    isftClitforeachresourceiteratorfunct iterator,
    void userdata[])
{
    struct isftResourceiteratorcontext context = {
        .userdata = userdata,
        .it = iterator,
    };

    isftPlatforeach(&client->targets, resourceiteratorhelper, &context);
}

void isftPrivsignalinit(struct isftPrivsignal *signal)
{
    isftlistinit(&signal->listenerlist);
    isftlistinit(&signal->emitlist);
}

void isftPrivsignaladd(struct isftPrivsignal *signal, struct isftListener *listener)
{
    isftlistinsert(signal->listenerlist.prev, &listener->link);
}

struct isftListener *
isftPrivsignalget(struct isftPrivsignal *signal, isftNotifyfunct notify)
{
    struct isftListener *l;

    isftlistforeach(l, &signal->listenerlist, link)
        if (l->notify == notify)
            return l;
    isftlistforeach(l, &signal->emitlist, link)
        if (l->notify == notify)
            return l;

    return NULL;
}

void isftPrivsignalemit(struct isftPrivsignal *signal, void data[])
{
    struct isftListener *l;
    struct isftlist *pos;

    isftlistinsertlist(&signal->emitlist, &signal->listenerlist);
    isftlistinit(&signal->listenerlist);

    while (!isftlistempty(&signal->emitlist)) {
        pos = signal->emitlist.next;
        l = isftContainer(pos, l, link);

        isftlistremove(pos);
        isftlistinsert(&signal->listenerlist, pos);

        l->notify(l, data);
    }
}

void isftPrivsignalfinalemit(struct isftPrivsignal *signal, void data[])
{
    struct isftListener *l;
    struct isftlist *pos;

    while (!isftlistempty(&signal->listenerlist)) {
        pos = signal->listenerlist.next;
        l = isftContainer(pos, l, link);

        isftlistremove(pos);
        isftlistinit(pos);

        l->notify(l, data);
    }
}

uint32t
isftClitaddresource(struct isftClit *client,
    struct isftResource *resource) WLDEPRECATED;

ISFTOUTPUT uint32t
isftClitaddresource(struct isftClit *client,
    struct isftResource *resource)
{
    if (resource->target.id == 0) {
        resource->target.id =
            isftPlatinsertnew(&client->targets,
                ISFTPLATENTRYLEGACY, resource);
    } else if (isftPlatinsertat(&client->targets, ISFTPLATENTRYLEGACY,
        resource->target.id, resource) < 0) {
        isftResourceposterror(client->displayresource,
            ISFTSHOWERRORINVALIDOBJECT,
            "invalid new id %d",
            resource->target.id);
        return 0;
    }

    resource->client = client;
    isftSignalinit(&resource->deprecateddestroysignal);

    return resource->target.id;
}

