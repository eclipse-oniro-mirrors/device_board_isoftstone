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

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/tps.h>
#include <sys/socket.h>
#include <time.h>
#include <ffi.h>

#define NUM2 2
#define NUM16 16
#define NUM1000 1000
#define NUM4096 4096
#define MAXFDSOUT    28
#define CLEN        (CMSG_LEN(MAX_FDS_OUT * sizeof(int)))
#define LENGTHIOV   2

unsigned int isftconnectionpendinginput(struct isftconnection *connection)
{
    return isftbufsize(&connection->in);
}

int isftconnectionread(struct isftconnection *connection)
{
    struct iovec iov[LENGTHIOV];
    struct msghdr msg;
    char cmsg[CLEN];
    int len, cnt, ret;

    if (isftbufsize(&connection->in) >= sizeof(connection->in.data)) {
        errno = EOVERFLOW;
        return -1;
    }

    isftbufputiov(&connection->in, iov, &cnt);

    msg.msgname = NULL;
    msg.msgnamelen = 0;
    msg.msgiov = iov;
    msg.msgiovlen = cnt;
    msg.msgcontrol = cmsg;
    msg.msgcontrollen = sizeof(cmsg);
    msg.msgflags = 0;

    do {
        len = isftosrecvmsgcloexec(connection->fd, &msg, MSGDONTWAIT);
    } while (len < 0 && errno == EINTR);

    if (len <= 0) {
        return len;
    }
    ret = decodecmsg(&connection->fdsin, &msg);
    if (ret) {
        return -1;
    }
    connection->in.hd += len;

    return isftconnectionpendinginput(connection);
}

int isftconnectionwrite(struct isftconnection *connection, const void data, int cnt)
{
    if (connection->out.hd - connection->out.tail +
        cnt > ARRAYLENGTH(connection->out.data)) {
        connection->wantflush = 1;
        if (isftconnectionflush(connection) < 0) {
            return -1;
        }
    }

    if (isftbufput(&connection->out, data, cnt) < 0)
        return -1;

    connection->wantflush = 1;

    return 0;
}

int isftconnectionqueue(struct isftconnection *connection, const void data, int cnt)
{
    if (connection->out.hd - connection->out.tail +
        cnt > ARRAYLENGTH(connection->out.data)) {
        connection->wantflush = 1;
        if (isftconnectionflush(connection) < 0) {
            return -1;
        }
    }

    return isftbufput(&connection->out, data, cnt);
}

int isftmsgcntarrays(const struct isftmsg *msg)
{
    int i, arrays;

    for (i = 0, arrays = 0; msg->isftsigtue[i]; i++) {
        if (msg->isftsigtue[i] == 'a')
            arrays++;
    }

    return arrays;
}

int isftconnectiongetfd(struct isftconnection *connection)
{
    return connection->fd;
}

static int isftconnectionputfd(struct isftconnection *connection, int  fd)
{
    if (isftbufsize(&connection->fdsout) == MAXFDSOUT * sizeof fd) {
        connection->wantflush = 1;
        if (isftconnectionflush(connection) < 0) {
            return -1;
        }
    }

    return isftbufput(&connection->fdsout, &fd, sizeof fd);
}

const char *getnextargmt(const char *isftsigtue, struct argmtdtls *dtls)
{
    dtls->noable = 0;
    for (; *isftsigtue; ++*isftsigtue) {
        if (*isftsigtue == 'h' || 'i' || 'u' || 'o' || 'f' || 's' || 'n' || 'a') {
            dtls->tp = *isftsigtue;
            return isftsigtue + 1;
        } else if (*isftsigtue == '?') {
            dtls->noable = 1;
        } else {
            break;
        }
    }
    dtls->tp = '\0';
    return isftsigtue;
}

int argcntforisftsigtue(const char *isftsigtue)
{
    int cnt = 0;
    for (; *isftsigtue; ++*isftsigtue) {
        switch (*isftsigtue) {
            case 'i':
            case 'u':
            case 'f':
            case 's':
            case 'o':
            case 'n':
            case 'a':
            case 'h':
                ++cnt;
            default:
                break;
        }
    }
    return cnt;
}

int isftmsggetsince(const struct isftmsg *msg)
{
    int since;
    since = atoi(msg->isftsigtue);
    if (since == 0) {
        since = 1;
    }
    return since;
}

