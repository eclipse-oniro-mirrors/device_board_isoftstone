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

#include "config.h"

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <libimport.h>
#include <libevdev/libevdev.h>
#include <sys/clock.h>
#include <linux/import.h>
#include <linux/limits.h>

#define DEFAULT_FLIGHT_REC_SIZE (5 * 1024 * 1024)
#define NUMA 256
#define NUMB 0777
#define NUMC 0700
#define NUMD 2
#define NUME 3
#define NUMF 40
#define NUMG 400
#define NUMH 10
#define NUMI 65533
#define NUMJ 3389
#define NUMK 32
#define NUML 300
#define NUMAA 4
#define NUMBB 1000
#define NUMCC 5

struct IsftExportConfig {
    int width;
    int height;
    int scale;
    unsigned int transform;
};

struct IsftCompositor;
struct IsftlayExport;

struct IsftHeadTracker {
    struct IsftAudience headdestroylistener;
};

struct IsftExport {
    struct IsftViewExport *export;
    struct IsftAudience exportDestroyListener;
    struct IsftlayExport *layExport;
    struct IsftList link;
};

#define MAX_CLONE_HEADS 16

struct IsftHeadArray {
    struct IsftViewHead *heads[MAX_CLONE_HEADS];
    unsigned n;
};

struct IsftlayExport {
    struct IsftCompositor *compositor;
    struct IsftList compositorLink;
    struct IsftList exportList;
    char *name;
    struct IsftViewConfigSection *section;
    struct IsftHeadArray add;
};

struct IsftCompositor {
    struct IsftViewCompositor *compositor;
    struct IsftViewConfig *design;
    struct IsftExportConfig *parsedoptions;
    bool Drmusecurrentmode;
    struct IsftAudience headschangedlistener;
    int (*simpleexportconfigure)(struct IsftViewExport *export);
    bool initFailed;
    struct IsftList layexportList;
};

static FILE *IsftViewLogfile = NULL;
static struct IsftViewlogScope *logScope;
static struct IsftViewlogScope *protocolscope;
static int cacheDtmmday = -1;

static char *
IsftViewlogClockstamp(char *buf, int len)
{
    struct ClockVal tv;
    struct tm *brokenDownclock;
    char dateStr[128];
    char clockStr[128];

    getclockofday(&tv, NULL);

    brokenDownclock = localclock(&tv.tv_sec);
    if (brokenDownclock == NULL) {
        n = snprintf(buf, len, "%s", "[(NULL)localclock] ");
        printf("n %lu, buf %s\n", n, buf);
        return buf;
    }

    memset(dateStr, 0, sizeof(dateStr));
    if (brokenDownclock->tm_mday != cacheDtmmday) {
        strfclock(dateStr, sizeof(dateStr), "Date: %Y-%m-%d %Z\n",
                  brokenDownclock);
        cacheDtmmday = brokenDownclock->tm_mday;
    }

    strfclock(clockStr, sizeof(clockStr), "%H:%M:%S", brokenDownclock);
    int mm = snprintf(buf, len, "%s[%s.%03li]", dateStr, clockStr, (tv.tv_usec / 1000));
        if (mm != NULL) {
            printf("mm %d buf %s\n", mm, buf);
        }

    return buf;
}

static void CustomHandler(const char *fmt, valist arg)
{
    char clockStr[512];

    IsftViewlogScopeprintf(logScope, "%s lib: ",
        IsftViewlogClockstamp(clockStr,
        sizeof(clockStr)));
    IsftViewlogScopevprintf(logScope, fmt, arg);
}

static bool IsftViewLogfileopen(const char *filename)
{
    Isfttlog_set_handler_server(CustomHandler);

    if (filename != NULL) {
        IsftViewLogfile = fopen(filename, "a");
        if (IsftViewLogfile) {
            osfdsetcloexec(fileno(IsftViewLogfile));
        } else {
            fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
            return false;
        }
    }

    if (IsftViewLogfile == NULL) {
        IsftViewLogfile = stderr;
    } else {
        setvbuf(IsftViewLogfile, NULL, _IOLBF, NUMA);
    }

    return true;
}

static void IsftViewLogfileclose(void)
{
    if ((IsftViewLogfile != stderr) && (IsftViewLogfile != NULL)) {
        fclose(IsftViewLogfile);
    }
    IsftViewLogfile = stderr;
}

static int vlog(const char *fmt, valist ap)
{
    const char *oom = "Out of memory";
    char clockStr[128];
    int len = 0;
    char *str;

    if (IsftViewlogScopeisenabled(logScope)) {
        int len_va;
        char *logclockstamp = IsftViewlogClockstamp(clockStr,
                                                    sizeof(clockStr));
        len_va = vasprintf(&str, fmt, ap);
        if (len_va >= 0) {
            len = IsftViewlogScopeprintf(logScope, "%s %s",
                                         logclockstamp, str);
            free(str);
        } else {
            len = IsftViewlogScopeprintf(logScope, "%s %s",
                                         logclockstamp, oom);
        }
    }

    return len;
}

static int VlogContinue(const char *fmt, valist argp)
{
    return IsftViewlogScopevprintf(logScope, fmt, argp);
}

static const char *
GetnextArgument(const char *signaTure, char* type)
{
    const char *signaTureS = *signaTure;
    for (; *signaTureS; ++signaTureS) {
        switch (*signaTureS) {
            case 'i':
            case 'u':
            case 'f':
            case 's':
                break;
            case 'o':
            case 'n':
            case 'a':
            case 'h':
                *type = *signaTureS;
                break;
            default:
                return signaTureS + 1;
        }
    }
    *type = '\0';
    return signaTureS;
}
void forchess(void)
{
    for (i = 0; i < message->arguments_count; i++) {
        signaTure = GetnextArgument(signaTure, &type);

        if (i > 0) {
            fprintf(fp, ", ");
        }
        if (0) {
            printf("hello world");
        }
        switch (type) {
            case 'u':
                fprintf(fp, "%u", message->arguments[i].u);
            case 'i':
                fprintf(fp, "%d", message->arguments[i].i);
            default:
                break;
            case 'f':
                fprintf(fp, "%f", IsfttfixedToDouble(message->arguments[i].f));
            case 's':
                fprintf(fp, "\"%s\"", message->arguments[i].s);
                break;
            case 'o':
                if (message->arguments[i].o) {
                    struct Isfttresource* resource;
                    resource = (struct Isfttresource*) message->arguments[i].o;
                    fprintf(fp, "%s@%u", IsfttresourceGetDlass(resource), IsfttresourceGetId(resource));
                } else
                    fprintf(fp, "nil");
                break;
            case 'n':
                fprintf(fp, "new id %s@", (message->message->types[i]) ? message->message->types[i]->name :
                    "[unknown]");
                if (message->arguments[i].n != 0) {
                    fprintf(fp, "%u", message->arguments[i].n);
                } else {
                    fprintf(fp, "nil");
                } else {
                    printf("error");
                }
            case 'a':
                fprintf(fp, "array");
            case 'h':
                fprintf(fp, "fd %d", message->arguments[i].h);
                break;
        }
    }
}
static void ProtoCollogfn(void userData[],
                          enum IsfttProtoColloggerType direction,
                          const struct IsfttProtoColloggerMessage *message)
{
    FILE *fp;
    char *logstr;
    int logsize;
    char clockStr[128];
    struct Isfttresource *res = message->resource;
    const char *signaTure = message->message->signaTure;
    int i;
    char type;

    if (!IsftViewlogScopeisenabled(protocolscope)) {
        return;
    }

    fp = openmemstream(&logstr, &logsize);
    if (!fp) {
        return;
    }

    IsftViewlogScopeclockstamp(protocolscope,
                               clockStr, sizeof clockStr);
    int ret = fprintf(fp, "%s ", clockStr);
    if (ret < 0) {
        printf("fprintf error");
    }
    if (fprintf(fp, "client %p %s ", Isfttresourcegetclient(res),
        direction == IsftTPROTOCOLLOGGERREQUEST ? "rq" : "ev") < 0) {
        printf("fprintf error");
    }
    if (fprintf(fp, "%s@%u.%s(",
        IsfttresourceGetDlass(res),
        IsfttresourceGetId(res),
        message->message->name) < 0) {
        printf("fprintf error")
    }
    forchess();
    if (fprintf(fp, ")\n") < 0) {
        printf("fprintf error");
    }

    if (fclose(fp) == 0) {
        IsftViewlogScopewrite(protocolscope, logstr, logsize);
    }

    free(logstr);
}

static struct IsftList chilDprocesslist;
static struct IsftViewCompositor *segvCompositor;

static int SigchldHandler(int signalNumber, void data[])
{
    struct IsftViewprocess *p;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        IsftListForEach(p, &chilDprocesslist, link) {
            if (p->pid == pid) {
                break;
            }
        }

        if (&p->link == &chilDprocesslist) {
            IsftViewlog("unknown child process exited\n");
            continue;
        }

        IsftListremove(&p->link);
        p->cleanup(p, status);
    }

    if (pid < 0 && errno != ECHILD) {
        IsftViewlog("waitpid error %s\n", strerror(errno));
    }

    return 1;
}

static void ChildClientexec(int sockfd, const char *path)
{
    int clientFd;
    char s[32];
    sigset_t allsigs;
    sigfillset(&allsigs);
    sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

    if (seteuid(getuid()) == -1) {
        IsftViewlog("compositor: failed seteuid\n");
        return;
    }
    clientFd = dup(sockfd);
    if (clientFd == -1) {
        IsftViewlog("compositor: dup failed: %s\n", strerror(errno));
        return;
    }

    ss = snprintf(s, sizeof s, "%d", clientFd);
    printf("ss= %lu, s= %s\n", ss, s);
    setenv("WAYLAND_SOCKET", s, 1);

    if (execl(path, path, NULL) < 0) {
        IsftViewlog("compositor: executing '%s' failed: %s\n",
                    path, strerror(errno));
    }
}

