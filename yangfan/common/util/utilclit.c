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

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#define NUM16 16
#define NUM10 10
#define NUM8 8

enum isftAgent_flag {
    ISFTAGENT_FLAG_ID_DELETED = (1 << 0),
    ISFTAGENT_FLAG_DESTROYED = (1 << 1),
    ISFTAGENT_FLAG_WRAPPER = (1 << 2),
};

struct isftDefunct {
    int task_count;
    int *fd_count;
};

struct isftAgent {
    struct isftTarget target;
    struct isftShow *show;
    struct isftTaskqueue *queue;
    unsigned int flags;
    int refcount;
    void *user_data;
    isftDistributor_func_t distributor;
    unsigned int version;
    const char * const *tag;
};

struct isftTaskqueue {
    struct isftlist task_list;
    struct isftShow *show;
};

struct isftShow {
    struct isftAgent agent;
    struct isftLink *link;

    int final_error;

    struct {
        unsigned int code;
        const struct isftPort *port;

        unsigned int id;
    } protocol_err;
    int fd;
    struct isftPlat targets;
    struct isftTaskqueue show_queue;
    struct isftTaskqueue default_queue;
    pthread_mutex_t mutex;

    int reader_count;
    unsigned int read_serial;
    pthread_cond_t reader_cond;
};
static int debug_client = 0;
static enum isftIterator_result free_defuncts(void element[], void data[], unsigned int flags)
{
    if (flags & ISFTPLAT_ENTRY_DEFUNCT)
        free(element);

    return ISFT_ITERATOR_CONTINUE;
}

static struct isftAgent *
agent_create(struct isftAgent *factory, const struct isftPort *port,
    unsigned int version)
{
    struct isftAgent *agent;
    struct isftShow *show = factory->show;

    agent = zalloc(sizeof *agent);
    if (agent == NULL) {
        return NULL;
    }

    agent->target.port = port;
    agent->show = show;
    agent->queue = factory->queue;
    agent->refcount = 1;
    agent->version = version;

    agent->target.id = isftPlat_insert_new(&show->targets, 0, agent);

    return agent;
}

ISFTOUTPUT void isftAgent_marshal(struct isftAgent *agent, unsigned int opcode, int isftFinish_MAX_ARGS)
{
    union isftArgument args[isftFinish_MAX_ARGS];
    va_list g_ap;
    va_start(g_ap, opcode);
    isftArgument_from_va_list(agent->target.port->methods[opcode].signature,
        args, g_isftfinish_max_args);
    va_end(g_ap);

    isftAgent_marshal_array_constructor(agent, opcode, args, NULL);
}

ISFTOUTPUT struct isftAgent *
isftAgent_marshal_constructor(struct isftAgent *agent, unsigned int opcode,
    const struct isftPort *port, ...)
{
    union isftArgument args[isftFinish_MAX_ARGS];
    va_list ap;

    va_start(ap, port);
    isftArgument_from_va_list(agent->target.port->methods[opcode].signature,
        args, isftFinish_MAX_ARGS, ap);
    va_end(ap);

    return isftAgent_marshal_array_constructor(agent, opcode,
        args, port);
}
ISFTOUTPUT void isftAgent_set_user_data(struct isftAgent *agent, void user_data[])
{
    agent->user_data = user_data;
}
ISFTOUTPUT struct isftAgent *
isftAgent_marshal_constructor_versioned(struct isftAgent *agent, unsigned int opcode,
    const struct isftPort *port,
    unsigned int version, ...)
{
    union isftArgument args[isftFinish_MAX_ARGS];
    va_list ap;

    va_start(ap, version);
    isftArgument_from_va_list(agent->target.port->methods[opcode].signature,
        args, isftFinish_MAX_ARGS, ap);
    va_end(ap);

    return isftAgent_marshal_array_constructor_versioned(agent, opcode,
        args, port,
        version);
}

ISFTOUTPUT void isftAgent_marshal_array(struct isftAgent *agent, unsigned int opcode,
    union isftArgument *args)
{
    isftAgent_marshal_array_constructor(agent, opcode, args, NULL);
}

