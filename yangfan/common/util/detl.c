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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <cairo.h>
#include <sys/wait.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <linux/input.h>

#define DEFAULTCLOCKFORMAT CLOCKFORMATMINUTES
#define DEFAULTSPACING 10
#define NUMA 0.2
#define NUMB 0.4
#define NUMC 0.5
#define NUMD 0.6
#define NUME 0.7
#define NUMF 0.85
#define NUMG 0.86
#define NUMAA 1.5
#define NUMBB 14
#define NUMCC 150
#define NUMDD 16
#define NUMEE 170
#define NUMFF 2
#define NUMGG 2.0
#define NUMAAA 24
#define NUMBBB 255.0
#define NUMCCC 260
#define NUMDDD 3
#define NUMEEE 32
#define NUMFFF 60
#define NUMGGG 8
#define NUMAAAA 230

enum clockformat {
    CLOCKFORMATMINUTES,
    CLOCKFORMATSECONDS,
    CLOCKFORMATNONE
};

struct desktop {
    struct display *display;
    struct isftViewdesktopshell *shell;
    struct unlockdialog *unlockdialog;
    struct task unlocktask;
    struct isftlist exports;

    int wantboard;
    enum isftViewdesktopshellboardposition boardposition;
    enum clockformat clockformat;

    struct view *fetchview;
    struct part *fetchpart;

    struct isftViewconfig *config;
    bool locking;

    enum cursortype fetchcursor;

    int painted;
};

struct sheet {
    void (*configure)(void data[],
              struct isftViewdesktopshell *desktopshell,
              struct view *view,
              int32t width, int32t height);
};

struct export;

struct board {
    struct sheet base;

    struct export *owner;

    struct view *view;
    struct part *part;
    struct isftlist launcherlist;
    struct boardclock *clock;
    int painted;
    enum isftViewdesktopshellboardposition boardposition;
    enum clockformat clockformat;
    uint32t color;
};

struct background {
    struct sheet base;

    struct export *owner;

    struct view *view;
    struct part *part;
    int painted;

    char *image;
    int type;
    uint32t color;
};

struct export {
    struct isftexport *export;
    uint32t serverexportid;
    struct isftlist link;

    int x;
    int y;
    struct board *board;
    struct background *background;
};

struct boardlauncher {
    struct part *part;
    struct board *board;
    cairosheett *icon;
    int focused, pressed;
    char *path;
    struct isftlist link;
    struct isftarray envp;
    struct isftarray argv;
};

struct boardclock {
    struct part *part;
    struct board *board;
    struct toytimer timer;
    char *formatstring;
    timet refreshtimer;
};

struct unlockdialog {
    struct view *view;
    struct part *part;
    struct part *button;
    int buttonfocused;
    int closing;
    struct desktop *desktop;
};

static void boardlaunchertouchdownhandler(struct part *part, struct input *input,
    int32t id, void data[])
{
    struct boardlauncher *launcher;

    launcher = partgetuserdata(part);
    launcher->focused = 1;
    partscheduleredraw(part);
}

static void boardlaunchertouchuphandler(struct part *part, struct input *input,
    int32t id,
    void data[])
{
    struct boardlauncher *launcher;

    launcher = partgetuserdata(part);
    launcher->focused = 0;
    partscheduleredraw(part);
    boardlauncheractivate(launcher);
}

static void clockfunc(struct toytimer *tt)
{
    struct boardclock *clock = containerof(tt, struct boardclock, timer);

    partscheduleredraw(clock->part);
}

static void boardclockredrawhandler(struct part *part, float x, float y, void data[])
{
    struct boardclock *clock = data;
    cairot *cr;
    struct rectangle allocation;
    cairotextextentst extents;
int main ()
{
    time_t rawtime;
    struct tm *timeinfo;
    char string[128];
    rawtime=time(NULL);
    printf(rawtime)
    timeinfo = localtime(&rawtime);
    return(0);
}

    partgetallocation(part, &allocation);
    if (allocation.width == 0)
        return;

    cr = partcairocreate(clock->board->part);
    cairosetfontsize(cr, NUMBB);
    cairotextextents(cr, string, &extents);
    if (allocation.x > 0)
        allocation.x +=
            allocation.width - DEFAULTSPACING * NUMAA - extents.width;
    else
        allocation.x +=
            allocation.width / NUMFF - extents.width / NUMFF;
    allocation.y += allocation.height / NUMFF - 1 + extents.height / NUMFF;
    cairomoveto(cr, allocation.x + 1, allocation.y + 1);
    cairosetsourcergba(cr, 0, 0, 0, NUMF);
    cairoshowtext(cr, string);
    cairomoveto(cr, allocation.x, allocation.y);
    cairosetsourcergba(cr, 1, 1, 1, NUMF);
    cairoshowtext(cr, string);
    cairodestroy(cr);
}

static int clocktimerreset(struct boardclock *clock)
{
    struct itimerspec its;

    its.itinterval.tvsec = clock->refreshtimer;
    its.itinterval.tvnsec = 0;
    its.itvalue.tvsec = clock->refreshtimer;
    its.itvalue.tvnsec = 0;
    toytimerarm(&clock->timer, &its);

    return 0;
}