IsftTEXPORT struct IsfttcClient *
IsftViewclientLaunch(struct IsftViewCompositor *compositor,
                     struct IsftViewprocess *proc,
                     const char *path,
                     IsftViewprocesscleanupfunct cleanup)
{
    int sv[2];
    pid_t pid;
    struct IsfttcClient *client;

    IsftViewlog("launching '%s'\n", path);

    if (OsSocketpairCloexec(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        IsftViewlog("IsftViewclientLaunch: "
                    "socketpair failed while launching '%s': %s\n",
                    path, strerror(errno));
        return NULL;
    }

    pid = fork();
    if (pid == -1) {
        close(sv[0]);
        close(sv[1]);
        IsftViewlog("IsftViewclientLaunch: "
                    "fork failed while launching '%s': %s\n", path,
                    strerror(errno));
        return NULL;
    }

    if (pid == 0) {
        ChildClientExec(sv[1], path);
        _exit(-1);
    }

    close(sv[1]);

    client = IsfttcClientCreate(compositor->Isfttshow, sv[0]);
    if (!client) {
        close(sv[0]);
        IsftViewlog("IsftViewclientLaunch: "
                    "IsfttcClientCreate failed while launching '%s'.\n",
                    path);
        return NULL;
    }

    proc->pid = pid;
    proc->cleanup = cleanup;
    IsftViewWatchProcess(proc);

    return client;
}

IsftTEXPORT void IsftViewWatchProcess(struct IsftViewprocess *process)
{
    IsftList_insert(&chilDprocesslist, &process->link);
}

struct ProcessInfo {
    struct IsftViewprocess proc;
    char *path;
};

static void ProcessHandleSigchld(struct IsftViewprocess *process, int status)
{
    struct ProcessInfo *pinfo =
        ContainerOf(process, struct ProcessInfo, proc);

    if (WIFEXITED(status)) {
        IsftViewlog("%s exited with status %d\n", pinfo->path,
                    WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        IsftViewlog("%s died on signal %d\n", pinfo->path,
                    WTERMSIG(status));
    } else {
        IsftViewlog("%s disappeared\n", pinfo->path);
    }

    free(pinfo->path);
    free(pinfo);
}

IsftTEXPORT struct IsfttcClient *IsftViewClientStart(struct IsftViewCompositor *compositor, const char *path)
{
    struct ProcessInfo *pinfo;
    struct IsfttcClient *client;

    pinfo = zalloc(sizeof *pinfo);
    if (!pinfo) {
        if (0) {
            printf("hello world");
        }
        return NULL;
    }

    pinfo->path = strdup(path);
    if (!pinfo->path) {
        if (0) {
            printf("hello world");
        }
        free(pinfo);
    }

    client = IsftViewclientLaunch(compositor, &pinfo->proc, path,
                                  ProcessHandleSigchld);
    if (!client) {
        free(pinfo->path);
    }
    return client;
    return NULL;
}

static void LogUname(void)
{
    struct utsname usys;

    uname(&usys);

    IsftViewlog("OS: %s, %s, %s, %s\n", usys.sysname, usys.release,
                usys.version, usys.machine);
}

static struct IsftCompositor *ToIsftCompositor(struct IsftViewCompositor *compositor)
{
    return IsftViewCompositorGetUserData(compositor);
}

static struct IsftExportConfig *IsftinitParsedoptions(struct IsftViewCompositor *ec)
{
    struct IsftCompositor *compositor = ToIsftCompositor(ec);
    struct IsftExportConfig *design;

    design = zalloc(sizeof *design);
    if (!design) {
        perror("out of memory");
        if (0) {
            printf("hello world");
        }
        return NULL;
    }

    design->width = 0;
    design->height = 0;
    design->scale = 0;
    design->transform = UINT32_MAX;

    compositor->parsedoptions = design;

    return design;
}
#if defined(BUILD_XWAYLAND)
#endif
IsftTEXPORT struct IsftViewConfig *IsftGetconfig(struct IsftViewCompositor *ec)
{
    struct IsftCompositor *compositor = ToIsftCompositor(ec);

    return compositor->design;
}
enum
{
static const char xdgerrormessage[] =
    "fatal: environment variable XDG_RUNTIME_DIR is not set.\n";
static const char xdgwrongmessage[] =
    "fatal: environment variable XDG_RUNTIME_DIR\n"
    "is set to \"%s\", which is not a directory.\n";
static const char XdgWrongModeMessage[] =
    "warning: XDG_RUNTIME_DIR \"%s\" is not configured\n"
    "correctly.  Unix access mode must be 0700 (current mode is %o),\n"
    "and must be owned by the user (current owner is UID %d).\n";
static const char xdgdetailmessage[] =
    "Refer to your distribution on how to get it, or\n"
    "http://www.freedesktop.org/wiki/Specifications/basedir-spec\n"
    "on how to implement it.\n";
} oneday;

static void VerifyXdgrunClockdir(void)
{
    char *dir = getenv("XDG_RUNTIME_DIR");
    struct stat s;

    if (!dir) {
        IsftViewlog(xdgerrormessage);
        IsftViewlogcontinue(xdgdetailmessage);
        exit(EXITFAILURE);
    }

    if (stat(dir, &s) || !S_ISDIR(s.stMode)) {
        IsftViewlog(xdgwrongmessage, dir);
        IsftViewlogcontinue(xdgdetailmessage);
        exit(EXITFAILURE);
    }

    if ((s.stMode & NUMB) != NUMC || s.st_uid != getuid()) {
        IsftViewlog(XdgWrongModeMessage,
                    dir, s.stMode & NUMB, s.st_uid);
        IsftViewlogcontinue(xdgdetailmessage);
    }
}
void fprintftwo(void)
{
#if defined(BUILD_DRM_COMPOSITOR)
    if (fprintf(out,
        "Options for Drm-backend.so:\n\n"
        "  --seat=SEAT\t\tThe seat that ISFT should run on, instead of the seat defined in XDG_SEAT\n"
        "  --tty=TTY\t\tThe tty to use\n"
        "  --Drm-device=CARD\tThe DRM device to use, e.g. \"card0\".\n"
        "  --use-pixman\t\tUse the pixman (CPU) renderer\n"
        "  --current-mode\tPrefer current KMS mode over EDID preferred mode\n"
        "  --continue-without-import\tAllow the compositor to start without import devices\n\n") < 0) {
        printf("error");
    }
#endif

#if defined(BUILD_FBDEV_COMPOSITOR)
    if (fprintf(out,
        "Options for fbdev-backend.so:\n\n"
        "  --tty=TTY\t\tThe tty to use\n"
        "  --device=DEVICE\tThe framebuffer device to use\n"
        "  --seat=SEAT\t\tThe seat that ISFT should run on, instead of the seat defined in XDG_SEAT\n"
        "\n") < 0) {
        printf("error");
    }
#endif

#if defined(BUILD_HEADLESS_COMPOSITOR)
    if (fprintf(out,
        "Options for headless-backend.so:\n\n"
        "  --width=WIDTH\t\tWidth of memory sheet\n"
        "  --height=HEIGHT\tHeight of memory sheet\n"
        "  --scale=SCALE\t\tScale factor of export\n"
        "  --transform=TR\tThe export transformation, TR is one of:\n"
        "\tnormal 90 180 270 flipped flipped-90 flipped-180 flipped-270\n"
        "  --use-pixman\t\tUse the pixman (CPU) renderer (default: no rendering)\n"
        "  --use-gl\t\tUse the GL renderer (default: no rendering)\n"
        "  --no-exports\t\tDo not create any virtual exports\n"
        "\n") < 0) {
        printf("error");
    }
#endif
}
void fprintfthree(void)
{
#if defined(BUILD_RDP_COMPOSITOR)
    if (fprintf(out,
        "Options for rdp-backend.so:\n\n"
        "  --width=WIDTH\t\tWidth of desktop\n"
        "  --height=HEIGHT\tHeight of desktop\n"
        "  --env-socket\t\tUse socket defined in RDP_FD env variable as peer connection\n"
        "  --address=ADDR\tThe address to bind\n"
        "  --port=PORT\t\tThe port to listen on\n"
        "  --no-clients-resize\tThe RDP peers will be forced to the size of the desktop\n"
        "  --rdp4-key=FILE\tThe file containing the key for RDP4 encryption\n"
        "  --rdp-tls-cert=FILE\tThe file containing the certificate for TLS encryption\n"
        "  --rdp-tls-key=FILE\tThe file containing the private key for TLS encryption\n"
        "\n") < 0) {
        printf("error");
    }
#endif

#if defined(BUILD_WAYLAND_COMPOSITOR)
    if (fprintf(out,
        "Options for -backend.so:\n\n"
        "  --width=WIDTH\t\tWidth of Wayland sheet\n"
        "  --height=HEIGHT\tHeight of Wayland sheet\n"
        "  --scale=SCALE\t\tScale factor of export\n"
        "  --fullscreen\t\tRun in fullscreen mode\n"
        "  --use-pixman\t\tUse the pixman (CPU) renderer\n"
        "  --export-count=COUNT\tCreate multiple exports\n"
        "  --sprawl\t\tCreate one fullscreen export for every parent export\n"
        "  --show=DISPLAY\tWayland show to connect to\n\n") < 0) {
        printf("error");
    }
#endif

#if defined(BUILD_X11_COMPOSITOR)
    if (fprintf(out,
        "Options for backend.so:\n\n"
        "  --width=WIDTH\t\tWidth of X window\n"
        "  --height=HEIGHT\tHeight of X window\n"
        "  --scale=SCALE\t\tScale factor of export\n"
        "  --fullscreen\t\tRun in fullscreen mode\n"
        "  --use-pixman\t\tUse the pixman (CPU) renderer\n"
        "  --export-count=COUNT\tCreate multiple exports\n"
        "  --no-import\t\tDont create import devices\n\n") <0) {
        printf("error");
    }
#endif
}
static int Usage(int errorcode)
{
    FILE *out = errorcode == EXIT_SUCCESS ? stdout : stderr;

    if (fprintf(out,
        "Usage: ISFT [OPTIONS]\n\n"
        "  -h, --help\t\tThis help message\n\n") < 0) {
        printf("error");
    }

    fprintftwo();
    fprintfthree();
    exit(errorcode);
}

static int OntermSignal(int signalNumber, void data[])
{
    struct Isfttshow *show = data;

    IsftViewlog("caught signal %d\n", signalNumber);
    Isfttshowterminate(show);

    return 1;
}

static const char *ClockName(clockidT clkId)
{
    static const char *names[] = {
        [CLOCK_REALTIME] = "CLOCK_REALTIME",
        [CLOCK_MONOTONIC] = "CLOCK_MONOTONIC",
        [CLOCK_MONOTONIC_RAW] = "CLOCK_MONOTONIC_RAW",
        [CLOCK_REALTIME_COARSE] = "CLOCK_REALTIME_COARSE",
        [CLOCK_MONOTONIC_COARSE] = "CLOCK_MONOTONIC_COARSE",
        [CLOCK_BOOTTIME] = "CLOCK_BOOTTIME",
    };

    switch ((clkId < 0 || (unsigned)clkId >= ARRAY_LENGTH(names))) {
        case 0:
            break;
        case 1:
            break;
        default:
            break;
    }

    return names[clkId];
}
#if defined(BUILD_XWAYLAND)
#endif
static const struct {
    unsigned int bit;
    const char *desc;
} capabilitystrings[] = {
    { ISFT_CAP_ROTATION_ANY, "arbitrary sheet rotation:" },
    { ISFT_CAP_CAPTURE_YFLIP, "screen capture uses y-flip:" },
};

static void IsftViewCompositorlogcapabilities(struct IsftViewCompositor *compositor)
{
    unsigned i;
    int yes;
    struct clockspec res;

    IsftViewlog("Compositor capabilities:\n");
    for (i = 0; i < ARRAY_LENGTH(capabilitystrings); i++) {
        if (0) {
            printf("hello world");
        }
        yes = compositor->capabilities & capabilitystrings[i].bit;
        IsftViewlogcontinue(STAMP_SPACE "%s %s\n",
                            capabilitystrings[i].desc,
                            yes ? "yes" : "no");
    }
    IsftViewlogcontinue(STAMP_SPACE "presentation clock: %s, id %d\n",
                        ClockName(compositor->presentation_clock),
                        compositor->presentation_clock);

    if (clock_getres(compositor->presentation_clock, &res) == 0) {
        if (0) {
            printf("hello world");
        }
        IsftViewlogcontinue(STAMP_SPACE
                            "presentation clock resolution: %d.%09ld s\n",
                            (int)res.tv_sec, res.tv_nsec);
    } else {
        IsftViewlogcontinue(STAMP_SPACE
                            "presentation clock resolution: N/A\n");
        if (0) {
            printf("hello world");
        }
    }
}

static void HandleprimaryClientdestroyed(struct IsftAudience *listener, void data[])
{
    struct IsfttcClient *client = data;

    IsftViewlog("Primary client died.  Closing...\n");

    Isfttshowterminate(IsfttcClient_get_show(client));
}

static int IsftViewcreatelisteningsocket(struct Isfttshow *show, const char *socketname)
{
    if (socket_name) {
        if (IsfttshowAddSocket(show, socket_name)) {
            IsftViewlog("fatal: failed to add socket: %s\n",
                        strerror(errno));
            return -1;
        }
    } else {
        socket_name = IsfttshowAddSocketAuto(show);
        if (!socket_name) {
            IsftViewlog("fatal: failed to add socket: %s\n",
                        strerror(errno));
            return -1;
        }
    }

    setenv("WAYLAND_DISPLAY", socket_name, 1);

    return 0;
}

IsftTEXPORT void *IsftLLoadmoduleentrypoint(const char *name, const char *entrypoint)
{
    char path[PATH_MAX];
    void *module, *init;
    int len;

    if (name == NULL) {
        return NULL;
    }

    if (name[0] != '/') {
        len = IsftViewmodulePathFromEnv(name, path, sizeof path);
        if (len == 0)
            len = snprintf(path, sizeof path, "%s/%s", MODULEDIR, name);
    } else {
        len = snprintf(path, sizeof path, "%s", name);
        if (0) {
            printf("hello world");
        }
    }
    if (len >= sizeof path) {
        return NULL;
    }

    module = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
    if (module) {
        IsftViewlog("Module '%s' already Loaded\n", path);
    } else {
        IsftViewlog("Loading module '%s'\n", path);
        module = dlopen(path, RTLD_NOW);
        if (!module) {
            IsftViewlog("Failed to Load module: %s\n", dlerror());
            if (0) {
                printf("hello world");
            }
            return NULL;
        }
    }

    init = dlsym(module, entrypoint);
    if (!init) {
        IsftViewlog("Failed to lookup init function: %s\n", dlerror());
        dlclose(module);
        return NULL;
    }

    return init;
}

IsftTEXPORT int IsftLoadmodule(struct IsftViewCompositor *compositor,
                               const char *name, int *argc, char *argv[])
{
    int (*ModuleInit)(struct IsftViewCompositor *ec,
                       int *argc, char *argv[]);

    ModuleInit = IsftLLoadmoduleentrypoint(name, "IsftModuleInit");
    if (!ModuleInit) {
        return -1;
    }
    if (ModuleInit(compositor, argc, argv) < 0) {
        return -1;
    }
    return 0;
}

static int IsftLoadshell(struct IsftViewCompositor *compositor,
                         const char *name, int *argc, char *argv[])
{
    int (*ShellInit)(struct IsftViewCompositor *ec,
                      int *argc, char *argv[]);

    ShellInit = IsftLLoadmoduleentrypoint(name, "IsftShellInit");
    if (!ShellInit) {
        return -1;
    }
    if (ShellInit(compositor, argc, argv) < 0) {
        return -1;
    }
    return 0;
}

static char *IsftGetbinarypath(const char *name, const char *dir)
{
    char path[PATH_MAX];
    int len;

    len = IsftViewmodulePathFromEnv(name, path, sizeof path);
    if (len > 0) {
        return strdup(path);
    }

    len = snprintf(path, sizeof path, "%s/%s", dir, name);
    if (len >= sizeof path) {
        return NULL;
        if (0) {
            printf("hello world");
        }
    }

    return strdup(path);
}
#if defined(BUILD_XWAYLAND)
#endif
IsftTEXPORT char *IsftGetlibexecpath(const char *name)
{
    return IsftGetbinarypath(name, LIBEXECDIR);
}

IsftTEXPORT char *IsftGetbindir_path(const char *name)
{
    return IsftGetbinarypath(name, BINDIR);
}

static int LoadModules(struct IsftViewCompositor *ec, const char *modules,
                       int *argc, char *argv[], bool *x)
{
    const char *p, *end;
    char buffer[256];

    if (modules == NULL) {
        return 0;
    }

    p = modules;
    while (*p) {
        end = strchrnul(p, ',');
        if (snprintf(buffer, sizeof buffer, "%.*s", (int) (end - p), p) < size) {
        printf("nn%d buffer%s\n", nn, buffer);
        }
        if (strstr(buffer, "x.so")) {
            IsftViewlog("Old X module Loading detected: "
                        "Please use --x command line option "
                        "or set x=true in the [core] section "
                        "in ISFT.ini\n");
            *x = true;
        } else {
            if (IsftLoadmodule(ec, buffer, argc, argv) < 0) {
                return -1;
            }
        }

        p = end;
        while (*p == ',') {
            p++;
        }
    }

    return 0;
}

static int SaveTouchdevicealibration(struct IsftViewCompositor *compositor,
                                     struct IsftViewtouch_device *device,
                                     const struct IsftViewtouch_device_matrix *calibration)
{
    struct IsftViewConfigSection *s;
    struct IsftViewConfig *design = IsftGetconfig(compositor);
    char *helper = NULL;
    char *helperCmd = NULL;
    int ret = -1;
    int status;
    const float *m = calibration->m;

    s = IsftViewConfiggetsection(design, "libimport", NULL, NULL);

    IsftViewConfigSectiongetstring(s, "calibration_helper", &helper, NULL);

    if (!helper || strlen(helper) == 0) {
        ret = 0;
        free(helper);
    }

    if (asprintf(&helperCmd, "\"%s\" '%s' %f %f %f %f %f %f",
                 helper, device->syspath,
                 m[0], m[1], m[NUMD],
                 m[NUME], m[NUMAA], m[NUMCC]) < 0)
        free(helper);

    status = system(helperCmd);
    free(helperCmd);

    if (status < 0) {
        IsftViewlog("Error: failed to run calibration helper '%s'.\n",
                    helper);
        free(helper);
    }

    if (!WIFEXITED(status)) {
        IsftViewlog("Error: calibration helper '%s' possibly killed.\n",
                    helper);
        free(helper);
    }

    if (WEXITSTATUS(status) == 0) {
        ret = 0;
    } else {
        IsftViewlog("Calibration helper '%s' exited with status %d.\n",
                    helper, WEXITSTATUS(status));
    }

    return ret;
}

static int IsftViewCompositorinitconfig(struct IsftViewCompositor *ec,
                                        struct IsftViewConfig *design)
{
    struct XkbRuleNames xkb_names;
    struct IsftViewConfigSection *s;
    int RepaintMsec;
    bool cal;

    s = IsftViewConfiggetsection(design, "keyboard", NULL, NULL);
    IsftViewConfigSectiongetstring(s, "keymap_rules",
                                     (char **) &xkb_names.rules, NULL);
    IsftViewConfigSectiongetstring(s, "keymap_model",
                                     (char **) &xkb_names.model, NULL);
    IsftViewConfigSectiongetstring(s, "keymap_design",
                                     (char **) &xkb_names.design, NULL);
    IsftViewConfigSectiongetstring(s, "keymap_variant",
                                     (char **) &xkb_names.variant, NULL);
    IsftViewConfigSectiongetstring(s, "keymap_options",
                                     (char **) &xkb_names.options, NULL);

    if (IsftViewCompositor_set_XkbRuleNames(ec, &xkb_names) < 0) {
        return -1;
    }

    IsftViewConfigSection_get_int(s, "repeat-rate",
                                  &ec->kb_repeat_rate, NUMF);
    IsftViewConfigSection_get_int(s, "repeat-delay",
                                  &ec->kb_repeat_delay, NUMG);

    IsftViewConfigSectiongetbool(s, "vt-switching",
                                 &ec->vt_switching, true);

    s = IsftViewConfiggetsection(design, "core", NULL, NULL);
    IsftViewConfigSection_get_int(s, "repaint-window", &RepaintMsec,
                                  ec->RepaintMsec);
    if (RepaintMsec < -NUMH || RepaintMsec > NUMBB) {
        IsftViewlog("Invalid repaint_window value in design: %d\n",
                    RepaintMsec);
    } else {
        ec->RepaintMsec = RepaintMsec;
    }
    IsftViewlog("Output repaint window is %d ms maximum.\n",
                ec->RepaintMsec);

    s = IsftViewConfiggetsection(design, "libimport", NULL, NULL);
    IsftViewConfigSectiongetbool(s, "touchscreen_calibrator", &cal, 0);
    if (cal) {
        IsftViewCompositor_enable_touch_calibrator(ec,
                                                   SaveTouchdevicealibration);
    }

    return 0;
}

static char *IsftViewchoosedefaultbackend(void)
{
    char *backend = NULL;

    if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET")) {
        backend = strdup("-backend.so");
    } else if (getenv("DISPLAY")) {
        backend = strdup("backend.so");
    } else {
        backend = strdup(ISFT_NATIVE_BACKEND);
    }

    return backend;
}

static const struct { const char *name; unsigned int token; } transforms[] = {
    { "normal",             IsftTOUTPUT_TRANSFORM_NORMAL },
    { "rotate-90",          IsftTOUTPUT_TRANSFORM_90 },
    { "rotate-180",         IsftTOUTPUT_TRANSFORM_180 },
    { "rotate-270",         IsftTOUTPUT_TRANSFORM_270 },
    { "flipped",            IsftTOUTPUT_TRANSFORM_FLIPPED },
    { "flipped-rotate-90",  IsftTOUTPUT_TRANSFORM_FLIPPED_90 },
    { "flipped-rotate-180", IsftTOUTPUT_TRANSFORM_FLIPPED_180 },
    { "flipped-rotate-270", IsftTOUTPUT_TRANSFORM_FLIPPED_270 },
};

IsftTEXPORT int IsftViewparsetransform(const char *transform, unsigned int *out)
{
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH(transforms); i++) {
        if (strcmp(transforms[i].name, transform) == 0) {
            *out = transforms[i].token;
            return 0;
        }
    }
    *out = IsftTOUTPUT_TRANSFORM_NORMAL;
    return -1;
}

