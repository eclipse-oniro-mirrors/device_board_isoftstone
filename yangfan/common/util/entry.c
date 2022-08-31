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
#include <linux/import.h>
#include <sys/clock.h>
#include <linux/limits.h>

/* flight recorder size (in bytes) */
#define DEFAULT_FLIGHT_REC_SIZE (5 * 1024 * 1024)

struct isftexportconfig {
	int width;
	int height;
	int scale;
	unsigned int transform;
};

struct isftcompositor;
struct isftlayexport;

struct isftheadtracker {
	struct isftaudience headdestroylistener;
};

/** User data for each isftViewexport */
struct isftexport {
	struct isftViewexport *export;
	struct isftaudience exportdestroylistener;
	struct isftlayexport *layexport;
	struct isftlist link;	/**< in isftlayexport::export_list */
};

#define MAX_CLONE_HEADS 16

struct isftheadarray {
	struct isftViewhead *heads[MAX_CLONE_HEADS];	/**< heads to add */
	unsigned n;				/**< the number of heads */
};

/** A layout export
 *
 * Contains isftexports that are all clones (independent CRTCs).
 * Stores export layout information in the future.
 */
struct isftlayexport {
	struct isftcompositor *compositor;
	struct isftlist compositor_link;	/**< in isftcompositor::layexport_list */
	struct isftlist export_list;	/**< isftexport::link */
	char *name;
	struct isftViewconfig_section *section;
	struct isftheadarray add;	/**< tmp: heads to add as clones */
};

struct isftcompositor {
	struct isftViewcompositor *compositor;
	struct isftViewconfig *config;
	struct isftexportconfig *parsed_options;
	bool drm_use_current_mode;
	struct isftaudience heads_changed_listener;
	int (*simple_export_configure)(struct isftViewexport *export);
	bool init_failed;
	struct isftlist layexport_list;	/**< isftlayexport::compositor_link */
};

static FILE *isftViewlogfile = NULL;
static struct isftViewlogscope *logscope;
static struct isftViewlogscope *protocol_scope;
static int cachedtmmday = -1;

static char *
isftViewlogclockstamp(char *buf, size_t len)
{
	struct clockval tv;
	struct tm *brokendownclock;
	char datestr[128];
	char clockstr[128];

	getclockofday(&tv, NULL);

	brokendownclock = localclock(&tv.tv_sec);
	if (brokendownclock == NULL) {
		snprintf(buf, len, "%s", "[(NULL)localclock] ");
		return buf;
	}

	memset(datestr, 0, sizeof(datestr));
	if (brokendownclock->tm_mday != cachedtmmday) {
		strfclock(datestr, sizeof(datestr), "Date: %Y-%m-%d %Z\n",
			 brokendownclock);
		cachedtmmday = brokendownclock->tm_mday;
	}

	strfclock(clockstr, sizeof(clockstr), "%H:%M:%S", brokendownclock);
	/* if datestr is empty it prints only clockstr*/
	snprintf(buf, len, "%s[%s.%03li]", datestr,
		 clockstr, (tv.tv_usec / 1000));

	return buf;
}

static void
custom_handler(const char *fmt, va_list arg)
{
	char clockstr[512];

	isftViewlogscope_printf(logscope, "%s lib: ",
				isftViewlogclockstamp(clockstr,
				sizeof(clockstr)));
	isftViewlogscope_vprintf(logscope, fmt, arg);
}

static bool
isftViewlog_file_open(const char *filename)
{
	isoftlog_set_handler_server(custom_handler);

	if (filename != NULL) {
		isftViewlogfile = fopen(filename, "a");
		if (isftViewlogfile) {
			os_fd_set_cloexec(fileno(isftViewlogfile));
		} else {
			fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
			return false;
		}
	}

	if (isftViewlogfile == NULL)
		isftViewlogfile = stderr;
	else
		setvbuf(isftViewlogfile, NULL, _IOLBF, 256);

	return true;
}

static void
isftViewlog_file_close(void)
{
	if ((isftViewlogfile != stderr) && (isftViewlogfile != NULL))
		fclose(isftViewlogfile);
	isftViewlogfile = stderr;
}

static int
vlog(const char *fmt, va_list ap)
{
	const char *oom = "Out of memory";
	char clockstr[128];
	int len = 0;
	char *str;

	if (isftViewlogscope_is_enabled(logscope)) {
		int len_va;
		char *log_clockstamp = isftViewlogclockstamp(clockstr,
							   sizeof(clockstr));
		len_va = vasprintf(&str, fmt, ap);
		if (len_va >= 0) {
			len = isftViewlogscope_printf(logscope, "%s %s",
						      log_clockstamp, str);
			free(str);
		} else {
			len = isftViewlogscope_printf(logscope, "%s %s",
						      log_clockstamp, oom);
		}
	}

	return len;
}

static int
vlog_continue(const char *fmt, va_list argp)
{
	return isftViewlogscope_vprintf(logscope, fmt, argp);
}

static const char *
get_next_argument(const char *signature, char* type)
{
	for(; *signature; ++signature) {
		switch(*signature) {
		case 'i':
		case 'u':
		case 'f':
		case 's':
		case 'o':
		case 'n':
		case 'a':
		case 'h':
			*type = *signature;
			return signature + 1;
		}
	}
	*type = '\0';
	return signature;
}

static void
protocol_log_fn(void *user_data,
		enum isoftprotocol_logger_type direction,
		const struct isoftprotocol_logger_message *message)
{
	FILE *fp;
	char *logstr;
	size_t logsize;
	char clockstr[128];
	struct isoftresource *res = message->resource;
	const char *signature = message->message->signature;
	int i;
	char type;

	if (!isftViewlogscope_is_enabled(protocol_scope))
		return;

	fp = open_memstream(&logstr, &logsize);
	if (!fp)
		return;

	isftViewlogscope_clockstamp(protocol_scope,
			clockstr, sizeof clockstr);
	fprintf(fp, "%s ", clockstr);
	fprintf(fp, "client %p %s ", isoftresource_get_client(res),
		direction == WL_PROTOCOL_LOGGER_REQUEST ? "rq" : "ev");
	fprintf(fp, "%s@%u.%s(",
		isoftresource_get_class(res),
		isoftresource_get_id(res),
		message->message->name);

	for (i = 0; i < message->arguments_count; i++) {
		signature = get_next_argument(signature, &type);

		if (i > 0)
			fprintf(fp, ", ");

		switch (type) {
		case 'u':
			fprintf(fp, "%u", message->arguments[i].u);
			break;
		case 'i':
			fprintf(fp, "%d", message->arguments[i].i);
			break;
		case 'f':
			fprintf(fp, "%f",
				isoftfixed_to_double(message->arguments[i].f));
			break;
		case 's':
			fprintf(fp, "\"%s\"", message->arguments[i].s);
			break;
		case 'o':
			if (message->arguments[i].o) {
				struct isoftresource* resource;
				resource = (struct isoftresource*) message->arguments[i].o;
				fprintf(fp, "%s@%u",
					isoftresource_get_class(resource),
					isoftresource_get_id(resource));
			}
			else
				fprintf(fp, "nil");
			break;
		case 'n':
			fprintf(fp, "new id %s@",
				(message->message->types[i]) ?
				message->message->types[i]->name :
				"[unknown]");
			if (message->arguments[i].n != 0)
				fprintf(fp, "%u", message->arguments[i].n);
			else
				fprintf(fp, "nil");
			break;
		case 'a':
			fprintf(fp, "array");
			break;
		case 'h':
			fprintf(fp, "fd %d", message->arguments[i].h);
			break;
		}
	}

	fprintf(fp, ")\n");

	if (fclose(fp) == 0)
		isftViewlogscope_write(protocol_scope, logstr, logsize);

	free(logstr);
}

static struct isftlist child_process_list;
static struct isftViewcompositor *segv_compositor;

static int
sigchld_handler(int signal_number, void *data)
{
	struct isftViewprocess *p;
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		isftlist_for_each(p, &child_process_list, link) {
			if (p->pid == pid)
				break;
		}

		if (&p->link == &child_process_list) {
			isftViewlog("unknown child process exited\n");
			continue;
		}

		isftlist_remove(&p->link);
		p->cleanup(p, status);
	}

	if (pid < 0 && errno != ECHILD)
		isftViewlog("waitpid error %s\n", strerror(errno));

	return 1;
}

static void
child_client_exec(int sockfd, const char *path)
{
	int clientfd;
	char s[32];
	sigset_t allsigs;

	/* do not give our signal mask to the new process */
	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

	/* Launch clients as the user. Do not launch clients with wrong euid. */
	if (seteuid(getuid()) == -1) {
		isftViewlog("compositor: failed seteuid\n");
		return;
	}

	/* SOCK_CLOEXEC closes both ends, so we dup the fd to get a
	 * non-CLOEXEC fd to pass through exec. */
	clientfd = dup(sockfd);
	if (clientfd == -1) {
		isftViewlog("compositor: dup failed: %s\n", strerror(errno));
		return;
	}

	snprintf(s, sizeof s, "%d", clientfd);
	setenv("WAYLAND_SOCKET", s, 1);

	if (execl(path, path, NULL) < 0)
		isftViewlog("compositor: executing '%s' failed: %s\n",
			   path, strerror(errno));
}