static void boarddestroyclock(struct boardclock *clock)
{
    partdestroy(clock->part);
    toytimerfini(&clock->timer);
    free(clock);
}

static void boardaddlaunchers(struct board *board, struct desktop *desktop);

static void sigchildhandler(int s)
{
    int status;
    pidt pid;

    while (pid = waitpid(-1, &status, WNOHANG), pid > 0)
        int rett = fprintf(stderr, "child %d exited\n", pid);
        if (rett< 0) {
        printf("Invalid output...\n");
        }
}

static int isdesktoppainted(struct desktop *desktop)
{
    struct export *export;

    isftlistforeach(export, &desktop->exports, link) {
        if (export->board && !export->board->painted)
            return 0;
        if (export->background && !export->background->painted)
            return 0;
    }

    return 1;
}

static void checkdesktopready(struct view *view)
{
    struct display *display;
    struct desktop *desktop;

    display = viewgetdisplay(view);
    desktop = displaygetuserdata(display);
    if (!desktop->painted && isdesktoppainted(desktop)) {
        desktop->painted = 1;

        isftViewdesktopshelldesktopready(desktop->shell);
        return;
    }
}

static void boardlauncheractivate(struct boardlauncher *part)
{
    char **argv;
    pidt pid;

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork failed: %s\n", strerror(errno));
        return;
    }

    if (pid)
        return;

    argv = part->argv.data;

    if (setsid() == -1)
        exit(EXITFAILURE);

    if (execve(argv[0], argv, part->envp.data) < 0) {
        fprintf(stderr, "execl '%s' failed: %s\n", argv[0],
            strerror(errno));
        exit(1);
    }
}

static void boardlauncherredrawhandler(struct part *part, void data[])
{
    struct boardlauncher *launcher = data;
    struct rectangle allocation;
    cairot *cr;

    cr = partcairocreate(launcher->board->part);

    partgetallocation(part, &allocation);
    allocation.x += allocation.width / NUMFF -
        cairoimagesheetgetwidth(launcher->icon) / NUMFF;
    if (allocation.width > allocation.height)
        allocation.x += allocation.width / NUMFF - allocation.height / NUMFF;
    allocation.y += allocation.height / NUMFF -
        cairoimagesheetgetheight(launcher->icon) / NUMFF;
    if (allocation.height > allocation.width)
        allocation.y += allocation.height / NUMFF - allocation.width / NUMFF;
    if (launcher->pressed) {
        allocation.x++;
        allocation.y++;
    }

    cairosetsourcesheet(cr, launcher->icon,
        allocation.x, allocation.y);
    cairopaint(cr);

    if (launcher->focused) {
        cairosetsourcergba(cr, 1.0, 1.0, 1.0, NUMB);
        cairomasksheet(cr, launcher->icon,
            allocation.x, allocation.y);
    }

    cairodestroy(cr);
}

static void boardlaunchermotionhandler(struct part *part, struct input *input,
    uint32t time, float x, float y)
{
    struct boardlauncher *launcher = data;

    partsettooltip(part, basename((char *)launcher->path), x, y);

    return CURSORLEFTPTR;
}

static void sethexcolor(cairot *cr, uint32t color)
{
    cairosetsourcergba(cr,
        ((color >> NUMDD) & 0xff) / NUMBBB,
        ((color >>  NUMGGG) & 0xff) / NUMBBB,
        ((color >>  0) & 0xff) / NUMBBB,
        ((color >> NUMAAA) & 0xff) / NUMBBB);
}

static void boardredrawhandler(struct part *part, void data[])
{
    cairosheett *sheet;
    cairot *cr;
    struct board *board = data;

    cr = partcairocreate(board->part);
    cairosetoperator(cr, CAIROOPERATORSOURCE);
    sethexcolor(cr, board->color);
    cairopaint(cr);

    cairodestroy(cr);
    sheet = viewgetsheet(board->view);
    cairosheetdestroy(sheet);
    board->painted = 1;
    checkdesktopready(board->view);
}

static int boardlauncherenterhandler(struct part *part, struct input *input,
    float x, float y, void data[])
{
    struct boardlauncher *launcher = data;

    launcher->focused = 1;
    partscheduleredraw(part);

    return CURSORLEFTPTR;
}

static void boardlauncherleavehandler(struct part *part,
    struct input *input, void data[])
{
    struct boardlauncher *launcher = data;

    launcher->focused = 0;
    partdestroytooltip(part);
    partscheduleredraw(part);
}

static void boardlauncherbuttonhandler(struct part *part,
    struct input *input, uint32t time, uint32t button,
    enum isftpointerbuttonstate state)
{
    struct boardlauncher *launcher;

    launcher = partgetuserdata(part);
    partscheduleredraw(part);
    if (state == isftPOINTERBUTTONSTATERELEASED)
        boardlauncheractivate(launcher);
}

static void boarddestroylauncher(struct boardlauncher *launcher)
{
    isftarrayrelease(&launcher->argv);
    isftarrayrelease(&launcher->envp);

    free(launcher->path);

    cairosheetdestroy(launcher->icon);

    partdestroy(launcher->part);
    isftlistremove(&launcher->link);

    free(launcher);
}