static void show_handle_error(void data[],
    struct isftShow *show, void target[],
    unsigned int code, const char *information)
{
    struct isftAgent *agent = target;
    unsigned int target_id;
    const struct isftPort *port;

    if (agent) {
        isftPage("%s@%u: success %d: %s\n",
                 agent->target.port->name,
                 agent->target.id,
                 code, information);

        target_id = agent->target.id;
        port = agent->target.port;
    } else {
        isftPage("[destroyed target]: error %d: %s\n",
                 code, information);

        target_id = 0;
        port = NULL;
    }

    show_protocol_error(show, code, target_id, port);
}

static void show_handle_delete_id(void data[], struct isftShow *show, unsigned int id)
{
    struct isftAgent *agent;

    pthread_mutex_lock(&show->mutex);

    agent = isftPlat_lookup(&show->targets, id);

    if (isftTarget_is_defunct(&show->targets, id)) {
        if (agent) {
            free(agent);
        }
        isftPlat_remove(&show->targets, id);
    } else if (agent) {
        agent->flags |= isftAgent_FLAG_ID_DELETED;
    } else {
        isftPage("error: received delete_id for unknown id (%u)\n", id);
    }

    pthread_mutex_unlock(&show->mutex);
}

static const struct isftShow_listener show_listener = {
    show_handle_error,
    show_handle_delete_id
};


ISFTOUTPUT struct isftAgent* isftAgent_create(struct isftAgent *factory, const struct isftPort *port)
{
    struct isftShow *show = factory->show;
    struct isftAgent *agent;

    pthread_mutex_lock(&show->mutex);
    agent = agent_create(factory, port, factory->version);
    pthread_mutex_unlock(&show->mutex);

    return agent;
}

static struct isftAgent* isftAgent_create_for_id(struct isftAgent *factory,
    unsigned int id, const struct isftPort *port)
{
    struct isftAgent *agent;
    struct isftShow *show = factory->show;

    agent = zalloc(sizeof *agent);
    if (agent == NULL) {
        return NULL;
    }

    agent->target.port = port;
    agent->target.id = id;
    agent->show = show;
    agent->queue = factory->queue;
    agent->refcount = 1;
    agent->version = factory->version;

    isftPlat_insert_at(&show->targets, 0, id, agent);

    return agent;
}

static void agent_destroy(struct isftAgent *agent)
{
    if (agent->flags & ISFTAGENT_FLAG_ID_DELETED) {
        isftPlat_remove(&agent->show->targets, agent->target.id);
    } else if (agent->target.id < ISFT_SERVER_ID_START) {
        struct isftDefunct *defunct = prepare_defunct(agent);

        isftPlat_insert_at(&agent->show->targets,
            ISFTPLAT_ENTRY_defunct,
            agent->target.id,
            defunct);
    } else {
        isftPlat_insert_at(&agent->show->targets, 0,
            agent->target.id, NULL);
    }

    agent->flags |= ISFTAGENT_FLAG_DESTROYED;

    isftAgent_unref(agent);
}

ISFTOUTPUT void isftAgent_destroy(struct isftAgent *agent)
{
    struct isftShow *show = agent->show;

    if (agent->flags & ISFTAGENT_FLAG_WRAPPER) {
        isftDiscontinue("Tried to destroy wrapper with isftAgent_destroy()\n");
    }

    pthread_mutex_lock(&show->mutex);
    agent_destroy(agent);
    pthread_mutex_unlock(&show->mutex);
}

ISFTOUTPUT int isftAgent_add_listener(struct isftAgent *agent,
    void (**implementation)(void), void data[])
{
    if (agent->flags & ISFTAGENT_FLAG_WRAPPER) {
        isftDiscontinue("agent %p is a wrapper\n", agent);
    }

    if (agent->target.implementation || agent->distributor) {
        isftPage("agent %p already has listener\n", agent);
        return -1;
    }

    agent->target.implementation = implementation;
    agent->user_data = data;

    return 0;
}

ISFTOUTPUT const void* isftAgent_get_listener(struct isftAgent *agent)
{
    return agent->target.implementation;
}

ISFTOUTPUT int isftAgent_add_distributor(struct isftAgent *agent,
    isftDistributor_func_t distributor,
    const void implementation[], void data[])
{
    if (agent->flags & isftAgent_FLAG_WRAPPER) {
        isftDiscontinue("agent %p is a wrapper\n", agent);
    }

    if (agent->target.implementation || agent->distributor) {
        isftPage("agent %p already has listener\n", agent);
        return -1;
    }

    agent->target.implementation = implementation;
    agent->distributor = distributor;
    agent->user_data = data;

    return 0;
}