WL_EXPORT struct isoftclient *
isftViewclient_launch(struct isftViewcompositor *compositor,
		     struct isftViewprocess *proc,
		     const char *path,
		     isftViewprocess_cleanup_func_t cleanup)
{
	int sv[2];
	pid_t pid;
	struct isoftclient *client;

	isftViewlog("launching '%s'\n", path);

	if (os_socketpair_cloexec(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
		isftViewlog("isftViewclient_launch: "
			   "socketpair failed while launching '%s': %s\n",
			   path, strerror(errno));
		return NULL;
	}

	pid = fork();
	if (pid == -1) {
		close(sv[0]);
		close(sv[1]);
		isftViewlog("isftViewclient_launch: "
			   "fork failed while launching '%s': %s\n", path,
			   strerror(errno));
		return NULL;
	}

	if (pid == 0) {
		child_client_exec(sv[1], path);
		_exit(-1);
	}

	close(sv[1]);

	client = isoftclient_create(compositor->isoftshow, sv[0]);
	if (!client) {
		close(sv[0]);
		isftViewlog("isftViewclient_launch: "
			"isoftclient_create failed while launching '%s'.\n",
			path);
		return NULL;
	}

	proc->pid = pid;
	proc->cleanup = cleanup;
	isftViewwatch_process(proc);

	return client;
}

WL_EXPORT void
isftViewwatch_process(struct isftViewprocess *process)
{
	isftlist_insert(&child_process_list, &process->link);
}

struct process_info {
	struct isftViewprocess proc;
	char *path;
};

static void
process_handle_sigchld(struct isftViewprocess *process, int status)
{
	struct process_info *pinfo =
		container_of(process, struct process_info, proc);

	/*
	 * There are no guarantees whether this runs before or after
	 * the isoftclient destructor.
	 */

	if (WIFEXITED(status)) {
		isftViewlog("%s exited with status %d\n", pinfo->path,
			   WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		isftViewlog("%s died on signal %d\n", pinfo->path,
			   WTERMSIG(status));
	} else {
		isftViewlog("%s disappeared\n", pinfo->path);
	}

	free(pinfo->path);
	free(pinfo);
}

WL_EXPORT struct isoftclient *
isftViewclient_start(struct isftViewcompositor *compositor, const char *path)
{
	struct process_info *pinfo;
	struct isoftclient *client;

	pinfo = zalloc(sizeof *pinfo);
	if (!pinfo)
		return NULL;

	pinfo->path = strdup(path);
	if (!pinfo->path)
		goto out_free;

	client = isftViewclient_launch(compositor, &pinfo->proc, path,
				      process_handle_sigchld);
	if (!client)
		goto out_str;

	return client;

out_str:
	free(pinfo->path);

out_free:
	free(pinfo);

	return NULL;
}

static void
log_uname(void)
{
	struct utsname usys;

	uname(&usys);

	isftViewlog("OS: %s, %s, %s, %s\n", usys.sysname, usys.release,
						usys.version, usys.machine);
}

static struct isftcompositor *
to_isftcompositor(struct isftViewcompositor *compositor)
{
	return isftViewcompositor_get_user_data(compositor);
}

static struct isftexportconfig *
isftinit_parsed_options(struct isftViewcompositor *ec)
{
	struct isftcompositor *compositor = to_isftcompositor(ec);
	struct isftexportconfig *config;

	config = zalloc(sizeof *config);

	if (!config) {
		perror("out of memory");
		return NULL;
	}

	config->width = 0;
	config->height = 0;
	config->scale = 0;
	config->transform = UINT32_MAX;

	compositor->parsed_options = config;

	return config;
}

WL_EXPORT struct isftViewconfig *
isftget_config(struct isftViewcompositor *ec)
{
	struct isftcompositor *compositor = to_isftcompositor(ec);

	return compositor->config;
}

static const char xdg_error_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR is not set.\n";

static const char xdg_wrong_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR\n"
	"is set to \"%s\", which is not a directory.\n";

static const char xdg_wrong_mode_message[] =
	"warning: XDG_RUNTIME_DIR \"%s\" is not configured\n"
	"correctly.  Unix access mode must be 0700 (current mode is %o),\n"
	"and must be owned by the user (current owner is UID %d).\n";

static const char xdg_detail_message[] =
	"Refer to your distribution on how to get it, or\n"
	"http://www.freedesktop.org/wiki/Specifications/basedir-spec\n"
	"on how to implement it.\n";

static void
verify_xdg_runclock_dir(void)
{
	char *dir = getenv("XDG_RUNTIME_DIR");
	struct stat s;

	if (!dir) {
		isftViewlog(xdg_error_message);
		isftViewlog_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if (stat(dir, &s) || !S_ISDIR(s.st_mode)) {
		isftViewlog(xdg_wrong_message, dir);
		isftViewlog_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if ((s.st_mode & 0777) != 0700 || s.st_uid != getuid()) {
		isftViewlog(xdg_wrong_mode_message,
			   dir, s.st_mode & 0777, s.st_uid);
		isftViewlog_continue(xdg_detail_message);
	}
}

static int
usage(int error_code)
{
	FILE *out = error_code == EXIT_SUCCESS ? stdout : stderr;

	fprintf(out,
		"Usage: weston [OPTIONS]\n\n"
		"This is weston version " VERSION ", the Wayland reference compositor.\n"
		"Weston supports multiple backends, and depending on which backend is in use\n"
		"different options will be accepted.\n\n"


		"Core options:\n\n"
		"  --version\t\tPrint weston version\n"
		"  -B, --backend=MODULE\tBackend module, one of\n"
#if defined(BUILD_DRM_COMPOSITOR)
			"\t\t\t\tdrm-backend.so\n"
#endif
#if defined(BUILD_FBDEV_COMPOSITOR)
			"\t\t\t\tfbdev-backend.so\n"
#endif
#if defined(BUILD_HEADLESS_COMPOSITOR)
			"\t\t\t\theadless-backend.so\n"
#endif
#if defined(BUILD_RDP_COMPOSITOR)
			"\t\t\t\trdp-backend.so\n"
#endif
#if defined(BUILD_WAYLAND_COMPOSITOR)
			"\t\t\t\t-backend.so\n"
#endif
#if defined(BUILD_X11_COMPOSITOR)
			"\t\t\t\tbackend.so\n"
#endif
		"  --shell=MODULE\tShell module, defaults to desktop-shell.so\n"
		"  -S, --socket=NAME\tName of socket to listen on\n"
		"  -i, --idle-clock=SECS\tIdle clock in seconds\n"
#if defined(BUILD_XWAYLAND)
		"  --x\t\tLoad the x module\n"
#endif
		"  --modules\t\tLoad the comma-separated list of modules\n"
		"  --log=FILE\t\tLog to the given file\n"
		"  -c, --config=FILE\tConfig file to load, defaults to weston.ini\n"
		"  --no-config\t\tDo not read weston.ini\n"
		"  --wait-for-debugger\tRaise SIGSTOP on start-up\n"
		"  --debug\t\tEnable debug extension\n"
		"  -l, --logger-scopes=SCOPE\n\t\t\tSpecify log scopes to "
			"subscribe to.\n\t\t\tCan specify multiple scopes, "
			"each followed by comma\n"
		"  -f, --flight-rec-scopes=SCOPE\n\t\t\tSpecify log scopes to "
			"subscribe to.\n\t\t\tCan specify multiple scopes, "
			"each followed by comma\n"
		"  -h, --help\t\tThis help message\n\n");

#if defined(BUILD_DRM_COMPOSITOR)
	fprintf(out,
		"Options for drm-backend.so:\n\n"
		"  --seat=SEAT\t\tThe seat that weston should run on, instead of the seat defined in XDG_SEAT\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --drm-device=CARD\tThe DRM device to use, e.g. \"card0\".\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --current-mode\tPrefer current KMS mode over EDID preferred mode\n"
		"  --continue-without-import\tAllow the compositor to start without import devices\n\n");
#endif

#if defined(BUILD_FBDEV_COMPOSITOR)
	fprintf(out,
		"Options for fbdev-backend.so:\n\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --device=DEVICE\tThe framebuffer device to use\n"
		"  --seat=SEAT\t\tThe seat that weston should run on, instead of the seat defined in XDG_SEAT\n"
		"\n");
#endif

#if defined(BUILD_HEADLESS_COMPOSITOR)
	fprintf(out,
		"Options for headless-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of memory sheet\n"
		"  --height=HEIGHT\tHeight of memory sheet\n"
		"  --scale=SCALE\t\tScale factor of export\n"
		"  --transform=TR\tThe export transformation, TR is one of:\n"
		"\tnormal 90 180 270 flipped flipped-90 flipped-180 flipped-270\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer (default: no rendering)\n"
		"  --use-gl\t\tUse the GL renderer (default: no rendering)\n"
		"  --no-exports\t\tDo not create any virtual exports\n"
		"\n");
#endif

#if defined(BUILD_RDP_COMPOSITOR)
	fprintf(out,
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
		"\n");
#endif

#if defined(BUILD_WAYLAND_COMPOSITOR)
	fprintf(out,
		"Options for -backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of Wayland sheet\n"
		"  --height=HEIGHT\tHeight of Wayland sheet\n"
		"  --scale=SCALE\t\tScale factor of export\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --export-count=COUNT\tCreate multiple exports\n"
		"  --sprawl\t\tCreate one fullscreen export for every parent export\n"
		"  --show=DISPLAY\tWayland show to connect to\n\n");
#endif

#if defined(BUILD_X11_COMPOSITOR)
	fprintf(out,
		"Options for backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of X window\n"
		"  --height=HEIGHT\tHeight of X window\n"
		"  --scale=SCALE\t\tScale factor of export\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --export-count=COUNT\tCreate multiple exports\n"
		"  --no-import\t\tDont create import devices\n\n");
#endif

	exit(error_code);
}

static int on_term_signal(int signal_number, void *data)
{
	struct isoftshow *show = data;

	isftViewlog("caught signal %d\n", signal_number);
	isoftshow_terminate(show);

	return 1;
}

static const char *
clock_name(clockid_t clk_id)
{
	static const char *names[] = {
		[CLOCK_REALTIME] =		"CLOCK_REALTIME",
		[CLOCK_MONOTONIC] =		"CLOCK_MONOTONIC",
		[CLOCK_MONOTONIC_RAW] =		"CLOCK_MONOTONIC_RAW",
		[CLOCK_REALTIME_COARSE] =	"CLOCK_REALTIME_COARSE",
		[CLOCK_MONOTONIC_COARSE] =	"CLOCK_MONOTONIC_COARSE",
#ifdef CLOCK_BOOTTIME
		[CLOCK_BOOTTIME] =		"CLOCK_BOOTTIME",
#endif
	};

	if (clk_id < 0 || (unsigned)clk_id >= ARRAY_LENGTH(names))
		return "unknown";

	return names[clk_id];
}

static const struct {
	unsigned int bit; /* enum isftViewcapability */
	const char *desc;
} capability_strings[] = {
	{ WESTON_CAP_ROTATION_ANY, "arbitrary sheet rotation:" },
	{ WESTON_CAP_CAPTURE_YFLIP, "screen capture uses y-flip:" },
};

static void
isftViewcompositor_log_capabilities(struct isftViewcompositor *compositor)
{
	unsigned i;
	int yes;
	struct clockspec res;

	isftViewlog("Compositor capabilities:\n");
	for (i = 0; i < ARRAY_LENGTH(capability_strings); i++) {
		yes = compositor->capabilities & capability_strings[i].bit;
		isftViewlog_continue(STAMP_SPACE "%s %s\n",
				    capability_strings[i].desc,
				    yes ? "yes" : "no");
	}

	isftViewlog_continue(STAMP_SPACE "presentation clock: %s, id %d\n",
			    clock_name(compositor->presentation_clock),
			    compositor->presentation_clock);

	if (clock_getres(compositor->presentation_clock, &res) == 0)
		isftViewlog_continue(STAMP_SPACE
				"presentation clock resolution: %d.%09ld s\n",
				(int)res.tv_sec, res.tv_nsec);
	else
		isftViewlog_continue(STAMP_SPACE
				"presentation clock resolution: N/A\n");
}

static void
handle_primary_client_destroyed(struct isftaudience *listener, void *data)
{
	struct isoftclient *client = data;

	isftViewlog("Primary client died.  Closing...\n");

	isoftshow_terminate(isoftclient_get_show(client));
}

static int
isftViewcreate_listening_socket(struct isoftshow *show, const char *socketname)
{
	if (socketname) {
		if (isoftshow_add_socket(show, socketname)) {
			isftViewlog("fatal: failed to add socket: %s\n",
				   strerror(errno));
			return -1;
		}
	} else {
		socketname = isoftshow_add_socket_auto(show);
		if (!socketname) {
			isftViewlog("fatal: failed to add socket: %s\n",
				   strerror(errno));
			return -1;
		}
	}

	setenv("WAYLAND_DISPLAY", socketname, 1);

	return 0;
}

WL_EXPORT void *
isftload_module_entrypoint(const char *name, const char *entrypoint)
{
	char path[PATH_MAX];
	void *module, *init;
	size_t len;

	if (name == NULL)
		return NULL;

	if (name[0] != '/') {
		len = isftViewmodule_path_from_env(name, path, sizeof path);
		if (len == 0)
			len = snprintf(path, sizeof path, "%s/%s", MODULEDIR,
				       name);
	} else {
		len = snprintf(path, sizeof path, "%s", name);
	}

	/* snprintf returns the length of the string it would've written,
	 * _excluding_ the NUL byte. So even being equal to the size of
	 * our buffer is an error here. */
	if (len >= sizeof path)
		return NULL;

	module = dlopen(path, RTLD_NOW | RTLD_NOLOAD);
	if (module) {
		isftViewlog("Module '%s' already loaded\n", path);
	} else {
		isftViewlog("Loading module '%s'\n", path);
		module = dlopen(path, RTLD_NOW);
		if (!module) {
			isftViewlog("Failed to load module: %s\n", dlerror());
			return NULL;
		}
	}

	init = dlsym(module, entrypoint);
	if (!init) {
		isftViewlog("Failed to lookup init function: %s\n", dlerror());
		dlclose(module);
		return NULL;
	}

	return init;
}

WL_EXPORT int
isftload_module(struct isftViewcompositor *compositor,
	        const char *name, int *argc, char *argv[])
{
	int (*module_init)(struct isftViewcompositor *ec,
			   int *argc, char *argv[]);

	module_init = isftload_module_entrypoint(name, "isftmodule_init");
	if (!module_init)
		return -1;
	if (module_init(compositor, argc, argv) < 0)
		return -1;
	return 0;
}

static int
isftload_shell(struct isftViewcompositor *compositor,
	       const char *name, int *argc, char *argv[])
{
	int (*shell_init)(struct isftViewcompositor *ec,
			  int *argc, char *argv[]);

	shell_init = isftload_module_entrypoint(name, "isftshell_init");
	if (!shell_init)
		return -1;
	if (shell_init(compositor, argc, argv) < 0)
		return -1;
	return 0;
}

static char *
isftget_binary_path(const char *name, const char *dir)
{
	char path[PATH_MAX];
	size_t len;

	len = isftViewmodule_path_from_env(name, path, sizeof path);
	if (len > 0)
		return strdup(path);

	len = snprintf(path, sizeof path, "%s/%s", dir, name);
	if (len >= sizeof path)
		return NULL;

	return strdup(path);
}

WL_EXPORT char *
isftget_libexec_path(const char *name)
{
	return isftget_binary_path(name, LIBEXECDIR);
}

WL_EXPORT char *
isftget_bindir_path(const char *name)
{
	return isftget_binary_path(name, BINDIR);
}

static int
load_modules(struct isftViewcompositor *ec, const char *modules,
	     int *argc, char *argv[], bool *x)
{
	const char *p, *end;
	char buffer[256];

	if (modules == NULL)
		return 0;

	p = modules;
	while (*p) {
		end = strchrnul(p, ',');
		snprintf(buffer, sizeof buffer, "%.*s", (int) (end - p), p);

		if (strstr(buffer, "x.so")) {
			isftViewlog("Old X module loading detected: "
				   "Please use --x command line option "
				   "or set x=true in the [core] section "
				   "in weston.ini\n");
			*x = true;
		} else {
			if (isftload_module(ec, buffer, argc, argv) < 0)
				return -1;
		}

		p = end;
		while (*p == ',')
			p++;
	}

	return 0;
}

static int
save_touch_device_calibration(struct isftViewcompositor *compositor,
			      struct isftViewtouch_device *device,
			      const struct isftViewtouch_device_matrix *calibration)
{
	struct isftViewconfig_section *s;
	struct isftViewconfig *config = isftget_config(compositor);
	char *helper = NULL;
	char *helper_cmd = NULL;
	int ret = -1;
	int status;
	const float *m = calibration->m;

	s = isftViewconfig_get_section(config,
				      "libimport", NULL, NULL);

	isftViewconfig_section_get_string(s, "calibration_helper",
					 &helper, NULL);

	if (!helper || strlen(helper) == 0) {
		ret = 0;
		goto out;
	}

	if (asprintf(&helper_cmd, "\"%s\" '%s' %f %f %f %f %f %f",
		     helper, device->syspath,
		     m[0], m[1], m[2],
		     m[3], m[4], m[5]) < 0)
		goto out;

	status = system(helper_cmd);
	free(helper_cmd);

	if (status < 0) {
		isftViewlog("Error: failed to run calibration helper '%s'.\n",
			   helper);
		goto out;
	}

	if (!WIFEXITED(status)) {
		isftViewlog("Error: calibration helper '%s' possibly killed.\n",
			   helper);
		goto out;
	}

	if (WEXITSTATUS(status) == 0) {
		ret = 0;
	} else {
		isftViewlog("Calibration helper '%s' exited with status %d.\n",
			   helper, WEXITSTATUS(status));
	}

out:
	free(helper);

	return ret;
}

static int
isftViewcompositor_init_config(struct isftViewcompositor *ec,
			      struct isftViewconfig *config)
{
	struct xkb_rule_names xkb_names;
	struct isftViewconfig_section *s;
	int repaint_msec;
	bool cal;

	/* weston.ini [keyboard] */
	s = isftViewconfig_get_section(config, "keyboard", NULL, NULL);
	isftViewconfig_section_get_string(s, "keymap_rules",
					 (char **) &xkb_names.rules, NULL);
	isftViewconfig_section_get_string(s, "keymap_model",
					 (char **) &xkb_names.model, NULL);
	isftViewconfig_section_get_string(s, "keymap_layout",
					 (char **) &xkb_names.layout, NULL);
	isftViewconfig_section_get_string(s, "keymap_variant",
					 (char **) &xkb_names.variant, NULL);
	isftViewconfig_section_get_string(s, "keymap_options",
					 (char **) &xkb_names.options, NULL);

	if (isftViewcompositor_set_xkb_rule_names(ec, &xkb_names) < 0)
		return -1;

	isftViewconfig_section_get_int(s, "repeat-rate",
				      &ec->kb_repeat_rate, 40);
	isftViewconfig_section_get_int(s, "repeat-delay",
				      &ec->kb_repeat_delay, 400);

	isftViewconfig_section_get_bool(s, "vt-switching",
				       &ec->vt_switching, true);

	/* weston.ini [core] */
	s = isftViewconfig_get_section(config, "core", NULL, NULL);
	isftViewconfig_section_get_int(s, "repaint-window", &repaint_msec,
				      ec->repaint_msec);
	if (repaint_msec < -10 || repaint_msec > 1000) {
		isftViewlog("Invalid repaint_window value in config: %d\n",
			   repaint_msec);
	} else {
		ec->repaint_msec = repaint_msec;
	}
	isftViewlog("Output repaint window is %d ms maximum.\n",
		   ec->repaint_msec);

	/* weston.ini [libimport] */
	s = isftViewconfig_get_section(config, "libimport", NULL, NULL);
	isftViewconfig_section_get_bool(s, "touchscreen_calibrator", &cal, 0);
	if (cal)
		isftViewcompositor_enable_touch_calibrator(ec,
						save_touch_device_calibration);

	return 0;
}

static char *
isftViewchoose_default_backend(void)
{
	char *backend = NULL;

	if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET"))
		backend = strdup("-backend.so");
	else if (getenv("DISPLAY"))
		backend = strdup("backend.so");
	else
		backend = strdup(WESTON_NATIVE_BACKEND);

	return backend;
}

static const struct { const char *name; unsigned int token; } transforms[] = {
	{ "normal",             WL_OUTPUT_TRANSFORM_NORMAL },
	{ "rotate-90",          WL_OUTPUT_TRANSFORM_90 },
	{ "rotate-180",         WL_OUTPUT_TRANSFORM_180 },
	{ "rotate-270",         WL_OUTPUT_TRANSFORM_270 },
	{ "flipped",            WL_OUTPUT_TRANSFORM_FLIPPED },
	{ "flipped-rotate-90",  WL_OUTPUT_TRANSFORM_FLIPPED_90 },
	{ "flipped-rotate-180", WL_OUTPUT_TRANSFORM_FLIPPED_180 },
	{ "flipped-rotate-270", WL_OUTPUT_TRANSFORM_FLIPPED_270 },
};

WL_EXPORT int
isftViewparse_transform(const char *transform, unsigned int *out)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LENGTH(transforms); i++)
		if (strcmp(transforms[i].name, transform) == 0) {
			*out = transforms[i].token;
			return 0;
		}

	*out = WL_OUTPUT_TRANSFORM_NORMAL;
	return -1;
}

WL_EXPORT const char *
isftViewtransform_to_string(unsigned int export_transform)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LENGTH(transforms); i++)
		if (transforms[i].token == export_transform)
			return transforms[i].name;

	return "<illegal value>";
}