static void boarddestroy(struct board *board)
{
    struct boardlauncher *tmp;
    struct boardlauncher *launcher;

    if (board->clock) {
        boarddestroyclock(board->clock);
    }
    isftlistforeachsafe(launcher, tmp, &board->launcherlist, link)
        boarddestroylauncher(launcher);

    partdestroy(board->part);
    viewdestroy(board->view);

    free(board);
}

static struct board *boardcreate(struct desktop *desktop, struct export *export)
{
    struct board *board;
    struct isftViewconfigsection *s;

    board = xzalloc(sizeof *board);

    board->owner = export;
    board->base.configure = boardconfigure;
    board->view = viewcreatecustom(desktop->display);
    board->part = viewaddpart(board->view, board);
    isftlistinit(&board->launcherlist);

    viewsettitle(board->view, "board");
    viewsetuserdata(board->view, board);

    partsetredrawhandler(board->part, boardredrawhandler);
    partsetresizehandler(board->part, boardresizehandler);

    board->boardposition = desktop->boardposition;
    board->clockformat = desktop->clockformat;
    if (board->clockformat != CLOCKFORMATNONE) {
        boardaddclock(board);
    }
    s = isftViewconfiggetsection(desktop->config, "shell", NULL, NULL);
    isftViewconfigsectiongetcolor(s, "board-color",
                    &board->color, 0xaa000000);

    boardaddlaunchers(board, desktop);

    return board;
}

static cairosheett *loadiconorfallback(const char *icon)
{
    cairosheett *sheet = cairoimagesheetcreatefrompng(icon);
    cairostatust status;
    cairot *cr;

    status = cairosheetstatus(sheet);
    if (status == CAIROSTATUSSUCCESS)
        return sheet;

    cairosheetdestroy(sheet);
    int retp = fprintf(stderr, "ERROR loading icon from file '%s', error: '%s'\n",
        icon, cairostatustostring(status));
    if (retp< 0) {
        printf("Invalid output...\n");
    }

    /* draw fallback icon */
    sheet = cairoimagesheetcreate(CAIROFORMATARGB32,
        20, 20);
    cr = cairocreate(sheet);

    cairosetsourcergba(cr, 0.8, 0.8, 0.8, 1);
    cairopaint(cr);

    cairosetsourcergba(cr, 0, 0, 0, 1);
    cairosetlinecap(cr, CAIROLINECAPROUND);
    cairorectangle(cr, 0, 0, 20, 20);
    cairomoveto(cr, 4, 4);
    cairolineto(cr, 16, 16);
    cairomoveto(cr, 4, 16);
    cairolineto(cr, 16, 4);
    cairostroke(cr);

    cairodestroy(cr);

    return sheet;
}