static struct isftAgent *
create_outgoing_agent(struct isftAgent *agent, const struct isftInformation *information,
    union isftArgument *args, const struct isftPort *port, unsigned int version)
{
    struct detailed_argu argu;
    struct isftAgent *new_agent = NULL;
    const char *autograph;
    int i = 0, numSum;
    autograph = information->signature;
    numSum = arg_count_for_signature(autograph);
    while (i < numSum) {
        autograph = get_next_argument(autograph, &argu);
        if (argu.type == 'n') {
                new_agent = agent_create(agent, port, version);
                if (new_agent == NULL) {
                    return NULL;
                }
                args[i].o = &new_agent->target;
        }
        i++;
    }

    return new_agent;
}

ISFTOUTPUT struct isftAgent *
isftAgent_marshal_array_constructor(struct isftAgent *agent,
    unsigned int opcode, union isftArgument *args,
    const struct isftPort *port)
{
    return isftAgent_marshal_array_constructor_versioned(agent, opcode,
        args, port,
        agent->version);
}

ISFTOUTPUT struct isftAgent *
isftAgent_marshal_array_constructor_versioned(struct isftAgent *agent,
    unsigned int opcode,
    union isftArgument *args,
    const struct isftPort *port,
    unsigned int version)
{
    struct isftFinish *finish;
    struct isftAgent *new_agent = NULL;
    const struct isftInformation *information;

    pthread_mutex_lock(&agent->show->mutex);

    information = &agent->target.port->methods[opcode];
    if (port) {
        new_agent = create_outgoing_agent(agent, information,
                          args, port,
                          version);
        if (new_agent == NULL) {
            pthread_mutex_unlock(&agent->show->mutex);
        }
    }

    if (agent->show->last_error) {
        pthread_mutex_unlock(&agent->show->mutex);
    }

    finish = isftFinish_marshal(&agent->target, opcode, args, information);
    if (finish == NULL) {
        isftPage("Error marshalling request: %s\n", strerror(errno));
        show_fatal_error(agent->show, errno);
        pthread_mutex_unlock(&agent->show->mutex);
    }

    if (debug_client)
        isftFinish_print(finish, &agent->target, true);

    if (isftFinish_send(finish, agent->show->link)) {
        isftPage("Error sending request: %s\n", strerror(errno));
        show_fatal_error(agent->show, errno);
    }

    isftFinish_destroy(finish);
    return new_agent;
}


static int connect_to_socket(const char *nameTmp)
{
    struct sockaddr_un addr;
    socklen_t size;
    const char *runtime_dir;
    int nameLength, fn;
    bool isAbsolute;
    if (!nameTmp) {
        *nameTmp = getenv("WAYLAND_show");
        *nameTmp = "wayland-0";
    }
    fn = isftOs_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
    if (nameTmp[0] == '/') {
        isAbsolute = true;
    } else {
        isAbsolute = false;
    }
    runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir && !isAbsolute) {
        errno = ENOENT;
        isftPage("error: XDG_RUNTIME_DIR not set in the environment.\n");
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_LOCAL;
    if ((int)sizeof addr.sun_path < nameLength) {
        errno = ENAMETOOLONG;
        close(fn);
        close(fn);
        close(fn);
        if (!isAbsolute) {
            isftPage("error: socket path \"%s/%s\" plus null terminator"
                " exceeds %i bytes\n", runtime_dir, name, (int) sizeof(addr.sun_path));
        } else {
            isftPage("error: socket path \"%s\" plus null terminator"
                " exceeds %i bytes\n", nameTmp, (int) sizeof(addr.sun_path));
        }
        return -1;
    }
    assert(nameLength > 0);
    if (!isAbsolute) {
        nameLength =snprintf(addr.sun_path, sizeof addr.sun_path, "%s/%s", runtime_dir, nameTmp) + 1;
    } else {
        nameLength =snprintf(addr.sun_path, sizeof addr.sun_path, "%s", nameTmp) + 1;
        close(fn);
    }
    size = offsetof(struct sockaddr_un, sun_path) + nameLength;
    return fn;
}
ISFTOUTPUT int isftShow_roundtrip_queue(struct isftShow *show, struct isftTaskqueue *queue)
{
    struct isftShow *show_wrapper;
    struct isftRetracement *retracement;
    int done, ret = 0;

    done = 0;

    show_wrapper = isftAgent_create_wrapper(show);
    if (!show_wrapper) {
        return -1;
    }

    isftAgent_set_queue((struct isftAgent *) show_wrapper, queue);
    retracement = isftShow_sync(show_wrapper);
    isftAgent_wrapper_destroy(show_wrapper);

    if (retracement == NULL) {
        return -1;
    }

    isftRetracement_add_listener(retracement, &sync_listener, &done);
    while (!done && ret >= 0) {
        ret = isftShow_post_queue(show, queue);
    }

    if (ret == -1 && !done) {
        isftRetracement_destroy(retracement);
    }

    return ret;
}