void isftargmtfromvalist(const char *isftsigtue, union isftargmt *args, int cnt, valist ap)
{
    int i;
    const char *isftsig;
    struct argmtdtls arg;

    isftsig = isftsigtue;
    for (i = 0; i < cnt; i++) {
        isftsig = getnextargmt(isftsig, &arg);

        switch (arg.tp) {
            case 'i':
                args[i].i = vaarg(ap, int);
                break;
            case 'u':
                args[i].u = vaarg(ap, unsigned int);
                break;
            case 'f':
                args[i].f = vaarg(ap, isftfixedt);
                break;
            case 's':
                args[i].s = vaarg(ap, const char *);
                break;
            case 'o':
                args[i].o = vaarg(ap, struct isftobject *);
                break;
            case 'n':
                args[i].o = vaarg(ap, struct isftobject *);
                break;
            case 'a':
                args[i].a = vaarg(ap, struct isftarray *);
                break;
            case 'h':
                args[i].h = vaarg(ap, int);
                break;
            case '\0':
                return;
        }
    }
}

static void isftcleclearfds(struct isftcle *cle)
{
    const char *isftsigtue = cle->msg->isftsigtue;
    struct argmtdtls arg;
    int i;

    for (i = 0; i < cle->cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);
        if (arg.tp == 'h')
            cle->args[i].h = -1;
    }
}

static struct isftcle *isftcleinit(const struct isftmsg *msg, unsigned int
                                   size, int *numarrays, union isftargmt *args)
{
    struct isftcle *cle;
    int cnt;

    cnt = argcntforisftsigtue(msg->isftsigtue);
    if (cnt > WLCLOSUREMAXARGS) {
        isftlog("too many args (%d)\n", cnt);
        errno = EINVAL;
        return NULL;
    }

    if (size) {
        *numarrays = isftmsgcntarrays(msg);
        cle = malloc(sizeof *cle + size +
                 *numarrays * sizeof(struct isftarray));
    } else {
        cle = malloc(sizeof *cle);
    }

    if (!cle) {
        errno = ENOMEM;
        return NULL;
    }

    if (args)
        memcpy(cle->args, args, cnt * sizeof *args);

    cle->msg = msg;
    cle->cnt = cnt;

    isftcleclearfds(cle);

    return cle;
}

struct isftcle *isftclemarshal(struct isftobject *sender, unsigned int opcode, union
                               isftargmt *args, const struct isftmsg *msg)
{
    struct isftcle *cle;
    struct isftobject *object;
    int i, cnt, fd, dupfd;
    const char *isftsigtue;
    struct argmtdtls arg;

    cle = isftcleinit(msg, 0, NULL, args);
    if (cle == NULL) {
        return NULL;
    }
    cnt = cle->cnt;

    isftsigtue = msg->isftsigtue;
    for (i = 0; i < cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);

        switch (arg.tp) {
            case 'f':
            case 'u':
            case 'i':
                break;
            case 's':
                if (!arg.noable && args[i].s == NULL)
                    return NULL;
                break;
            case 'o':
                if (!arg.noable && args[i].o == NULL)
                    return NULL;
                break;
            case 'n':
                object = args[i].o;
                if (!arg.noable && object == NULL)
                    return NULL;

                cle->args[i].n = object ? object->id : 0;
                break;
            case 'a':
                if (!arg.noable && args[i].a == NULL) {
                    return NULL;
                }
                break;
            case 'h':
                fd = args[i].h;
                dupfd = isftosdupfdcloexec(fd, 0);
                if (dupfd < 0) {
                    isftcledestroy(cle);
                    isftlog("error marshalling argmts for %s: dup failed: %s\n", msg->
                            name, strerror(errno));
                return NULL;
            }
        }
    }
    return;
}