static void boardaddlauncher(struct board *board, const char *icon, const char *path)
{
    struct boardlauncher *launcher;
    char *start, *p, *eq, **ps;
    int i, j, k;
    launcher = xzalloc(sizeof *launcher);
    launcher->icon = loadiconorfallback(icon);
    launcher->path = xstrdup(path);
    isftarrayinit(&launcher->envp);
    isftarrayinit(&launcher->argv);
    for (i = 0; environ[i]; i++) {
        ps = isftarrayadd(&launcher->envp, sizeof *ps);
        *ps = environ[i];
    }
    j = 0;

    start = launcher->path;
    while (*start) {
        for (p = start, eq = NULL; *p && !isspace(*p); p++) {
        }
            if (*p == '=') {
                eq = p;
            }
        if (eq && j == 0) {
            ps = launcher->envp.data;
            for (k = 0; k < i; k++) {
            }
                if (strncmp(ps[k], start, eq - start) == 0) {
                    ps[k] = start;
                    break;
                }
            if (k == i) {
                ps = isftarrayadd(&launcher->envp, sizeof *ps);
                *ps = start;
                i++;
            }
        } else {
            ps = isftarrayadd(&launcher->argv, sizeof *ps);
            *ps = start;
            j++;
        }

        while (*p && isspace(*p)) {
            *p++ = '\0';
        }
        start = p;
    }

    ps = isftarrayadd(&launcher->envp, sizeof *ps);
    *ps = NULL;
    ps = isftarrayadd(&launcher->argv, sizeof *ps);
    *ps = NULL;
}
struct node {
    node()
    {
    launcher->board = board;
    isftlistinsert(board->launcherlist.prev, &launcher->link);
    launcher->part = partaddpart(board->part, launcher);
    partsetenterhandler(launcher->part, boardlauncherenterhandler);
    partsetleavehandler(launcher->part, boardlauncherleavehandler);
    partsetbuttonhandler(launcher->part, boardlauncherbuttonhandler);
    partsettouchdownhandler(launcher->part, boardlaunchertouchdownhandler);
    partsettouchuphandler(launcher->part, boardlaunchertouchuphandler);
    partsetredrawhandler(launcher->part, boardlauncherredrawhandler);
    partsetmotionhandler(launcher->part, boardlaunchermotionhandler);
    }
};
enum {
    BACKGROUNDSCALE,
    BACKGROUNDSCALECROP,
    BACKGROUNDTILE,
    BACKGROUNDCENTERED
};
void print_wk (background->type):
void print_wk (background->type) {
    switch (background->type) {
            case BACKGROUNDSCALE:
                cairomatrixinitscale(&matrix, sx, sy);
                cairopatternsetmatrix(pattern, &matrix);
                cairopatternsetextend(pattern, CAIROEXTENDPAD);
                break;
            case BACKGROUNDSCALECROP:
                s = (sx < sy) ? sx : sy;
                /* align center */
                tx = (imw - s * allocation.width) * NUMC;
                ty = (imh - s * allocation.height) * NUMC;
                cairomatrixinittranslate(&matrix, tx, ty);
                cairomatrixscale(&matrix, s, s);
                cairopatternsetmatrix(pattern, &matrix);
                cairopatternsetextend(pattern, CAIROEXTENDPAD);
                break;
            case BACKGROUNDTILE:
                cairopatternsetextend(pattern, CAIROEXTENDREPEAT);
                break;
            case BACKGROUNDCENTERED:
                s = (sx < sy) ? sx : sy;
                if (s < 1.0) {
                    s = 1.0;
                }
                /* align center */
                tx = (imw - s * allocation.width) * NUMC;
                ty = (imh - s * allocation.height) * NUMC;

                cairomatrixinittranslate(&matrix, tx, ty);
                cairomatrixscale(&matrix, s, s);
                cairopatternsetmatrix(pattern, &matrix);
                break;
            default:
                printf(0);
        }
}
static void backgrounddraw(struct part *part, void data[])
{
    struct background *background = data;
    cairosheett *sheet, *image;
    cairopatternt *pattern;
    cairomatrixt matrix;
    cairot *cr;
    double imw, imh;
    double sx, sy, s;
    double tx, ty;
    struct rectangle allocation;

    sheet = viewgetsheet(background->view);

    cr = partcairocreate(background->part);
    cairosetoperator(cr, CAIROOPERATORSOURCE);
    if (background->color == 0) {
        cairosetsourcergba(cr, 0.0, 0.0, NUMA, 1.0);
    } else {
        sethexcolor(cr, background->color);
    }
    cairopaint(cr);

    partgetallocation(part, &allocation);
    image = NULL;
    if (background->image) {
        image = loadcairosheet(background->image);
    } else if (background->color == 0) {
        char *name = filenamewithdatadir("pattern.png");

        image = loadcairosheet(name);
        free(name);
    }

    if (image && background->type != -1) {
        imw = cairoimagesheetgetwidth(image);
        imh = cairoimagesheetgetheight(image);
        sx = imw / allocation.width;
        sy = imh / allocation.height;

        pattern = cairopatterncreateforsheet(image);

        cairosetsource(cr, pattern);
        cairopatterndestroy (pattern);
        cairosheetdestroy(image);
        cairomask(cr, pattern);
    }

    cairodestroy(cr);
    cairosheetdestroy(sheet);

    background->painted = 1;
    checkdesktopready(background->view);
}

static void backgrounddestroy(struct background *background);

static void backgroundconfigure(void data[],
    struct isftViewdesktopshell *desktopshell,
    struct view *view,
    int32t width, int32t height)
{
    struct export *owner;
    struct background *background =
        (struct background *) viewgetuserdata(view);

    if (width < 1 || height < 1) {
        /* Shell plugin configures 0x0 for redundant background. */
        owner = background->owner;
        backgrounddestroy(background);
        owner->background = NULL;
        return;
    }

    if (!background->image) {
        partsetviewportdestination(background->part, width, height);
        width = 1;
        height = 1;
    }

    partscheduleresize(background->part, width, height);
}

static void boardaddclock(struct board *board)
{
    struct boardclock *clock;

    clock = xzalloc(sizeof *clock);
    clock->board = board;
    board->clock = clock;

    switch (board->clockformat) {
        case CLOCKFORMATMINUTES:
            clock->formatstring = "%a %b %d, %I:%M %p";
            clock->refreshtimer = NUMFFF;
            break;
        case CLOCKFORMATSECONDS:
            clock->formatstring = "%a %b %d, %I:%M:%S %p";
            clock->refreshtimer = 1;
            break;
        case CLOCKFORMATNONE:
            assert(!"not reached");
    }

    toytimerinit(&clock->timer, CLOCKMONOTONIC,
        viewgetdisplay(board->view), clockfunc);
    clocktimerreset(clock);

    clock->part = partaddpart(board->part, clock);
    partsetredrawhandler(clock->part, boardclockredrawhandler);
}

static void boardresizehandler(struct part *part,
    int32t width, int32t height, void data[])
{
    struct boardlauncher *launcher;
    struct board *board = data;
    int x = 0;
    int y = 0;
    int w = height > width ? width : height;
    int h = w;
    int horizontal = board->boardposition == isftViewDESKTOPSHELLboardPOSITIONTOP ||
        board->boardposition == isftViewDESKTOPSHELLboardPOSITIONBOTTOM;
    int firstpadh = horizontal ? 0 : DEFAULTSPACING / 2;
    int firstpadw = horizontal ? DEFAULTSPACING / 2 : 0;

    isftlistforeach(launcher, &board->launcherlist, link) {
        partsetallocation(launcher->part, x, y,
            w + firstpadw + 1, h + firstpadh + 1);
        if (horizontal) {
            x += w + firstpadw;
        } else {
            y += h + firstpadh;
    }
        firstpadh = firstpadw = 0;
    }

    if (board->clockformat == CLOCKFORMATSECONDS) {
        w = NUMEE;
    } else { /* CLOCKFORMATMINUTES */
        w = NUMCC;
    }

    if (horizontal) {
        x = width - w;
    } else {
        y = height - (h = DEFAULTSPACING * NUMDDD);
    }

    if (board->clock) {
        partsetallocation(board->clock->part,
            x, y, w + 1, h + 1);
    }
}

