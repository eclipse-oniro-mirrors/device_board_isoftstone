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
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <ffi.h>

#define NUM2 2
#define NUM16 16
#define NUM1000 1000
#define NUM4096 4096

unsigned int isftconnectionpendinginput(struct isftconnection *connection)
{
    return isftbuffersize(&connection->in);
}

int isftconnectionread(struct isftconnection *connection)
{
    struct iovec iov[2];
    struct msghdr msg;
    char cmsg[CLEN];
    int len, count, ret;

    if (isftbuffersize(&connection->in) >= sizeof(connection->in.data)) {
        errno = EOVERFLOW;
        return -1;
    }

    isftbufferputiov(&connection->in, iov, &count);

    msg.msgname = NULL;
    msg.msgnamelen = 0;
    msg.msgiov = iov;
    msg.msgiovlen = count;
    msg.msgcontrol = cmsg;
    msg.msgcontrollen = sizeof cmsg;
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
    connection->in.head += len;

    return isftconnectionpendinginput(connection);
}

int isftconnectionwrite(struct isftconnection *connection, const void data, int count)
{
    if (connection->out.head - connection->out.tail +
        count > ARRAYLENGTH(connection->out.data)) {
        connection->wantflush = 1;
        if (isftconnectionflush(connection) < 0) {
            return -1;
        }
    }

    if (isftbufferput(&connection->out, data, count) < 0)
        return -1;

    connection->wantflush = 1;

    return 0;
}

int isftconnectionqueue(struct isftconnection *connection, const void data, int count)
{
    if (connection->out.head - connection->out.tail +
        count > ARRAYLENGTH(connection->out.data)) {
        connection->wantflush = 1;
        if (isftconnectionflush(connection) < 0) {
            return -1;
        }
    }

    return isftbufferput(&connection->out, data, count);
}