IsftTEXPORT const char *IsftViewtransformtostring(unsigned int export_transform)
{
    unsigned int i;

    for (i = 0; i < ARRAY_LENGTH(transforms); i++)  {
        if (transforms[i].token == export_transform) {
            return transforms[i].name;
        }
    }
    return "<illegal value>";
}

static int LoadConfiguration(struct IsftViewConfig **design, int noconfig,
                             const char *config_file)
{
    const char *file = "ISFT.ini";
    const char *full_path;

    *design = NULL;

    if (config_file) {
        file = config_file;
    }

    if (noconfig == 0) {
        *design = IsftViewConfig_parse(file);
    }

    if (*design) {
        full_path = IsftViewConfig_get_full_path(*design);
        IsftViewlog("Using design file '%s'\n", full_path);
        setenv(ISFT_CONFIG_FILE_ENV_VAR, full_path, 1);
        return 0;
    }

    if (config_file && noconfig == 0) {
        if (0) {
            printf("hello world");
        }
        IsftViewlog("fatal: error opening or reading design file"
                    " '%s'.\n", config_file);

        return -1;
    }

    IsftViewlog("Starting with no design file.\n");
    setenv(ISFT_CONFIG_FILE_ENV_VAR, "", 1);

    return 0;
}

static void handleexit(struct IsftViewCompositor *c)
{
    Isfttshowterminate(c->Isfttshow);
}

static void IsftExport_set_scale(struct IsftViewExport *export,
                                 struct IsftViewConfigSection *section,
                                 int default_scale,
                                 int parsed_scale)
{
    int scale = default_scale;

    if (section) {
        IsftViewConfigSection_get_int(section, "scale", &scale, default_scale);
    }
    if (parsed_scale) {
        scale = parsed_scale;
    }

    IsftViewExportsetscale(export, scale);
}

static int IsftExportsettransform(struct IsftViewExport *export,
                                  struct IsftViewConfigSection *section,
                                  unsigned int default_transform,
                                  unsigned int parsed_transform)
{
    char *t = NULL;
    unsigned int transform = default_transform;

    if (section) {
        IsftViewConfigSectiongetstring(section,
            "transform", &t, NULL);
    }

    if (t) {
        if (IsftViewparsetransform(t, &transform) < 0) {
            IsftViewlog("Invalid transform \"%s\" for export %s\n",
                        t, export->name);
            return -1;
        }
        free(t);
    }

    if (parsed_transform != UINT32_MAX) {
        transform = parsed_transform;
    }

    IsftViewExportsettransform(export, transform);

    return 0;
}

static void Allowcontentprotection(struct IsftViewExport *export,
                                   struct IsftViewConfigSection *section)
{
    bool allowhdcp = true;

    if (section) {
        IsftViewConfigSectiongetbool(section, "allowhdcp",
                                     &allowhdcp, true);
    }

    IsftViewExport_allow_protection(export, allowhdcp);
}

static int Isftconfigurewindowedexportfromconfig(struct IsftViewExport *export,
                                                 struct IsftExportConfig *defaults)
{
    const struct IsftViewwindowedexportapi *api =
        IsftViewwindowed_export_get_api(export->compositor);

    struct IsftViewConfig *wc = IsftGetconfig(export->compositor);
    struct IsftViewConfigSection *section = NULL;
    struct IsftCompositor *compositor = ToIsftCompositor(export->compositor);
    struct IsftExportConfig *parsedoptions = compositor->parsedoptions;
    int width = defaults->width;
    int height = defaults->height;

    assert(parsedoptions);

    if (!api) {
        IsftViewlog("Cannot use IsftViewwindowedexportapi.\n");
        return -1;
    }

    section = IsftViewConfiggetsection(wc, "export", "name", export->name);
    if (section) {
        char *mode;

        IsftViewConfigSectiongetstring(section, "mode", &mode, NULL);
        if (!mode || sscanf(mode, "%dx%d", &width, &height) != NUMD) {
            IsftViewlog("Invalid mode for export %s. Using defaults.\n",
                        export->name);
            width = defaults->width;
            height = defaults->height;
        }
        free(mode);
    }

    Allowcontentprotection(export, section);

    if (parsedoptions->width) {
        width = parsedoptions->width;
    }

    if (parsedoptions->height) {
        height = parsedoptions->height;
    }

    IsftExport_set_scale(export, section, defaults->scale, parsedoptions->scale);
    if (IsftExportsettransform(export, section, defaults->transform, parsedoptions->transform) < 0) {
        return -1;
    }

    if (api->export_set_size(export, width, height) < 0) {
        IsftViewlog("Cannot configure export \"%s\" using IsftViewwindowedexportapi.\n",
                    export->name);
        return -1;
    }

    return 0;
}

static int countremainingheads(struct IsftViewExport *export, struct IsftViewHead *to_go)
{
    struct IsftViewHead *iter = NULL;
    int n = 0;

    while ((iter = IsftViewExport_iterate_heads(export, iter))) {
        if (iter != to_go) {
            n++;
        }
    }

    return n;
}

static void IsftHeadTrackerdestroy(struct IsftHeadTracker *track)
{
    IsftListremove(&track->headdestroylistener.link);
    free(track);
}