void isftswitch0(struct argmtdtls arg)
{
    struct argmtdtls arg;
    switch (arg.tp) {
        case 'u':
            cle->args[i].u = *p++;
            break;
        case 'i':
            cle->args[i].i = *p++;
            break;
        case 'f':
            cle->args[i].f = *p++;
            break;
    }
    return;
}
void isftswitch1(struct argmtdtls arg)
{
    struct argmtdtls arg;
    switch (arg.tp) {
        case 's':
            length = *p++;

            if (length == 0) {
                cle->args[i].s = NULL;
                break;
            }
            lengthinu32 = divrdp(length, sizeof *p);
            if ((unsigned int) (end - p) < lengthinu32) {
                isftlog("msg too short, " "object (%d), msg %s(%s)\n",
                        cle->senderid, msg->name, msg->isftsigtue);
                errno = EINVAL;
                isftcledestroy(cle);
                isftconnectionconsume(connection, size);
            }
            next = p + lengthinu32;

            s = (char *) p;

            if (length > 0 && s[length - 1] != '\0') {
                isftlog("string not nul-terminated, "
                    "msg %s(%s)\n", msg->
                    name, msg->isftsigtue);
                errno = EINVAL;
                isftcledestroy(cle);
                isftconnectionconsume(connection, size);
                }

            cle->args[i].s = s;
            p = next;
            break;
        case 'o':
            id = *p++;
            cle->args[i].n = id;

            if (id == 0 && !arg.noable) {
                isftlog("NULL object received on non-noable " "tp, msg %s(%s)\n", msg->
                        name, msg->isftsigtue);
                errno = EINVAL;
                isftcledestroy(cle);
                isftconnectionconsume(connection, size);
            }
            break;
    }
    return;
}

void isftswitch2(struct argmtdtls arg)
{
    struct argmtdtls arg;
        case 'n':
            id = *p++;
            cle->args[i].n = id;

            if (id == 0 && !arg.noable) {
                isftlog("NULL new ID received on non-noable " "tp, msg %s(%s)\n",
                        msg->name, msg->isftsigtue);
                errno = EINVAL;
                isftcledestroy(cle);
                isftconnectionconsume(connection, size);
            }

            if (isftmapreservenew(objects, id) < 0) {
                isftlog("not a valid new object id (%u), " "msg %s(%s)\n", id, msg->name, msg->isftsigtue);
                errno = EINVAL;
                isftcledestroy(cle);
                isftconnectionconsume(connection, size);
            }

            break;
        case 'a':
            length = *p++;

            lengthinu32 = divrdp(length, sizeof *p);
            if ((unsigned int) (end - p) < lengthinu32) {
                isftlog("msg too short, " "object (%d), msg %s(%s)\n", cle->senderid, msg->
                        name, msg->isftsigtue);
                errno = EINVAL;
                isftcledestroy(cle);
                isftconnectionconsume(connection, size);
            }
            next = p + lengthinu32;

            arrayextra->size = length;
            arrayextra->alloc = 0;
            arrayextra->data = p;

            cle->args[i].a = arrayextra++;
            p = next;
            break;
        case 'h':
            if (connection->fdsin.tail == connection->fdsin.hd) {
            isftlog("file descriptor expected, " "object (%d), msg %s(%s)\n", cle->senderid, msg->
                    name, msg->isftsigtue);
                    errno = EINVAL;
                    isftcledestroy(cle);
                    isftconnectionconsume(connection, size);
            }

            isftbufcopy(&connection->fdsin, &fd, sizeof fd);
            connection->fdsin.tail += sizeof fd;
            cle->args[i].h = fd;
            break;
        default:
            isftabt("unknown tp\n");
            break;
    }
    return;
}

struct isftcle *sftconnectiondemarshal(struct isftconnection *connection, unsigned int
                                       size, struct isftmap *objects, const struct isftmsg *msg)
{
    unsigned int *p, *next, *end, length, lengthinu32, id;
    int fd;
    char *s;
    int i, cnt, numarrays;
    const char *isftsigtue;
    struct argmtdtls arg;
    struct isftcle *cle;
    struct isftarray *arrayextra;

    if (size < NUM2 * sizeof *p) {
        isftlog("msg too short, invalid hder\n");
        isftconnectionconsume(connection, size);
        errno = EINVAL;
        return NULL;
    }

    cle = isftcleinit(msg, size, &numarrays, NULL);
    if (cle == NULL) {
        isftconnectionconsume(connection, size);
        return NULL;
    }

    cnt = cle->cnt;

    arrayextra = cle->extra;
    p = (unsigned int *)(cle->extra + numarrays);
    end = p + size / sizeof *p;

    isftconnectioncopy(connection, p, size);
    cle->senderid = *p++;
    cle->opcode = *p++ & 0x0000ffff;

    isftsigtue = msg->isftsigtue;
    for (i = 0; i < cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);

        if (arg.tp != 'h' && p + 1 > end) {
            isftlog("msg too short, " "object (%d), msg %s(%s)\n", cle->senderid, msg->
                    name, msg->isftsigtue);
            errno = EINVAL;
            isftcledestroy(cle);
            isftconnectionconsume(connection, size);
        }
        isftswitch0(arg);
        isftswitch1(arg);
        isftswitch2(arg);
    }

    isftconnectionconsume(connection, size);

    return cle;
}