int isftmessagecountarrays(const struct isftmessage *message)
{
    int i, arrays;

    for (i = 0, arrays = 0; message->signature[i]; i++) {
        if (message->signature[i] == 'a')
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
    if (isftbuffersize(&connection->fdsout) == MAXFDSOUT * sizeof fd) {
        connection->wantflush = 1;
        if (isftconnectionflush(connection) < 0) {
            return -1;
        }
    }

    return isftbufferput(&connection->fdsout, &fd, sizeof fd);
}

const char *getnextargument(const char *signature, struct argumentdetails *details)
{
    details->nullable = 0;
    for (; *signature; ++*signature) {
        switch (*signature) {
            case 'i':
            case 'u':
            case 'f':
            case 's':
            case 'o':
            case 'n':
            case 'a':
            case 'h':
                details->type = *signature;
                return signature + 1;
            case '?':
                details->nullable = 1;
            default:
                break;
        }
    }
    details->type = '\0';
    return signature;
}

int argcountforsignature(const char *signature)
{
    int count = 0;
    for (; *signature; ++*signature) {
        switch (*signature) {
            case 'i':
            case 'u':
            case 'f':
            case 's':
            case 'o':
            case 'n':
            case 'a':
            case 'h':
                ++count;
            default:
                break;
        }
    }
    return count;
}

int isftmessagegetsince(const struct isftmessage *message)
{
    int since;
    since = atoi(message->signature);
    if (since == 0) {
        since = 1;
    }
    return since;
}

void isftargumentfromvalist(const char *signature, union isftargument *args, int count, valist ap)
{
    int i;
    const char *sigiter;
    struct argumentdetails arg;

    sigiter = signature;
    for (i = 0; i < count; i++) {
        sigiter = getnextargument(sigiter, &arg);

        switch (arg.type) {
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

static void isftclosureclearfds(struct isftclosure *closure)
{
    const char *signature = closure->message->signature;
    struct argumentdetails arg;
    int i;

    for (i = 0; i < closure->count; i++) {
        signature = getnextargument(signature, &arg);
        if (arg.type == 'h')
            closure->args[i].h = -1;
    }
}

static struct isftclosure *
isftclosureinit(const struct isftmessage *message, unsigned int size,
                int *numarrays, union isftargument *args)
{
    struct isftclosure *closure;
    int count;

    count = argcountforsignature(message->signature);
    if (count > WLCLOSUREMAXARGS) {
        isftlog("too many args (%d)\n", count);
        errno = EINVAL;
        return NULL;
    }

    if (size) {
        *numarrays = isftmessagecountarrays(message);
        closure = malloc(sizeof *closure + size +
                 *numarrays * sizeof(struct isftarray));
    } else {
        closure = malloc(sizeof *closure);
    }

    if (!closure) {
        errno = ENOMEM;
        return NULL;
    }

    if (args)
        memcpy(closure->args, args, count * sizeof *args);

    closure->message = message;
    closure->count = count;

    isftclosureclearfds(closure);

    return closure;
}

struct isftclosure *isftclosuremarshal(struct isftobject *sender, unsigned int opcode, union
                                       isftargument *args, const struct isftmessage *message)
{
    struct isftclosure *closure;
    struct isftobject *object;
    int i, count, fd, dupfd;
    const char *signature;
    struct argumentdetails arg;

    closure = isftclosureinit(message, 0, NULL, args);
    if (closure == NULL) {
        return NULL;
    }
    count = closure->count;

    signature = message->signature;
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);

        switch (arg.type) {
            case 'f':
            case 'u':
            case 'i':
                break;
            case 's':
                if (!arg.nullable && args[i].s == NULL)
                    return NULL;
                break;
            case 'o':
                if (!arg.nullable && args[i].o == NULL)
                    return NULL;
                break;
            case 'n':
                object = args[i].o;
                if (!arg.nullable && object == NULL)
                    return NULL;

                closure->args[i].n = object ? object->id : 0;
                break;
            case 'a':
                if (!arg.nullable && args[i].a == NULL) {
                    return NULL;
                }
                break;
            case 'h':
                fd = args[i].h;
                dupfd = isftosdupfdcloexec(fd, 0);
                if (dupfd < 0) {
                    isftclosuredestroy(closure);
                    isftlog("error marshalling arguments for %s: dup failed: %s\n", message->
                            name, strerror(errno));
                return NULL;
            }
        }
    }
}

void isftswitch0(struct argumentdetails arg)
{
    struct argumentdetails arg;
    switch (arg.type) {
        case 'u':
                closure->args[i].u = *p++;
                break;
        case 'i':
            closure->args[i].i = *p++;
            break;
        case 'f':
            closure->args[i].f = *p++;
            break;
    }
    return;
}
void isftswitch1(struct argumentdetails arg)
{
    struct argumentdetails arg;
    switch (arg.type) {
        case 's':
            length = *p++;

            if (length == 0) {
                closure->args[i].s = NULL;
                break;
            }
            lengthinu32 = divroundup(length, sizeof *p);
            if ((unsigned int) (end - p) < lengthinu32) {
                isftlog("message too short, " "object (%d), message %s(%s)\n",
                        closure->senderid, message->name, message->signature);
                errno = EINVAL;
                isftclosuredestroy(closure);
                isftconnectionconsume(connection, size);
            }
            next = p + lengthinu32;

            s = (char *) p;

            if (length > 0 && s[length - 1] != '\0') {
                isftlog("string not nul-terminated, "
                    "message %s(%s)\n", message->
                    name, message->signature);
                errno = EINVAL;
                isftclosuredestroy(closure);
                isftconnectionconsume(connection, size);
                }

            closure->args[i].s = s;
            p = next;
            break;
        case 'o':
            id = *p++;
            closure->args[i].n = id;

            if (id == 0 && !arg.nullable) {
                isftlog("NULL object received on non-nullable " "type, message %s(%s)\n", message->
                        name, message->signature);
                errno = EINVAL;
                isftclosuredestroy(closure);
                isftconnectionconsume(connection, size);
            }
            break;
    }
    return;
}

void isftswitch2(struct argumentdetails arg)
{
    struct argumentdetails arg;
        case 'n':
            id = *p++;
            closure->args[i].n = id;

            if (id == 0 && !arg.nullable) {
                isftlog("NULL new ID received on non-nullable " "type, message %s(%s)\n",
                        message->name, message->signature);
                errno = EINVAL;
                isftclosuredestroy(closure);
                isftconnectionconsume(connection, size);
            }

            if (isftmapreservenew(objects, id) < 0) {
                isftlog("not a valid new object id (%u), " "message %s(%s)\n", id, message->name, message->signature);
                errno = EINVAL;
                isftclosuredestroy(closure);
                isftconnectionconsume(connection, size);
            }

            break;
        case 'a':
            length = *p++;

            lengthinu32 = divroundup(length, sizeof *p);
            if ((unsigned int) (end - p) < lengthinu32) {
                isftlog("message too short, " "object (%d), message %s(%s)\n", closure->senderid, message->
                        name, message->signature);
                errno = EINVAL;
                isftclosuredestroy(closure);
                isftconnectionconsume(connection, size);
            }
            next = p + lengthinu32;

            arrayextra->size = length;
            arrayextra->alloc = 0;
            arrayextra->data = p;

            closure->args[i].a = arrayextra++;
            p = next;
            break;
        case 'h':
            if (connection->fdsin.tail == connection->fdsin.head) {
            isftlog("file descriptor expected, " "object (%d), message %s(%s)\n", closure->senderid, message->
                    name, message->signature);
                    errno = EINVAL;
                    isftclosuredestroy(closure);
                    isftconnectionconsume(connection, size);
            }

            isftbuffercopy(&connection->fdsin, &fd, sizeof fd);
            connection->fdsin.tail += sizeof fd;
            closure->args[i].h = fd;
            break;
        default:
            isftabort("unknown type\n");
            break;
    }
    return;
}

struct isftclosure *sftconnectiondemarshal(struct isftconnection *connection, unsigned int
                                           size, struct isftmap *objects, const struct
                                           isftmessage *message)
{
    unsigned int *p, *next, *end, length, lengthinu32, id;
    int fd;
    char *s;
    int i, count, numarrays;
    const char *signature;
    struct argumentdetails arg;
    struct isftclosure *closure;
    struct isftarray *arrayextra;

    if (size < NUM2 * sizeof *p) {
        isftlog("message too short, invalid header\n");
        isftconnectionconsume(connection, size);
        errno = EINVAL;
        return NULL;
    }

    closure = isftclosureinit(message, size, &numarrays, NULL);
    if (closure == NULL) {
        isftconnectionconsume(connection, size);
        return NULL;
    }

    count = closure->count;

    arrayextra = closure->extra;
    p = (unsigned int *)(closure->extra + numarrays);
    end = p + size / sizeof *p;

    isftconnectioncopy(connection, p, size);
    closure->senderid = *p++;
    closure->opcode = *p++ & 0x0000ffff;

    signature = message->signature;
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);

        if (arg.type != 'h' && p + 1 > end) {
            isftlog("message too short, " "object (%d), message %s(%s)\n", closure->senderid, message->
                    name, message->signature);
            errno = EINVAL;
            isftclosuredestroy(closure);
            isftconnectionconsume(connection, size);
        }
        isftswitch0(arg);
        isftswitch1(arg);
        isftswitch2(arg);
    }

    isftconnectionconsume(connection, size);

    return closure;
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