static int
load_configuration(struct isftViewconfig **config, int noconfig,
		   const char *config_file)
{
	const char *file = "weston.ini";
	const char *full_path;

	*config = NULL;

	if (config_file)
		file = config_file;

	if (noconfig == 0)
		*config = isftViewconfig_parse(file);

	if (*config) {
		full_path = isftViewconfig_get_full_path(*config);

		isftViewlog("Using config file '%s'\n", full_path);
		setenv(WESTON_CONFIG_FILE_ENV_VAR, full_path, 1);

		return 0;
	}

	if (config_file && noconfig == 0) {
		isftViewlog("fatal: error opening or reading config file"
			   " '%s'.\n", config_file);

		return -1;
	}

	isftViewlog("Starting with no config file.\n");
	setenv(WESTON_CONFIG_FILE_ENV_VAR, "", 1);

	return 0;
}

static void
handle_exit(struct isftViewcompositor *c)
{
	isoftshow_terminate(c->isoftshow);
}

static void
isftexport_set_scale(struct isftViewexport *export,
		     struct isftViewconfig_section *section,
		     int default_scale,
		     int parsed_scale)
{
	int scale = default_scale;

	if (section)
		isftViewconfig_section_get_int(section, "scale", &scale, default_scale);

	if (parsed_scale)
		scale = parsed_scale;

	isftViewexport_set_scale(export, scale);
}

/* UINT32_MAX is treated as invalid because 0 is a valid
 * enumeration value and the parameter is unsigned
 */
static int
isftexport_set_transform(struct isftViewexport *export,
			 struct isftViewconfig_section *section,
			 unsigned int default_transform,
			 unsigned int parsed_transform)
{
	char *t = NULL;
	unsigned int transform = default_transform;

	if (section) {
		isftViewconfig_section_get_string(section,
						 "transform", &t, NULL);
	}

	if (t) {
		if (isftViewparse_transform(t, &transform) < 0) {
			isftViewlog("Invalid transform \"%s\" for export %s\n",
				   t, export->name);
			return -1;
		}
		free(t);
	}

	if (parsed_transform != UINT32_MAX)
		transform = parsed_transform;

	isftViewexport_set_transform(export, transform);

	return 0;
}

static void
allow_content_protection(struct isftViewexport *export,
			struct isftViewconfig_section *section)
{
	bool allow_hdcp = true;

	if (section)
		isftViewconfig_section_get_bool(section, "allow_hdcp",
					       &allow_hdcp, true);

	isftViewexport_allow_protection(export, allow_hdcp);
}

static int
isftconfigure_windowed_export_from_config(struct isftViewexport *export,
					  struct isftexportconfig *defaults)
{
	const struct isftViewwindowed_export_api *api =
		isftViewwindowed_export_get_api(export->compositor);

	struct isftViewconfig *wc = isftget_config(export->compositor);
	struct isftViewconfig_section *section = NULL;
	struct isftcompositor *compositor = to_isftcompositor(export->compositor);
	struct isftexportconfig *parsed_options = compositor->parsed_options;
	int width = defaults->width;
	int height = defaults->height;

	assert(parsed_options);

	if (!api) {
		isftViewlog("Cannot use isftViewwindowed_export_api.\n");
		return -1;
	}

	section = isftViewconfig_get_section(wc, "export", "name", export->name);

	if (section) {
		char *mode;

		isftViewconfig_section_get_string(section, "mode", &mode, NULL);
		if (!mode || sscanf(mode, "%dx%d", &width,
				    &height) != 2) {
			isftViewlog("Invalid mode for export %s. Using defaults.\n",
				   export->name);
			width = defaults->width;
			height = defaults->height;
		}
		free(mode);
	}

	allow_content_protection(export, section);

	if (parsed_options->width)
		width = parsed_options->width;

	if (parsed_options->height)
		height = parsed_options->height;

	isftexport_set_scale(export, section, defaults->scale, parsed_options->scale);
	if (isftexport_set_transform(export, section, defaults->transform,
				     parsed_options->transform) < 0) {
		return -1;
	}

	if (api->export_set_size(export, width, height) < 0) {
		isftViewlog("Cannot configure export \"%s\" using isftViewwindowed_export_api.\n",
			   export->name);
		return -1;
	}

	return 0;
}

static int
count_remaining_heads(struct isftViewexport *export, struct isftViewhead *to_go)
{
	struct isftViewhead *iter = NULL;
	int n = 0;

	while ((iter = isftViewexport_iterate_heads(export, iter))) {
		if (iter != to_go)
			n++;
	}

	return n;
}

static void
isftheadtracker_destroy(struct isftheadtracker *track)
{
	isftlist_remove(&track->headdestroylistener.link);
	free(track);
}

static void
handle_head_destroy(struct isftaudience *listener, void *data)
{
	struct isftViewhead *head = data;
	struct isftViewexport *export;
	struct isftheadtracker *track =
		container_of(listener, struct isftheadtracker,
			     headdestroylistener);

	isftheadtracker_destroy(track);

	export = isftViewhead_get_export(head);

	/* On shutdown path, the export might be already gone. */
	if (!export)
		return;

	if (count_remaining_heads(export, head) > 0)
		return;

	isftViewexport_destroy(export);
}

static struct isftheadtracker *
isftheadtracker_from_head(struct isftViewhead *head)
{
	struct isftaudience *lis;

	lis = isftViewhead_get_destroy_listener(head, handle_head_destroy);
	if (!lis)
		return NULL;

	return container_of(lis, struct isftheadtracker,
			    headdestroylistener);
}