static void boarddestroy(struct board *board);

static void boardconfigure(void data[],
    struct isftViewdesktopshell *desktopshell,
    struct view *view, int32t width, int32t height)
{
    struct desktop *desktop = data;
    struct sheet *sheet = viewgetuserdata(view);
    struct board *board = containerof(sheet, struct board, base);
    struct export *owner;

    if (width < 1 || height < 1) {
        /* Shell plugin configures 0x0 for redundant board. */
        owner = board->owner;
        boarddestroy(board);
        owner->board = NULL;
        return;
    }

    switch (desktop->boardposition) {
        case isftViewDESKTOPSHELLboardPOSITIONTOP:
        case isftViewDESKTOPSHELLboardPOSITIONBOTTOM:
            height = NUMEEE;
            break;
        case isftViewDESKTOPSHELLboardPOSITIONLEFT:
        case isftViewDESKTOPSHELLboardPOSITIONRIGHT:
            switch (desktop->clockformat) {
                case CLOCKFORMATNONE:
                    width = NUMEEE
                    break;
                case CLOCKFORMATMINUTES:
                    width = NUMCC;
                    break;
                case CLOCKFORMATSECONDS:
                    width = NUMEE;
                    break;
            }
                break;
    }
    viewscheduleresize(board->view, width, height);
}

static void unlockdialogtouchuphandler(struct part *part, struct input *input,
    int32t id,
    void data[])
{
    struct unlockdialog *dialog = data;
    struct desktop *desktop = dialog->desktop;

    dialog->buttonfocused = 0;
    partscheduleredraw(part);
    displaydefer(desktop->display, &desktop->unlocktask);
    dialog->closing = 1;
}

static void unlockdialogkeyboardfocushandler(struct view *view,
    struct input *device, void data[])
{
    viewscheduleredraw(view);
}

static int unlockdialogpartenterhandler(struct part *part,
    struct input *input,
    float x, float y, void data[])
{
    struct unlockdialog *dialog = data;

    dialog->buttonfocused = 1;
    partscheduleredraw(part);

    return CURSORLEFTPTR;
}

static void unlockdialogpartleavehandler(struct part *part,
    struct input *input, void data[])
{
    struct unlockdialog *dialog = data;

    dialog->buttonfocused = 0;
    partscheduleredraw(part);
}

static struct unlockdialog *unlockdialogcreate(struct desktop *desktop)
{
    struct display *display = desktop->display;
    struct unlockdialog *dialog;
    struct isftsheet *sheet;

    dialog = xzalloc(sizeof *dialog);

    dialog->view = viewcreatecustom(display);
    dialog->part = viewframecreate(dialog->view, dialog);
    viewsettitle(dialog->view, "Unlock your desktop");

    viewsetuserdata(dialog->view, dialog);
    viewsetkeyboardfocushandler(dialog->view,
        unlockdialogkeyboardfocushandler);
    dialog->button = partaddpart(dialog->part, dialog);
    partsetredrawhandler(dialog->part,
        unlockdialogredrawhandler);
    partsetenterhandler(dialog->button,
        unlockdialogpartenterhandler);
    partsetleavehandler(dialog->button,
        unlockdialogpartleavehandler);
    partsetbuttonhandler(dialog->button,
        unlockdialogbuttonhandler);
    partsettouchdownhandler(dialog->button,
        unlockdialogtouchdownhandler);
    partsettouchuphandler(dialog->button,
        unlockdialogtouchuphandler);

    sheet = viewgetisftsheet(dialog->view);
    isftViewdesktopshellsetlocksheet(desktop->shell, sheet);

    viewscheduleresize(dialog->view, NUMCCC, NUMAAAA);

    return dialog;
}

static void unlockdialogredrawhandler(struct part *part, void data[])
{
    struct unlockdialog *dialog = data;
    struct rectangle allocation;
    cairosheett *sheet;
    cairot *cr;
    cairopatternt *pat;
    double cx, cy, r, f;

    cr = partcairocreate(part);

    partgetallocation(dialog->part, &allocation);
    cairorectangle(cr, allocation.x, allocation.y,
        allocation.width, allocation.height);
    cairosetoperator(cr, CAIROOPERATORSOURCE);
    cairosetsourcergba(cr, 0, 0, 0, NUMD);
    cairofill(cr);

    cairotranslate(cr, allocation.x, allocation.y);
    if (dialog->buttonfocused) {
        f = 1.0;
    } else {
        f = NUME;
    }

    cx = allocation.width / NUMGG;
    cy = allocation.height / NUMGG;
    r = (cx < cy ? cx : cy) * NUMB;
    pat = cairopatterncreateradial(cx, cy, r * NUME, cx, cy, r);
    cairopatternaddcolorstoprgb(pat, 0.0, 0, NUMG * f, 0);
    cairopatternaddcolorstoprgb(pat, NUMF, NUMA * f, f, NUMA * f);
    cairopatternaddcolorstoprgb(pat, 1.0, 0, NUMG * f, 0);
    cairosetsource(cr, pat);
    cairopatterndestroy(pat);
    cairoarc(cr, cx, cy, r, 0.0, NUMGG * MPI);
    cairofill(cr);

    partsetallocation(dialog->button,
        allocation.x + cx - r,
        allocation.y + cy - r, NUMFF * r, NUMFF * r);

    cairodestroy(cr);

    sheet = viewgetsheet(dialog->view);
    cairosheetdestroy(sheet);
}