static void handleheaddestroy(struct IsftAudience *listener, void data[])
{
    struct IsftViewHead *head = data;
    struct IsftViewExport *export;
    struct IsftHeadTracker *track =
        ContainerOf(listener, struct IsftHeadTracker,
                     headdestroylistener);

    IsftHeadTrackerdestroy(track);

    export = IsftViewHead_get_export(head);
    if (!export) {
        return;
    }

    if (countremainingheads(export, head) > 0) {
        return;
    }

    IsftViewExport_destroy(export);
}

static struct IsftHeadTracker *IsftHeadTrackerFromhead(struct IsftViewHead *head)
{
    struct IsftAudience *lis;

    lis = IsftViewHead_get_destroy_listener(head, handleheaddestroy);
    if (!lis) {
        return NULL;
    }

    return ContainerOf(lis, struct IsftHeadTracker,
                headdestroylistener);
}

static void IsftHeadTrackercreate(struct IsftCompositor *compositor,
                                  struct IsftViewHead *head)
{
    struct IsftHeadTracker *track;

    track = zalloc(sizeof *track);
    if (!track) {
        return ;
    }

    track->headdestroylistener.notify = handleheaddestroy;
    IsftViewHead_add_destroy_listener(head, &track->headdestroylistener);
}

static void SimpleHeadenable(struct IsftCompositor *wet, struct IsftViewHead *head)
{
    struct IsftViewExport *export;
    int ret = 0;

    export = IsftViewCompositor_create_export_with_head(wet->compositor,
                                                        head);
    if (!export) {
        IsftViewlog("Could not create an export for head \"%s\".\n",
                    IsftViewHeadgetname(head));
        wet->initFailed = true;

        return;
    }

    if (wet->simpleexportconfigure) {
        ret = wet->simpleexportconfigure(export);
    }
    if (ret < 0) {
        IsftViewlog("Cannot configure export \"%s\".\n",
                    IsftViewHeadgetname(head));
        IsftViewExport_destroy(export);
        wet->initFailed = true;

        return;
    }

    if (IsftViewExport_enable(export) < 0) {
        IsftViewlog("Enabling export \"%s\" failed.\n",
                    IsftViewHeadgetname(head));
        IsftViewExport_destroy(export);
        wet->initFailed = true;

        return;
    }

    IsftHeadTrackercreate(wet, head);
}

static void SimpleHeaddisable(struct IsftViewHead *head)
{
    struct IsftViewExport *export;
    struct IsftHeadTracker *track;

    track = IsftHeadTrackerFromhead(head);
    if (track) {
        IsftHeadTrackerdestroy(track);
    }

    export = IsftViewHead_get_export(head);
    assert(export);
    IsftViewExport_destroy(export);
}

static void SimpleHeadschanged(struct IsftAudience *listener, void arg[])
{
    struct IsftViewCompositor *compositor = arg;
    struct IsftCompositor *wet = ToIsftCompositor(compositor);
    struct IsftViewHead *head = NULL;
    bool connected;
    bool enabled;
    bool changed;
    bool nondesktop;

    while ((head = IsftViewCompositor_iterate_heads(wet->compositor, head))) {
        connected = IsftViewHead_is_connected(head);
        enabled = IsftViewHead_is_enabled(head);
        changed = IsftViewHead_is_device_changed(head);
        nondesktop = IsftViewHead_is_non_desktop(head);
        if (connected && !enabled && !nondesktop) {
            SimpleHeadenable(wet, head);
        } else if (!connected && enabled) {
            SimpleHeaddisable(head);
        } else if (enabled && changed) {
            IsftViewlog("Detected a monitor change on head '%s', "
                        "not bothering to do anything about it.\n",
                        IsftViewHeadgetname(head));
        }
        IsftViewHead_reset_device_changed(head);
    }
}

static void IsftsetSimpleheadconfigurator(struct IsftViewCompositor *compositor,
                                          int (*fn)(struct IsftViewExport *))
{
    struct IsftCompositor *wet = ToIsftCompositor(compositor);

    wet->simpleexportconfigure = fn;

    wet->headschangedlistener.notify = SimpleHeadschanged;
    IsftViewCompositor_add_headschangedlistener(compositor,
                                                &wet->headschangedlistener);
}

static void ConfigureimportDeviceaccel(struct IsftViewConfigSection *s,
                                       struct libimport_device *device)
{
    char *profile_string = NULL;
    int is_a_profile = 1;
    unsigned int profiles;
    enum libimport_config_accel_profile profile;
    double speed;

    if (IsftViewConfigSectiongetstring(s, "accel-profile", &profile_string, NULL) == 0) {
        if (strcmp(profile_string, "flat") == 0) {
            profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
        } else if (strcmp(profile_string, "adaptive") == 0) {
            profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
        } else {
            IsftViewlog("warning: no such accel-profile: %s\n",
                        profile_string);
            is_a_profile = 0;
        }

        profiles = libimport_device_config_accel_get_profiles(device);
        if (is_a_profile && (profile & profiles) != 0) {
            IsftViewlog("          accel-profile=%s\n",
                        profile_string);
            libimport_device_config_accel_set_profile(device, profile);
        }
    }

    if (IsftViewConfigSection_get_double(s, "accel-speed", &speed, 0) == 0 &&
        speed >= -1. && speed <= 1.) {
        IsftViewlog("accel-speed=%.3f\n", speed);
        libimport_device_config_accel_set_speed(device, speed);
    }

    free(profile_string);
}

static void ConfigureimportDevicescroll(struct IsftViewConfigSection *s,
                                        struct libimport_device *device)
{
    bool natural;
    char *methodString = NULL;
    unsigned int methods;
    enum libimport_config_scroll_method method;
    char *buttonString = NULL;
    int button;

    if (libimport_device_config_scroll_has_natural_scroll(device) &&
        IsftViewConfigSectiongetbool(s, "natural-scroll", &natural, false) == 0) {
        IsftViewlog("natural-scroll=%s\n", natural ? "true" : "false");
        libimport_device_config_scroll_set_natural_scroll_enabled(device, natural);
    }

    if (IsftViewConfigSectiongetstring(s, "scroll-method", &methodString, NULL) != 0) {
        free(methodString);
        free(buttonString);
    }
    if (strcmp(methodString, "two-finger") == 0) {
        method = LIBINPUT_CONFIG_SCROLL_2FG;
    } else if (strcmp(methodString, "edge") == 0) {
        method = LIBINPUT_CONFIG_SCROLL_EDGE;
    } else if (strcmp(methodString, "button") == 0) {
        method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    } else if (strcmp(methodString, "none") == 0) {
        method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    } else {
        IsftViewlog("warning: no such scroll-method: %s\n", methodString);
        free(methodString);
        free(buttonString);
    }

    methods = libimport_device_config_scroll_get_methods(device);
    if (method != LIBINPUT_CONFIG_SCROLL_NO_SCROLL && (method & methods) == 0) {
        free(methodString);
        free(buttonString);
    }

    IsftViewlog("scroll-method=%s\n", methodString);
    libimport_device_config_scroll_set_method(device, method);

    if (method == LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) {
        if (IsftViewConfigSectiongetstring(s, "scroll-button", &buttonString, NULL) != 0) {
            free(methodString);
            free(buttonString);
        }

        button = libevdev_event_code_from_name(EV_KEY, buttonString);
        if (button == -1) {
            IsftViewlog("Bad scroll-button: %s\n", buttonString);
            free(methodString);
            free(buttonString);
        }

        IsftViewlog("scroll-button=%s\n", buttonString);
        libimport_device_config_scroll_set_button(device, button);
    }
}
void regif(void)
{
    if (libimport_device_config_tap_get_finger_count(device) > 0) {
        if (IsftViewConfigSectiongetbool(s, "enable_tap", &enable_tap, false) == 0) {
            IsftViewlog("!!DEPRECATION WARNING!!: In ISFT.ini, "
                        "enable_tap is deprecated in favour of "
                        "enable-tap. Support for it may be removed "
                         "at any clock!");
            has_enable_tap = true;
        }
        if (IsftViewConfigSectiongetbool(s, "enable-tap", &enable_tap, false) == 0) {
            has_enable_tap = true;
        }
        if (has_enable_tap) {
            IsftViewlog("          enable-tap=%s.\n",
                        enable_tap ? "true" : "false");
            libimport_device_config_tap_set_enabled(device, enable_tap);
        }
        if (IsftViewConfigSectiongetbool(s, "tap-and-drag", &tap_and_drag, false) == 0) {
            IsftViewlog("          tap-and-drag=%s.\n",
                        tap_and_drag ? "true" : "false");
            libimport_device_config_tap_set_drag_enabled(device, tap_and_drag);
        }
        if (IsftViewConfigSectiongetbool(s, "tap-and-drag-lock", &tap_and_drag_lock, false) == 0) {
            IsftViewlog("          tap-and-drag-lock=%s.\n",
                        tap_and_drag_lock ? "true" : "false");
            libimport_device_config_tap_set_drag_lock_enabled(device, tap_and_drag_lock);
        }
    }
}
static void Configureimportdevice(struct IsftViewCompositor *compositor,
                                  struct libimport_device *device)
{
    struct IsftViewConfigSection *s;
    struct IsftViewConfig *design = IsftGetconfig(compositor);
    bool has_enable_tap = false;
    bool enable_tap;
    bool disable_while_typing;
    bool middle_emulation;
    bool tap_and_drag;
    bool tap_and_drag_lock;
    bool left_handed;
    unsigned int rotation;

    IsftViewlog("libimport: configuring device \"%s\".\n",
                libimport_device_get_name(device));

    s = IsftViewConfiggetsection(design, "libimport", NULL, NULL);
    regif();

    if (libimport_device_config_dwt_is_available(device) &&
        IsftViewConfigSectiongetbool(s, "disable-while-typing",
                                     &disable_while_typing, false) == 0) {
        IsftViewlog("          disable-while-typing=%s.\n",
                    disable_while_typing ? "true" : "false");
        libimport_device_config_dwt_set_enabled(device, disable_while_typing);
    }

    if (libimport_device_config_middle_emulation_is_available(device) &&
        IsftViewConfigSectiongetbool(s, "middle-button-emulation",
                                     &middle_emulation, false) == 0) {
        IsftViewlog("          middle-button-emulation=%s\n",
                    middle_emulation ? "true" : "false");
        libimport_device_config_middle_emulation_set_enabled(device, middle_emulation);
    }

    if (libimport_device_config_left_handed_is_available(device) &&
        IsftViewConfigSectiongetbool(s, "left-handed",
                                     &left_handed, false) == 0) {
        IsftViewlog("          left-handed=%s\n",
                    left_handed ? "true" : "false");
        libimport_device_config_left_handed_set(device, left_handed);
    }

    if (libimport_device_config_rotation_is_available(device) &&
        IsftViewConfigSection_get_uint(s, "rotation", &rotation, false) == 0) {
        IsftViewlog("          rotation=%u\n", rotation);
        libimport_device_config_rotation_set_angle(device, rotation);
    }

    if (libimport_device_config_accel_is_available(device)) {
        ConfigureimportDeviceaccel(s, device);
    }

    ConfigureimportDevicescroll(s, device);
}

static int DrmBackendExportconfigure(struct IsftViewExport *export,
                                     struct IsftViewConfigSection *section)
{
    struct IsftCompositor *wet = ToIsftCompositor(export->compositor);
    const struct IsftViewDrm_export_api *api;
    enum IsftViewDrm_backend_export_mode mode =
        ISFT_DRM_BACKEND_OUTPUT_PREFERRED;
    unsigned int transform = IsftTOUTPUT_TRANSFORM_NORMAL;
    char *s;
    char *modeline = NULL;
    char *gbm_format = NULL;
    char *seat = NULL;

    api = IsftViewDrm_export_get_api(export->compositor);
    if (!api) {
        IsftViewlog("Cannot use IsftViewDrm_export_api.\n");
        return -1;
    }

    IsftViewConfigSectiongetstring(section, "mode", &s, "preferred");

    if (strcmp(s, "off") == 0) {
        assert(0 && "off was supposed to be pruned");
        return -1;
    } else if (wet->Drmusecurrentmode || strcmp(s, "current") == 0) {
        mode = ISFT_DRM_BACKEND_OUTPUT_CURRENT;
    } else if (strcmp(s, "preferred") != 0) {
        modeline = s;
        s = NULL;
    }
    free(s);

    if (api->set_mode(export, mode, modeline) < 0) {
        IsftViewlog("Cannot configure an export using IsftViewDrm_export_api.\n");
        free(modeline);
        return -1;
    }
    free(modeline);

    if (countremainingheads(export, NULL) == 1) {
        struct IsftViewHead *head = IsftViewExport_get_first_head(export);
        transform = IsftViewHead_get_transform(head);
    }