unsigned int divroundup(unsigned int n, int a)
{
    return (unsigned int) (((unsigned long) n + (a - 1)) / a);
}

struct isftbuffer {
    char data[4096];
    unsigned int head, tail;
};

int MASK(i)
{
    return ((i) & NUM4096);
}

#define MAXFDSOUT    28
#define CLEN        (CMSG_LEN(MAX_FDS_OUT * sizeof(int)))

struct isftconnection {
    struct isftbuffer in, out;
    struct isftbuffer fdsin, fdsout;
    int fd;
    int wantflush;
};

static int isftbufferput(struct isftbuffer *b, const void data, int count)
{
    unsigned int head, size;

    if (count > sizeof(b->data)) {
        isftlog("Data too big for buffer (%d > %d).\n", count, sizeof(b->data));
        errno = E2BIG;
        return -1;
    }

    head = MASK(b->head);
    if (head + count <= sizeof b->data) {
        memcpy(b->data + head, data, count);
    } else {
        size = sizeof b->data - head;
        memcpy(b->data + head, data, size);
        memcpy(b->data, (const char *) data + size, count - size);
    }

    b->head += count;

    return 0;
}

static void isftbufferputiov(struct isftbuffer *b, struct iovec *iov, int *count)
{
    unsigned int head, tail;

    head = MASK(b->head);
    tail = MASK(b->tail);
    if (head < tail) {
        iov[0].iovbase = b->data + head;
        iov[0].iovlen = tail - head;
        *count = 1;
    } else if (tail == 0) {
        iov[0].iovbase = b->data + head;
        iov[0].iovlen = sizeof b->data - head;
        *count = 1;
    } else {
        iov[0].iovbase = b->data + head;
        iov[0].iovlen = sizeof b->data - head;
        iov[1].iovbase = b->data;
        iov[1].iovlen = tail;
        *count = NUM2;
    }
}