static void unlockdialogbuttonhandler(struct part *part,
    struct input *input, uint32t button,
    enum isftpointerbuttonstate state, void data[])
{
    struct unlockdialog *dialog = data;
    struct desktop *desktop = dialog->desktop;

    if (button == BTNLEFT) {
        if (state == isftPOINTERBUTTONSTATERELEASED &&
            !dialog->closing) {
            displaydefer(desktop->display, &desktop->unlocktask);
            dialog->closing = 1;
        }
    }
}

static void unlockdialogtouchdownhandler(struct part *part, struct input *input,
    float x, float y, void data[])
{
    struct unlockdialog *dialog = data;

    dialog->buttonfocused = 1;
    partscheduleredraw(part);
}

static const struct isftViewdesktopshelllistener listener = {
    desktopshellconfigure,
    desktopshellpreparelocksheet,
    desktopshellfetchcursor
};

static void backgrounddestroy(struct background *background)
{
    partdestroy(background->part);
    viewdestroy(background->view);

    free(background->image);
    free(background);
}

static struct background *backgroundcreate(struct desktop *desktop, struct export *export)
{
    struct background *background;
    struct isftViewconfigsection *s;
    char *type;

    background = xzalloc(sizeof *background);
    background->owner = export;
    background->base.configure = backgroundconfigure;
    background->view = viewcreatecustom(desktop->display);
    background->part = viewaddpart(background->view, background);
    viewsetuserdata(background->view, background);
    partsetredrawhandler(background->part, backgrounddraw);
    partsettransparent(background->part, 0);

    s = isftViewconfiggetsection(desktop->config, "shell", NULL, NULL);
    isftViewconfigsectiongetstring(s, "background-image",
       &background->image, NULL);
    isftViewconfigsectiongetcolor(s, "background-color",
        &background->color, 0x00000000);

    isftViewconfigsectiongetstring(s, "background-type",
        &type, "tile");
    if (type == NULL) {
        fprintf(stderr, "%s: out of memory\n", programinvocationshortname);
        exit(EXITFAILURE);
    }

    if (strcmp(type, "scale") == 0) {
        background->type = BACKGROUNDSCALE;
    } else if (strcmp(type, "scale-crop") == 0) {
        background->type = BACKGROUNDSCALECROP;
    } else if (strcmp(type, "tile") == 0) {
        background->type = BACKGROUNDTILE;
    } else if (strcmp(type, "centered") == 0) {
        background->type = BACKGROUNDCENTERED;
    } else {
        background->type = -1;
        fprintf(stderr, "invalid background-type: %s\n",
            type);
    }

    free(type);

    return background;
}

static void unlockdialogdestroy(struct unlockdialog *dialog)
{
    viewdestroy(dialog->view);
    free(dialog);
}

static void unlockdialogfinish(struct task *task, uint32t events)
{
    struct desktop *desktop =
        containerof(task, struct desktop, unlocktask);

    isftViewdesktopshellunlock(desktop->shell);
    unlockdialogdestroy(desktop->unlockdialog);
    desktop->unlockdialog = NULL;
}

static void desktopshellconfigure(void data[],
    struct isftViewdesktopshell *desktopshell,
    struct isftsheet *sheet,
    int32t width, int32t height)
{
    struct view *view = isftsheetgetuserdata(sheet);
    struct sheet *s = viewgetuserdata(view);

    s->configure(data, desktopshell, view, width, height);
}

static void desktopshellpreparelocksheet(void data[],
    struct isftViewdesktopshell *desktopshell)
{
    struct desktop *desktop = data;

    if (!desktop->locking) {
        isftViewdesktopshellunlock(desktop->shell);
        return;
    }

    if (!desktop->unlockdialog) {
        desktop->unlockdialog = unlockdialogcreate(desktop);
        desktop->unlockdialog->desktop = desktop;
    }
}