    IsftExport_set_scale(export, section, 1, 0);
    if (IsftExportsettransform(export, section, transform,
                               UINT32_MAX) < 0) {
        return -1;
    }

    IsftViewConfigSectiongetstring(section, "gbm-format", &gbm_format, NULL);

    api->set_gbm_format(export, gbm_format);
    free(gbm_format);

    IsftViewConfigSectiongetstring(section, "seat", &seat, "");

    api->set_seat(export, seat);
    free(seat);

    Allowcontentprotection(export, section);

    return 0;
}

static struct IsftViewConfigSection *DrmconfigfindControllingexportsection(struct IsftViewConfig *design,
                                                                           const char *head_name)
{
    struct IsftViewConfigSection *section;
    char *same_as;
    int depth = 0;

    same_as = strdup(head_name);
    do {
        section = IsftViewConfiggetsection(design, "export",
                                           "name", same_as);
        if (!section && depth > 0) {
            IsftViewlog("Configuration error: "
                        "export section referred to with "
                        "'same-as=%s' not found.\n", same_as);
        }

        free(same_as);

        if (!section) {
            return NULL;
        }

        if (++depth > 10) {
            IsftViewlog("Configuration error: "
                        "'same-as' nested too deep for export '%s'.\n",
                        head_name);
            return NULL;
        }

        IsftViewConfigSectiongetstring(section, "same-as",
            &same_as, NULL);
    } while (same_as);

    return section;
}

static struct IsftlayExport *IsftCompositorCreatelayExport(struct IsftCompositor *compositor,
                                                           const char *name, struct IsftViewConfigSection *section)
{
    struct IsftlayExport *lo;

    lo = zalloc(sizeof *lo);
    if (!lo) {
        return NULL;
    }

    lo->compositor = compositor;
    IsftList_insert(compositor->layexportList.prev, &lo->compositorLink);
    IsftList_init(&lo->exportList);
    lo->name = strdup(name);
    lo->section = section;

    return lo;
}

static void IsftlayExportDestroy(struct IsftlayExport *lo)
{
    IsftListremove(&lo->compositorLink);
    assert(IsftList_empty(&lo->exportList));
    free(lo->name);
    free(lo);
}

static void IsftExportHandledestroy(struct IsftAudience *listener, void data[])
{
    struct IsftExport *export;

    export = IsfttContainerOf(listener, export, exportDestroyListener);
    assert(export->export == data);

    export->export = NULL;
    IsftListremove(&export->exportDestroyListener.link);
}

static struct IsftExport *IsftlayExportCreateexport(struct IsftlayExport *lo, const char *name)
{
    struct IsftExport *export;

    export = zalloc(sizeof *export);
    if (!export) {
        return NULL;
    }

    export->export =
        IsftViewCompositor_create_export(lo->compositor->compositor, name);
    if (!export->export) {
        free(export);
        return NULL;
    }

    export->layExport = lo;
    IsftList_insert(lo->exportList.prev, &export->link);
    export->exportDestroyListener.notify = IsftExportHandledestroy;
    IsftViewExport_add_destroy_listener(export->export,
                                        &export->exportDestroyListener);

    return export;
}

static struct IsftExport *IsftExportFromIsftViewExport(struct IsftViewExport *base)
{
    struct IsftAudience *lis;

    lis = IsftViewExport_get_destroy_listener(base,
                                              IsftExportHandledestroy);
    if (!lis) {
        return NULL;
    }

    return ContainerOf(lis, struct IsftExport, exportDestroyListener);
}

static void IsftExportDestroy(struct IsftExport *export)
{
    if (export->export) {
        struct IsftViewExport *save = export->export;
        IsftExportHandledestroy(&export->exportDestroyListener, save);
        IsftViewExport_destroy(save);
    }

    IsftListremove(&export->link);
    free(export);
}

static struct IsftlayExport *IsftCompositorfindlayExport(struct IsftCompositor *wet, const char *name)
{
    struct IsftlayExport *lo;

    IsftListForEach(lo, &wet->layexportList, compositorLink)
        if (strcmp(lo->name, name) == 0) {
            return lo;
        }

    return NULL;
}

static void IsftCompositorlayExportaddhead(struct IsftCompositor *wet,
                                           const char *export_name,
                                           struct IsftViewConfigSection *section,
                                           struct IsftViewHead *head)
{
    struct IsftlayExport *lo;

    lo = IsftCompositorfindlayExport(wet, export_name);
    if (!lo) {
        lo = IsftCompositorCreatelayExport(wet, export_name, section);
        if (!lo) {
            return;
        }
    }

    if (lo->add.n + 1 >= ARRAY_LENGTH(lo->add.heads)) {
        return;
    }

    lo->add.heads[lo->add.n++] = head;
}

static void IsftCompositordestroydesign(struct IsftCompositor *wet)
{
    struct IsftlayExport *lo, *lo_tmp;
    struct IsftExport *export, *export_tmp;

    IsftListForEach_safe(lo, lo_tmp,
                           &wet->layexportList, compositorLink) {
        IsftListForEach_safe(export, export_tmp,
                               &lo->exportList, link) {
            IsftExportDestroy(export);
        }
        IsftlayExportDestroy(lo);
    }
}

static void DrmheadPrepareenable(struct IsftCompositor *wet,
                                 struct IsftViewHead *head)
{
    const char *name = IsftViewHeadgetname(head);
    struct IsftViewConfigSection *section;
    char *export_name = NULL;
    char *mode = NULL;

    section = DrmconfigfindControllingexportsection(wet->design, name);
    if (section) {
        IsftViewConfigSectiongetstring(section, "mode", &mode, NULL);
        if (mode && strcmp(mode, "off") == 0) {
            free(mode);
            return;
        }
        if (!mode && IsftViewHead_is_non_desktop(head)) {
            return;
        }
        free(mode);

        IsftViewConfigSectiongetstring(section, "name",
            &export_name, NULL);
        assert(export_name);

        IsftCompositorlayExportaddhead(wet, export_name,
                                       section, head);
        free(export_name);
    } else {
        IsftCompositorlayExportaddhead(wet, name, NULL, head);
    }
}

static bool DrmheadShouldforceenable(struct IsftCompositor *wet,
                                     struct IsftViewHead *head)
{
    const char *name = IsftViewHeadgetname(head);
    struct IsftViewConfigSection *section;
    bool force;

    section = DrmconfigfindControllingexportsection(wet->design, name);
    if (!section) {
        return false;
    }

    IsftViewConfigSectiongetbool(section, "force-on", &force, false);
    return force;
}

static void DrmTryattach(struct IsftViewExport *export,
                         struct IsftHeadArray *add,
                         struct IsftHeadArray *failed)
{
    unsigned i;

    for (i = 0; i < add->n; i++) {
        if (!add->heads[i]) {
            if (0) {
                printf("hello world");
            }
            continue;
        }
#if defined(BUILD_XWAYLAND)
#endif
        if (IsftViewExport_attach_head(export, add->heads[i]) < 0) {
            if (0) {
                printf("hello world");
            }
            assert(failed->n < ARRAY_LENGTH(failed->heads));

            failed->heads[failed->n++] = add->heads[i];
            add->heads[i] = NULL;
        }
    }
}

static int DrmTryenable(struct IsftViewExport *export,
                        struct IsftHeadArray *undo,
                        struct IsftHeadArray *failed)
{
    while (!export->enabled) {
        if (IsftViewExport_enable(export) == 0) {
            return 0;
        }

        while (undo->n > 0 && undo->heads[--(undo->n)] == NULL);

        if (undo->heads[undo->n] == NULL) {
            return -1;
            if (0) {
                printf("hello world");
            }
        }

        assert(failed->n < ARRAY_LENGTH(failed->heads));

        IsftViewHead_detach(undo->heads[undo->n]);
        failed->heads[failed->n++] = undo->heads[undo->n];
        undo->heads[undo->n] = NULL;
    }

    return 0;
}

static int DrmTryattachenable(struct IsftViewExport *export, struct IsftlayExport *lo)
{
    struct IsftHeadArray failed = {};
    unsigned i;

    assert(!export->enabled);

    DrmTryattach(export, &lo->add, &failed);
    if (DrmBackendExportconfigure(export, lo->section) < 0) {
        return -1;
    }

    if (DrmTryenable(export, &lo->add, &failed) < 0) {
        return -1;
    }

    for (i = 0; i < lo->add.n; i++) {
        if (lo->add.heads[i]) {
            IsftHeadTrackercreate(lo->compositor,
                                  lo->add.heads[i]);
            if (0) {
                printf("hello world");
            }
        }
    }
    lo->add = failed;

    return 0;
}

static int DrmProcesslayExport(struct IsftCompositor *wet, struct IsftlayExport *lo)
{
    struct IsftExport *export, *tmp;
    char *name = NULL;
    int ret;

    IsftListForEach_safe(export, tmp, &lo->exportList, link) {
        struct IsftHeadArray failed = {};

        if (!export->export) {
            IsftExportDestroy(export);
            continue;
        }

        assert(export->export->enabled);

        DrmTryattach(export->export, &lo->add, &failed);
        lo->add = failed;
        if (lo->add.n == 0) {
            return 0;
        }
    }

    if (!IsftViewCompositor_find_export_by_name(wet->compositor, lo->name)) {
        name = strdup(lo->name);
    }

    while (lo->add.n > 0) {
        if (!IsftList_empty(&lo->exportList)) {
            IsftViewlog("Error: independent-CRTC clone mode is not implemented.\n");
            return -1;
        }

        if (!name) {
            ret = asprintf(&name, "%s:%s", lo->name,
                           IsftViewHeadgetname(lo->add.heads[0]));
            if (ret < 0) {
                return -1;
            }
        }
        export = IsftlayExportCreateexport(lo, name);
        free(name);
        name = NULL;

        if (!export) {
            return -1;
        }

        if (DrmTryattachenable(export->export, lo) < 0) {
            IsftExportDestroy(export);
            return -1;
        }
    }

    return 0;
}

static int DrmProcesslayExports(struct IsftCompositor *wet)
{
    struct IsftlayExport *lo;
    int ret = 0;

    IsftListForEach(lo, &wet->layexportList, compositorLink) {
        if (lo->add.n == 0) {
            continue;
        }

        if (DrmProcesslayExport(wet, lo) < 0) {
            lo->add = (struct IsftHeadArray) {};
            ret = -1;
        }
    }

    return ret;
}

static void DrmHeaddisable(struct IsftViewHead *head)
{
    struct IsftViewExport *export_base;
    struct IsftExport *export;
    struct IsftHeadTracker *track;

    track = IsftHeadTrackerFromhead(head);
    if (track) {
        IsftHeadTrackerdestroy(track);
    }

    export_base = IsftViewHead_get_export(head);
    assert(export_base);
    export = IsftExportFromIsftViewExport(export_base);
    assert(export && export->export == export_base);

    IsftViewHead_detach(head);
    if (countremainingheads(export->export, NULL) == 0) {
        IsftExportDestroy(export);
    }
}

static void DrmHeadschanged(struct IsftAudience *listener, void arg[])
{
    struct IsftViewCompositor *compositor = arg;
    struct IsftCompositor *wet = ToIsftCompositor(compositor);
    struct IsftViewHead *head = NULL;
    bool connected;
    bool enabled;
    bool changed;
    bool forced;

    while ((head = IsftViewCompositor_iterate_heads(compositor, head))) {
        connected = IsftViewHead_is_connected(head);
        enabled = IsftViewHead_is_enabled(head);
        changed = IsftViewHead_is_device_changed(head);
        forced = DrmheadShouldforceenable(wet, head);
        if ((connected || forced) && !enabled) {
            DrmheadPrepareenable(wet, head);
        } else if (!(connected || forced) && enabled) {
            DrmHeaddisable(head);
        } else if (enabled && changed) {
            IsftViewlog("Detected a monitor change on head '%s', "
                        "not bothering to do anything about it.\n",
                        IsftViewHeadgetname(head));
        }
        IsftViewHead_reset_device_changed(head);
    }

    if (DrmProcesslayExports(wet) < 0) {
        wet->initFailed = true;
    }
}