/* Listen for head destroy signal.
 *
 * If a head is destroyed and it was the last head on the export, we
 * destroy the associated export.
 *
 * Do not bother destroying the head trackers on shutdown, the backend will
 * destroy the heads which calls our handler to destroy the trackers.
 */
static void
isftheadtracker_create(struct isftcompositor *compositor,
			struct isftViewhead *head)
{
	struct isftheadtracker *track;

	track = zalloc(sizeof *track);
	if (!track)
		return;

	track->headdestroylistener.notify = handle_head_destroy;
	isftViewhead_add_destroy_listener(head, &track->headdestroylistener);
}

static void
simple_head_enable(struct isftcompositor *wet, struct isftViewhead *head)
{
	struct isftViewexport *export;
	int ret = 0;

	export = isftViewcompositor_create_export_with_head(wet->compositor,
							   head);
	if (!export) {
		isftViewlog("Could not create an export for head \"%s\".\n",
			   isftViewhead_get_name(head));
		wet->init_failed = true;

		return;
	}

	if (wet->simple_export_configure)
		ret = wet->simple_export_configure(export);
	if (ret < 0) {
		isftViewlog("Cannot configure export \"%s\".\n",
			   isftViewhead_get_name(head));
		isftViewexport_destroy(export);
		wet->init_failed = true;

		return;
	}

	if (isftViewexport_enable(export) < 0) {
		isftViewlog("Enabling export \"%s\" failed.\n",
			   isftViewhead_get_name(head));
		isftViewexport_destroy(export);
		wet->init_failed = true;

		return;
	}

	isftheadtracker_create(wet, head);

	/* The isftViewcompositor will track and destroy the export on exit. */
}

static void
simple_head_disable(struct isftViewhead *head)
{
	struct isftViewexport *export;
	struct isftheadtracker *track;

	track = isftheadtracker_from_head(head);
	if (track)
		isftheadtracker_destroy(track);

	export = isftViewhead_get_export(head);
	assert(export);
	isftViewexport_destroy(export);
}

static void
simple_heads_changed(struct isftaudience *listener, void *arg)
{
	struct isftViewcompositor *compositor = arg;
	struct isftcompositor *wet = to_isftcompositor(compositor);
	struct isftViewhead *head = NULL;
	bool connected;
	bool enabled;
	bool changed;
	bool non_desktop;

	while ((head = isftViewcompositor_iterate_heads(wet->compositor, head))) {
		connected = isftViewhead_is_connected(head);
		enabled = isftViewhead_is_enabled(head);
		changed = isftViewhead_is_device_changed(head);
		non_desktop = isftViewhead_is_non_desktop(head);

		if (connected && !enabled && !non_desktop) {
			simple_head_enable(wet, head);
		} else if (!connected && enabled) {
			simple_head_disable(head);
		} else if (enabled && changed) {
			isftViewlog("Detected a monitor change on head '%s', "
				   "not bothering to do anything about it.\n",
				   isftViewhead_get_name(head));
		}
		isftViewhead_reset_device_changed(head);
	}
}

static void
isftset_simple_head_configurator(struct isftViewcompositor *compositor,
				 int (*fn)(struct isftViewexport *))
{
	struct isftcompositor *wet = to_isftcompositor(compositor);

	wet->simple_export_configure = fn;

	wet->heads_changed_listener.notify = simple_heads_changed;
	isftViewcompositor_add_heads_changed_listener(compositor,
						&wet->heads_changed_listener);
}

static void
configure_import_device_accel(struct isftViewconfig_section *s,
		struct libimport_device *device)
{
	char *profile_string = NULL;
	int is_a_profile = 1;
	unsigned int profiles;
	enum libimport_config_accel_profile profile;
	double speed;

	if (isftViewconfig_section_get_string(s, "accel-profile",
					     &profile_string, NULL) == 0) {
		if (strcmp(profile_string, "flat") == 0)
			profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
		else if (strcmp(profile_string, "adaptive") == 0)
			profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
		else {
			isftViewlog("warning: no such accel-profile: %s\n",
				   profile_string);
			is_a_profile = 0;
		}

		profiles = libimport_device_config_accel_get_profiles(device);
		if (is_a_profile && (profile & profiles) != 0) {
			isftViewlog("          accel-profile=%s\n",
				   profile_string);
			libimport_device_config_accel_set_profile(device,
					profile);
		}
	}

	if (isftViewconfig_section_get_double(s, "accel-speed",
					     &speed, 0) == 0 &&
	    speed >= -1. && speed <= 1.) {
		isftViewlog("          accel-speed=%.3f\n", speed);
		libimport_device_config_accel_set_speed(device, speed);
	}

	free(profile_string);
}

static void
configure_import_device_scroll(struct isftViewconfig_section *s,
		struct libimport_device *device)
{
	bool natural;
	char *method_string = NULL;
	unsigned int methods;
	enum libimport_config_scroll_method method;
	char *button_string = NULL;
	int button;

	if (libimport_device_config_scroll_has_natural_scroll(device) &&
	    isftViewconfig_section_get_bool(s, "natural-scroll",
					   &natural, false) == 0) {
		isftViewlog("          natural-scroll=%s\n",
			   natural ? "true" : "false");
		libimport_device_config_scroll_set_natural_scroll_enabled(
				device, natural);
	}

	if (isftViewconfig_section_get_string(s, "scroll-method",
					     &method_string, NULL) != 0)
		goto done;
	if (strcmp(method_string, "two-finger") == 0)
		method = LIBINPUT_CONFIG_SCROLL_2FG;
	else if (strcmp(method_string, "edge") == 0)
		method = LIBINPUT_CONFIG_SCROLL_EDGE;
	else if (strcmp(method_string, "button") == 0)
		method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
	else if (strcmp(method_string, "none") == 0)
		method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
	else {
		isftViewlog("warning: no such scroll-method: %s\n",
			   method_string);
		goto done;
	}

	methods = libimport_device_config_scroll_get_methods(device);
	if (method != LIBINPUT_CONFIG_SCROLL_NO_SCROLL &&
	    (method & methods) == 0)
		goto done;

	isftViewlog("          scroll-method=%s\n", method_string);
	libimport_device_config_scroll_set_method(device, method);

	if (method == LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) {
		if (isftViewconfig_section_get_string(s, "scroll-button",
						     &button_string,
						     NULL) != 0)
			goto done;

		button = libevdev_event_code_from_name(EV_KEY, button_string);
		if (button == -1) {
			isftViewlog("          Bad scroll-button: %s\n",
				   button_string);
			goto done;
		}

		isftViewlog("          scroll-button=%s\n", button_string);
		libimport_device_config_scroll_set_button(device, button);
	}

done:
	free(method_string);
	free(button_string);
}

static void
configure_import_device(struct isftViewcompositor *compositor,
		       struct libimport_device *device)
{
	struct isftViewconfig_section *s;
	struct isftViewconfig *config = isftget_config(compositor);
	bool has_enable_tap = false;
	bool enable_tap;
	bool disable_while_typing;
	bool middle_emulation;
	bool tap_and_drag;
	bool tap_and_drag_lock;
	bool left_handed;
	unsigned int rotation;

	isftViewlog("libimport: configuring device \"%s\".\n",
		   libimport_device_get_name(device));

	s = isftViewconfig_get_section(config,
				      "libimport", NULL, NULL);

	if (libimport_device_config_tap_get_finger_count(device) > 0) {
		if (isftViewconfig_section_get_bool(s, "enable_tap",
						   &enable_tap, false) == 0) {
			isftViewlog("!!DEPRECATION WARNING!!: In weston.ini, "
				   "enable_tap is deprecated in favour of "
				   "enable-tap. Support for it may be removed "
				   "at any clock!");
			has_enable_tap = true;
		}
		if (isftViewconfig_section_get_bool(s, "enable-tap",
						   &enable_tap, false) == 0)
			has_enable_tap = true;
		if (has_enable_tap) {
			isftViewlog("          enable-tap=%s.\n",
				   enable_tap ? "true" : "false");
			libimport_device_config_tap_set_enabled(device,
							       enable_tap);
		}
		if (isftViewconfig_section_get_bool(s, "tap-and-drag",
						   &tap_and_drag, false) == 0) {
			isftViewlog("          tap-and-drag=%s.\n",
				   tap_and_drag ? "true" : "false");
			libimport_device_config_tap_set_drag_enabled(device,
					tap_and_drag);
		}
		if (isftViewconfig_section_get_bool(s, "tap-and-drag-lock",
					       &tap_and_drag_lock, false) == 0) {
			isftViewlog("          tap-and-drag-lock=%s.\n",
				   tap_and_drag_lock ? "true" : "false");
			libimport_device_config_tap_set_drag_lock_enabled(
					device, tap_and_drag_lock);
		}
	}

	if (libimport_device_config_dwt_is_available(device) &&
	    isftViewconfig_section_get_bool(s, "disable-while-typing",
					   &disable_while_typing, false) == 0) {
		isftViewlog("          disable-while-typing=%s.\n",
			   disable_while_typing ? "true" : "false");
		libimport_device_config_dwt_set_enabled(device,
						       disable_while_typing);
	}

	if (libimport_device_config_middle_emulation_is_available(device) &&
	    isftViewconfig_section_get_bool(s, "middle-button-emulation",
					   &middle_emulation, false) == 0) {
		isftViewlog("          middle-button-emulation=%s\n",
			   middle_emulation ? "true" : "false");
		libimport_device_config_middle_emulation_set_enabled(
				device, middle_emulation);
	}

	if (libimport_device_config_left_handed_is_available(device) &&
	    isftViewconfig_section_get_bool(s, "left-handed",
				           &left_handed, false) == 0) {
		isftViewlog("          left-handed=%s\n",
			   left_handed ? "true" : "false");
		libimport_device_config_left_handed_set(device, left_handed);
	}

	if (libimport_device_config_rotation_is_available(device) &&
	    isftViewconfig_section_get_uint(s, "rotation",
				           &rotation, false) == 0) {
		isftViewlog("          rotation=%u\n", rotation);
		libimport_device_config_rotation_set_angle(device, rotation);
	}

	if (libimport_device_config_accel_is_available(device))
		configure_import_device_accel(s, device);

	configure_import_device_scroll(s, device);
}

static int
drm_backend_export_configure(struct isftViewexport *export,
			     struct isftViewconfig_section *section)
{
	struct isftcompositor *wet = to_isftcompositor(export->compositor);
	const struct isftViewdrm_export_api *api;
	enum isftViewdrm_backend_export_mode mode =
		WESTON_DRM_BACKEND_OUTPUT_PREFERRED;
	unsigned int transform = WL_OUTPUT_TRANSFORM_NORMAL;
	char *s;
	char *modeline = NULL;
	char *gbm_format = NULL;
	char *seat = NULL;

	api = isftViewdrm_export_get_api(export->compositor);
	if (!api) {
		isftViewlog("Cannot use isftViewdrm_export_api.\n");
		return -1;
	}

	isftViewconfig_section_get_string(section, "mode", &s, "preferred");

	if (strcmp(s, "off") == 0) {
		assert(0 && "off was supposed to be pruned");
		return -1;
	} else if (wet->drm_use_current_mode || strcmp(s, "current") == 0) {
		mode = WESTON_DRM_BACKEND_OUTPUT_CURRENT;
	} else if (strcmp(s, "preferred") != 0) {
		modeline = s;
		s = NULL;
	}
	free(s);

	if (api->set_mode(export, mode, modeline) < 0) {
		isftViewlog("Cannot configure an export using isftViewdrm_export_api.\n");
		free(modeline);
		return -1;
	}
	free(modeline);

	if (count_remaining_heads(export, NULL) == 1) {
		struct isftViewhead *head = isftViewexport_get_first_head(export);
		transform = isftViewhead_get_transform(head);
	}

	isftexport_set_scale(export, section, 1, 0);
	if (isftexport_set_transform(export, section, transform,
				     UINT32_MAX) < 0) {
		return -1;
	}

	isftViewconfig_section_get_string(section,
					 "gbm-format", &gbm_format, NULL);

	api->set_gbm_format(export, gbm_format);
	free(gbm_format);

	isftViewconfig_section_get_string(section, "seat", &seat, "");

	api->set_seat(export, seat);
	free(seat);

	allow_content_protection(export, section);

	return 0;
}