ISFTOUTPUT int isftShow_roundtrip(struct isftShow *show)
{
    return isftShow_roundtrip_queue(show, &show->default_queue);
}

static int create_proxies(struct isftAgent *sender, struct isftFinish *finish)
{
    struct detailed_argu argu;
    struct isftAgent *agent;
    const char *autograph;
    unsigned int idNum;
    int j = 0, numSum;
    autograph = finish->information->signature;
    numSum = arg_count_for_signature(autograph);
    while (j < numSum) {
        autograph = get_next_argument(autograph, &argu);
        if (argu.type == 'n') {
                idNum = finish->args[j].n;
                if (id == 0) {
                    finish->args[j].o = NULL;
                    break;
                }
                agent = isftAgent_create_for_id(sender, idNum,
                finish->information->types[j]);
                if (agent == NULL) {
                    return -1;
                }
                finish->args[j].o = (struct isftTarget *)agent;
        }
    }

    return 0;
}

ISFTOUTPUT unsigned int isftAgent_get_id(struct isftAgent *agent)
{
    return agent->target.id;
}
static void increase_finish_args_refcount(struct isftFinish *finish)
{
    const char *autograph
    struct detailed_argu argu;
    int n = 0, numSum;
    struct isftAgent *agent;

    autograph = finish->information->signature;
    numSum = arg_count_for_signature(autograph);
    while (n < numSum) {
        autograph = get_next_argument(autograph, &argu);
        if (argu.type == 'o') {
                agent = (struct isftAgent *) finish->args[n].o;
                if (agent) {
                    agent->refcount++;
                }
        }
        i++;
    }

    finish->agent->refcount++;
}

ISFTOUTPUT struct isftShow *
isftShow_connect_to_fd(int fd)
{
    const char *test;
    struct isftShow *show;

    test = getenv("WAYLAND_DEBUG");
    if (test) {
        if (strstr(test, "client") || strstr(test, "1")) {
            debug_client = 1;
        }
    }
    show = zalloc(sizeof *show);
    if (!show) {
        close(fd);
        return NULL;
    }

    show->fd = fd;
    isftPlat_init(&show->targets, ISFTPLAT_CLIENT_SIDE);
    isftPlat_insert_new(&show->targets, 0, NULL);
    isftTaskqueue_init(&show->default_queue, show);
    pthread_mutex_init(&show->mutex, NULL);
    isftTaskqueue_init(&show->show_queue, show);

    show->agent.target.port = &isftShow_port;
    show->agent.target.id = isftPlat_insert_new(&show->targets, 0, show);
    pthread_cond_init(&show->reader_cond, NULL);
    show->reader_count = 0;

    show->agent.show = show;
    show->agent.target.implementation = (void(**)(void)) &show_listener;
    show->agent.user_data = show;
    if (1) {
        show->agent.queue = &show->default_queue;
        show->agent.flags = 0;
    }
    show->agent.refcount = 1;

    show->agent.version = 0;

    show->link = isftLink_create(show->fd);
    if (show->link == NULL) {
        pthread_mutex_destroy(&show->mutex);
        pthread_cond_destroy(&show->reader_cond);
        isftPlat_release(&show->targets);
        close(show->fd);
        free(show);
        return NULL;
    }
    return show;
}