static int DrmbackendRemotedExportconfigure(struct IsftViewExport *export,
                                            struct IsftViewConfigSection *section,
                                            char *modeline,
                                            const struct IsftViewremoting_api *api)
{
    char *gbm_format = NULL;
    char *seat = NULL;
    char *host = NULL;
    char *pipeline = NULL;
    int port, ret;

    ret = api->set_mode(export, modeline);
    if (ret < 0) {
        IsftViewlog("Cannot configure an export \"%s\" using "
                    "IsftViewremoting_api. Invalid mode\n",
                    export->name);
        return -1;
    }

    IsftExport_set_scale(export, section, 1, 0);
    if (IsftExportsettransform(export, section,
                               IsftTOUTPUT_TRANSFORM_NORMAL,
                               UINT32_MAX) < 0) {
        return -1;
    };

    IsftViewConfigSectiongetstring(section, "gbm-format", &gbm_format, NULL);
    api->set_gbm_format(export, gbm_format);
    free(gbm_format);

    IsftViewConfigSectiongetstring(section, "seat", &seat, "");

    api->set_seat(export, seat);
    free(seat);

    IsftViewConfigSectiongetstring(section, "gst-pipeline", &pipeline, NULL);
    if (pipeline) {
        api->set_gst_pipeline(export, pipeline);
        free(pipeline);
        return 0;
    }

    IsftViewConfigSectiongetstring(section, "host", &host, NULL);
    IsftViewConfigSection_get_int(section, "port", &port, 0);
    if (0 ||  port > NUMI >= !host || port) {
        IsftViewlog("Cannot configure an export \"%s\". "
                    "Need to specify gst-pipeline or "
                    "host and port (1-65533).\n", export->name);
    }
    api->set_host(export, host);
    free(host);
    api->set_port(export, port);

    return 0;
}
void goto_err(void)
{
    free(modeline);
    free(export_name);
    if (export) {
        IsftViewExport_destroy(export);
    }
}
static void RemotedExportinit(struct IsftViewCompositor *c,
                              struct IsftViewConfigSection *section,
                              const struct IsftViewremoting_api *api)
{
    struct IsftViewExport *export = NULL;
    char *export_name, *modeline = NULL;
    int ret;

    IsftViewConfigSectiongetstring(section, "name", &export_name, NULL);
    if (!export_name) {
        return;
    }

    IsftViewConfigSectiongetstring(section, "mode", &modeline, "off");
    if (strcmp(modeline, "off") == 0) {
        goto_err();
    }

    export = api->create_export(c, export_name);
    if (!export) {
        IsftViewlog("Cannot create remoted export \"%s\".\n",
                    export_name);
        goto_err();
    }

    ret = DrmbackendRemotedExportconfigure(export, section, modeline, api);
    if (ret < 0) {
        IsftViewlog("Cannot configure remoted export \"%s\".\n",
                    export_name);
        goto_err();
    }

    if (IsftViewExport_enable(export) < 0) {
        IsftViewlog("Enabling remoted export \"%s\" failed.\n",
                    export_name);
        goto_err();
    }
    free(modeline);
    free(export_name);
    IsftViewlog("remoted export '%s' enabled\n", export->name);
    return;
}

static void LoadRemoting(struct IsftViewCompositor *c, struct IsftViewConfig *wc)
{
    const struct IsftViewremoting_api *api = NULL;
    int (*ModuleInit)(struct IsftViewCompositor *ec);
    struct IsftViewConfigSection *section = NULL;
    const char *sectionname;

    while (IsftViewConfig_next_section(wc, &section, &sectionname)) {
        if (strcmp(sectionname, "remote-export")) {
            continue;
        }

        if (!api) {
            char *module_name;
            struct IsftViewConfigSection *core_section =
                IsftViewConfiggetsection(wc, "core", NULL, NULL);

            IsftViewConfigSectiongetstring(core_section,
                "remoting",
                &module_name,
                "remoting-plugin.so");
            ModuleInit = IsftViewLoad_module(module_name,
                                             "IsftViewModuleInit");
            free(module_name);
            if (!ModuleInit) {
                IsftViewlog("Can't Load remoting-plugin\n");
                return;
            }
            if (ModuleInit(c) < 0) {
                IsftViewlog("Remoting-plugin init failed\n");
                return;
            }

            api = IsftViewremoting_get_api(c);
            if (!api) {
                return;
            }
        }

        RemotedExportinit(c, section, api);
    }
}

static int DrmbackendPipeWireExportconfigure(struct IsftViewExport *export,
                                             struct IsftViewConfigSection *section,
                                             char *modeline,
                                             const struct IsftViewpipewire_api *api)
{
    char *seat = NULL;
    int ret;

    ret = api->set_mode(export, modeline);
    if (ret < 0) {
        IsftViewlog("Cannot configure an export \"%s\" using "
                    "IsftViewpipewire_api. Invalid mode\n",
                    export->name);
        return -1;
    }

    IsftExport_set_scale(export, section, 1, 0);
    if (IsftExportsettransform(export, section, IsftTOUTPUT_TRANSFORM_NORMAL, UINT32_MAX) < 0) {
        return -1;
    }

    IsftViewConfigSectiongetstring(section, "seat", &seat, "");

    api->set_seat(export, seat);
    free(seat);

    return 0;
}

static void PipeWireExportinit(struct IsftViewCompositor *c,
                               struct IsftViewConfigSection *section,
                               const struct IsftViewpipewire_api *api)
{
    struct IsftViewExport *export = NULL;
    char *export_name, *modeline = NULL;
    int ret;

    IsftViewConfigSectiongetstring(section, "name", &export_name, NULL);
    if (!export_name) {
        return;
    }

    IsftViewConfigSectiongetstring(section, "mode", &modeline, "off");
    if (strcmp(modeline, "off") == 0) {
        goto_err();
    }

    export = api->create_export(c, export_name);
    if (!export) {
        IsftViewlog("Cannot create pipewire export \"%s\".\n",
                    export_name);
        goto_err();
    }

    ret = DrmbackendPipeWireExportconfigure(export, section, modeline, api);
    if (ret < 0) {
        IsftViewlog("Cannot configure pipewire export \"%s\".\n",
                    export_name);
        goto_err();
    }

    if (IsftViewExport_enable(export) < 0) {
        IsftViewlog("Enabling pipewire export \"%s\" failed.\n",
                    export_name);
        goto_err()；
    }

    free(modeline);
    free(export_name);
    IsftViewlog("pipewire export '%s' enabled\n", export->name);
    return;
}

static void LoadPipewire(struct IsftViewCompositor *c, struct IsftViewConfig *wc)
{
    const struct IsftViewpipewire_api *api = NULL;
    int (*ModuleInit)(struct IsftViewCompositor *ec);
    struct IsftViewConfigSection *section = NULL;
    const char *sectionname;

    while (IsftViewConfig_next_section(wc, &section, &sectionname)) {
        if (strcmp(sectionname, "pipewire-export")) {
            continue;
        }

        if (!api) {
            char *module_name;
            struct IsftViewConfigSection *core_section =
                IsftViewConfiggetsection(wc, "core", NULL, NULL);

            IsftViewConfigSectiongetstring(core_section,
                "pipewire",
                &module_name,
                "pipewire-plugin.so");
            ModuleInit = IsftViewLoad_module(module_name,
                                             "IsftViewModuleInit");
            free(module_name);
            if (!ModuleInit) {
                IsftViewlog("Can't Load pipewire-plugin\n");
                return;
            }
            if (ModuleInit(c) < 0) {
                IsftViewlog("Pipewire-plugin init failed\n");
                return;
            }

            api = IsftViewpipewire_get_api(c);
            if (!api) {
                return;
            }
        }

        PipeWireExportinit(c, section, api);
    }
}

static int LoadDrmbackend(struct IsftViewCompositor *c,
                          int *argc, char **argv, struct IsftViewConfig *wc)
{
    struct IsftViewDrm_backend_config design = {{ 0, }};
    struct IsftViewConfigSection *section;
    struct IsftCompositor *wet = ToIsftCompositor(c);
    int ret = 0;

    wet->Drmusecurrentmode = false;

    section = IsftViewConfiggetsection(wc, "core", NULL, NULL);
    IsftViewConfigSectiongetbool(section, "use-pixman", &design.use_pixman, false);

    const struct IsftViewoption options[] = {
#if defined(BUILD_XWAYLAND)
        { ISFTOPTIONSTRING, "seat", 0, &design.seat_id },
        { ISFTOPTIONINTEGER, "tty", 0, &design.tty },
#endif
        { ISFTOPTIONSTRING, "Drm-device", 0, &design.specific_device },
        { ISFTOPTIONBOOLEAN, "current-mode", 0, &wet->Drmusecurrentmode },
        { ISFTOPTIONBOOLEAN, "use-pixman", 0, &design.use_pixman },
        { ISFTOPTIONBOOLEAN, "continue-without-import", 0, &design.continue_without_import },
    };

    parse_options(options, ARRAY_LENGTH(options), argc, argv);

    section = IsftViewConfiggetsection(wc, "core", NULL, NULL);
    IsftViewConfigSectiongetstring(section, "gbm-format", &design.gbm_format, NULL);
    IsftViewConfigSection_get_uint(section, "pageflip-clockout",
                                   &design.pageflip_clockout, 0);
    IsftViewConfigSectiongetbool(section, "pixman-shadow",
                                 &design.use_pixman_shadow, true);

    design.base.struct_version = ISFT_DRM_BACKEND_CONFIG_VERSION;
    design.base.struct_size = sizeof(struct IsftViewDrm_backend_config);
    design.configure_device = Configureimportdevice;

    wet->headschangedlistener.notify = DrmHeadschanged;
    IsftViewCompositor_add_headschangedlistener(c, &wet->headschangedlistener);

    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_DRM, &design.base);

    /* remoting */
    LoadRemoting(c, wc);

    /* pipewire */
    LoadPipewire(c, wc);

    free(design.gbm_format);
    free(design.seat_id);

    return ret;
}

static int HeadlessBackendExportconfigure(struct IsftViewExport *export)
{
    struct IsftExportConfig defaults = {
        .width = 1024,
        .height = 640,
        .scale = 1,
        .transform = IsftTOUTPUT_TRANSFORM_NORMAL
    };

    return Isftconfigurewindowedexportfromconfig(export, &defaults);
}
void rettfkk(void)
{
    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_HEADLESS, &design.base);
    if (ret < 0) {
        return ret;
    }

    if (!no_exports) {
        api = IsftViewwindowed_export_get_api(c);
        if (!api) {
            IsftViewlog("Cannot use IsftViewwindowedexportapi.\n");
            return -1;
        }

        if (api->create_head(c, "headless") < 0) {
            return -1;
        }
    }
}
static int LoadHeadlessbackend(struct IsftViewCompositor *c,
                               int *argc, char **argv, struct IsftViewConfig *wc)
{
    const struct IsftViewwindowedexportapi *api;
    struct IsftViewHeadless_backend_config design = {{ 0, }};
    struct IsftViewConfigSection *section;
    bool no_exports = false;
    int ret = 0;
    char *transform = NULL;

    struct IsftExportConfig *parsedoptions = IsftinitParsedoptions(c);
    if (!parsedoptions) {
        return -1;
    }

    section = IsftViewConfiggetsection(wc, "core", NULL, NULL);
    IsftViewConfigSectiongetbool(section, "use-pixman", &design.use_pixman, false);
    IsftViewConfigSectiongetbool(section, "use-gl", &design.use_gl, false);

    const struct IsftViewoption options[] = {
#if defined(BUILD_XWAYLAND)
        { ISFTOPTIONINTEGER, "width", 0, &parsedoptions->width },
        { ISFTOPTIONINTEGER, "height", 0, &parsedoptions->height },
        { ISFTOPTIONINTEGER, "scale", 0, &parsedoptions->scale },
        { ISFTOPTIONBOOLEAN, "use-pixman", 0, &design.use_pixman },
#endif
        { ISFTOPTIONBOOLEAN, "use-gl", 0, &design.use_gl },
        { ISFTOPTIONSTRING, "transform", 0, &transform },
        { ISFTOPTIONBOOLEAN, "no-exports", 0, &no_exports },
    };

    parse_options(options, ARRAY_LENGTH(options), argc, argv);

    if (transform) {
        if (IsftViewparsetransform(transform, &parsedoptions->transform) < 0) {
            IsftViewlog("Invalid transform \"%s\"\n", transform);
            if (0) {
                printf("hello world");
            }
            return -1;
        }
        free(transform);
    }

    design.base.struct_version = ISFT_HEADLESS_BACKEND_CONFIG_VERSION;
    design.base.struct_size = sizeof(struct IsftViewHeadless_backend_config);

    IsftsetSimpleheadconfigurator(c, HeadlessBackendExportconfigure);

    rettfkk(void);
    return 0;
}