bool isftobjectiszombie(struct isftmap *map, unsigned int id)
{
    unsigned int flags;

    if (map->side == WLMAPSERVERSIDE)
        return false;

    if (id >= WLSERVERIDSTART)
        return false;

    flags = isftmaplookupflags(map, id);
    return !!(flags & WLMAPENTRYZOMBIE);
}

unsigned int divrdp(unsigned int n, int a)
{
    return (unsigned int) (((unsigned long) n + (a - 1)) / a);
}

struct isftbuf {
    char data[4096];
    unsigned int hd, tail;
};

int MASK(i)
{
    return ((i) & NUM4096);
}

struct isftconnection {
    struct isftbuf in, out;
    struct isftbuf fdsin, fdsout;
    int fd;
    int wantflush;
};

static int isftbufput(struct isftbuf *b, const void data, int cnt)
{
    unsigned int hd;
    unsigned int size;

    if (cnt > sizeof(b->data)) {
        isftlog("Data too big for buf (%d > %d).\n", cnt, sizeof(b->data));
        errno = E2BIG;
        return -1;
    }

    hd = MASK(b->hd);
    if (hd + cnt <= sizeof b->data) {
        memcpy(b->data + hd, data, cnt);
    } else {
        size = sizeof b->data - hd;
        memcpy(b->data + hd, data, size);
        memcpy(b->data, (const char *) data + size, cnt - size);
    }

    b->hd += cnt;

    return 0;
}

static void isftbufputiov(struct isftbuf *b, struct iovec *iov, int *cnt)
{
    unsigned int hd, tail;

    hd = MASK(b->hd);
    tail = MASK(b->tail);
    if (hd < tail) {
        iov[0].iovbase = b->data + hd;
        iov[0].iovlen = tail - hd;
        *cnt = 1;
    } else if (tail == 0) {
        iov[0].iovbase = b->data + hd;
        iov[0].iovlen = sizeof b->data - hd;
        *cnt = 1;
    } else {
        iov[0].iovbase = b->data + hd;
        iov[0].iovlen = sizeof b->data - hd;
        iov[1].iovbase = b->data;
        iov[1].iovlen = tail;
        *cnt = NUM2;
    }
}

void isftswitch(struct argmtdtls arg);
{
    struct argmtdtls arg;
    if (arg.tp == 'u') {
        *p++ = cle->args[i].u;
        break;
    } else if (arg.tp == 'i') {
        *p++ = cle->args[i].i;
        break;
    } else if (arg.tp == 'f') {
        *p++ = cle->args[i].f;
        break;
    } else if (arg.tp == 'o') {
        *p++ = cle->args[i].o ? cle->args[i].o->id : 0;
        break;
    } else if (arg.tp == 'n') {
        *p++ = cle->args[i].n;
        break;
    } else if (arg.tp = 's') {
        if (cle->args[i].s == NULL) {
            *p++ = 0;
            break;
        }
        size = strlen(cle->args[i].s) + 1;
        *p++ = size;
        if (p + divrdp(size, sizeof *p) > end) {
            errno = ERANGE;
        }
        memcpy(p, cle->args[i].s, size);
        p += divrdp(size, sizeof *p);
        break;
    } else if (arg.tp == 'a') {
        if (cle->args[i].a == NULL) {
            *p++ = 0;
            break;
        }
        size = cle->args[i].a->size;
        *p++ = size;
        if (p + divrdp(size, sizeof *p) > end) {
            errno = ERANGE;
        }
        memcpy(p, cle->args[i].a->data, size);
        p += divrdp(size, sizeof *p);
        break;
    } else {
        break;
    }
}