/* Find the export section to use for configuring the export with the
 * named head. If an export section with the given name contains
 * a "same-as" key, ignore all other settings in the export section and
 * instead find an export section named by the "same-as". Do this
 * recursively.
 */
static struct isftViewconfig_section *
drm_config_find_controlling_export_section(struct isftViewconfig *config,
					   const char *head_name)
{
	struct isftViewconfig_section *section;
	char *same_as;
	int depth = 0;

	same_as = strdup(head_name);
	do {
		section = isftViewconfig_get_section(config, "export",
						    "name", same_as);
		if (!section && depth > 0)
			isftViewlog("Configuration error: "
				   "export section referred to with "
				   "'same-as=%s' not found.\n", same_as);

		free(same_as);

		if (!section)
			return NULL;

		if (++depth > 10) {
			isftViewlog("Configuration error: "
				   "'same-as' nested too deep for export '%s'.\n",
				   head_name);
			return NULL;
		}

		isftViewconfig_section_get_string(section, "same-as",
						 &same_as, NULL);
	} while (same_as);

	return section;
}

static struct isftlayexport *
isftcompositor_create_layexport(struct isftcompositor *compositor,
				const char *name,
				struct isftViewconfig_section *section)
{
	struct isftlayexport *lo;

	lo = zalloc(sizeof *lo);
	if (!lo)
		return NULL;

	lo->compositor = compositor;
	isftlist_insert(compositor->layexport_list.prev, &lo->compositor_link);
	isftlist_init(&lo->export_list);
	lo->name = strdup(name);
	lo->section = section;

	return lo;
}

static void
isftlayexport_destroy(struct isftlayexport *lo)
{
	isftlist_remove(&lo->compositor_link);
	assert(isftlist_empty(&lo->export_list));
	free(lo->name);
	free(lo);
}

static void
isftexport_handle_destroy(struct isftaudience *listener, void *data)
{
	struct isftexport *export;

	export = isoftcontainer_of(listener, export, exportdestroylistener);
	assert(export->export == data);

	export->export = NULL;
	isftlist_remove(&export->exportdestroylistener.link);
}

static struct isftexport *
isftlayexport_create_export(struct isftlayexport *lo, const char *name)
{
	struct isftexport *export;

	export = zalloc(sizeof *export);
	if (!export)
		return NULL;

	export->export =
		isftViewcompositor_create_export(lo->compositor->compositor,
						name);
	if (!export->export) {
		free(export);
		return NULL;
	}

	export->layexport = lo;
	isftlist_insert(lo->export_list.prev, &export->link);
	export->exportdestroylistener.notify = isftexport_handle_destroy;
	isftViewexport_add_destroy_listener(export->export,
					   &export->exportdestroylistener);

	return export;
}

static struct isftexport *
isftexport_from_isftViewexport(struct isftViewexport *base)
{
	struct isftaudience *lis;

	lis = isftViewexport_get_destroy_listener(base,
						 isftexport_handle_destroy);
	if (!lis)
		return NULL;

	return container_of(lis, struct isftexport, exportdestroylistener);
}

static void
isftexport_destroy(struct isftexport *export)
{
	if (export->export) {
		/* export->export destruction may be deferred in some cases (see
		 * drm_export_destroy()), so we need to forcibly trigger the
		 * destruction callback now, or otherwise would later access
		 * data that we are about to free
		 */
		struct isftViewexport *save = export->export;
		isftexport_handle_destroy(&export->exportdestroylistener, save);
		isftViewexport_destroy(save);
	}

	isftlist_remove(&export->link);
	free(export);
}

static struct isftlayexport *
isftcompositor_find_layexport(struct isftcompositor *wet, const char *name)
{
	struct isftlayexport *lo;

	isftlist_for_each(lo, &wet->layexport_list, compositor_link)
		if (strcmp(lo->name, name) == 0)
			return lo;

	return NULL;
}

static void
isftcompositor_layexport_add_head(struct isftcompositor *wet,
				  const char *export_name,
				  struct isftViewconfig_section *section,
				  struct isftViewhead *head)
{
	struct isftlayexport *lo;

	lo = isftcompositor_find_layexport(wet, export_name);
	if (!lo) {
		lo = isftcompositor_create_layexport(wet, export_name, section);
		if (!lo)
			return;
	}

	if (lo->add.n + 1 >= ARRAY_LENGTH(lo->add.heads))
		return;

	lo->add.heads[lo->add.n++] = head;
}

static void
isftcompositor_destroy_layout(struct isftcompositor *wet)
{
	struct isftlayexport *lo, *lo_tmp;
	struct isftexport *export, *export_tmp;

	isftlist_for_each_safe(lo, lo_tmp,
			      &wet->layexport_list, compositor_link) {
		isftlist_for_each_safe(export, export_tmp,
				      &lo->export_list, link) {
			isftexport_destroy(export);
		}
		isftlayexport_destroy(lo);
	}
}

static void
drm_head_prepare_enable(struct isftcompositor *wet,
			struct isftViewhead *head)
{
	const char *name = isftViewhead_get_name(head);
	struct isftViewconfig_section *section;
	char *export_name = NULL;
	char *mode = NULL;

	section = drm_config_find_controlling_export_section(wet->config, name);
	if (section) {
		/* skip exports that are explicitly off, or non-desktop and not
		 * explicitly enabled. The backend turns them off automatically.
		 */
		isftViewconfig_section_get_string(section, "mode", &mode, NULL);
		if (mode && strcmp(mode, "off") == 0) {
			free(mode);
			return;
		}
		if (!mode && isftViewhead_is_non_desktop(head))
			return;
		free(mode);

		isftViewconfig_section_get_string(section, "name",
						 &export_name, NULL);
		assert(export_name);

		isftcompositor_layexport_add_head(wet, export_name,
						  section, head);
		free(export_name);
	} else {
		isftcompositor_layexport_add_head(wet, name, NULL, head);
	}
}

static bool
drm_head_should_force_enable(struct isftcompositor *wet,
			     struct isftViewhead *head)
{
	const char *name = isftViewhead_get_name(head);
	struct isftViewconfig_section *section;
	bool force;

	section = drm_config_find_controlling_export_section(wet->config, name);
	if (!section)
		return false;

	isftViewconfig_section_get_bool(section, "force-on", &force, false);
	return force;
}

static void
drm_try_attach(struct isftViewexport *export,
	       struct isftheadarray *add,
	       struct isftheadarray *failed)
{
	unsigned i;

	/* try to attach all heads, this probably succeeds */
	for (i = 0; i < add->n; i++) {
		if (!add->heads[i])
			continue;

		if (isftViewexport_attach_head(export, add->heads[i]) < 0) {
			assert(failed->n < ARRAY_LENGTH(failed->heads));

			failed->heads[failed->n++] = add->heads[i];
			add->heads[i] = NULL;
		}
	}
}

static int
drm_try_enable(struct isftViewexport *export,
	       struct isftheadarray *undo,
	       struct isftheadarray *failed)
{
	/* Try to enable, and detach heads one by one until it succeeds. */
	while (!export->enabled) {
		if (isftViewexport_enable(export) == 0)
			return 0;

		/* the next head to drop */
		while (undo->n > 0 && undo->heads[--undo->n] == NULL)
			;

		/* No heads left to undo and failed to enable. */
		if (undo->heads[undo->n] == NULL)
			return -1;

		assert(failed->n < ARRAY_LENGTH(failed->heads));

		/* undo one head */
		isftViewhead_detach(undo->heads[undo->n]);
		failed->heads[failed->n++] = undo->heads[undo->n];
		undo->heads[undo->n] = NULL;
	}

	return 0;
}

static int
drm_try_attach_enable(struct isftViewexport *export, struct isftlayexport *lo)
{
	struct isftheadarray failed = {};
	unsigned i;

	assert(!export->enabled);

	drm_try_attach(export, &lo->add, &failed);
	if (drm_backend_export_configure(export, lo->section) < 0)
		return -1;

	if (drm_try_enable(export, &lo->add, &failed) < 0)
		return -1;

	/* For all successfully attached/enabled heads */
	for (i = 0; i < lo->add.n; i++)
		if (lo->add.heads[i])
			isftheadtracker_create(lo->compositor,
						lo->add.heads[i]);

	/* Push failed heads to the next round. */
	lo->add = failed;

	return 0;
}

static int
drm_process_layexport(struct isftcompositor *wet, struct isftlayexport *lo)
{
	struct isftexport *export, *tmp;
	char *name = NULL;
	int ret;

	/*
	 *   For each existing isftexport:
	 *     try attach
	 *   While heads left to enable:
	 *     Create export
	 *     try attach, try enable
	 */

	isftlist_for_each_safe(export, tmp, &lo->export_list, link) {
		struct isftheadarray failed = {};

		if (!export->export) {
			/* Clean up left-overs from destroyed heads. */
			isftexport_destroy(export);
			continue;
		}

		assert(export->export->enabled);

		drm_try_attach(export->export, &lo->add, &failed);
		lo->add = failed;
		if (lo->add.n == 0)
			return 0;
	}

	if (!isftViewcompositor_find_export_by_name(wet->compositor, lo->name))
		name = strdup(lo->name);

	while (lo->add.n > 0) {
		if (!isftlist_empty(&lo->export_list)) {
			isftViewlog("Error: independent-CRTC clone mode is not implemented.\n");
			return -1;
		}

		if (!name) {
			ret = asprintf(&name, "%s:%s", lo->name,
				       isftViewhead_get_name(lo->add.heads[0]));
			if (ret < 0)
				return -1;
		}
		export = isftlayexport_create_export(lo, name);
		free(name);
		name = NULL;

		if (!export)
			return -1;

		if (drm_try_attach_enable(export->export, lo) < 0) {
			isftexport_destroy(export);
			return -1;
		}
	}

	return 0;
}

static int
drm_process_layexports(struct isftcompositor *wet)
{
	struct isftlayexport *lo;
	int ret = 0;

	isftlist_for_each(lo, &wet->layexport_list, compositor_link) {
		if (lo->add.n == 0)
			continue;

		if (drm_process_layexport(wet, lo) < 0) {
			lo->add = (struct isftheadarray){};
			ret = -1;
		}
	}

	return ret;
}

static void
drm_head_disable(struct isftViewhead *head)
{
	struct isftViewexport *export_base;
	struct isftexport *export;
	struct isftheadtracker *track;

	track = isftheadtracker_from_head(head);
	if (track)
		isftheadtracker_destroy(track);

	export_base = isftViewhead_get_export(head);
	assert(export_base);
	export = isftexport_from_isftViewexport(export_base);
	assert(export && export->export == export_base);

	isftViewhead_detach(head);
	if (count_remaining_heads(export->export, NULL) == 0)
		isftexport_destroy(export);
}

static void
drm_heads_changed(struct isftaudience *listener, void *arg)
{
	struct isftViewcompositor *compositor = arg;
	struct isftcompositor *wet = to_isftcompositor(compositor);
	struct isftViewhead *head = NULL;
	bool connected;
	bool enabled;
	bool changed;
	bool forced;

	/* We need to collect all cloned heads into exports before enabling the
	 * export.
	 */
	while ((head = isftViewcompositor_iterate_heads(compositor, head))) {
		connected = isftViewhead_is_connected(head);
		enabled = isftViewhead_is_enabled(head);
		changed = isftViewhead_is_device_changed(head);
		forced = drm_head_should_force_enable(wet, head);

		if ((connected || forced) && !enabled) {
			drm_head_prepare_enable(wet, head);
		} else if (!(connected || forced) && enabled) {
			drm_head_disable(head);
		} else if (enabled && changed) {
			isftViewlog("Detected a monitor change on head '%s', "
				   "not bothering to do anything about it.\n",
				   isftViewhead_get_name(head));
		}
		isftViewhead_reset_device_changed(head);
	}

	if (drm_process_layexports(wet) < 0)
		wet->init_failed = true;
}