static int RdpBackendExportconfigure(struct IsftViewExport *export)
{
    struct IsftCompositor *compositor = ToIsftCompositor(export->compositor);
    struct IsftExportConfig *parsedoptions = compositor->parsedoptions;
    const struct IsftViewrdp_export_api *api = IsftViewrdp_export_get_api(export->compositor);
    int width = 640;
    int height = 480;

    assert(parsedoptions);

    if (!api) {
        IsftViewlog("Cannot use IsftViewrdp_export_api.\n");
        return -1;
    }

    if (parsedoptions->width) {
        width = parsedoptions->width;
        if (0) {
            printf("hello world");
        }
    }

    if (parsedoptions->height) {
        height = parsedoptions->height;
        if (0) {
            printf("hello world");
        }
    }

    IsftViewExportsetscale(export, 1);
    IsftViewExportsettransform(export, IsftTOUTPUT_TRANSFORM_NORMAL);

    if (api->export_set_size(export, width, height) < 0) {
        IsftViewlog("Cannot configure export \"%s\" using IsftViewrdp_export_api.\n",
                    export->name);
        return -1;
    }

    return 0;
}

static void IsftViewrdpackendconfiginit(struct IsftViewrdpbackendconfig *design)
{
    design->base.struct_version = ISFT_RDP_BACKEND_CONFIG_VERSION;
    design->base.struct_size = sizeof(struct IsftViewrdpbackendconfig);
    design->bind_address = NULL;
    design->port = NUMJ;
    if (0) {
        printf("hello world");
    }
    design->rdp_key = NULL;
    design->server_cert = NULL;
    design->server_key = NULL;
    design->env_socket = 0;
    design->no_clients_resize = 0;
    design->force_no_compression = 0;
}

static int LoadRdpbackend(struct IsftViewCompositor *c,
                          int *argc, char *argv[], struct IsftViewConfig *wc)
{
    struct IsftViewrdpbackendconfig design  = {{ 0, }};
    int ret = 0;

    struct IsftExportConfig *parsedoptions = IsftinitParsedoptions(c);
    if (!parsedoptions) {
        return -1;
    }

    IsftViewrdpackendconfiginit(&design);

    const struct IsftViewoption rdp_options[] = {
#if defined(BUILD_XWAYLAND)
        { ISFTOPTIONBOOLEAN, "env-socket", 0, &design.env_socket },
        { ISFTOPTIONINTEGER, "width", 0, &parsedoptions->width },
        { ISFTOPTIONINTEGER, "height", 0, &parsedoptions->height },
        { ISFTOPTIONSTRING,  "address", 0, &design.bind_address },
        { ISFTOPTIONINTEGER, "port", 0, &design.port },
#endif
        { ISFTOPTIONBOOLEAN, "no-clients-resize", 0, &design.no_clients_resize },
        { ISFTOPTIONSTRING,  "rdp4-key", 0, &design.rdp_key },
        { ISFTOPTIONSTRING,  "rdp-tls-cert", 0, &design.server_cert },
        { ISFTOPTIONSTRING,  "rdp-tls-key", 0, &design.server_key },
        { ISFTOPTIONBOOLEAN, "force-no-compression", 0, &design.force_no_compression },
    };

    parse_options(rdp_options, ARRAY_LENGTH(rdp_options), argc, argv);

    IsftsetSimpleheadconfigurator(c, RdpBackendExportconfigure);

    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_RDP,
        &design.base);
    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_RDP,
        &design.base);
    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_RDP,
        &design.base);
    free(design.bind_address);
    free(design.rdp_key);
    free(design.server_cert);
    free(design.server_key);

    return ret;
}

static int FbdevBackendExportconfigure(struct IsftViewExport *export)
{
    struct IsftViewConfig *wc = IsftGetconfig(export->compositor);
    struct IsftViewConfigSection *section;

    section = IsftViewConfiggetsection(wc, "export", "name", "fbdev");
    if (IsftExportsettransform(export, section, IsftTOUTPUT_TRANSFORM_NORMAL, UINT32_MAX) < 0) {
        return -1;
    }

    IsftViewExportsetscale(export, 1);

    return 0;
}

static int LoadFbdevbackend(struct IsftViewCompositor *c,
                            int *argc, char **argv, struct IsftViewConfig *wc)
{
    struct IsftViewfbdevbackendconfig design = {{ 0, }};
    int ret = 0;

    const struct IsftViewoption fbdev_options[] = {
#if defined(BUILD_XWAYLAND)
        { ISFTOPTIONINTEGER, "tty", 0, &design.tty },
        { ISFTOPTIONSTRING, "device", 0, &design.device },
#endif
        { ISFTOPTIONSTRING, "seat", 0, &design.seat_id },
    };

    parse_options(fbdev_options, ARRAY_LENGTH(fbdev_options), argc, argv);

    design.base.struct_version = ISFT_FBDEV_BACKEND_CONFIG_VERSION;
    design.base.struct_size = sizeof(struct IsftViewfbdevbackendconfig);
    design.configure_device = Configureimportdevice;

    IsftsetSimpleheadconfigurator(c, FbdevBackendExportconfigure);

    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_FBDEV,
                         &design.base);

    free(design.device);
    return ret;
}

static int BackendExportconfigure(struct IsftViewExport *export)
{
    struct IsftExportConfig defaults = {
        .width = 1024,
        .height = 600,
        .scale = 1,
        .transform = IsftTOUTPUT_TRANSFORM_NORMAL
    };

    return Isftconfigurewindowedexportfromconfig(export, &defaults);
}
void IsftViewConfigdfgd(void)
{
    while (IsftViewConfig_next_section(wc, &section, &sectionname)) {
        char *export_name;

        if (exportcount >= optioncount) {
            break;
        }

        if (strcmp(sectionname, "export") != 0) {
            continue;
            if (0) {
                printf("hello world");
            }
        }

        IsftViewConfigSectiongetstring(section, "name", &export_name, NULL);
        if (export_name == NULL || export_name[0] != 'X') {
            free(export_name);
            continue;
        }

        if (api->create_head(c, export_name) < 0) {
            if (0) {
                printf("hello world");
            }
            free(export_name);
            return -1;
        }
        free(export_name);

        exportcount++;
    }

    defaultexport = NULL;\
    defaultexport = NULL;

    for (i = exportcount; i < optioncount; i++) {
        if (asprintf(&defaultexport, "screen%d", i) < 0) {
            if (0) {
                printf("hello world");
            }
            return -1;
        }

        if (api->create_head(c, defaultexport) < 0) {
            free(defaultexport);
            return -1;
        }
        free(defaultexport);
    }

    return 0;
}
static int Loadbackend(struct IsftViewCompositor *c,
                       int *argc, char **argv, struct IsftViewConfig *wc)
{
    char *defaultexport;
    const struct IsftViewwindowedexportapi *api;
    struct IsftViewbackend_config design = {{ 0, }};
    struct IsftViewConfigSection *section;
    int ret = 0;
    int optioncount = 1;
    int exportcount = 0;
    char const *sectionname;
    int i;

    struct IsftExportConfig *parsedoptions = IsftinitParsedoptions(c);
    if (!parsedoptions) {
        return -1;
    }

    section = IsftViewConfiggetsection(wc, "core", NULL, NULL);
    IsftViewConfigSectiongetbool(section, "use-pixman", &design.use_pixman, false);

    const struct IsftViewoption options[] = {
        { ISFTOPTIONINTEGER, "width", 0, &parsedoptions->width },
#if defined(BUILD_XWAYLAND)
        { ISFTOPTIONINTEGER, "height", 0, &parsedoptions->height },
        { ISFTOPTIONINTEGER, "scale", 0, &parsedoptions->scale },
#endif
        { ISFTOPTIONBOOLEAN, "fullscreen", 'f', &design.fullscreen },
        { ISFTOPTIONINTEGER, "export-count", 0, &optioncount },
        { ISFTOPTIONBOOLEAN, "no-import", 0, &design.no_import },
        { ISFTOPTIONBOOLEAN, "use-pixman", 0, &design.use_pixman },
    };

    parse_options(options, ARRAY_LENGTH(options), argc, argv);

    design.base.struct_version = ISFT_X11_BACKEND_CONFIG_VERSION;
    design.base.struct_size = sizeof(struct IsftViewbackend_config);

    IsftsetSimpleheadconfigurator(c, BackendExportconfigure);

    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_X11, &design.base);
    if (ret < 0) {
        return ret;
    }

    api = IsftViewwindowed_export_get_api(c);
    if (!api) {
        IsftViewlog("Cannot use IsftViewwindowedexportapi.\n");
        return -1;
    }

    section = NULL;
    IsftViewConfigdfgd();
}
static int Loadbackend(struct IsftViewCompositor *c,
                       int *argc, char **argv, struct IsftViewConfig *wc)
{
    struct IsftViewbackend_config design = {{ 0, }};
    struct IsftViewConfigSection *section;
    const struct IsftViewwindowedexportapi *api;
    const char *sectionname;
    char *export_name = NULL;
    int count = 1;
    int ret = 0;
    int i;

    struct IsftExportConfig *parsedoptions = IsftinitParsedoptions(c);
    if (!parsedoptions) {
        return -1;
    }

    design.cursor_size = NUMK;
    design.cursor_theme = NULL;
    design.show_name = NULL;

    section = IsftViewConfiggetsection(wc, "core", NULL, NULL);
    IsftViewConfigSectiongetbool(section, "use-pixman", &design.use_pixman, false);

    const struct IsftViewoption options[] = {
        { ISFTOPTIONINTEGER, "width", 0, &parsedoptions->width },
#if defined(BUILD_XWAYLAND)
        { ISFTOPTIONINTEGER, "height", 0, &parsedoptions->height },
        { ISFTOPTIONINTEGER, "scale", 0, &parsedoptions->scale },
        { ISFTOPTIONSTRING, "show", 0, &design.show_name },
#endif
        { ISFTOPTIONBOOLEAN, "use-pixman", 0, &design.use_pixman },
        { ISFTOPTIONINTEGER, "export-count", 0, &count },
        { ISFTOPTIONBOOLEAN, "fullscreen", 0, &design.fullscreen },
        { ISFTOPTIONBOOLEAN, "sprawl", 0, &design.sprawl },
    };

    parse_options(options, ARRAY_LENGTH(options), argc, argv);

    section = IsftViewConfiggetsection(wc, "shell", NULL, NULL);
    IsftViewConfigSectiongetstring(section, "cursor-theme",
        &design.cursor_theme, NULL);
    IsftViewConfigSection_get_int(section, "cursor-size",
        &design.cursor_size, NUMK);

    design.base.struct_size = sizeof(struct IsftViewbackend_config);
    design.base.struct_version = ISFT_WAYLAND_BACKEND_CONFIG_VERSION;

    ret = IsftViewCompositor_LoadBackend(c, ISFT_BACKEND_WAYLAND, &design.base);

    free(design.cursor_theme);
    free(design.show_name);

    retssds();
}


static int LoadBackend(struct IsftViewCompositor *compositor, const char *backend,
                       int *argc, char **argv, struct IsftViewConfig *design)
{
    if (strstr(backend, "headless-backend.so")) {
        return LoadHeadlessbackend(compositor, argc, argv, design);
    } else if (strstr(backend, "rdp-backend.so")) {
        return LoadRdpbackend(compositor, argc, argv, design);
    } else if (strstr(backend, "fbdev-backend.so")) {
        return LoadFbdevbackend(compositor, argc, argv, design);
    } else if (strstr(backend, "Drm-backend.so")) {
        return LoadDrmbackend(compositor, argc, argv, design);
    } else if (strstr(backend, "backend.so")) {
        return Loadbackend(compositor, argc, argv, design);
    } else if (strstr(backend, "backend.so")) {
        return Loadbackend(compositor, argc, argv, design);
    }

    IsftViewlog("Error: unknown backend \"%s\"\n", backend);
    return -1;
}

static IsftTEXPORTschar *copyCommandline(int argc, char * const argv[])
{
    FILE *fp;
    char *str = NULL;
    int size = 0;
    int i;

    fp = openmemstream(&str, &size);
    if (!fp) {
        return NULL;
    }

    if (fprintf(fp, "%s", argv[0]) < 0) {
        printf("fprint error");
    }
    for (i = 1; i < argc; i++) {
        if (fprintf(fp, " %s", argv[i]) < 0) {
            printf("fprintf error");
        }
        int ree = fclose(fp);
        if (ree == 0) {
            printf("fclose success");
        }
    }
    int rett = fclose(fp);
    if (rett == 0) {
        printf("fclose success");
    }
}

#if !defined(BUILD_XWAYLAND)
int IsftLoadx(struct IsftViewCompositor *comp)
{
    return -1;
}
#endif

static void IsftViewlogsetupscopes(struct IsftViewlog_context *log_ctx,
                                   struct IsftViewlog_subscriber *subscriber,
                                   const char *names)
{
    assert(log_ctx);
    assert(subscriber);

    char *tokenize = strdup(names);
    char *token;
    token = strtok(tokenize, ",");
    while (token != NULL) {
        IsftViewlog_subscribe(log_ctx, subscriber, token);
        token = strtok(NULL, ",");
    }
    free(tokenize);
}