void isftswitch(struct argumentdetails arg);
{
    struct argumentdetails arg;
    switch (arg.type) {
        case 'u':
            *p++ = closure->args[i].u;
                break;
        case 'i':
            *p++ = closure->args[i].i;
            break;
        case 'f':
            *p++ = closure->args[i].f;
            break;
        case 'o':
            *p++ = closure->args[i].o ? closure->args[i].o->id : 0;
            break;
        case 'n':
            *p++ = closure->args[i].n;
            break;
        case 's':
            if (closure->args[i].s == NULL) {
                *p++ = 0;
                break;
            }

            size = strlen(closure->args[i].s) + 1;
            *p++ = size;

            if (p + divroundup(size, sizeof *p) > end) {
                errno = ERANGE;
            }
            memcpy(p, closure->args[i].s, size);
            p += divroundup(size, sizeof *p);
            break;
        case 'a':
            if (closure->args[i].a == NULL) {
                *p++ = 0;
                break;
            }

            size = closure->args[i].a->size;
            *p++ = size;

            if (p + divroundup(size, sizeof *p) > end) {
                errno = ERANGE;
            }
            memcpy(p, closure->args[i].a->data, size);
            p += divroundup(size, sizeof *p);
            break;
        default:
            break;
    }
}

static int serializeclosure(struct isftclosure *closure, unsigned int *buffer, int buffercount)
{
    const struct isftmessage *message = closure->message;
    unsigned int i, count, size;
    unsigned int *p, *end;
    struct argumentdetails arg;
    const char *signature;

    if (buffercount < NUM2) {
        errno = ERANGE;
    }
    p = buffer + NUM2;
    end = buffer + buffercount;

    signature = message->signature;
    count = argcountforsignature(signature);
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);

        if (arg.type == 'h') {
            continue;
        }
        if (p + 1 > end) {
            errno = ERANGE;
        }
        isftswitch(arg);
    }

    size = (p - buffer) * sizeof *p;

    buffer[0] = closure->senderid;
    buffer[1] = size << NUM16 | (closure->opcode & 0x0000ffff);

    return size;
}

int isftclosuresend(struct isftclosure *closure, struct isftconnection *connection)
{
    int size;
    unsigned int buffersize;
    unsigned int *buffer;
    int result;

    if (copyfdstoconnection(closure, connection)) {
        return -1;
    }
    buffersize = buffersizeforclosure(closure);
    buffer = zalloc(buffersize * sizeof buffer[0]);
    if (buffer == NULL) {
        return -1;
    }
    size = serializeclosure(closure, buffer, buffersize);
    if (size < 0) {
        free(buffer);
        return -1;
    }

    result = isftconnectionwrite(connection, buffer, size);
    free(buffer);

    return result;
}

int isftclosurequeue(struct isftclosure *closure, struct isftconnection *connection)
{
    int size;
    unsigned int buffersize;
    unsigned int *buffer;
    int result;

    if (copyfdstoconnection(closure, connection)) {
        return -1;
    }
    buffersize = buffersizeforclosure(closure);
    buffer = malloc(buffersize * sizeof buffer[0]);
    if (buffer == NULL) {
        return -1;
    }
    size = serializeclosure(closure, buffer, buffersize);
    if (size < 0) {
        free(buffer);
        return -1;
    }

    result = isftconnectionqueue(connection, buffer, size);
    free(buffer);

    return result;
}