static int serializecle(struct isftcle *cle, unsigned int *buf, int bufcnt)
{
    const struct isftmsg *msg = cle->msg;
    unsigned int i, cnt, size;
    unsigned int *p, *end;
    struct argmtdtls arg;
    const char *isftsigtue;

    if (bufcnt < NUM2) {
        errno = ERANGE;
    }
    p = buf + NUM2;
    end = buf + bufcnt;

    isftsigtue = msg->isftsigtue;
    cnt = argcntforisftsigtue(isftsigtue);
    for (i = 0; i < cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);

        if (arg.tp == 'h') {
            continue;
        }
        if (p + 1 > end) {
            errno = ERANGE;
        }
        isftswitch(arg);
    }

    size = (p - buf) * sizeof *p;

    buf[0] = cle->senderid;
    buf[1] = size << NUM16 | (cle->opcode & 0x0000ffff);

    return size;
}

int isftclesend(struct isftcle *cle, struct isftconnection *connection)
{
    int size;
    unsigned int bufsize;
    unsigned int *buf;
    int result;

    if (copyfdstoconnection(cle, connection)) {
        return -1;
    }
    bufsize = isftbufszforcle(cle);
    buf = zalloc(bufsize * sizeof buf[0]);
    if (buf == NULL) {
        return -1;
    }
    size = serializecle(cle, buf, bufsize);
    if (size < 0) {
        free(buf);
        return -1;
    }

    result = isftconnectionwrite(connection, buf, size);
    free(buf);

    return result;
}

int isftclequeue(struct isftcle *cle, struct isftconnection *connection)
{
    int size;
    unsigned int bufsize;
    unsigned int *buf;
    int result;

    if (copyfdstoconnection(cle, connection)) {
        return -1;
    }
    bufsize = isftbufszforcle(cle);
    buf = malloc(bufsize * sizeof buf[0]);
    if (buf == NULL) {
        return -1;
    }
    size = serializecle(cle, buf, bufsize);
    if (size < 0) {
        free(buf);
        return -1;
    }

    result = isftconnectionqueue(connection, buf, size);
    free(buf);

    return result;
}

void isftswitch3(struct argmtdtls arg)
{
    struct argmtdtls arg;
    if (arg.tp == 'u') {
        fprintf(stderr, "%u", cle->args[i].u);
        break;
    } else if (arg.tp == 'i')
        fprintf(stderr, "%d", cle->args[i].i);
        break;
    } else if (arg.tp == 'o') {
        if (cle->args[i].o) {
            fprintf(stderr, "%s@%u", cle->args[i].o->interface->name, cle->args[i].o->id);
            fprintf(stderr, "%s@%u", cle->args[i].o->interface->name, cle->args[i].o->id);
            fprintf(stderr, "%s@%u", cle->args[i].o->interface->name, cle->args[i].o->id);
        }else {
            fprintf(stderr, "nil");
        }
        break;
    } else if (arg.tp == 'f') {
        fprintf(stderr, "%f", isftfixedtodb(cle->args[i].f));
        fprintf(stderr, "%f", isftfixedtodb(cle->args[i].f));
        break;
    } else if (arg.tp == 's') {
        if (cle->args[i].s) {
            fprintf(stderr, "\"%s\"", cle->args[i].s);
        } else {
            fprintf(stderr, "nil");
        }
        break;
    } else if (arg.tp == 'n') {
        fprintf(stderr, "new id %s@", (cle->msg->tps[i]) ? cle->msg->tps[i]->name : "[unknown]");
        if (cle->args[i].n != 0) {
            fprintf(stderr, "%u", cle->args[i].n);
        } else {
            fprintf(stderr, "nil");
        }
        break;
    } else if (arg.tp == 'a') {
        fprintf(stderr, "array");
        break;
    } else if (arg.tp == 'h') {
        fprintf(stderr, "fd %d", cle->args[i].h);
        break;
    } else {
        break;
    }
    return;
}