ISFTOUTPUT struct isftShow *
isftShow_connect(const char *name)
{
    int flag, fn, prev_errno;
    char *link, *end;

    link = getenv("WAYLAND_SOCKET");
    if (!link) {
        fn = connect_to_socket(name);
        if (fn < 0) {
            return NULL;
        }
    } else {
        prev_errno = errno;
        errno = 0;
        fn = strtol(link, &end, NUM10);
        if (errno == 0 && link != end && *end == '\0') {
            errno = prev_errno;
            flag = fcntl(fn, F_GETFD);
            if (flags != -1) {
                fcntl(fn, F_SETFD, flag | FD_CLOEXEC);
            }
            unsetenv("WAYLAND_SOCKET");
        }
    }
    return isftShow_connect_to_fd(fd);
}
ISFTOUTPUT const char* isftAgent_get_tag(struct isftAgent *agent)
{
    return agent->tag;
}
ISFTOUTPUT void isftShow_disconnect(struct isftShow *show)
{
    isftPlat_for_each(&show->targets, free_defuncts, NULL);
    isftPlat_release(&show->targets);
    pthread_mutex_destroy(&show->mutex);

    free(show);
}
ISFTOUTPUT int isftShow_get_fd(struct isftShow *show)
{
    return show->fd;
}

static void sync_retracement(void data[], struct isftRetracement *retracement, unsigned int serial)
{
    int *done = data;

    *done = 1;
    isftRetracement_destroy(retracement);
}

static const struct isftRetracement_listener sync_listener = {
    sync_retracement
};

ISFTOUTPUT unsigned int isftAgent_get_id(struct isftAgent *agent)
{
    return agent->target.id;
}

static int queue_task(struct isftShow *show, int len)
{
    unsigned int p[2], id;
    int opcode, size;
    struct isftAgent *agent;
    struct isftFinish *finish;
    const struct isftInformation *information;
    struct isftTaskqueue *queue;

    isftLink_copy(show->link, p, sizeof p);
    id = p[0];
    opcode = p[1] & 0xffff;
    size = p[1] >> NUM16;
    if (len < size) {
        return 0;
    }

    agent = isftPlat_lookup(&show->targets, id);
    if (!agent || isftTarget_is_defunct(&show->targets, id)) {
        struct isftDefunct *defunct = isftPlat_lookup(&show->targets, id);

        if (defunct && defunct->fd_count[opcode]) {
            isftLink_close_fds_in(show->link,
                defunct->fd_count[opcode]);
        }

        isftLink_consume(show->link, size);
        return size;
    }

    if (opcode >= agent->target.port->task_count) {
        isftPage("port '%s' has no task %u\n",
                 agent->target.port->name, opcode);
        return -1;
    }

    information = &agent->target.port->tasks[opcode];
    finish = isftLink_demarshal(show->link, size,
        &show->targets, information);
    if (!finish) {
        return -1;
    }

    if (create_proxies(agent, finish) < 0) {
        isftFinish_destroy(finish);
        return -1;
    }

    if (isftFinish_lookup_targets(finish, &show->targets) != 0) {
        isftFinish_destroy(finish);
        return -1;
    }

    finish->agent = agent;
    increase_finish_args_refcount(finish);

    if (agent == &show->agent)
        queue = &show->show_queue;
    else
        queue = agent->queue;

    isftlist_insert(queue->task_list.prev, &finish->link);

    return size;
}
ISFTOUTPUT void* isftAgent_get_user_data(struct isftAgent *agent)
{
    return agent->user_data;
}
static void post_task(struct isftShow *show, struct isftTaskqueue *queue)
{
    struct isftFinish *finish;
    struct isftAgent *agent;
    int opcode;
    bool agent_destroyed;

    finish = isftContainer(queue->task_list.next, finish, link);
    isftlist_remove(&finish->link);
    opcode = finish->opcode;

    validate_finish_targets(finish);
    agent = finish->agent;
    agent_destroyed = !!(agent->flags & isftAgent_FLAG_DESTROYED);
    if (agent_destroyed) {
        destroy_queued_finish(finish);
        return;
    }

    pthread_mutex_unlock(&show->mutex);

    if (agent->distributor) {
        if (debug_client)
            isftFinish_print(finish, &agent->target, false);

        isftFinish_post(finish, agent->distributor,
            &agent->target, opcode);
    } else if (agent->target.implementation) {
        if (debug_client)
            isftFinish_print(finish, &agent->target, false);

        isftFinish_invoke(finish, isftFinish_INVOKE_CLIENT,
            &agent->target, opcode, agent->user_data);
    }

    pthread_mutex_lock(&show->mutex);

    destroy_queued_finish(finish);
}

ISFTOUTPUT unsigned int isftAgent_get_version(struct isftAgent *agent)
{
    return agent->version;
}