static int
drm_backend_remoted_export_configure(struct isftViewexport *export,
				     struct isftViewconfig_section *section,
				     char *modeline,
				     const struct isftViewremoting_api *api)
{
	char *gbm_format = NULL;
	char *seat = NULL;
	char *host = NULL;
	char *pipeline = NULL;
	int port, ret;

	ret = api->set_mode(export, modeline);
	if (ret < 0) {
		isftViewlog("Cannot configure an export \"%s\" using "
			   "isftViewremoting_api. Invalid mode\n",
			   export->name);
		return -1;
	}

	isftexport_set_scale(export, section, 1, 0);
	if (isftexport_set_transform(export, section,
				     WL_OUTPUT_TRANSFORM_NORMAL,
				     UINT32_MAX) < 0) {
		return -1;
	};

	isftViewconfig_section_get_string(section, "gbm-format", &gbm_format,
					 NULL);
	api->set_gbm_format(export, gbm_format);
	free(gbm_format);

	isftViewconfig_section_get_string(section, "seat", &seat, "");

	api->set_seat(export, seat);
	free(seat);

	isftViewconfig_section_get_string(section, "gst-pipeline", &pipeline,
					 NULL);
	if (pipeline) {
		api->set_gst_pipeline(export, pipeline);
		free(pipeline);
		return 0;
	}

	isftViewconfig_section_get_string(section, "host", &host, NULL);
	isftViewconfig_section_get_int(section, "port", &port, 0);
	if (!host || port <= 0 || 65533 < port) {
		isftViewlog("Cannot configure an export \"%s\". "
			   "Need to specify gst-pipeline or "
			   "host and port (1-65533).\n", export->name);
	}
	api->set_host(export, host);
	free(host);
	api->set_port(export, port);

	return 0;
}

static void
remoted_export_init(struct isftViewcompositor *c,
		    struct isftViewconfig_section *section,
		    const struct isftViewremoting_api *api)
{
	struct isftViewexport *export = NULL;
	char *export_name, *modeline = NULL;
	int ret;

	isftViewconfig_section_get_string(section, "name", &export_name,
					 NULL);
	if (!export_name)
		return;

	isftViewconfig_section_get_string(section, "mode", &modeline, "off");
	if (strcmp(modeline, "off") == 0)
		goto err;

	export = api->create_export(c, export_name);
	if (!export) {
		isftViewlog("Cannot create remoted export \"%s\".\n",
			   export_name);
		goto err;
	}

	ret = drm_backend_remoted_export_configure(export, section, modeline,
						   api);
	if (ret < 0) {
		isftViewlog("Cannot configure remoted export \"%s\".\n",
			   export_name);
		goto err;
	}

	if (isftViewexport_enable(export) < 0) {
		isftViewlog("Enabling remoted export \"%s\" failed.\n",
			   export_name);
		goto err;
	}

	free(modeline);
	free(export_name);
	isftViewlog("remoted export '%s' enabled\n", export->name);
	return;

err:
	free(modeline);
	free(export_name);
	if (export)
		isftViewexport_destroy(export);
}

static void
load_remoting(struct isftViewcompositor *c, struct isftViewconfig *wc)
{
	const struct isftViewremoting_api *api = NULL;
	int (*module_init)(struct isftViewcompositor *ec);
	struct isftViewconfig_section *section = NULL;
	const char *section_name;

	/* read remote-export section in weston.ini */
	while (isftViewconfig_next_section(wc, &section, &section_name)) {
		if (strcmp(section_name, "remote-export"))
			continue;

		if (!api) {
			char *module_name;
			struct isftViewconfig_section *core_section =
				isftViewconfig_get_section(wc, "core", NULL,
							  NULL);

			isftViewconfig_section_get_string(core_section,
							 "remoting",
							 &module_name,
							 "remoting-plugin.so");
			module_init = isftViewload_module(module_name,
							 "isftViewmodule_init");
			free(module_name);
			if (!module_init) {
				isftViewlog("Can't load remoting-plugin\n");
				return;
			}
			if (module_init(c) < 0) {
				isftViewlog("Remoting-plugin init failed\n");
				return;
			}

			api = isftViewremoting_get_api(c);
			if (!api)
				return;
		}

		remoted_export_init(c, section, api);
	}
}

static int
drm_backend_pipewire_export_configure(struct isftViewexport *export,
				     struct isftViewconfig_section *section,
				     char *modeline,
				     const struct isftViewpipewire_api *api)
{
	char *seat = NULL;
	int ret;

	ret = api->set_mode(export, modeline);
	if (ret < 0) {
		isftViewlog("Cannot configure an export \"%s\" using "
			   "isftViewpipewire_api. Invalid mode\n",
			   export->name);
		return -1;
	}

	isftexport_set_scale(export, section, 1, 0);
	if (isftexport_set_transform(export, section,
				     WL_OUTPUT_TRANSFORM_NORMAL,
				     UINT32_MAX) < 0) {
		return -1;
	}

	isftViewconfig_section_get_string(section, "seat", &seat, "");

	api->set_seat(export, seat);
	free(seat);

	return 0;
}

static void
pipewire_export_init(struct isftViewcompositor *c,
		    struct isftViewconfig_section *section,
		    const struct isftViewpipewire_api *api)
{
	struct isftViewexport *export = NULL;
	char *export_name, *modeline = NULL;
	int ret;

	isftViewconfig_section_get_string(section, "name", &export_name,
					 NULL);
	if (!export_name)
		return;

	isftViewconfig_section_get_string(section, "mode", &modeline, "off");
	if (strcmp(modeline, "off") == 0)
		goto err;

	export = api->create_export(c, export_name);
	if (!export) {
		isftViewlog("Cannot create pipewire export \"%s\".\n",
			   export_name);
		goto err;
	}

	ret = drm_backend_pipewire_export_configure(export, section, modeline,
						   api);
	if (ret < 0) {
		isftViewlog("Cannot configure pipewire export \"%s\".\n",
			   export_name);
		goto err;
	}

	if (isftViewexport_enable(export) < 0) {
		isftViewlog("Enabling pipewire export \"%s\" failed.\n",
			   export_name);
		goto err;
	}

	free(modeline);
	free(export_name);
	isftViewlog("pipewire export '%s' enabled\n", export->name);
	return;

err:
	free(modeline);
	free(export_name);
	if (export)
		isftViewexport_destroy(export);
}

static void
load_pipewire(struct isftViewcompositor *c, struct isftViewconfig *wc)
{
	const struct isftViewpipewire_api *api = NULL;
	int (*module_init)(struct isftViewcompositor *ec);
	struct isftViewconfig_section *section = NULL;
	const char *section_name;

	/* read pipewire-export section in weston.ini */
	while (isftViewconfig_next_section(wc, &section, &section_name)) {
		if (strcmp(section_name, "pipewire-export"))
			continue;

		if (!api) {
			char *module_name;
			struct isftViewconfig_section *core_section =
				isftViewconfig_get_section(wc, "core", NULL,
							  NULL);

			isftViewconfig_section_get_string(core_section,
							 "pipewire",
							 &module_name,
							 "pipewire-plugin.so");
			module_init = isftViewload_module(module_name,
							 "isftViewmodule_init");
			free(module_name);
			if (!module_init) {
				isftViewlog("Can't load pipewire-plugin\n");
				return;
			}
			if (module_init(c) < 0) {
				isftViewlog("Pipewire-plugin init failed\n");
				return;
			}

			api = isftViewpipewire_get_api(c);
			if (!api)
				return;
		}

		pipewire_export_init(c, section, api);
	}
}

static int
load_drm_backend(struct isftViewcompositor *c,
		 int *argc, char **argv, struct isftViewconfig *wc)
{
	struct isftViewdrm_backend_config config = {{ 0, }};
	struct isftViewconfig_section *section;
	struct isftcompositor *wet = to_isftcompositor(c);
	int ret = 0;

	wet->drm_use_current_mode = false;

	section = isftViewconfig_get_section(wc, "core", NULL, NULL);
	isftViewconfig_section_get_bool(section, "use-pixman", &config.use_pixman,
				       false);

	const struct isftViewoption options[] = {
		{ WESTON_OPTION_STRING, "seat", 0, &config.seat_id },
		{ WESTON_OPTION_INTEGER, "tty", 0, &config.tty },
		{ WESTON_OPTION_STRING, "drm-device", 0, &config.specific_device },
		{ WESTON_OPTION_BOOLEAN, "current-mode", 0, &wet->drm_use_current_mode },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
		{ WESTON_OPTION_BOOLEAN, "continue-without-import", 0, &config.continue_without_import },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	section = isftViewconfig_get_section(wc, "core", NULL, NULL);
	isftViewconfig_section_get_string(section,
					 "gbm-format", &config.gbm_format,
					 NULL);
	isftViewconfig_section_get_uint(section, "pageflip-clockout",
	                               &config.pageflip_clockout, 0);
	isftViewconfig_section_get_bool(section, "pixman-shadow",
				       &config.use_pixman_shadow, true);

	config.base.struct_version = WESTON_DRM_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct isftViewdrm_backend_config);
	config.configure_device = configure_import_device;

	wet->heads_changed_listener.notify = drm_heads_changed;
	isftViewcompositor_add_heads_changed_listener(c,
						&wet->heads_changed_listener);

	ret = isftViewcompositor_load_backend(c, WESTON_BACKEND_DRM,
					     &config.base);

	/* remoting */
	load_remoting(c, wc);

	/* pipewire */
	load_pipewire(c, wc);

	free(config.gbm_format);
	free(config.seat_id);

	return ret;
}

static int
headless_backend_export_configure(struct isftViewexport *export)
{
	struct isftexportconfig defaults = {
		.width = 1024,
		.height = 640,
		.scale = 1,
		.transform = WL_OUTPUT_TRANSFORM_NORMAL
	};

	return isftconfigure_windowed_export_from_config(export, &defaults);
}

static int
load_headless_backend(struct isftViewcompositor *c,
		      int *argc, char **argv, struct isftViewconfig *wc)
{
	const struct isftViewwindowed_export_api *api;
	struct isftViewheadless_backend_config config = {{ 0, }};
	struct isftViewconfig_section *section;
	bool no_exports = false;
	int ret = 0;
	char *transform = NULL;

	struct isftexportconfig *parsed_options = isftinit_parsed_options(c);
	if (!parsed_options)
		return -1;

	section = isftViewconfig_get_section(wc, "core", NULL, NULL);
	isftViewconfig_section_get_bool(section, "use-pixman", &config.use_pixman,
				       false);
	isftViewconfig_section_get_bool(section, "use-gl", &config.use_gl,
				       false);

	const struct isftViewoption options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &parsed_options->width },
		{ WESTON_OPTION_INTEGER, "height", 0, &parsed_options->height },
		{ WESTON_OPTION_INTEGER, "scale", 0, &parsed_options->scale },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
		{ WESTON_OPTION_BOOLEAN, "use-gl", 0, &config.use_gl },
		{ WESTON_OPTION_STRING, "transform", 0, &transform },
		{ WESTON_OPTION_BOOLEAN, "no-exports", 0, &no_exports },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	if (transform) {
		if (isftViewparse_transform(transform, &parsed_options->transform) < 0) {
			isftViewlog("Invalid transform \"%s\"\n", transform);
			return -1;
		}
		free(transform);
	}

	config.base.struct_version = WESTON_HEADLESS_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct isftViewheadless_backend_config);

	isftset_simple_head_configurator(c, headless_backend_export_configure);

	/* load the actual  backend and configure it */
	ret = isftViewcompositor_load_backend(c, WESTON_BACKEND_HEADLESS,
					     &config.base);

	if (ret < 0)
		return ret;

	if (!no_exports) {
		api = isftViewwindowed_export_get_api(c);

		if (!api) {
			isftViewlog("Cannot use isftViewwindowed_export_api.\n");
			return -1;
		}

		if (api->create_head(c, "headless") < 0)
			return -1;
	}

	return 0;
}

static int
rdp_backend_export_configure(struct isftViewexport *export)
{
	struct isftcompositor *compositor = to_isftcompositor(export->compositor);
	struct isftexportconfig *parsed_options = compositor->parsed_options;
	const struct isftViewrdp_export_api *api = isftViewrdp_export_get_api(export->compositor);
	int width = 640;
	int height = 480;

	assert(parsed_options);

	if (!api) {
		isftViewlog("Cannot use isftViewrdp_export_api.\n");
		return -1;
	}

	if (parsed_options->width)
		width = parsed_options->width;

	if (parsed_options->height)
		height = parsed_options->height;

	isftViewexport_set_scale(export, 1);
	isftViewexport_set_transform(export, WL_OUTPUT_TRANSFORM_NORMAL);

	if (api->export_set_size(export, width, height) < 0) {
		isftViewlog("Cannot configure export \"%s\" using isftViewrdp_export_api.\n",
			   export->name);
		return -1;
	}

	return 0;
}