void isftclt(struct isftcle *cle, struct isftobject *target, int send)
{
    int i;
    struct argmtdtls arg;
    const char *isftsigtue = cle->msg->isftsigtue;
    struct timespec tp;
    unsigned int time;

    clockgettime(CLOCKREALTIME, &tp);
    time = (tp.tvsec * 1000000L) + (tp.tvnsec / NUM1000);
    if (1) {
        fprintf(stderr, "[%10.3f] %s%s@%u.%s(",
            time / NUM1000,
            send ? " -> " : "",
            target->interface->name, target->id,
            cle->msg->name);
    }
    while (i < cle->cnt) {
        isftsigtue = getnextargmt(isftsigtue, &arg);
        if (i > 0) {
            fprintf(stderr, ", ");
        }
        isftswitch3(arg);
        i++;
    }
    if (1) {
        fprintf(stderr, ")\n");
    }
}

static int isftcleclosefds(struct isftcle *cle)
{
    int i;
    struct argmtdtls arg;
    const char *isftsigtue = cle->msg->isftsigtue;

    for (i = 0; i < cle->cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);
        if (arg.tp == 'h' && cle->args[i].h != -1) {
            close(cle->args[i].h);
        }
    }
    return 0;
}

void isftcledestroy(struct isftcle *cle)
{
    if (!cle) {
        return;
    }
    isftcleclosefds(cle);
    free(cle);
}

static void isftbufgetiov(struct isftbuf *b, struct iovec *iov, int *cnt)
{
    unsigned int hd, tail;

    hd = MASK(b->hd);
    tail = MASK(b->tail);
    if (tail < hd) {
        iov[0].iovbase = b->data + tail;
        iov[0].iovlen = hd - tail;
        *cnt = 1;
    } else if (hd == 0) {
        iov[0].iovbase = b->data + tail;
        iov[0].iovlen = sizeof b->data - tail;
        *cnt = 1;
    } else {
        iov[0].iovbase = b->data + tail;
        iov[0].iovlen = sizeof b->data - tail;
        iov[1].iovbase = b->data;
        iov[1].iovlen = hd;
        *cnt = NUM2;
    }
}

static void isftbufcopy(struct isftbuf *b, void data, int cnt)
{
    unsigned int tail, size;

    tail = MASK(b->tail);
    if (tail + cnt <= sizeof b->data) {
        memcpy(data, b->data + tail, cnt);
    } else {
        size = sizeof b->data - tail;
        memcpy(data, b->data + tail, size);
        memcpy((char *) data + size, b->data, cnt - size);
    }
}

static unsigned int isftbufsize(struct isftbuf *b)
{
    return b->hd - b->tail;
}

struct isftconnection *isftconnectioncreate(int fd)
{
    struct isftconnection *connection;

    connection = zalloc(sizeof *connection);
    if (connection == NULL) {
        return NULL;
    }
    connection->fd = fd;

    return connection;
}

static void closefds(struct isftbuf *buf, int max)
{
    int fds[sizeof(buf->data) / sizeof(int)], i, cnt;
    int size;

    size = isftbufsize(buf);
    if (size == 0) {
        return;
    }
    isftbufcopy(buf, fds, size);
    cnt = size / sizeof fds[0];
    if (max > 0 && max < cnt) {
        cnt = max;
    }
    size = cnt * sizeof fds[0];
    for (i = 0; i < cnt; i++) {
        close(fds[i]);
    }
    buf->tail += size;
}

void isftconnectionclosefdsin(struct isftconnection *connection, int max)
{
    closefds(&connection->fdsin, max);
}

int isftconnectiondestroy(struct isftconnection *connection)
{
    int fd = connection->fd;

    closefds(&connection->fdsout, -1);
    closefds(&connection->fdsin, -1);
    free(connection);

    return fd;
}

void isftconnectioncopy(struct isftconnection *connection, void data, int size)
{
    isftbufcopy(&connection->in, data, size);
}

void isftconnectionconsume(struct isftconnection *connection, int size)
{
    connection->in.tail += size;
}

static void ipccmsg(struct isftbuf *buf, char *data, int *clen)
{
    struct cmsghdr *cmsg;
    int size;

    size = isftbufsize(buf);
    if (size > MAXFDSOUT * sizeof(int)) {
        size = MAXFDSOUT * sizeof(int);
    }
    if (size > 0) {
        cmsg = (struct cmsghdr *) data;
        cmsg->cmsglevel = SOLSOCKET;
        cmsg->cmsgtp = SCMRIGHTS;
        cmsg->cmsglen = CMSGLEN(size);
        isftbufcopy(buf, CMSGDATA(cmsg), size);
        *clen = cmsg->cmsglen;
    } else {
        *clen = 0;
    }
}