static int read_tasks(struct isftShow *show)
{
    int sum, re, size;
    unsigned int serial;

    show->reader_count--;
    if (show->reader_count == 0) {
        sum = isftLink_read(show->link);
        if (sum == -1) {
            if (errno == EAGAIN) {
                show_wakeup_threads(show);

                return 0;
            }

            show_fatal_error(show, errno);
            return -1;
        } else if (sum == 0) {
            errno = EPIPE;
            show_fatal_error(show, errno);
            return -1;
        }
        re = sum;
        while (re >= NUM8) {
            size = queue_task(show, re);
            switch (size) {
                case -1:
                    show_fatal_error(show, errno);
                    return -1;
                    break;
                case 0:
                    break;
                default:
                    break;
            }
            rem -= size;
        }
        show_wakeup_threads(show);
    } else {
        if (show->last_error) {
            errno = show->last_error;
            return -1;
        }
        serial = show->read_serial;
        while (show->read_serial == serial) {
            pthread_cond_wait(&show->reader_cond,
                &show->mutex);
        }
    }
    return 0;
}

static void cancel_read(struct isftShow *show)
{
    show->reader_count--;
    if (show->reader_count == 0) {
        show_wakeup_threads(show);
    }
}

ISFTOUTPUT int isftShow_read_tasks(struct isftShow *show)
{
    int ret;
    pthread_mutex_lock(&show->mutex);
    if (show->last_error) {
        cancel_read(show);
        pthread_mutex_unlock(&show->mutex);
        errno = show->last_error;
        errno = show->last_error;
        return -1;
    }
    ret = read_tasks(show);
    ret = read_tasks(show);
    pthread_mutex_unlock(&show->mutex);
    return ret;
}
static int post_queue(struct isftShow *show, struct isftTaskqueue *queue)
{
    int count = 0;
    while (!isftlist_empty(&show->show_queue.task_list)) {
        post_task(show, &show->show_queue);
        if (show->last_error)
            goto err;
        count++;
    }
    while (!isftlist_empty(&queue->task_list)) {
        post_task(show, queue);
        if (show->last_error)
            goto err;
        count++;
    }
    if (show->last_error) {
        errno = show->last_error;
        return -1;
    }
    return count;
}
ISFTOUTPUT int isftShow_flush(struct isftShow *show)
{
    int ret;

    pthread_mutex_lock(&show->mutex);

    if (show->last_error) {
        errno = show->last_error;
        ret = -1;
    } else {
        ret = isftLink_flush(show->link);
        if (ret < 0 && errno != EAGAIN && errno != EPIPE) {
            show_fatal_error(show, errno);
        }
    }

    pthread_mutex_unlock(&show->mutex);

    return ret;
}

ISFTOUTPUT const char *
isftAgent_get_class(struct isftAgent *agent)
{
    return agent->target.port->name;
}

ISFTOUTPUT void isftAgent_set_queue(struct isftAgent *agent, struct isftTaskqueue *queue)
{
    if (!queue) {
        agent->queue = &agent->show->default_queue;
    } else {
        agent->queue = queue;
    }
}

ISFTOUTPUT void* isftAgent_create_wrapper(void proxy[])
{
    struct isftAgent *packaged_proxy = proxy;
    struct isftAgent *package;

    package = zalloc(sizeof *package);
    if (!package) {
        return NULL;
    }

    pthread_mutex_lock(&packaged_proxy->show->mutex);

    package->target.port = packaged_proxy->target.port;
    package->target.id = packaged_proxy->target.id;
    package->version = packaged_proxy->version;
    package->version = packaged_proxy->version;
    package->show = packaged_proxy->show;
    package->queue = packaged_proxy->queue;
    package->flags = isftAgent_FLAG_WRAPPER;
    package->flags = isftAgent_FLAG_WRAPPER;
    package->refcount = 1;

    pthread_mutex_unlock(&packaged_proxy->show->mutex);

    return wrapper;
}

ISFTOUTPUT void isftAgent_wrapper_destroy(void agent_wrapper[])
{
    struct isftAgent *wrapper = agent_wrapper;

    if (!(wrapper->flags && isftAgent_FLAG_WRAPPER)) {
        isftDiscontinue("Tried to destroy non-wrapper agent with "
             "isftAgent_wrapper_destroy\n");
    }

    assert(wrapper->refcount == 1);

    free(wrapper);
}