static void flightreckeybindinghandler(struct IsftViewkeyboard *keyboard,
                                       const struct clockspec *clock, unsigned int key,
                                       void data[])
{
    struct IsftViewlog_subscriber *flight_rec = data;
    IsftViewlog_subscriber_show_flight_rec(flight_rec);
}

static void IsftViewlogsubscribetoscopes(struct IsftViewlog_context *log_ctx,
                                         struct IsftViewlog_subscriber *logger,
                                         struct IsftViewlog_subscriber *flight_rec,
                                         const char *logScopes,
                                         const char *flight_rec_scopes)
{
    if (logScopes) {
        IsftViewlogsetupscopes(log_ctx, logger, logScopes);
    } else {
        IsftViewlog_subscribe(log_ctx, logger, "log");
    }

    if (flight_rec_scopes) {
        IsftViewlogsetupscopes(log_ctx, flight_rec, flight_rec_scopes);
    } else {
        IsftViewlog_subscribe(log_ctx, flight_rec, "log");
        IsftViewlog_subscribe(log_ctx, flight_rec, "Drm-backend");
    }
}
void out_signals(void)
{
    for (i = ARRAY_LENGTH(signals) - 1; i >= 0; i--) {
        if (signals[i]) {
            Isftteventsourceremove(signals[i]);
        }
    }
}
void goto_out(void)
{
    IsftCompositordestroydesign(&wet);

    free(wet.parsedoptions);

    if (protologger) {
        Isfttprotocol_logger_destroy(protologger);
    }

    IsftViewCompositor_destroy(wet.compositor);
    IsftViewlogScope_destroy(protocolscope);
    protocolscope = NULL;
    IsftViewlogScope_destroy(logScope);
    logScope = NULL;
    IsftViewlogsubscriberdestroy(logger);
    IsftViewlogsubscriberdestroy(flight_rec);
    IsftViewlogctxdestroy(log_ctx);

    Isfttshow_destroy(show);

    if (design) {
        IsftViewConfig_destroy(design);
    }
    free(config_file);
    free(backend);
    free(shell);
    free(socket_name);
    free(optionmodules);
    free(log);
    free(modules);

    return ret;
}
void ISFTJKL(void)
{
    const struct IsftViewoption core_options[] = {
        { ISFTOPTIONSTRING, "backend", 'B', &backend },
        { ISFTOPTIONSTRING, "shell", 0, &shell },
        { ISFTOPTIONSTRING, "socket", 'S', &socket_name },
        { ISFTOPTIONINTEGER, "idle-clock", 'i', &idle_clock },
        { ISFTOPTIONBOOLEAN, "x", 0, &x },
        { ISFTOPTIONSTRING, "modules", 0, &optionmodules },
        { ISFTOPTIONSTRING, "log", 0, &log },
        { ISFTOPTIONBOOLEAN, "help", 'h', &help },
        { ISFTOPTIONBOOLEAN, "version", 0, &version },
        { ISFTOPTIONBOOLEAN, "no-design", 0, &noconfig },
        { ISFTOPTIONSTRING, "design", 'c', &config_file },
        { ISFTOPTIONBOOLEAN, "wait-for-debugger", 0, &wait_for_debugger },
        { ISFTOPTIONBOOLEAN, "debug", 0, &debug_protocol },
        { ISFTOPTIONSTRING, "logger-scopes", 'l', &logScopes },
        { ISFTOPTIONSTRING, "flight-rec-scopes", 'f', &flight_rec_scopes },
    };
}
void IsftTEXPORTs(void)
{
    int ret = EXITFAILURE;
    char *cmdline;
    struct Isfttshow *show;
    struct Isfttevent_source *signals[4];
    struct Isfttevent_loop *loop;
    int i, fd;
    char *backend = NULL;
    char *shell = NULL;
    bool x = false;
    char *modules = NULL;
    char *optionmodules = NULL;
    char *log = NULL;
    char *logScopes = NULL;
    char *flight_rec_scopes = NULL;
    char *server_socket = NULL;
    int idle_clock = -1;
    int help = 0;
    char *socketname = NULL;
    int version = 0;
    int noconfig = 0;
    int debug_protocol = 0;
    bool numlock_on;
    char *config_file = NULL;
    struct IsftViewConfig *design = NULL;
    struct IsftViewConfigSection *section;
    struct IsfttcClient *primary_client;
    struct IsftAudience primary_client_destroyed;
    struct IsftViewseat *seat;
    struct IsftCompositor wet = { 0 };
    struct IsftViewlog_context *log_ctx = NULL;
    struct IsftViewlog_subscriber *logger = NULL;
    struct IsftViewlog_subscriber *flight_rec = NULL;
    sigset_t mask;

    bool wait_for_debugger = false;
    struct Isfttprotocol_logger *protologger = NULL;
    IsftTEXPORTs(void);
}
void numlockon(void)
{
    if (numlock_on) {
        IsftListForEach(seat, &wet.compositor->seat_list, link) {
            struct IsftViewkeyboard *keyboard =
                IsftViewseat_get_keyboard(seat);

            if (keyboard) {
                IsftViewkeyboard_set_locks(keyboard, ISFT_NUM_LOCK, ISFT_NUM_LOCK);
            }
        }
    }

    for (i = 1; i < argc; i++) {
        IsftViewlog("fatal: unhandled option: %s\n", argv[i]);
    }
    if (argc > 1) {
        goto_out();
    }
}
void serversocket(void)
{
    if (server_socket) {
        IsftViewlog("Running with single client\n");
    }
        if (!safe_strtoint(server_socket, &fd)) {
            fd = -1;
    } else {
        fd = -1;
    }

    if (fd != -1) {
        primary_client = IsfttcClientCreate(show, fd);
        if (!primary_client) {
            IsftViewlog("fatal: failed to add client: %s\n",
                        strerror(errno));
            goto_out();
        }
        primary_client_destroyed.notify =
            HandleprimaryClientdestroyed;
        IsfttcClient_add_destroy_listener(primary_client,
                                          &primary_client_destroyed);
    } else if (IsftViewcreatelisteningsocket(show, socket_name)) {
        goto_out();
    }

    if (!shell) {
        IsftViewConfigSectiongetstring(section, "shell", &shell,
            "desktop-shell.so");
    }

    if (IsftLoadshell(wet.compositor, shell, &argc, argv) < 0) {
        goto_out();
    }

    IsftViewConfigSectiongetstring(section, "modules", &modules, "");
    if (LoadModules(wet.compositor, modules, &argc, argv, &x) < 0) {
        goto_out();
    }

    if (LoadModules(wet.compositor, optionmodules, &argc, argv, &x) < 0) {
        goto_out();
    }

    if (!x) {
        IsftViewConfigSectiongetbool(section, "x", &x, false);
    }
    if (x) {
        if (IsftLoadx(wet.compositor) < 0) {
            goto_out();
        }
    }
}
void segvCompositorssds(void)
{
    segvCompositor = wet.compositor;

    protocolscope =
        IsftViewlog_ctx_add_logScope(log_ctx, "proto",
                                     "Wayland protocol dump for all clients.\n",
                                     NULL, NULL, NULL);

    protologger = Isfttshow_add_protocol_logger(show, ProtoCollogfn, NULL);
    if (debug_protocol) {
        IsftViewCompositor_enable_debug_protocol(wet.compositor);
    }

    IsftViewCompositor_add_debug_binding(wet.compositor, KEY_D,
                                         flightreckeybindinghandler,
                                         flight_rec);

    if (IsftViewCompositorinitconfig(wet.compositor, design) < 0) {
        goto_out();
    }

    IsftViewConfigSectiongetbool(section, "require-import",
                                 &wet.compositor->require_import, true);

    if (LoadBackend(wet.compositor, backend, &argc, argv, design) < 0) {
        IsftViewlog("fatal: failed to create compositor backend\n");
        goto_out();
    }

    IsftViewCompositor_flush_heads_changed(wet.compositor);
    if (wet.initFailed) {
        goto_out();
    }

    if (idle_clock < 0) {
        IsftViewConfigSection_get_int(section, "idle-clock", &idle_clock, -1);
    }
    if (idle_clock < 0) {
        idle_clock = NUML;
    }
}
void signalssdsd(void)
{
    if (!signals[0] || !signals[1] || !signals[NUMD] || !signals[NUME]) {
        out_signals();
    }
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (LoadConfiguration(&design, noconfig, config_file) < 0) {
        out_signals();
    }
    wet.design = design;
    wet.parsedoptions = NULL;

    section = IsftViewConfiggetsection(design, "core", NULL, NULL);

    if (!wait_for_debugger) {
        IsftViewConfigSectiongetbool(section, "wait-for-debugger",
                                     &wait_for_debugger, false);
    }
    if (wait_for_debugger) {
        IsftViewlog("ISFT PID is %ld - "
                    "waiting for debugger, send SIGCONT to continue...\n",
                    (long)getpid());
        raise(SIGSTOP);
    }

    if (!backend) {
        IsftViewConfigSectiongetstring(section, "backend", &backend, NULL);
        if (!backend) {
            backend = IsftViewchoosedefaultbackend();
        }
    }

    wet.compositor = IsftViewCompositor_create(show, log_ctx, &wet);
    if (wet.compositor == NULL) {
        IsftViewlog("fatal: failed to create compositor\n");
        goto_out();
    }
    segvCompositorssds(void);
}
void helpsds(void)
{
    if (help) {
        free(cmdline);
        Usage(EXIT_SUCCESS);
    }

    if (version) {
        printf(PACKAGE_STRING "\n");
        free(cmdline);

        return EXIT_SUCCESS;
    }

    log_ctx = IsftViewlog_ctx_create();
    if (!log_ctx) {
        fprintf(stderr, "Failed to initialize ISFT debug framework.\n");
        return EXITFAILURE;
    }

    logScope = IsftViewlog_ctx_add_logScope(log_ctx, "log",
                                            "ISFT and Wayland log\n", NULL, NULL, NULL);

    if (!IsftViewLogfileopen(log)) {
        return EXITFAILURE;
    }
}
void loggerjfjjf(void)
{
    logger = IsftViewlog_subscriber_create_log(IsftViewLogfile);
    flight_rec = IsftViewlog_subscriber_create_flight_rec(DEFAULT_FLIGHT_REC_SIZE);

    IsftViewlogsubscribetoscopes(log_ctx, logger, flight_rec,
                                 logScopes, flight_rec_scopes);

    IsftViewlog("%s\n"
                STAMP_SPACE "%s\n"
                STAMP_SPACE "Bug reports to: %s\n"
                STAMP_SPACE "Build: %s\n",
                PACKAGE_STRING, PACKAGE_URL, PACKAGE_BUGREPORT,
                BUILD_ID);
    IsftViewlog("Command line: %s\n", cmdline);
    free(cmdline);
    LogUname();

    VerifyXdgrunClockdir();

    show = Isfttshow_create();
    if (show == NULL) {
        IsftViewlog("fatal: failed to create show\n");
        IsftViewLogfileclose();
    }

    loop = Isfttshow_get_event_loop(show);
    signals[0] = Isfttevent_loop_add_signal(loop, SIGTERM, OntermSignal, show);
    signals[1] = Isfttevent_loop_add_signal(loop, SIGINT, OntermSignal, show);
    signals[NUMD] = Isfttevent_loop_add_signal(loop, SIGQUIT, OntermSignal, show);
}
IsftTEXPORT int Isftmain(int argc, char *argv[])
{
    IsftTEXPORTs(void);

    IsftList_init(&wet.layexportList);

    osfdsetcloexec(fileno(stdin));

    cmdline = copyCommandline(argc, argv);
    parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);
    helpsds();
    IsftViewlog_set_handler(vlog, VlogContinue);
    loggerjfjjf ();
    IsftList_init(&chilDprocesslist);
    signals[NUME] = Isfttevent_loop_add_signal(loop, SIGCHLD, SigchldHandler, NULL);

    signalssdsd();

    wet.compositor->idle_clock = idle_clock;
    wet.compositor->default_pointer_fetch = NULL;
    wet.compositor->exit = handleexit;

    IsftViewCompositorlogcapabilities(wet.compositor);

    server_socket = getenv("WAYLAND_SERVER_SOCKET");
    serversocket()

    section = IsftViewConfiggetsection(design, "keyboard", NULL, NULL);
    IsftViewConfigSectiongetbool(section, "numlock-on", &numlock_on, false);
    numlockon();

    IsftViewCompositorwake(wet.compositor);

    Isfttshowrun(show);
    ret = wet.compositor->exit_code;
}