void isftswitch3(struct argumentdetails arg)
{
    struct argumentdetails arg;
    switch (arg.type) {
        case 'u':
            fprintf(stderr, "%u", closure->args[i].u);
            break;
        case 'i':
            fprintf(stderr, "%d", closure->args[i].i);
            break;
        case 'f':
            fprintf(stderr, "%f",
                isftfixedtodouble(closure->args[i].f));
            break;
        case 's':
            if (closure->args[i].s)
                fprintf(stderr, "\"%s\"", closure->args[i].s);
            else
                fprintf(stderr, "nil");
            break;
        case 'o':
            if (closure->args[i].o)
                fprintf(stderr, "%s@%u",
                    closure->args[i].o->interface->name,
                    closure->args[i].o->id);
            else
                fprintf(stderr, "nil");
            break;
        case 'n':
            fprintf(stderr, "new id %s@",
                (closure->message->types[i]) ?
                 closure->message->types[i]->name :
                  "[unknown]");
            if (closure->args[i].n != 0)
                fprintf(stderr, "%u", closure->args[i].n);
            else
                fprintf(stderr, "nil");
            break;
        case 'a':
            fprintf(stderr, "array");
            break;
        case 'h':
            fprintf(stderr, "fd %d", closure->args[i].h);
            break;
        }
    return;
}

void isftclt(struct isftclosure *closure, struct isftobject *target, int send)
{
    int i;
    struct argumentdetails arg;
    const char *signature = closure->message->signature;
    struct timespec tp;
    unsigned int time;

    clockgettime(CLOCKREALTIME, &tp);
    time = (tp.tvsec * 1000000L) + (tp.tvnsec / NUM1000);
    if (1) {
        fprintf(stderr, "[%10.3f] %s%s@%u.%s(",
            time / NUM1000,
            send ? " -> " : "",
            target->interface->name, target->id,
            closure->message->name);
    }
    for (i = 0; i < closure->count; i++) {
        signature = getnextargument(signature, &arg);
        if (i > 0) {
            fprintf(stderr, ", ");
        }
        isftswitch3(arg);
    }
    if (1) {
        fprintf(stderr, ")\n");
    }
}

static int isftclosureclosefds(struct isftclosure *closure)
{
    int i;
    struct argumentdetails arg;
    const char *signature = closure->message->signature;

    for (i = 0; i < closure->count; i++) {
        signature = getnextargument(signature, &arg);
        if (arg.type == 'h' && closure->args[i].h != -1) {
            close(closure->args[i].h);
        }
    }
    return 0;
}

void isftclosuredestroy(struct isftclosure *closure)
{
    if (!closure) {
        return;
    }
    isftclosureclosefds(closure);
    free(closure);
}

static void isftbuffergetiov(struct isftbuffer *b, struct iovec *iov, int *count)
{
    unsigned int head, tail;

    head = MASK(b->head);
    tail = MASK(b->tail);
    if (tail < head) {
        iov[0].iovbase = b->data + tail;
        iov[0].iovlen = head - tail;
        *count = 1;
    } else if (head == 0) {
        iov[0].iovbase = b->data + tail;
        iov[0].iovlen = sizeof b->data - tail;
        *count = 1;
    } else {
        iov[0].iovbase = b->data + tail;
        iov[0].iovlen = sizeof b->data - tail;
        iov[1].iovbase = b->data;
        iov[1].iovlen = head;
        *count = NUM2;
    }
}

static void isftbuffercopy(struct isftbuffer *b, void data, int count)
{
    unsigned int tail, size;

    tail = MASK(b->tail);
    if (tail + count <= sizeof b->data) {
        memcpy(data, b->data + tail, count);
    } else {
        size = sizeof b->data - tail;
        memcpy(data, b->data + tail, size);
        memcpy((char *) data + size, b->data, count - size);
    }
}