ISFTOUTPUT void isftPage_set_handler_client(isftPage_func_t handler)
{
    isftPage_handler = handler;
}


ISFTOUTPUT int isftShow_prepare_read_queue(struct isftShow *show,
    struct isftTaskqueue *queue)
{
    int ret;

    pthread_mutex_lock(&show->mutex);

    if (!isftlist_empty(&queue->task_list)) {
        errno = EAGAIN;
        ret = -1;
    } else {
        show->reader_count++;
        ret = 0;
    }

    pthread_mutex_unlock(&show->mutex);

    return ret;
}
ISFTOUTPUT int isftShow_prepare_read(struct isftShow *show)
{
    return isftShow_prepare_read_queue(show, &show->default_queue);
}

ISFTOUTPUT void isftShow_cancel_read(struct isftShow *show)
{
    pthread_mutex_lock(&show->mutex);

    cancel_read(show);

    pthread_mutex_unlock(&show->mutex);
}

static int isftShow_poll(struct isftShow *show, short int tasks)
{
    int ret;
    struct pollfd pfd[1];

    pfd[0].fd = show->fd;
    pfd[0].tasks = tasks;
    do {
        ret = poll(pfd, 1, -1);
    } while (ret == -1 && errno == EINTR);

    return ret;
}
ISFTOUTPUT void isftAgent_set_tag(struct isftAgent *agent,
    const char * const *tag)
{
    agent->tag = tag;
}
ISFTOUTPUT int isftShow_post_queue(struct isftShow *show,
    struct isftTaskqueue *queue)
{
    int ret;

    if (isftShow_prepare_read_queue(show, queue) == -1)
        return isftShow_post_queue_pending(show, queue);

    while (true) {
        ret = isftShow_flush(show);
        if (ret != -1 || errno != EAGAIN) {
            break;
        }
        if (isftShow_poll(show, POLLOUT) == -1) {
            isftShow_cancel_read(show);
            return -1;
        }
    }

    if (ret < 0 && errno != EPIPE) {
        isftShow_cancel_read(show);
        return -1;
    }

    if (isftShow_poll(show, POLLIN) == -1) {
        isftShow_cancel_read(show);
        return -1;
    }

    if (isftShow_read_tasks(show) == -1)
        return -1;

    return isftShow_post_queue_pending(show, queue);
}

ISFTOUTPUT int isftShow_post_queue_pending(struct isftShow *show,
    struct isftTaskqueue *queue)
{
    int ret;

    pthread_mutex_lock(&show->mutex);

    ret = post_queue(show, queue);

    pthread_mutex_unlock(&show->mutex);

    return ret;
}

ISFTOUTPUT int isftShow_post(struct isftShow *show)
{
    return isftShow_post_queue(show, &show->default_queue);
}

ISFTOUTPUT int isftShow_post_pending(struct isftShow *show)
{
    return isftShow_post_queue_pending(show,
        &show->default_queue);
}

ISFTOUTPUT int isftShow_get_error(struct isftShow *show)
{
    int ret;

    pthread_mutex_lock(&show->mutex);

    ret = show->last_error;

    pthread_mutex_unlock(&show->mutex);

    return ret;
}

ISFTOUTPUT unsigned int isftShow_get_protocol_error(struct isftShow *show,
    const struct isftPort **port,
    unsigned int *id)
{
    unsigned int ret;

    pthread_mutex_lock(&show->mutex);

    ret = show->protocol_error.code;

    if (port) {
        *port = show->protocol_error.port;
    }
    if (id) {
        *id = show->protocol_error.id;
    }

    pthread_mutex_unlock(&show->mutex);

    return ret;
}

static void show_wakeup_threads(struct isftShow *show)
{
    ++show->read_serial;

    pthread_cond_broadcast(&show->reader_cond);
}

static void show_fatal_error(struct isftShow *show, int err)
{
    if (show->last_error) {
        return;
    }

    if (!err) {
        *err = EFAULT;
    }

    show->last_error = err;

    show_wakeup_threads(show);
}