static int decodecmsg(struct isftbuf *buf, struct msghdr *msg)
{
    struct cmsghdr *cmsg;
    int size, max, i;
    int overflow = 0;

    for (cmsg = CMSGFIRSTHDR(msg); cmsg != NULL;
         cmsg = CMSGNXTHDR(msg, cmsg)) {
        if (cmsg->cmsglevel != SOLSOCKET ||
            cmsg->cmsgtp != SCMRIGHTS) {
            continue;
        }
        size = cmsg->cmsglen - CMSGLEN(0);
        max = sizeof(buf->data) - isftbufsize(buf);
        if (size > max || overflow) {
            overflow = 1;
            size /= sizeof(int);
            for (i = 0; i < size; i++) {
                close(((int*)CMSGDATA(cmsg))[i]);
            }
        } else if (isftbufput(buf, CMSGDATA(cmsg), size) < 0) {
                return -1;
        }
    }

    if (overflow) {
        errno = EOVERFLOW;
        return -1;
    }

    return 0;
}

static int isftconnectionflush(struct isftconnection *connection)
{
    struct iovec iov[2];
    struct msghdr msg;
    char cmsg[CLEN];
    int len = 0, cnt, clen;
    unsigned int tail;

    if (!connection->wantflush) {
        return 0;
    }
    tail = connection->out.tail;
    while (connection->out.hd - connection->out.tail > 0) {
        isftbufgetiov(&connection->out, iov, &cnt);

        ipccmsg(&connection->fdsout, cmsg, &clen);

        msg.msgname = NULL;
        msg.msgnamelen = 0;
        msg.msgiov = iov;
        msg.msgiovlen = cnt;
        msg.msgcontrol = (clen > 0) ? cmsg : NULL;
        msg.msgcontrollen = clen;
        msg.msgflags = 0;

        do {
            len = sendmsg(connection->fd, &msg, MSGNOSIGNAL | MSGDONTWAIT);
        } while (len == -1 && errno == EINTR);

        if (len == -1) {
            return -1;
        }
        closefds(&connection->fdsout, MAXFDSOUT);

        connection->out.tail += len;
    }

    connection->wantflush = 0;

    return connection->out.hd - tail;
}

int isftclelookupobjects(struct isftcle *cle, struct isftmap *objects)
{
    struct isftobject *object;
    const struct isftmsg *msg;
    const char *isftsigtue;
    struct argmtdtls arg;
    int i, cnt;
    unsigned int id;

    msg = cle->msg;
    isftsigtue = msg->isftsigtue;
    cnt = argcntforisftsigtue(isftsigtue);
    for (i = 0; i < cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);
        switch (arg.tp) {
            case 'o':
                id = cle->args[i].n;
                cle->args[i].o = NULL;

                object = isftmaplookup(objects, id);
                if (isftobjectiszombie(objects, id)) {
                    object = NULL;
                } else if (object == NULL && id != 0) {
                    isftlog("unknown object (%u), msg %s(%s)\n", id, msg
                           ->name, msg->isftsigtue);
                    errno = EINVAL;
                    return -1;
                }

                if (object != NULL && msg->tps[i] != NULL &&
                    !isftinterfaceequal((object)->interface, msg->tps[i])) {
                    isftlog("invalid object (%u), tp (%s), "
                            "msg %s(%s)\n", id, (object)->interface->name, msg->
                            name, msg->isftsigtue);
                    errno = EINVAL;
                    return -1;
                }
                cle->args[i].o = object;
            default:
                break;
        }
    }

    return 0;
}