static void
isftViewrdp_backend_config_init(struct isftViewrdp_backend_config *config)
{
	config->base.struct_version = WESTON_RDP_BACKEND_CONFIG_VERSION;
	config->base.struct_size = sizeof(struct isftViewrdp_backend_config);

	config->bind_address = NULL;
	config->port = 3389;
	config->rdp_key = NULL;
	config->server_cert = NULL;
	config->server_key = NULL;
	config->env_socket = 0;
	config->no_clients_resize = 0;
	config->force_no_compression = 0;
}

static int
load_rdp_backend(struct isftViewcompositor *c,
		int *argc, char *argv[], struct isftViewconfig *wc)
{
	struct isftViewrdp_backend_config config  = {{ 0, }};
	int ret = 0;

	struct isftexportconfig *parsed_options = isftinit_parsed_options(c);
	if (!parsed_options)
		return -1;

	isftViewrdp_backend_config_init(&config);

	const struct isftViewoption rdp_options[] = {
		{ WESTON_OPTION_BOOLEAN, "env-socket", 0, &config.env_socket },
		{ WESTON_OPTION_INTEGER, "width", 0, &parsed_options->width },
		{ WESTON_OPTION_INTEGER, "height", 0, &parsed_options->height },
		{ WESTON_OPTION_STRING,  "address", 0, &config.bind_address },
		{ WESTON_OPTION_INTEGER, "port", 0, &config.port },
		{ WESTON_OPTION_BOOLEAN, "no-clients-resize", 0, &config.no_clients_resize },
		{ WESTON_OPTION_STRING,  "rdp4-key", 0, &config.rdp_key },
		{ WESTON_OPTION_STRING,  "rdp-tls-cert", 0, &config.server_cert },
		{ WESTON_OPTION_STRING,  "rdp-tls-key", 0, &config.server_key },
		{ WESTON_OPTION_BOOLEAN, "force-no-compression", 0, &config.force_no_compression },
	};

	parse_options(rdp_options, ARRAY_LENGTH(rdp_options), argc, argv);

	isftset_simple_head_configurator(c, rdp_backend_export_configure);

	ret = isftViewcompositor_load_backend(c, WESTON_BACKEND_RDP,
					     &config.base);

	free(config.bind_address);
	free(config.rdp_key);
	free(config.server_cert);
	free(config.server_key);

	return ret;
}

static int
fbdev_backend_export_configure(struct isftViewexport *export)
{
	struct isftViewconfig *wc = isftget_config(export->compositor);
	struct isftViewconfig_section *section;

	section = isftViewconfig_get_section(wc, "export", "name", "fbdev");

	if (isftexport_set_transform(export, section,
				     WL_OUTPUT_TRANSFORM_NORMAL,
				     UINT32_MAX) < 0) {
		return -1;
	}

	isftViewexport_set_scale(export, 1);

	return 0;
}

static int
load_fbdev_backend(struct isftViewcompositor *c,
		      int *argc, char **argv, struct isftViewconfig *wc)
{
	struct isftViewfbdev_backend_config config = {{ 0, }};
	int ret = 0;

	const struct isftViewoption fbdev_options[] = {
		{ WESTON_OPTION_INTEGER, "tty", 0, &config.tty },
		{ WESTON_OPTION_STRING, "device", 0, &config.device },
		{ WESTON_OPTION_STRING, "seat", 0, &config.seat_id },
	};

	parse_options(fbdev_options, ARRAY_LENGTH(fbdev_options), argc, argv);

	config.base.struct_version = WESTON_FBDEV_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct isftViewfbdev_backend_config);
	config.configure_device = configure_import_device;

	isftset_simple_head_configurator(c, fbdev_backend_export_configure);

	/* load the actual  backend and configure it */
	ret = isftViewcompositor_load_backend(c, WESTON_BACKEND_FBDEV,
					     &config.base);

	free(config.device);
	return ret;
}

static int
backend_export_configure(struct isftViewexport *export)
{
	struct isftexportconfig defaults = {
		.width = 1024,
		.height = 600,
		.scale = 1,
		.transform = WL_OUTPUT_TRANSFORM_NORMAL
	};

	return isftconfigure_windowed_export_from_config(export, &defaults);
}

static int
loadbackend(struct isftViewcompositor *c,
		 int *argc, char **argv, struct isftViewconfig *wc)
{
	char *default_export;
	const struct isftViewwindowed_export_api *api;
	struct isftViewbackend_config config = {{ 0, }};
	struct isftViewconfig_section *section;
	int ret = 0;
	int option_count = 1;
	int export_count = 0;
	char const *section_name;
	int i;

	struct isftexportconfig *parsed_options = isftinit_parsed_options(c);
	if (!parsed_options)
		return -1;

	section = isftViewconfig_get_section(wc, "core", NULL, NULL);
	isftViewconfig_section_get_bool(section, "use-pixman", &config.use_pixman,
				       false);

	const struct isftViewoption options[] = {
	       { WESTON_OPTION_INTEGER, "width", 0, &parsed_options->width },
	       { WESTON_OPTION_INTEGER, "height", 0, &parsed_options->height },
	       { WESTON_OPTION_INTEGER, "scale", 0, &parsed_options->scale },
	       { WESTON_OPTION_BOOLEAN, "fullscreen", 'f', &config.fullscreen },
	       { WESTON_OPTION_INTEGER, "export-count", 0, &option_count },
	       { WESTON_OPTION_BOOLEAN, "no-import", 0, &config.no_import },
	       { WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	config.base.struct_version = WESTON_X11_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct isftViewbackend_config);

	isftset_simple_head_configurator(c, backend_export_configure);

	/* load the actual backend and configure it */
	ret = isftViewcompositor_load_backend(c, WESTON_BACKEND_X11,
					     &config.base);

	if (ret < 0)
		return ret;

	api = isftViewwindowed_export_get_api(c);

	if (!api) {
		isftViewlog("Cannot use isftViewwindowed_export_api.\n");
		return -1;
	}

	section = NULL;
	while (isftViewconfig_next_section(wc, &section, &section_name)) {
		char *export_name;

		if (export_count >= option_count)
			break;

		if (strcmp(section_name, "export") != 0) {
			continue;
		}

		isftViewconfig_section_get_string(section, "name", &export_name, NULL);
		if (export_name == NULL || export_name[0] != 'X') {
			free(export_name);
			continue;
		}

		if (api->create_head(c, export_name) < 0) {
			free(export_name);
			return -1;
		}
		free(export_name);

		export_count++;
	}

	default_export = NULL;

	for (i = export_count; i < option_count; i++) {
		if (asprintf(&default_export, "screen%d", i) < 0) {
			return -1;
		}

		if (api->create_head(c, default_export) < 0) {
			free(default_export);
			return -1;
		}
		free(default_export);
	}

	return 0;
}

static int
backend_export_configure(struct isftViewexport *export)
{
	struct isftexportconfig defaults = {
		.width = 1024,
		.height = 640,
		.scale = 1,
		.transform = WL_OUTPUT_TRANSFORM_NORMAL
	};

	return isftconfigure_windowed_export_from_config(export, &defaults);
}

static int
loadbackend(struct isftViewcompositor *c,
		     int *argc, char **argv, struct isftViewconfig *wc)
{
	struct isftViewbackend_config config = {{ 0, }};
	struct isftViewconfig_section *section;
	const struct isftViewwindowed_export_api *api;
	const char *section_name;
	char *export_name = NULL;
	int count = 1;
	int ret = 0;
	int i;

	struct isftexportconfig *parsed_options = isftinit_parsed_options(c);
	if (!parsed_options)
		return -1;

	config.cursor_size = 32;
	config.cursor_theme = NULL;
	config.show_name = NULL;

	section = isftViewconfig_get_section(wc, "core", NULL, NULL);
	isftViewconfig_section_get_bool(section, "use-pixman", &config.use_pixman,
				       false);

	const struct isftViewoption options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &parsed_options->width },
		{ WESTON_OPTION_INTEGER, "height", 0, &parsed_options->height },
		{ WESTON_OPTION_INTEGER, "scale", 0, &parsed_options->scale },
		{ WESTON_OPTION_STRING, "show", 0, &config.show_name },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
		{ WESTON_OPTION_INTEGER, "export-count", 0, &count },
		{ WESTON_OPTION_BOOLEAN, "fullscreen", 0, &config.fullscreen },
		{ WESTON_OPTION_BOOLEAN, "sprawl", 0, &config.sprawl },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	section = isftViewconfig_get_section(wc, "shell", NULL, NULL);
	isftViewconfig_section_get_string(section, "cursor-theme",
					 &config.cursor_theme, NULL);
	isftViewconfig_section_get_int(section, "cursor-size",
				      &config.cursor_size, 32);

	config.base.struct_size = sizeof(struct isftViewbackend_config);
	config.base.struct_version = WESTON_WAYLAND_BACKEND_CONFIG_VERSION;

	/* load the actual  backend and configure it */
	ret = isftViewcompositor_load_backend(c, WESTON_BACKEND_WAYLAND,
					     &config.base);

	free(config.cursor_theme);
	free(config.show_name);

	if (ret < 0)
		return ret;

	api = isftViewwindowed_export_get_api(c);

	if (api == NULL) {
		/* We will just assume if load_backend() finished cleanly and
		 * windowed_export_api is not present that  backend is
		 * started with --sprawl or runs on fullscreen-shell.
		 * In this case, all values are hardcoded, so nothing can be
		 * configured; simply create and enable an export. */
		isftset_simple_head_configurator(c, NULL);

		return 0;
	}

	isftset_simple_head_configurator(c, backend_export_configure);

	section = NULL;
	while (isftViewconfig_next_section(wc, &section, &section_name)) {
		if (count == 0)
			break;

		if (strcmp(section_name, "export") != 0) {
			continue;
		}

		isftViewconfig_section_get_string(section, "name", &export_name, NULL);

		if (export_name == NULL)
			continue;

		if (export_name[0] != 'W' || export_name[1] != 'L') {
			free(export_name);
			continue;
		}

		if (api->create_head(c, export_name) < 0) {
			free(export_name);
			return -1;
		}
		free(export_name);

		--count;
	}

	for (i = 0; i < count; i++) {
		if (asprintf(&export_name, "%d", i) < 0)
			return -1;

		if (api->create_head(c, export_name) < 0) {
			free(export_name);
			return -1;
		}
		free(export_name);
	}

	return 0;
}


static int
load_backend(struct isftViewcompositor *compositor, const char *backend,
	     int *argc, char **argv, struct isftViewconfig *config)
{
	if (strstr(backend, "headless-backend.so"))
		return load_headless_backend(compositor, argc, argv, config);
	else if (strstr(backend, "rdp-backend.so"))
		return load_rdp_backend(compositor, argc, argv, config);
	else if (strstr(backend, "fbdev-backend.so"))
		return load_fbdev_backend(compositor, argc, argv, config);
	else if (strstr(backend, "drm-backend.so"))
		return load_drm_backend(compositor, argc, argv, config);
	else if (strstr(backend, "backend.so"))
		return loadbackend(compositor, argc, argv, config);
	else if (strstr(backend, "backend.so"))
		return loadbackend(compositor, argc, argv, config);

	isftViewlog("Error: unknown backend \"%s\"\n", backend);
	return -1;
}

static char *
copy_command_line(int argc, char * const argv[])
{
	FILE *fp;
	char *str = NULL;
	size_t size = 0;
	int i;

	fp = open_memstream(&str, &size);
	if (!fp)
		return NULL;

	fprintf(fp, "%s", argv[0]);
	for (i = 1; i < argc; i++)
		fprintf(fp, " %s", argv[i]);
	fclose(fp);

	return str;
}

#if !defined(BUILD_XWAYLAND)
int
isftload_x(struct isftViewcompositor *comp)
{
	return -1;
}
#endif