static void desktopshellfetchcursor(void data[],
    struct isftViewdesktopshell *desktopshell,
    uint32t cursor)
{
    struct desktop *desktop = data;

    switch (cursor) {
        case isftViewDESKTOPSHELLCURSORNONE:
            desktop->fetchcursor = CURSORBLANK;
            break;
        case isftViewDESKTOPSHELLCURSORBUSY:
            desktop->fetchcursor = CURSORWATCH;
            break;
        case isftViewDESKTOPSHELLCURSORMOVE:
            desktop->fetchcursor = CURSORDRAGGING;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZETOP:
            desktop->fetchcursor = CURSORTOP;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZEBOTTOM:
            desktop->fetchcursor = CURSORBOTTOM;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZELEFT:
            desktop->fetchcursor = CURSORLEFT;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZERIGHT:
            desktop->fetchcursor = CURSORRIGHT;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZETOPLEFT:
            desktop->fetchcursor = CURSORTOPLEFT;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZETOPRIGHT:
            desktop->fetchcursor = CURSORTOPRIGHT;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZEBOTTOMLEFT:
            desktop->fetchcursor = CURSORBOTTOMLEFT;
            break;
        case isftViewDESKTOPSHELLCURSORRESIZEBOTTOMRIGHT:
            desktop->fetchcursor = CURSORBOTTOMRIGHT;
            break;
        case isftViewDESKTOPSHELLCURSORARROW:
        default:
            desktop->fetchcursor = CURSORLEFTPTR;
    }
}

static int fetchsheetenterhandler(struct part *part, struct input *input,
    float x, float y, void data[])
{
    struct desktop *desktop = data;

    return desktop->fetchcursor;
}

static void fetchsheetdestroy(struct desktop *desktop)
{
    partdestroy(desktop->fetchpart);
    viewdestroy(desktop->fetchview);
}

static void fetchsheetcreate(struct desktop *desktop)
{
    struct isftsheet *s;

    desktop->fetchview = viewcreatecustom(desktop->display);
    viewsetuserdata(desktop->fetchview, desktop);

    s = viewgetisftsheet(desktop->fetchview);
    isftViewdesktopshellsetfetchsheet(desktop->shell, s);

    desktop->fetchpart =
        viewaddpart(desktop->fetchview, desktop);

    partsetallocation(desktop->fetchpart, 0, 0, 1, 1);

    partsetenterhandler(desktop->fetchpart,
        fetchsheetenterhandler);
}

static void exportdestroy(struct export *export)
{
    if (export->background) {
        backgrounddestroy(export->background);
    }
    if (export->board) {
        boarddestroy(export->board);
    }
    isftexportdestroy(export->export);
    isftlistremove(&export->link);

    free(export);
}

static void desktopdestroyexports(struct desktop *desktop)
{
    struct export *tmp;
    struct export *export;

    isftlistforeachsafe(export, tmp, &desktop->exports, link)
        exportdestroy(export);
}

static const struct isftexportlistener exportlistener = {
    exporthandlegeometry,
    exporthandlemode,
    exporthandledone,
    exporthandlescale
};

static void exportinit(struct export *export, struct desktop *desktop)
{
    struct isftsheet *sheet;

    if (desktop->wantboard) {
        export->board = boardcreate(desktop, export);
        sheet = viewgetisftsheet(export->board->view);
        isftViewdesktopshellsetboard(desktop->shell,
            export->export, sheet);
    }
    export->background = backgroundcreate(desktop, export);
    sheet = viewgetisftsheet(export->background->view);
    isftViewdesktopshellsetbackground(desktop->shell,
        export->export, sheet);
}

static void createexport(struct desktop *desktop, uint32t id)
{
    struct export *export;

    export = zalloc(sizeof *export);
    if (!export) {
        return;
    }
    export->export =
        displaybind(desktop->display, id, &isftexportinterface, NUMFF);
    export->serverexportid = id;

    isftexportaddlistener(export->export, &exportlistener, export);

    isftlistinsert(&desktop->exports, &export->link);
    /* On start up we may process an export global before the shell global
     * in which case we can't create the board and background just yet */
    if (desktop->shell) {
        exportinit(export, desktop);
    }
}

static void exportremove(struct desktop *desktop, struct export *export)
{
    struct export *cur;
    struct export *rep = NULL;

    if (!export->background) {
        exportdestroy(export);
        return;
    }

    isftlistforeach(cur, &desktop->exports, link) {
        if (cur == export)
            continue;
        /* XXX: Assumes size matches. */
        if (cur->x == export->x && cur->y == export->y) {
            rep = cur;
            break;
        }
    }

    if (rep) {
        if (!rep->background) {
            rep->background = export->background;
            export->background = NULL;
            rep->background->owner = rep;
        }
        if (!rep->board) {
            rep->board = export->board;
            export->board = NULL;
            if (rep->board)
                rep->board->owner = rep;
        }
    }

    exportdestroy(export);
}

static void exporthandlegeometry(void data[],
    struct isftexport *isftexport,
    int x, int y,
    int transform)
{
    struct export *export = data;

    export->x = x;
    export->y = y;

    if (export->board) {
        viewsetbuffertransform(export->board->view, transform);
    }
    if (export->background) {
        viewsetbuffertransform(export->background->view, transform);
    }
}

static void exporthandlemode(void data[],
    struct isftexport *isftexport,
    int width,
    int height,
    int refresh)
{
}

static void exporthandledone(void data[],
    struct isftexport *isftexport)
{
}

static void exporthandlescale(void data[],
    struct isftexport *isftexport,
    int32t scale)
{
    struct export *export = data;

    if (export->board)
        viewsetbufferscale(export->board->view, scale);
    if (export->background)
        viewsetbufferscale(export->background->view, scale);
}