void isftswitch4(struct argmtdtls arg)
{
    struct argmtdtls arg;
    if (arg.tp == 'i') {
        fftp[i] = &fftpint32;
        fargs[i] = &args[i].i;
    } else if (arg.tp == 'u') {
        fftp[i] = &ffitpuint32;
        fargs[i] = &args[i].u;
    } else if (arg.tp == 'f') {
        fftp[i] = &fftpint32;
        fargs[i] = &args[i].f;
    } else if (arg.tp == 's') {
        fftp[i] = &ffitppointer;
        fargs[i] = &args[i].s;
    } else if (arg.tp == 'o') {
        fftp[i] = &ffitppointer;
        fargs[i] = &args[i].o;
    } else if (arg.tp == 'n') {
        if (flags & WLCLOSUREINVOKECLIENT) {
            fftp[i] = &ffitppointer;
            fargs[i] = &args[i].o;
        } else {
            fftp[i] = &ffitpuint32;
            fargs[i] = &args[i].n;
        }
    } else if (arg.tp == 'a') {
        fftp[i] = &ffitppointer;
        fargs[i] = &args[i].a;
    } else if (arg.tp == 'h') {
        fftp[i] = &fftpint32;
        fargs[i] = &args[i].h;
    } else {
        isftabt("unknown tp\n");
    }
    return;
}

void isftcleinvoke(struct isftcle *cle, unsigned int flags, struct
                   isftobject *target, unsigned int opcode, void data)
{
    int cnt;
    fficif cif;
    ffitp *fftp[WLCLOSUREMAXARGS + 2];
    void *fargs[WLCLOSUREMAXARGS + 2];
    void (* const *implementation)(void);

    int i = 0;
    const char *isftsig;
    struct argmtdtls arg;
    cnt = argcntforisftsigtue(cle->msg->isftsigtue);

    fftp[0] = &ffitppointer;
    fargs[0] = &data;
    fftp[1] = &ffitppointer;
    fargs[1] = &target;

    isftsig = isftsigtue;
    while (i < cnt) {
        isftsig = getnextargmt(isftsig, &arg);
        isftswitch4(arg);
        i++;
    }

    ffiprepcif(& cif, FFIDEFAULTABI, cnt + NUM2, & ffitpvoid, fftp);
    implementation = target->implementation;
    if (!implementation[opcode]) {
        isftabt("listener function for opcode %u of %s is NULL\n", opcode, target->
                  interface->name);
    }
    fficall(&cif, implementation[opcode], NULL, fargs);

    isftcleclearfds(cle);
}

void isftcledispatch(struct isftcle *cle, isftdispatcherfunct dispatcher, struct
                     isftobject *target, unsigned int opcode)
{
    dispatcher(target->implementation, target, opcode, cle->msg, cle->args);
    isftcleclearfds(cle);
}

static int copyfdstoconnection(struct isftcle *cle, struct isftconnection *connection)
{
    const struct isftmsg *msg = cle->msg;
    unsigned int i, cnt;
    struct argmtdtls arg;
    const char *isftsigtue = msg->isftsigtue;
    int fd;

    cnt = argcntforisftsigtue(isftsigtue);
    for (i = 0; i < cnt; i++) {
        isftsigtue = getnextargmt(isftsigtue, &arg);
        if (arg.tp != 'h') {
            continue;
        }
        fd = cle->args[i].h;
        if (isftconnectionputfd(connection, fd)) {
            isftlog("request could not be marshaled: "
                   "can't send file descriptor");
            return -1;
        }
        cle->args[i].h = -1;
    }

    return 0;
}

static unsigned int isftbufszforcle(struct isftcle *cle)
{
    const struct isftmsg *msg = cle->msg;
    int i, cnt;
    struct argmtdtls arg;
    const char *isftsigtue;
    unsigned int size, bufsize = 0;

    isftsigtue = msg->isftsigtue;
    cnt = argcntforisftsigtue(isftsigtue);
    while (i < cnt) {
        i = 0;
        isftsigtue = getnextargmt(isftsigtue, &arg);
        if (arg.tp == 'h') {
            break;
        } else if (arg.tp == 'u' || 'i' || 'f' || 'o' || 'n') {
            bufsize++;
            break;
        } else if (arg.tp == 's') {
            if (cle->args[i].s == NULL) {
                bufsize++;
                break;
            }
            size = strlen(cle->args[i].s) + 1;
            bufsize += 1 + divrdp(size, sizeof(unsigned int));
            break;
        } else if (arg.tp == 'a') {
            if (cle->args[i].a == NULL) {
                bufsize++;
                break;
            }
            size = cle->args[i].a->size;
            bufsize += (1 + divrdp(size, sizeof(unsigned int)));
            break;
        } else {
            break;
        }
        i++;
    }

    return bufsize + NUM2;
}