static unsigned int isftbuffersize(struct isftbuffer *b)
{
    return b->head - b->tail;
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

static void closefds(struct isftbuffer *buffer, int max)
{
    int fds[sizeof(buffer->data) / sizeof(int)], i, count;
    int size;

    size = isftbuffersize(buffer);
    if (size == 0) {
        return;
    }
    isftbuffercopy(buffer, fds, size);
    count = size / sizeof fds[0];
    if (max > 0 && max < count) {
        count = max;
    }
    size = count * sizeof fds[0];
    for (i = 0; i < count; i++) {
        close(fds[i]);
    }
    buffer->tail += size;
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
    isftbuffercopy(&connection->in, data, size);
}

void isftconnectionconsume(struct isftconnection *connection, int size)
{
    connection->in.tail += size;
}

static void ipccmsg(struct isftbuffer *buffer, char *data, int *clen)
{
    struct cmsghdr *cmsg;
    int size;

    size = isftbuffersize(buffer);
    if (size > MAXFDSOUT * sizeof(int)) {
        size = MAXFDSOUT * sizeof(int);
    }
    if (size > 0) {
        cmsg = (struct cmsghdr *) data;
        cmsg->cmsglevel = SOLSOCKET;
        cmsg->cmsgtype = SCMRIGHTS;
        cmsg->cmsglen = CMSGLEN(size);
        isftbuffercopy(buffer, CMSGDATA(cmsg), size);
        *clen = cmsg->cmsglen;
    } else {
        *clen = 0;
    }
}

static int decodecmsg(struct isftbuffer *buffer, struct msghdr *msg)
{
    struct cmsghdr *cmsg;
    int size, max, i;
    int overflow = 0;

    for (cmsg = CMSGFIRSTHDR(msg); cmsg != NULL;
         cmsg = CMSGNXTHDR(msg, cmsg)) {
        if (cmsg->cmsglevel != SOLSOCKET ||
            cmsg->cmsgtype != SCMRIGHTS) {
            continue;
        }
        size = cmsg->cmsglen - CMSGLEN(0);
        max = sizeof(buffer->data) - isftbuffersize(buffer);
        if (size > max || overflow) {
            overflow = 1;
            size /= sizeof(int);
            for (i = 0; i < size; i++) {
                close(((int*)CMSGDATA(cmsg))[i]);
            }
        } else if (isftbufferput(buffer, CMSGDATA(cmsg), size) < 0) {
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
    int len = 0, count, clen;
    unsigned int tail;

    if (!connection->wantflush) {
        return 0;
    }
    tail = connection->out.tail;
    while (connection->out.head - connection->out.tail > 0) {
        isftbuffergetiov(&connection->out, iov, &count);

        ipccmsg(&connection->fdsout, cmsg, &clen);

        msg.msgname = NULL;
        msg.msgnamelen = 0;
        msg.msgiov = iov;
        msg.msgiovlen = count;
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

    return connection->out.head - tail;
}

int isftclosurelookupobjects(struct isftclosure *closure, struct isftmap *objects)
{
    struct isftobject *object;
    const struct isftmessage *message;
    const char *signature;
    struct argumentdetails arg;
    int i, count;
    unsigned int id;

    message = closure->message;
    signature = message->signature;
    count = argcountforsignature(signature);
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);
        switch (arg.type) {
            case 'o':
                id = closure->args[i].n;
                closure->args[i].o = NULL;

                object = isftmaplookup(objects, id);
                if (isftobjectiszombie(objects, id)) {
                    object = NULL;
                } else if (object == NULL && id != 0) {
                    isftlog("unknown object (%u), message %s(%s)\n", id, message
                           ->name, message->signature);
                    errno = EINVAL;
                    return -1;
                }

                if (object != NULL && message->types[i] != NULL &&
                    !isftinterfaceequal((object)->interface, message->types[i])) {
                    isftlog("invalid object (%u), type (%s), "
                            "message %s(%s)\n", id, (object)->interface->name, message->
                            name, message->signature);
                    errno = EINVAL;
                    return -1;
                }
                closure->args[i].o = object;
            default:
                break;
        }
    }

    return 0;
}

static void convertargumentstoffi(const char *signature, unsigned int flags, union
                                  isftargument *args, int count, ffitype **ffitypes, void ffiargs)
{
    int i;
    const char *sigiter;
    struct argumentdetails arg;

    sigiter = signature;
    for (i = 0; i < count; i++) {
        sigiter = getnextargument(sigiter, &arg);

        switch (arg.type) {
            case 'i':
                ffitypes[i] = &ffitypesint32;
                ffiargs[i] = &args[i].i;
                break;
            case 'u':
                ffitypes[i] = &ffitypeuint32;
                ffiargs[i] = &args[i].u;
                break;
            case 'f':
                ffitypes[i] = &ffitypesint32;
                ffiargs[i] = &args[i].f;
                break;
            case 's':
                ffitypes[i] = &ffitypepointer;
                ffiargs[i] = &args[i].s;
                break;
            case 'o':
                ffitypes[i] = &ffitypepointer;
                ffiargs[i] = &args[i].o;
                break;
            case 'n':
                if (flags & WLCLOSUREINVOKECLIENT) {
                    ffitypes[i] = &ffitypepointer;
                    ffiargs[i] = &args[i].o;
                } else {
                    ffitypes[i] = &ffitypeuint32;
                    ffiargs[i] = &args[i].n;
                }
                break;
            case 'a':
                ffitypes[i] = &ffitypepointer;
                ffiargs[i] = &args[i].a;
                break;
            case 'h':
                ffitypes[i] = &ffitypesint32;
                ffiargs[i] = &args[i].h;
                break;
            default:
                isftabort("unknown type\n");
                break;
        }
    }
}

void isftclosureinvoke(struct isftclosure *closure, unsigned int flags, struct
                       isftobject *target, unsigned int opcode, void data)
{
    int count;
    fficif cif;
    ffitype *ffitypes[WLCLOSUREMAXARGS + 2];
    void *ffiargs[WLCLOSUREMAXARGS + 2];
    void (* const *implementation)(void);

    count = argcountforsignature(closure->message->signature);

    ffitypes[0] = &ffitypepointer;
    ffiargs[0] = &data;
    ffitypes[1] = &ffitypepointer;
    ffiargs[1] = &target;

    convertargumentstoffi(closure->message->signature, flags, closure->
                 args, count, ffitypes + NUM2, ffiargs + NUM2);

    ffiprepcif(& cif, FFIDEFAULTABI, count + NUM2, & ffitypevoid, ffitypes);
    implementation = target->implementation;
    if (!implementation[opcode]) {
        isftabort("listener function for opcode %u of %s is NULL\n", opcode, target->
                  interface->name);
    }
    fficall(&cif, implementation[opcode], NULL, ffiargs);

    isftclosureclearfds(closure);
}

void isftclosuredispatch(struct isftclosure *closure, isftdispatcherfunct dispatcher, struct
                         isftobject *target, unsigned int opcode)
{
    dispatcher(target->implementation, target, opcode, closure->message, closure->args);
    isftclosureclearfds(closure);
}

static int copyfdstoconnection(struct isftclosure *closure, struct isftconnection *connection)
{
    const struct isftmessage *message = closure->message;
    unsigned int i, count;
    struct argumentdetails arg;
    const char *signature = message->signature;
    int fd;

    count = argcountforsignature(signature);
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);
        if (arg.type != 'h') {
            continue;
        }
        fd = closure->args[i].h;
        if (isftconnectionputfd(connection, fd)) {
            isftlog("request could not be marshaled: "
                   "can't send file descriptor");
            return -1;
        }
        closure->args[i].h = -1;
    }

    return 0;
}

static unsigned int buffersizeforclosure(struct isftclosure *closure)
{
    const struct isftmessage *message = closure->message;
    int i, count;
    struct argumentdetails arg;
    const char *signature;
    unsigned int size, buffersize = 0;

    signature = message->signature;
    count = argcountforsignature(signature);
    for (i = 0; i < count; i++) {
        signature = getnextargument(signature, &arg);

        switch (arg.type) {
            case 'h':
                break;
            case 'u':
            case 'i':
            case 'f':
            case 'o':
            case 'n':
                buffersize++;
                break;
            case 's':
                if (closure->args[i].s == NULL) {
                    buffersize++;
                    break;
                }

                size = strlen(closure->args[i].s) + 1;
                buffersize += 1 + divroundup(size, sizeof(unsigned int));
                break;
            case 'a':
                if (closure->args[i].a == NULL) {
                    buffersize++;
                    break;
                }

                size = closure->args[i].a->size;
                buffersize += (1 + divroundup(size, sizeof(unsigned int)));
                break;
            default:
                break;
        }
    }

    return buffersize + NUM2;
}