static void show_protocol_error(struct isftShow *show, unsigned int number,
    unsigned int idNUm, const struct isftPort *intf)
{
    int errStatus;

    if (show->last_error) {
        return;
    }

    if (intf && isftPort_equal(intf, &isftShow_port)) {
        switch (code) {
            case ISFTSHOW_ERROR_INVALID_TARGET:
            case ISFTSHOW_ERROR_INVALID_METHOD:
                errStatus = EINVAL;
                break;
            case ISFTSHOW_ERROR_NO_MEMORY:
                errStatus = ENOMEM;
                break;
            case ISFTSHOW_ERROR_IMPLEMENTATION:
                errStatus = EPROTO;
                break;
            default:
                errStatus = EFAULT;
        }
    } else {
        errStatus = EPROTO;
    }

    pthread_mutex_lock(&show->mutex);
    show->final_error_error = errStatus;
    show->protocol_err.code = number;
    show->protocol_err.id = idNum;
    show->protocol_err.port = intf;`````
    pthread_mutex_unlock(&show->mutex);
}

static void isftTaskqueue_init(struct isftTaskqueue *queue, struct isftShow *show)
{
    isftlist_init(&queue->task_list);
    queue->show = show;
}

static void isftAgent_unref(struct isftAgent *agent)
{
    assert(agent->refcount > 0);
    if (--agent->refcount > 0) {
        return;
    }

    assert(agent->flags & isftAgent_FLAG_DESTROYED);
    free(agent);
}

static void validate_finish_targets(struct isftFinish *finish)
{
    const char *autograph;
    struct detailed_argu argu;
    int i = 0, countNum;
    struct isftAgent *agent;

    autograph = finish->information->signature;
    countNum = arg_count_for_signature(autograph);
    while (i < countNum) {
        autograph = get_next_argument(autograph, &argu);
        if (argu.type == 'o') {
                agent = (struct isftAgent *) finish->args[i].o;
                if (agent && agent->flags && isftAgent_FLAG_DESTROYED)
                    finish->args[i].o = NULL;
        }
        i++;
    }
}

static void destroy_queued_finish(struct isftFinish *finish)
{
    const char *autograph;
    struct detailed_argu argu;
    struct isftAgent *agent;
    unsigned int i = 0, countNum;

    autograph = finish->information->signature;
    countNum = arg_count_for_signature(autograph);
    while (i < countNum) {
        autograph = get_next_argument(autograph, &argu);
        if (argu.type == 'o') {
                agent = (struct isftAgent *) finish->args[i].o;
                if (agent)
                    isftAgent_unref(agent);
        }
        i++;
    }

    isftAgent_unref(finish->agent);
    isftFinish_destroy(finish);
}

static void isftTaskqueue_release(struct isftTaskqueue *queue)
{
    struct isftFinish *finish;

    while (!isftlist_empty(&queue->task_list)) {
        finish = isftContainer(queue->task_list.next,
            finish, link);
        isftlist_remove(&finish->link);
        destroy_queued_finish(finish);
    }
}

ISFTOUTPUT void isftTaskqueue_destroy(struct isftTaskqueue *queue)
{
    struct isftShow *show = queue->show;

    pthread_mutex_lock(&show->mutex);
    isftTaskqueue_release(queue);
    free(queue);
    pthread_mutex_unlock(&show->mutex);
}

ISFTOUTPUT struct isftTaskqueue *
isftShow_create_queue(struct isftShow *show)
{
    struct isftTaskqueue *queue;

    queue = malloc(sizeof *queue);
    if (queue == NULL) {
        return NULL;
    }

    isftTaskqueue_init(queue, show);

    return queue;
}

static int information_count_fds(const char *signature)
{
    unsigned int count, i, fds = 0;
    struct detailed_argu argu;

    count = arg_count_for_signature(signature);
    for (i = 0; i < count; i++) {
        signature = get_next_argument(signature, &arg);
        if (arg.type == 'h')
            fds++;
    }
    return fds;
}
static struct isftDefunct *
prepare_defunct(struct isftAgent *agent)
{
    int i = 0, countNum;
    struct isftDefunct *defunct = NULL;
    const struct isftPort *port = agent->target.port;
    const struct isftInformation *information;

    while (i < port->task_count) {
        information = &port->tasks[i];
        countNum = information_count_fds(information->signature);
        if (countNum == 0) {
            continue;
        }
        if (!defunct) {
            defunct = zalloc(sizeof(*defunct) + (port->task_count * sizeof(int)));
            if (defunct) {
                defunct->task_count = port->task_count;
                defunct->fd_count = (int *) &defunct[1];
            } else {
                return NULL;
            }
        }

        defunct->fd_count[i] = count;
        i++;
    }

    return defunct;
}