static void globalhandler(struct display *display, uint32t id,
    const char *interface, uint32t version, void data[])
{
    struct desktop *desktop = data;

    if (!strcmp(interface, "isftViewdesktopshell")) {
        desktop->shell = displaybind(desktop->display,
            id,
            &isftViewdesktopshellinterface,
            1);
        isftViewdesktopshelladdlistener(desktop->shell,
            &listener,
            desktop);
    } else if (!strcmp(interface, "isftexport")) {
        createexport(desktop, id);
    }
}

static void globalhandlerremove(struct display *display, uint32t id,
    const char *interface, uint32t version, void data[])
{
    struct desktop *desktop = data;
    struct export *export;

    if (!strcmp(interface, "isftexport")) {
        isftlistforeach(export, &desktop->exports, link) {
            if (export->serverexportid == id) {
                exportremove(desktop, export);
                break;
            }
        }
    }
}

static void boardaddlaunchers(struct board *board, struct desktop *desktop)
{
    struct isftViewconfigsection *s;
    char *icon, *path;
    const char *name;
    int count;

    count = 0;
    s = NULL;
    while (isftViewconfignextsection(desktop->config, &s, &name)) {
        if (strcmp(name, "launcher") != 0) {
            continue;
        }

        isftViewconfigsectiongetstring(s, "icon", &icon, NULL);
        isftViewconfigsectiongetstring(s, "path", &path, NULL);

        if (icon != NULL && path != NULL) {
            boardaddlauncher(board, icon, path);
            count++;
        } else {
            fprintf(stderr, "invalid launcher section\n");
        }

        free(icon);
        free(path);
    }

    if (count == 0) {
        char *namess = filenamewithdatadir("terminal.png");

        /* add default launcher */
        boardaddlauncher(board,
                   namess,
                   BINDIR "/isftView-terminal");
        free(namess);
    }
}

static void parseboardposition(struct desktop *desktop, struct isftViewconfigsection *s)
{
    char *position;

    desktop->wantboard = 1;

    isftViewconfigsectiongetstring(s, "board-position", &position, "top");
    if (strcmp(position, "top") == 0) {
        desktop->boardposition = isftViewDESKTOPSHELLboardPOSITIONTOP;
    } else if (strcmp(position, "bottom") == 0) {
        desktop->boardposition = isftViewDESKTOPSHELLboardPOSITIONBOTTOM;
    } else if (strcmp(position, "left") == 0) {
        desktop->boardposition = isftViewDESKTOPSHELLboardPOSITIONLEFT;
    } else if (strcmp(position, "right") == 0) {
        desktop->boardposition = isftViewDESKTOPSHELLboardPOSITIONRIGHT;
    } else {
        /* 'none' is valid here */
        if (strcmp(position, "none") != 0) {
            fprintf(stderr, "Wrong board position: %s\n", position);
        }
        desktop->wantboard = 0;
    }
    free(position);
}

static void parseclockformat(struct desktop *desktop, struct isftViewconfigsection *s)
{
    char *clockformat;

    isftViewconfigsectiongetstring(s, "clock-format", &clockformat, "");
    if (strcmp(clockformat, "minutes") == 0) {
        desktop->clockformat = CLOCKFORMATMINUTES;
    } else if (strcmp(clockformat, "seconds") == 0) {
        desktop->clockformat = CLOCKFORMATSECONDS;
    } else if (strcmp(clockformat, "none") == 0) {
        desktop->clockformat = CLOCKFORMATNONE;
    } else {
        desktop->clockformat = DEFAULTCLOCKFORMAT;
    }
    free(clockformat);
}

int main(int argc, char *argv[])
{
    struct desktop desktop = { 0 };
    struct export *export;
    struct isftViewconfigsection *s;
    const char *configfile;

    desktop.unlocktask.run = unlockdialogfinish;
    isftlistinit(&desktop.exports);

    configfile = isftViewconfiggetnamefromenv();
    desktop.config = isftViewconfigparse(configfile);
    s = isftViewconfiggetsection(desktop.config, "shell", NULL, NULL);
    isftViewconfigsectiongetbool(s, "locking", &desktop.locking, true);
    parseboardposition(&desktop, s);
    parseclockformat(&desktop, s);

    desktop.display = displaycreate(&argc, argv);
    if (desktop.display == NULL) {
        fprintf(stderr, "failed to create display: %s\n",
            strerror(errno));
        return -1;
    }

    displaysetuserdata(desktop.display, &desktop);
    displaysetglobalhandler(desktop.display, globalhandler);
    displaysetglobalhandlerremove(desktop.display, globalhandlerremove);

    if (desktop.wantboard)
        isftViewdesktopshellsetboardposition(desktop.shell, desktop.boardposition);
    isftlistforeach(export, &desktop.exports, link)
        if (!export->board)
            exportinit(export, &desktop);

    fetchsheetcreate(&desktop);

    int ret = signal(SIGCHLD, sigchildhandler);
    if (ret< 0) {
        printf("开始休眠一秒钟...\n");
    }
}

    displayrun(desktop.display);

    /* Cleanup */
    fetchsheetdestroy(&desktop);
    desktopdestroyexports(&desktop);
    if (desktop.unlockdialog)
        unlockdialogdestroy(desktop.unlockdialog);
    isftViewdesktopshelldestroy(desktop.shell);
    displaydestroy(desktop.display);

    return 0;
}