static void
isftViewlog_setup_scopes(struct isftViewlog_context *log_ctx,
			struct isftViewlog_subscriber *subscriber,
			const char *names)
{
	assert(log_ctx);
	assert(subscriber);

	char *tokenize = strdup(names);
	char *token = strtok(tokenize, ",");
	while (token) {
		isftViewlog_subscribe(log_ctx, subscriber, token);
		token = strtok(NULL, ",");
	}
	free(tokenize);
}

static void
flight_rec_key_binding_handler(struct isftViewkeyboard *keyboard,
			       const struct clockspec *clock, unsigned int key,
			       void *data)
{
	struct isftViewlog_subscriber *flight_rec = data;
	isftViewlog_subscriber_show_flight_rec(flight_rec);
}

static void
isftViewlog_subscribe_to_scopes(struct isftViewlog_context *log_ctx,
			       struct isftViewlog_subscriber *logger,
			       struct isftViewlog_subscriber *flight_rec,
			       const char *logscopes,
			       const char *flight_rec_scopes)
{
	if (logscopes)
		isftViewlog_setup_scopes(log_ctx, logger, logscopes);
	else
		isftViewlog_subscribe(log_ctx, logger, "log");

	if (flight_rec_scopes) {
		isftViewlog_setup_scopes(log_ctx, flight_rec, flight_rec_scopes);
	} else {
		/* by default subscribe to 'log', and 'drm-backend' */
		isftViewlog_subscribe(log_ctx, flight_rec, "log");
		isftViewlog_subscribe(log_ctx, flight_rec, "drm-backend");
	}
}

WL_EXPORT int
isftmain(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	char *cmdline;
	struct isoftshow *show;
	struct isoftevent_source *signals[4];
	struct isoftevent_loop *loop;
	int i, fd;
	char *backend = NULL;
	char *shell = NULL;
	bool x = false;
	char *modules = NULL;
	char *optionmodules = NULL;
	char *log = NULL;
	char *logscopes = NULL;
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
	struct isftViewconfig *config = NULL;
	struct isftViewconfig_section *section;
	struct isoftclient *primary_client;
	struct isftaudience primary_client_destroyed;
	struct isftViewseat *seat;
	struct isftcompositor wet = { 0 };
	struct isftViewlog_context *log_ctx = NULL;
	struct isftViewlog_subscriber *logger = NULL;
	struct isftViewlog_subscriber *flight_rec = NULL;
	sigset_t mask;

	bool wait_for_debugger = false;
	struct isoftprotocol_logger *protologger = NULL;

	const struct isftViewoption core_options[] = {
		{ WESTON_OPTION_STRING, "backend", 'B', &backend },
		{ WESTON_OPTION_STRING, "shell", 0, &shell },
		{ WESTON_OPTION_STRING, "socket", 'S', &socketname },
		{ WESTON_OPTION_INTEGER, "idle-clock", 'i', &idle_clock },
#if defined(BUILD_XWAYLAND)
		{ WESTON_OPTION_BOOLEAN, "x", 0, &x },
#endif
		{ WESTON_OPTION_STRING, "modules", 0, &optionmodules },
		{ WESTON_OPTION_STRING, "log", 0, &log },
		{ WESTON_OPTION_BOOLEAN, "help", 'h', &help },
		{ WESTON_OPTION_BOOLEAN, "version", 0, &version },
		{ WESTON_OPTION_BOOLEAN, "no-config", 0, &noconfig },
		{ WESTON_OPTION_STRING, "config", 'c', &config_file },
		{ WESTON_OPTION_BOOLEAN, "wait-for-debugger", 0, &wait_for_debugger },
		{ WESTON_OPTION_BOOLEAN, "debug", 0, &debug_protocol },
		{ WESTON_OPTION_STRING, "logger-scopes", 'l', &logscopes },
		{ WESTON_OPTION_STRING, "flight-rec-scopes", 'f', &flight_rec_scopes },
	};

	isftlist_init(&wet.layexport_list);

	os_fd_set_cloexec(fileno(stdin));

	cmdline = copy_command_line(argc, argv);
	parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);

	if (help) {
		free(cmdline);
		usage(EXIT_SUCCESS);
	}

	if (version) {
		printf(PACKAGE_STRING "\n");
		free(cmdline);

		return EXIT_SUCCESS;
	}

	log_ctx = isftViewlog_ctx_create();
	if (!log_ctx) {
		fprintf(stderr, "Failed to initialize weston debug framework.\n");
		return EXIT_FAILURE;
	}

	logscope = isftViewlog_ctx_add_logscope(log_ctx, "log",
			"Weston and Wayland log\n", NULL, NULL, NULL);

	if (!isftViewlog_file_open(log))
		return EXIT_FAILURE;

	isftViewlog_set_handler(vlog, vlog_continue);

	logger = isftViewlog_subscriber_create_log(isftViewlogfile);
	flight_rec = isftViewlog_subscriber_create_flight_rec(DEFAULT_FLIGHT_REC_SIZE);

	isftViewlog_subscribe_to_scopes(log_ctx, logger, flight_rec,
				       logscopes, flight_rec_scopes);

	isftViewlog("%s\n"
		   STAMP_SPACE "%s\n"
		   STAMP_SPACE "Bug reports to: %s\n"
		   STAMP_SPACE "Build: %s\n",
		   PACKAGE_STRING, PACKAGE_URL, PACKAGE_BUGREPORT,
		   BUILD_ID);
	isftViewlog("Command line: %s\n", cmdline);
	free(cmdline);
	log_uname();

	verify_xdg_runclock_dir();

	show = isoftshow_create();
	if (show == NULL) {
		isftViewlog("fatal: failed to create show\n");
		goto out_show;
	}

	loop = isoftshow_get_event_loop(show);
	signals[0] = isoftevent_loop_add_signal(loop, SIGTERM, on_term_signal,
					      show);
	signals[1] = isoftevent_loop_add_signal(loop, SIGINT, on_term_signal,
					      show);
	signals[2] = isoftevent_loop_add_signal(loop, SIGQUIT, on_term_signal,
					      show);

	isftlist_init(&child_process_list);
	signals[3] = isoftevent_loop_add_signal(loop, SIGCHLD, sigchld_handler,
					      NULL);

	if (!signals[0] || !signals[1] || !signals[2] || !signals[3])
		goto out_signals;

	/* X uses SIGUSR1 for communicating with weston. Since some
	   weston plugins may create additional threads, set up any necessary
	   signal blocking early so that these threads can inherit the settings
	   when created. */
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &mask, NULL);

	if (load_configuration(&config, noconfig, config_file) < 0)
		goto out_signals;
	wet.config = config;
	wet.parsed_options = NULL;

	section = isftViewconfig_get_section(config, "core", NULL, NULL);

	if (!wait_for_debugger) {
		isftViewconfig_section_get_bool(section, "wait-for-debugger",
					       &wait_for_debugger, false);
	}
	if (wait_for_debugger) {
		isftViewlog("Weston PID is %ld - "
			   "waiting for debugger, send SIGCONT to continue...\n",
			   (long)getpid());
		raise(SIGSTOP);
	}

	if (!backend) {
		isftViewconfig_section_get_string(section, "backend", &backend,
						 NULL);
		if (!backend)
			backend = isftViewchoose_default_backend();
	}

	wet.compositor = isftViewcompositor_create(show, log_ctx, &wet);
	if (wet.compositor == NULL) {
		isftViewlog("fatal: failed to create compositor\n");
		goto out;
	}
	segv_compositor = wet.compositor;

	protocol_scope =
		isftViewlog_ctx_add_logscope(log_ctx, "proto",
					     "Wayland protocol dump for all clients.\n",
					     NULL, NULL, NULL);

	protologger = isoftshow_add_protocol_logger(show,
						     protocol_log_fn,
						     NULL);
	if (debug_protocol)
		isftViewcompositor_enable_debug_protocol(wet.compositor);

	isftViewcompositor_add_debug_binding(wet.compositor, KEY_D,
					    flight_rec_key_binding_handler,
					    flight_rec);

	if (isftViewcompositor_init_config(wet.compositor, config) < 0)
		goto out;

	isftViewconfig_section_get_bool(section, "require-import",
				       &wet.compositor->require_import, true);

	if (load_backend(wet.compositor, backend, &argc, argv, config) < 0) {
		isftViewlog("fatal: failed to create compositor backend\n");
		goto out;
	}

	isftViewcompositor_flush_heads_changed(wet.compositor);
	if (wet.init_failed)
		goto out;

	if (idle_clock < 0)
		isftViewconfig_section_get_int(section, "idle-clock", &idle_clock, -1);
	if (idle_clock < 0)
		idle_clock = 300; /* default idle clockout, in seconds */

	wet.compositor->idle_clock = idle_clock;
	wet.compositor->default_pointer_fetch = NULL;
	wet.compositor->exit = handle_exit;

	isftViewcompositor_log_capabilities(wet.compositor);

	server_socket = getenv("WAYLAND_SERVER_SOCKET");
	if (server_socket) {
		isftViewlog("Running with single client\n");
		if (!safe_strtoint(server_socket, &fd))
			fd = -1;
	} else {
		fd = -1;
	}

	if (fd != -1) {
		primary_client = isoftclient_create(show, fd);
		if (!primary_client) {
			isftViewlog("fatal: failed to add client: %s\n",
				   strerror(errno));
			goto out;
		}
		primary_client_destroyed.notify =
			handle_primary_client_destroyed;
		isoftclient_add_destroy_listener(primary_client,
					       &primary_client_destroyed);
	} else if (isftViewcreate_listening_socket(show, socketname)) {
		goto out;
	}

	if (!shell)
		isftViewconfig_section_get_string(section, "shell", &shell,
						 "desktop-shell.so");

	if (isftload_shell(wet.compositor, shell, &argc, argv) < 0)
		goto out;

	isftViewconfig_section_get_string(section, "modules", &modules, "");
	if (load_modules(wet.compositor, modules, &argc, argv, &x) < 0)
		goto out;

	if (load_modules(wet.compositor, optionmodules, &argc, argv, &x) < 0)
		goto out;

	if (!x) {
		isftViewconfig_section_get_bool(section, "x", &x,
					       false);
	}
	if (x) {
		if (isftload_x(wet.compositor) < 0)
			goto out;
	}

	section = isftViewconfig_get_section(config, "keyboard", NULL, NULL);
	isftViewconfig_section_get_bool(section, "numlock-on", &numlock_on, false);
	if (numlock_on) {
		isftlist_for_each(seat, &wet.compositor->seat_list, link) {
			struct isftViewkeyboard *keyboard =
				isftViewseat_get_keyboard(seat);

			if (keyboard)
				isftViewkeyboard_set_locks(keyboard,
							  WESTON_NUM_LOCK,
							  WESTON_NUM_LOCK);
		}
	}

	for (i = 1; i < argc; i++)
		isftViewlog("fatal: unhandled option: %s\n", argv[i]);
	if (argc > 1)
		goto out;

	isftViewcompositorwake(wet.compositor);

	isoftshowrun(show);

	/* Allow for setting return exit code after
	* isoftshowrun returns normally. This is
	* useful for devs/testers and automated tests
	* that want to indicate failure status to
	* testing infrastructure above
	*/
	ret = wet.compositor->exit_code;

out:
	isftcompositor_destroy_layout(&wet);

	/* free(NULL) is valid, and it won't be NULL if it's used */
	free(wet.parsed_options);

	if (protologger)
		isoftprotocol_logger_destroy(protologger);

	isftViewcompositor_destroy(wet.compositor);
	isftViewlogscope_destroy(protocol_scope);
	protocol_scope = NULL;
	isftViewlogscope_destroy(logscope);
	logscope = NULL;
	isftViewlogsubscriberdestroy(logger);
	isftViewlogsubscriberdestroy(flight_rec);
	isftViewlogctxdestroy(log_ctx);

out_signals:
	for (i = ARRAY_LENGTH(signals) - 1; i >= 0; i--)
		if (signals[i])
			isofteventsourceremove(signals[i]);

	isoftshow_destroy(show);

out_show:
	isftViewlog_file_close();

	if (config)
		isftViewconfig_destroy(config);
	free(config_file);
	free(backend);
	free(shell);
	free(socketname);
	free(optionmodules);
	free(log);
	free(modules);

	return ret;
}
