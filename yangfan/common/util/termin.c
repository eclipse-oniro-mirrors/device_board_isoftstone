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

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <pty.h>
#include <ctype.h>
#include <cairo.h>
#include <sys/epoll.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>

#include <linux/input.h>

#include <wayland-client.h>

#include <libweston/config-parser.h>
#include "shared/helpers.h"
#include "shared/xalloc.h"
#include "window.h"
static bool option_fullscreen;
static bool option_maximize;
static char *option_font;
static int option_font_size;
static char *option_term;
static char *option_shell;

static struct wl_list terminal_list;

static struct terminal *terminal_create(struct display *display);
static void terminal_destroy(struct terminal *terminal);
static int terminal_run(struct terminal *terminal, const char *path);

#define TERMINAL_DRAW_SINGLE_WIDE_CHARACTERS    \
    " !\"#$%&'()*+,-./"                         \
    "0123456789"                                \
    ":;<=>?@"                                   \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                \
    "[\\]^_`"                                   \
    "abcdefghijklmnopqrstuvwxyz"                \
    "{|}~"                                      \
    ""

#define MOD_SHIFT    0x01
#define MOD_ALT      0x02
#define MOD_CTRL     0x04

#define ATTRMASK_BOLD         0x01
#define ATTRMASK_UNDERLINE    0x02
#define ATTRMASK_BLINK        0x04
#define ATTRMASK_INVERSE      0x08
#define ATTRMASK_CONCEALED    0x10

/* Buffer sizes */
#define MAX_RESPONSE      256
#define MAX_ESCAPE        255

/* Terminal modes */
#define MODE_SHOW_CURSOR    0x00000001
#define MODE_INVERSE        0x00000002
#define MODE_AUTOWRAP       0x00000004
#define MODE_AUTOREPEAT     0x00000008
#define MODE_LF_NEWLINE     0x00000010
#define MODE_IRM            0x00000020
#define MODE_DELETE_SENDS_DEL    0x00000040
#define MODE_ALT_SENDS_ESC       0x00000080
#define NUM100 100
#define NUM1034 1034
#define NUM1037 1037
#define NUM1039 1039
#define NUM1049 1049
#define NUM107 107
#define NUM127 127
#define NUM128 128
#define NUM132 132
#define NUM80 80
#define NUM90 90
#define NUM64 64
#define NUM232 232
#define NUM500 500
#define NUM512 512
#define NUM777 777
#define NUM2560 256.0
#define NUM97 97
#define NUM768 768
enum NumBer {
    NUM0 = 0.5, NUM1, NUM2 = 2, NUM3, NUM4, NUM5, NUM6, NUM7, NUM8, NUM9, NUM10,
    NUM11, NUM12, NUM13, NUM14, NUM15, NUM16, NUM17, NUM18, NUM19, NUM20,
    NUM21, NUM22, NUM23, NUM24, NUM25, NUM26, NUM27, NUM28, NUM29, NUM30,
    NUM32 = 32, NUM37 = 37, NUM38, NUM39, NUM40, NUM47 = 47, NUM48, NUM49,
};
union utf8_char {
    unsigned char byte[4];
    uint32_t ch;
};

enum utf8_state {
    utf8state_start,
    utf8state_accept,
    utf8state_reject,
    utf8state_expect3,
    utf8state_expect2,
    utf8state_expect1
};

struct utf8_state_machine {
    enum utf8_state state;
    int len;
    union utf8_char s;
    uint32_t unicode;
};

static void init_state_machine(struct utf8_state_machine *machine)
{
    machine->state = utf8state_start;
    machine->len = 0;
    machine->s.ch = 0;
    machine->unicode = 0;
}
void case_reject(struct utf8_state_machine *machine, unsigned char c)
{
    machine->s.ch = 0;
    machine->len = 0;
    if (c == 0xC0 || c == 0xC1) {
        /* overlong encoding, reject */
        machine->state = utf8state_reject;
    } else if ((c & 0x80) == 0) {
        /* single byte, accept */
        machine->s.byte[machine->len++] = c;
        machine->state = utf8state_accept;
        machine->unicode = c;
    } else if ((c & 0xC0) == 0x80) {
        /* parser out of sync, ignore byte */
        machine->state = utf8state_start;
    } else if ((c & 0xE0) == 0xC0) {
        /* start of two byte sequence */
        machine->s.byte[machine->len++] = c;
        machine->state = utf8state_expect1;
        machine->unicode = c & 0x1f;
    } else if ((c & 0xF0) == 0xE0) {
        /* start of three byte sequence */
        machine->s.byte[machine->len++] = c;
        machine->state = utf8state_expect2;
        machine->unicode = c & 0x0f;
    } else if ((c & 0xF8) == 0xF0) {
        /* start of four byte sequence */
        machine->s.byte[machine->len++] = c;
        machine->state = utf8state_expect3;
        machine->unicode = c & 0x07;
    } else {
        /* overlong encoding, reject */
        machine->state = utf8state_reject;
    }
}
static enum utf8_state utf8_next_char(struct utf8_state_machine *machine, unsigned char c)
{
    switch (machine->state) {
        case utf8state_start:
        case utf8state_accept:
        case utf8state_reject:
            case_reject(machine, c);
            break;
        case utf8state_expect3:
            machine->s.byte[machine->len++] = c;
            machine->unicode = (machine->unicode << NUM6) | (c & 0x3f);
            if ((c & 0xC0) == 0x80) {
                /* all good, continue */
                machine->state = utf8state_expect2;
            } else {
                /* missing extra byte, reject */
                machine->state = utf8state_reject;
            }
            break;
        case utf8state_expect2:
            machine->s.byte[machine->len++] = c;
            machine->unicode = (machine->unicode << NUM6) | (c & 0x3f);
            if ((c & 0xC0) == 0x80) {
                /* all good, continue */
                machine->state = utf8state_expect1;
            } else {
                /* missing extra byte, reject */
                machine->state = utf8state_reject;
            }
            break;
        case utf8state_expect1:
            machine->s.byte[machine->len++] = c;
            machine->unicode = (machine->unicode << NUM6) | (c & 0x3f);
            if ((c & 0xC0) == 0x80) {
                /* all good, accept */
                machine->state = utf8state_accept;
            } else {
                /* missing extra byte, reject */
                machine->state = utf8state_reject;
            }
            break;
        default:
            machine->state = utf8state_reject;
            break;
    }

    return machine->state;
}

static uint32_t get_unicode(union utf8_char utf8)
{
    struct utf8_state_machine machine;
    int i;

    init_state_machine(&machine);
    for (i = 0; i < NUM4; i++) {
        utf8_next_char(&machine, utf8.byte[i]);
        if (machine.state == utf8state_accept ||
            machine.state == utf8state_reject) {
            break;
        }
    }

    if (machine.state == utf8state_reject) {
        return 0xfffd;
    }

    return machine.unicode;
}

static bool is_wide(union utf8_char utf8)
{
    uint32_t unichar = get_unicode(utf8);
    return wcwidth(unichar) > 1;
}

struct char_sub {
    union utf8_char match;
    union utf8_char replace;
};
/* Set last char_sub match to NULL char */
typedef struct char_sub *character_set;

struct char_sub CS_US[] = {
    {{{0, }}, {{0, }}}
};
static struct char_sub CS_UK[] = {
    {{{'#', 0, }}, {{0xC2, 0xA3, 0, }}}, /* POUND: £ */
    {{{0, }}, {{0, }}}
};
static struct char_sub CS_SPECIAL[] = {
    {{{'`', 0, }}, {{0xE2, 0x99, 0xA6, 0}}}, /* diamond: ♦ */
    {{{'a', 0, }}, {{0xE2, 0x96, 0x92, 0}}}, /* 50% cell: ▒ */
    {{{'b', 0, }}, {{0xE2, 0x90, 0x89, 0}}}, /* HT: ␉ */
    {{{'c', 0, }}, {{0xE2, 0x90, 0x8C, 0}}}, /* FF: ␌ */
    {{{'d', 0, }}, {{0xE2, 0x90, 0x8D, 0}}}, /* CR: ␍ */
    {{{'e', 0, }}, {{0xE2, 0x90, 0x8A, 0}}}, /* LF: ␊ */
    {{{'f', 0, }}, {{0xC2, 0xB0, 0, }}}, /* Degree: ° */
    {{{'g', 0, }}, {{0xC2, 0xB1, 0, }}}, /* Plus/Minus: ± */
    {{{'h', 0, }}, {{0xE2, 0x90, 0xA4, 0}}}, /* NL: ␤ */
    {{{'i', 0, }}, {{0xE2, 0x90, 0x8B, 0}}}, /* VT: ␋ */
    {{{'j', 0, }}, {{0xE2, 0x94, 0x98, 0}}}, /* CN_RB: ┘ */
    {{{'k', 0, }}, {{0xE2, 0x94, 0x90, 0}}}, /* CN_RT: ┐ */
    {{{'l', 0, }}, {{0xE2, 0x94, 0x8C, 0}}}, /* CN_LT: ┌ */
    {{{'m', 0, }}, {{0xE2, 0x94, 0x94, 0}}}, /* CN_LB: └ */
    {{{'n', 0, }}, {{0xE2, 0x94, 0xBC, 0}}}, /* CROSS: ┼ */
    {{{'o', 0, }}, {{0xE2, 0x8E, 0xBA, 0}}}, /* Horiz. Scan Line 1: ⎺ */
    {{{'p', 0, }}, {{0xE2, 0x8E, 0xBB, 0}}}, /* Horiz. Scan Line 3: ⎻ */
    {{{'q', 0, }}, {{0xE2, 0x94, 0x80, 0}}}, /* Horiz. Scan Line 5: ─ */
    {{{'r', 0, }}, {{0xE2, 0x8E, 0xBC, 0}}}, /* Horiz. Scan Line 7: ⎼ */
    {{{'s', 0, }}, {{0xE2, 0x8E, 0xBD, 0}}}, /* Horiz. Scan Line 9: ⎽ */
    {{{'t', 0, }}, {{0xE2, 0x94, 0x9C, 0}}}, /* TR: ├ */
    {{{'u', 0, }}, {{0xE2, 0x94, 0xA4, 0}}}, /* TL: ┤ */
    {{{'v', 0, }}, {{0xE2, 0x94, 0xB4, 0}}}, /* TU: ┴ */
    {{{'w', 0, }}, {{0xE2, 0x94, 0xAC, 0}}}, /* TD: ┬ */
    {{{'x', 0, }}, {{0xE2, 0x94, 0x82, 0}}}, /* V: │ */
    {{{'y', 0, }}, {{0xE2, 0x89, 0xA4, 0}}}, /* LE: ≤ */
    {{{'z', 0, }}, {{0xE2, 0x89, 0xA5, 0}}}, /* GE: ≥ */
    {{{'{', 0, }}, {{0xCF, 0x80, 0, }}}, /* PI: π */
    {{{'|', 0, }}, {{0xE2, 0x89, 0xA0, 0}}}, /* NEQ: ≠ */
    {{{'}', 0, }}, {{0xC2, 0xA3, 0, }}}, /* POUND: £ */
    {{{'~', 0, }}, {{0xE2, 0x8B, 0x85, 0}}}, /* DOT: ⋅ */
    {{{0, }}, {{0, }}}
};

static void apply_char_set(character_set cs, union utf8_char *utf8)
{
    int i = 0;

    while (cs[i].match.byte[0]) {
        if ((*utf8).ch == cs[i].match.ch) {
            *utf8 = cs[i].replace;
            break;
        }
        i++;
    }
}

struct key_map {
    int sym;
    int num;
    char escape;
    char code;
};
/* Set last key_sub sym to NULL */
typedef struct key_map *keyboard_mode;

static struct key_map KM_NORMAL[] = {
    { XKB_KEY_Left,  1, '[', 'D' },
    { XKB_KEY_Right, 1, '[', 'C' },
    { XKB_KEY_Up,    1, '[', 'A' },
    { XKB_KEY_Down,  1, '[', 'B' },
    { XKB_KEY_Home,  1, '[', 'H' },
    { XKB_KEY_End,   1, '[', 'F' },
    { 0, 0, 0, 0 }
};
static struct key_map KM_APPLICATION[] = {
    { XKB_KEY_Left,          1, 'O', 'D' },
    { XKB_KEY_Right,         1, 'O', 'C' },
    { XKB_KEY_Up,            1, 'O', 'A' },
    { XKB_KEY_Down,          1, 'O', 'B' },
    { XKB_KEY_Home,          1, 'O', 'H' },
    { XKB_KEY_End,           1, 'O', 'F' },
    { XKB_KEY_KP_Enter,      1, 'O', 'M' },
    { XKB_KEY_KP_Multiply,   1, 'O', 'j' },
    { XKB_KEY_KP_Add,        1, 'O', 'k' },
    { XKB_KEY_KP_Separator,  1, 'O', 'l' },
    { XKB_KEY_KP_Subtract,   1, 'O', 'm' },
    { XKB_KEY_KP_Divide,     1, 'O', 'o' },
    { 0, 0, 0, 0 }
};

static int function_key_response(char escape, int num, uint32_t modifiers,
                                 char code, char *response)
{
    int mod_num = 0;
    int len;

    if (modifiers & MOD_SHIFT_MASK) {
        mod_num   |= 1;
    }
    if (modifiers & MOD_ALT_MASK) {
        mod_num    |= NUM2;
    }
    if (modifiers & MOD_CONTROL_MASK) {
        mod_num |= NUM4;
    }

    if (mod_num != 0) {
        len = snprintf(response, MAX_RESPONSE, "\e[%d;%d%c",
                       num, mod_num + 1, code);
    } else if (code != '~') {
        len = snprintf(response, MAX_RESPONSE, "\e%c%c",
                       escape, code);
    } else {
        len = snprintf(response, MAX_RESPONSE, "\e%c%d%c",
                       escape, num, code);
    }

    if (len >= MAX_RESPONSE) {
        return MAX_RESPONSE - 1;
    } else {
        return len;
    }
}

/* returns the number of bytes written into response,
 * which must have room for MAX_RESPONSE bytes */
static int apply_key_map(keyboard_mode mode, int sym, uint32_t modifiers, char *response)
{
    struct key_map map;
    int len = 0;
    int i = 0;

    while (mode[i].sym) {
        map = mode[i++];
        if (sym == map.sym) {
            len = function_key_response(map.escape, map.num,
                                        modifiers, map.code,
                                        response);
            break;
        }
    }

    return len;
}

struct terminal_color { double r, g, b, a; };
struct attr {
    unsigned char fg, bg;
    char a;        /* attributes format:
                    * 76543210
            *    cilub */
    char s;        /* in selection */
};
struct color_scheme {
    struct terminal_color palette[16];
    char border;
    struct attr default_attr;
};

static void attr_init(struct attr *data_attr, struct attr attr, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        data_attr[i] = attr;
    }
}

enum escape_state {
    escape_state_normal = 0,
    escape_state_escape,
    escape_state_dcs,
    escape_state_csi,
    escape_state_osc,
    escape_state_inner_escape,
    escape_state_ignore,
    escape_state_special
};

#define ESC_FLAG_WHAT    0x01
#define ESC_FLAG_GT    0x02
#define ESC_FLAG_BANG    0x04
#define ESC_FLAG_CASH    0x08
#define ESC_FLAG_SQUOTE 0x10
#define ESC_FLAG_DQUOTE    0x20
#define ESC_FLAG_SPACE    0x40

enum {
    SELECT_NONE,
    SELECT_CHAR,
    SELECT_WORD,
    SELECT_LINE
};

struct terminal {
    struct window *window;
    struct widget *widget;
    struct display *display;
    char *title;
    union utf8_char *data;
    struct task io_task;
    char *tab_ruler;
    struct attr *data_attr;
    struct attr curr_attr;
    uint32_t mode;
    char origin_mode;
    char saved_origin_mode;
    struct attr saved_attr;
    union utf8_char last_char;
    int margin_top, margin_bottom;
    character_set cs, g0, g1;
    character_set saved_cs, saved_g0, saved_g1;
    keyboard_mode key_mode;
    int data_pitch, attr_pitch;  /* The width in bytes of a line */
    int width, height, row, column, max_width;
    uint32_t buffer_height;
    uint32_t start, end, saved_start, log_size;
    wl_fixed_t smooth_scroll;
    int saved_row, saved_column;
    int scrolling;
    int send_cursor_position;
    int fd, master;
    uint32_t modifiers;
    char escape[MAX_ESCAPE+1];
    int escape_length;
    enum escape_state state;
    enum escape_state outer_state;
    int escape_flags;
    struct utf8_state_machine state_machine;
    int margin;
    struct color_scheme *color_scheme;
    struct terminal_color color_table[256];
    cairo_font_extents_t extents;
    double average_width;
    cairo_scaled_font_t *font_normal, *font_bold;
    uint32_t hide_cursor_serial;
    int size_in_title;

    struct wl_data_source *selection;
    uint32_t click_time;
    int dragging, click_count;
    int selection_start_x, selection_start_y;
    int selection_end_x, selection_end_y;
    int selection_start_row, selection_start_col;
    int selection_end_row, selection_end_col;
    struct wl_list link;
    int pace_pipe;
};

/* Create default tab stops, every 8 characters */
static void terminal_init_tabs(struct terminal *terminal)
{
    int i = 0;

    while (i < terminal->width) {
        if (i % NUM8 == 0) {
            terminal->tab_ruler[i] = 1;
        } else {
            terminal->tab_ruler[i] = 0;
        }
        i++;
    }
}

static void terminal_init(struct terminal *terminal)
{
    terminal->curr_attr = terminal->color_scheme->default_attr;
    terminal->origin_mode = 0;
    terminal->mode = MODE_SHOW_CURSOR |
             MODE_AUTOREPEAT |
             MODE_ALT_SENDS_ESC |
             MODE_AUTOWRAP;

    terminal->row = 0;
    terminal->column = 0;

    terminal->g0 = CS_US;
    terminal->g1 = CS_US;
    terminal->cs = terminal->g0;
    terminal->key_mode = KM_NORMAL;

    terminal->saved_g0 = terminal->g0;
    terminal->saved_g1 = terminal->g1;
    terminal->saved_cs = terminal->cs;

    terminal->saved_attr = terminal->curr_attr;
    terminal->saved_origin_mode = terminal->origin_mode;
    terminal->saved_row = terminal->row;
    terminal->saved_column = terminal->column;

    if (terminal->tab_ruler != NULL) {
        terminal_init_tabs(terminal);
    }
}

static void init_color_table(struct terminal *terminal)
{
    int c, r;
    struct terminal_color *color_table = terminal->color_table;

    for (c = 0; c < MAX_RESPONSE; c ++) {
        if (c < NUM16) {
            color_table[c] = terminal->color_scheme->palette[c];
        } else if (c < NUM232) {
            r = c - NUM16;
            color_table[c].b = ((double)(r % NUM6) / 6.0); /* color NUM6.0 */
            r /= NUM6;
            color_table[c].g = ((double)(r % NUM6) / 6.0); /* color NUM6.0 */
            r /= NUM6;
            color_table[c].r = ((double)(r % NUM6) / 6.0); /* color NUM6.0 */
            color_table[c].a = 1.0;
        } else {
            r = (c - NUM232) * NUM10 + NUM8;
            color_table[c].r = ((double) r) / NUM2560;
            color_table[c].g = color_table[c].r;
            color_table[c].b = color_table[c].r;
            color_table[c].a = 1.0;
        }
    }
}

static union utf8_char *
terminal_get_row(struct terminal *terminal, int row)
{
    int index;

    index = (row + terminal->start) & (terminal->buffer_height - 1);

    return (void *) terminal->data + index * terminal->data_pitch;
}

static struct attr *
terminal_get_attr_row(struct terminal *terminal, int row)
{
    int index;

    index = (row + terminal->start) & (terminal->buffer_height - 1);

    return (void *) terminal->data_attr + index * terminal->attr_pitch;
}

union decoded_attr {
    struct attr attr;
    uint32_t key;
};

static void terminal_decode_attr(struct terminal *terminal, int row, int col,
                                 union decoded_attr *decoded)
{
    struct attr attr;
    int foreground, background, tmp;

    decoded->attr.s = 0;
    if (((row == terminal->selection_start_row &&
          col >= terminal->selection_start_col) ||
         row > terminal->selection_start_row) &&
        ((row == terminal->selection_end_row &&
          col < terminal->selection_end_col) ||
         row < terminal->selection_end_row)) {
        decoded->attr.s = 1;
    }

    /* get the attributes for this character cell */
    attr = terminal_get_attr_row(terminal, row)[col];
    if ((attr.a & ATTRMASK_INVERSE) ||
        decoded->attr.s ||
        ((terminal->mode & MODE_SHOW_CURSOR) &&
         window_has_focus(terminal->window) && terminal->row == row &&
         terminal->column == col)) {
        foreground = attr.bg;
        background = attr.fg;
        if (attr.a & ATTRMASK_BOLD) {
            if (foreground <= NUM16) {
                foreground |= 0x08;
            }
            if (background <= NUM16) {
                background &= 0x07;
            }
        }
    } else {
        foreground = attr.fg;
        background = attr.bg;
    }

    if (terminal->mode & MODE_INVERSE) {
        tmp = foreground;
        foreground = background;
        background = tmp;
        if (attr.a & ATTRMASK_BOLD) {
            if (foreground <= NUM16) {
                foreground |= 0x08;
            }
            if (background <= NUM16) {
                background &= 0x07;
            }
        }
    }

    decoded->attr.fg = foreground;
    decoded->attr.bg = background;
    decoded->attr.a = attr.a;
}


static void terminal_scroll_buffer(struct terminal *terminal, int d)
{
    int i;
    int td;
    td = d;
    terminal->start += td;
    if (td < 0) {
        td = 0 - td;
        for (i = 0; i < td; i++) {
            memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
            attr_init(terminal_get_attr_row(terminal, i),
                terminal->curr_attr, terminal->width);
        }
    } else {
        for (i = terminal->height - td; i < terminal->height; i++) {
            memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
            attr_init(terminal_get_attr_row(terminal, i),
                terminal->curr_attr, terminal->width);
        }
    }

    terminal->selection_start_row -= td;
    terminal->selection_end_row -= td;
}

static void terminal_scroll_window(struct terminal *terminal, int d)
{
    int i, td;
    int window_height;
    int from_row, to_row;
    td = d;
    // scrolling range is inclusive
    window_height = terminal->margin_bottom - terminal->margin_top + 1;
    td = td % (window_height + 1);
    if (td < 0) {
        td = 0 - td;
        to_row = terminal->margin_bottom;
        from_row = terminal->margin_bottom - td;

        for (i = 0; i < (window_height - td); i++) {
            memcpy(terminal_get_row(terminal, to_row - i),
                   terminal_get_row(terminal, from_row - i),
                   terminal->data_pitch);
            memcpy(terminal_get_attr_row(terminal, to_row - i),
                   terminal_get_attr_row(terminal, from_row - i),
                   terminal->attr_pitch);
        }
        for (i = terminal->margin_top; i < (terminal->margin_top + td); i++) {
            memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
            attr_init(terminal_get_attr_row(terminal, i),
                terminal->curr_attr, terminal->width);
        }
    } else {
        to_row = terminal->margin_top;
        from_row = terminal->margin_top + td;

        for (i = 0; i < (window_height - td); i++) {
            memcpy(terminal_get_row(terminal, to_row + i),
                   terminal_get_row(terminal, from_row + i),
                   terminal->data_pitch);
            memcpy(terminal_get_attr_row(terminal, to_row + i),
                   terminal_get_attr_row(terminal, from_row + i),
                   terminal->attr_pitch);
        }
        for (i = terminal->margin_bottom - td + 1; i <= terminal->margin_bottom; i++) {
            memset(terminal_get_row(terminal, i), 0, terminal->data_pitch);
            attr_init(terminal_get_attr_row(terminal, i),
                terminal->curr_attr, terminal->width);
        }
    }
}

static void terminal_scroll(struct terminal *terminal, int d)
{
    if (terminal->margin_top == 0 && terminal->margin_bottom == terminal->height - 1) {
        terminal_scroll_buffer(terminal, d);
    } else {
        terminal_scroll_window(terminal, d);
    }
}

static void terminal_shift_line(struct terminal *terminal, int d)
{
    union utf8_char *row;
    struct attr *attr_row;
    int td = d;
    row = terminal_get_row(terminal, terminal->row);
    attr_row = terminal_get_attr_row(terminal, terminal->row);

    if ((terminal->width + td) <= terminal->column) {
        td = terminal->column + 1 - terminal->width;
    }
    if ((terminal->column + td) >= terminal->width) {
        td = terminal->width - terminal->column - 1;
    }

    if (td < 0) {
        td = 0 - td;
        memmove(&row[terminal->column],
                &row[terminal->column + td],
            (terminal->width - terminal->column - td) * sizeof(union utf8_char));
        memmove(&attr_row[terminal->column], &attr_row[terminal->column + td],
                (terminal->width - terminal->column - td) * sizeof(struct attr));
        memset(&row[terminal->width - td], 0, td * sizeof(union utf8_char));
        attr_init(&attr_row[terminal->width - td], terminal->curr_attr, td);
    } else {
        memmove(&row[terminal->column + td], &row[terminal->column],
            (terminal->width - terminal->column - td) * sizeof(union utf8_char));
        memmove(&attr_row[terminal->column + td], &attr_row[terminal->column],
            (terminal->width - terminal->column - td) * sizeof(struct attr));
        memset(&row[terminal->column], 0, td * sizeof(union utf8_char));
        attr_init(&attr_row[terminal->column], terminal->curr_attr, td);
    }
}
void if_block(struct terminal *terminal, int width, int height, union utf8_char *data,
              struct attr *data_attr)
{
    int i, l, total_rows;
    if (terminal->data && terminal->data_attr) {
        if (width > terminal->width) {
            l = terminal->width;
        } else {
            l = width;
        }

        if (terminal->height > theight) {
            total_rows = theight;
            i = 1 + terminal->row - height;
            if (i > 0) {
                terminal->start += i;
                terminal->row = terminal->row - i;
            }
        } else {
            total_rows = terminal->height;
        }
        for (i = 0; i < total_rows; i++) {
            memcpy(&data[width * i],
                   terminal_get_row(terminal, i),
                   l * sizeof(union utf8_char));
            memcpy(&data_attr[width * i],
                   terminal_get_attr_row(terminal, i),
                   l * sizeof(struct attr));
        }
        free(terminal->data);
            free(terminal->data_attr);
            free(terminal->tab_ruler);
    }
    return ;
}
void ifJudge(uint32_t *d, struct terminal *terminal, int theight)
{
    *d = 0;
    if (theight < terminal->height && theight <= terminal->row) {
        *d = terminal->height - theight;
    } else if (theight > terminal->height &&
         terminal->height - 1 == terminal->row) {
        *d = terminal->height - theight;
        if (terminal->log_size < uheight) {
            *d = -terminal->start;
        }
    }
    terminal->start += d;
    terminal->row -= d;
    return ;
}
static void terminal_resize_cells(struct terminal *terminal,
                                  int width, int height)
{
    union utf8_char *data;
    struct attr *data_attr;
    char *tab_ruler;
    int data_pitch, attr_pitch;
    uint32_t d, uheight = height;
    struct rectangle allocation;
    struct winsize ws;
    int theight = height;
    if (uheight > terminal->buffer_height) {
        theight = terminal->buffer_height;
    }

    if (terminal->width == width && terminal->height == theight) {
        return;
    }

    if (terminal->data && width <= terminal->max_width) {
        ifJudge(&d, terminal, theight);
    } else {
        terminal->max_width = width;
        data_pitch = width * sizeof(union utf8_char);
        data = xzalloc(data_pitch * terminal->buffer_height);
        attr_pitch = width * sizeof(struct attr);
        data_attr = xmalloc(attr_pitch * terminal->buffer_height);
        tab_ruler = xzalloc(width);
        attr_init(data_attr, terminal->curr_attr, width * terminal->buffer_height);
        if_block(terminal, width, height, data, *data_attr);

        terminal->data_pitch = data_pitch;
        terminal->attr_pitch = attr_pitch;
        terminal->data = data;
        terminal->data_attr = data_attr;
        terminal->tab_ruler = tab_ruler;
        terminal->start = 0;
    }

    terminal->margin_bottom = theight - (terminal->height - terminal->margin_bottom);
    terminal->width = width;
    terminal->height = theight;
    terminal_init_tabs(terminal);

    /* Update the window size */
    ws.ws_row = terminal->height;
    ws.ws_col = terminal->width;
    widget_get_allocation(terminal->widget, &allocation);
    ws.ws_xpixel = allocation.width;
    ws.ws_ypixel = allocation.height;
    ioctl(terminal->master, TIOCSWINSZ, &ws);
}

static void update_title(struct terminal *terminal)
{
    if (window_is_resizing(terminal->window)) {
        char *p;
        if (asprintf(&p, "%s — [%dx%d]", terminal->title, terminal->width, terminal->height) > 0) {
            window_set_title(terminal->window, p);
            free(p);
        }
    } else {
        window_set_title(terminal->window, terminal->title);
    }
}

static void resize_handler(struct widget *widget,
                           int32_t width, int32_t height, void data[])
{
    struct terminal *terminal = data;
    int32_t columns, rows, m, twidth = width;

    if (terminal->pace_pipe >= 0) {
        close(terminal->pace_pipe);
        terminal->pace_pipe = -1;
    }
    m = NUM2 * terminal->margin;
    columns = (twidth - m) / (int32_t) terminal->average_width;
    rows = (height - m) / (int32_t) terminal->extents.height;

    if (!window_is_fullscreen(terminal->window) &&
        !window_is_maximized(terminal->window)) {
        twidth = columns * terminal->average_width + m;
        height = rows * terminal->extents.height + m;
        widget_set_size(terminal->widget, twidth, height);
    }

    terminal_resize_cells(terminal, columns, rows);
    update_title(terminal);
}

static void state_changed_handler(struct window *window, void data[])
{
    struct terminal *terminal = data;
    update_title(terminal);
}

static void terminal_resize(struct terminal *terminal, int columns, int rows)
{
    int32_t width, height, m;

    if (window_is_fullscreen(terminal->window) ||
        window_is_maximized(terminal->window)) {
        return;
    }

    m = NUM2 * terminal->margin;
    width = columns * terminal->average_width + m;
    height = rows * terminal->extents.height + m;

    window_frame_set_child_size(terminal->widget, width, height);
}

struct color_scheme DEFAULT_COLORS = {
    {
        {0,    0,    0,    1}, /* black */
        {0.66, 0,    0,    1}, /* red */
        {0,    0.66, 0,    1}, /* green */
        {0.66, 0.33, 0,    1}, /* orange (nicer than muddy yellow) */
        {0,    0,    0.66, 1}, /* blue */
        {0.66, 0,    0.66, 1}, /* magenta */
        {0,    0.66, 0.66, 1}, /* cyan */
        {0.66, 0.66, 0.66, 1}, /* light grey */
        {0.22, 0.33, 0.33, 1}, /* dark grey */
        {1,    0.33, 0.33, 1}, /* high red */
        {0.33, 1,    0.33, 1}, /* high green */
        {1,    1,    0.33, 1}, /* high yellow */
        {0.33, 0.33, 1,    1}, /* high blue */
        {1,    0.33, 1,    1}, /* high magenta */
        {0.33, 1,    1,    1}, /* high cyan */
        {1,    1,    1,    1}  /* white */
    },
    0,                             /* black border */
    {7, 0, 0, }                    /* bg:black (0), fg:light gray (7)  */
};

static void terminal_set_color(struct terminal *terminal, cairo_t *cr, int index)
{
    cairo_set_source_rgba(cr,
                          terminal->color_table[index].r,
                          terminal->color_table[index].g,
                          terminal->color_table[index].b,
                          terminal->color_table[index].a);
}

static void terminal_send_selection(struct terminal *terminal, int fd)
{
    int row, col;
    union utf8_char *p_row;
    union decoded_attr attr;
    FILE *fp;
    int len;

    fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        return;
    }
    for (row = terminal->selection_start_row; row < terminal->height; row++) {
        p_row = terminal_get_row(terminal, row);
        for (col = 0; col < terminal->width; col++) {
            if (p_row[col].ch == 0x200B) {
                /* space glyph */
                continue;
            }
            /* get the attributes for this character cell */
            terminal_decode_attr(terminal, row, col, &attr);
            if (!attr.attr.s) {
                continue;
            }
            len = strnlen((char *) p_row[col].byte, NUM4);
            if (len > 0) {
                fwrite(p_row[col].byte, 1, len, fp);
            }
            if (len == 0 || col == terminal->width - 1) {
                fwrite("\n", 1, 1, fp);
                break;
            }
        }
    }
    if (fclose(fp) != 0) {
        printf("fclose faild");
    }
}

struct glyph_run {
    struct terminal *terminal;
    cairo_t *cr;
    unsigned int count;
    union decoded_attr attr;
    cairo_glyph_t glyphs[256], *g;
};

static void glyph_run_init(struct glyph_run *run, struct terminal *terminal, cairo_t *cr)
{
    run->terminal = terminal;
    run->cr = cr;
    run->g = run->glyphs;
    run->count = 0;
    run->attr.key = 0;
}

static void glyph_run_flush(struct glyph_run *run, union decoded_attr attr)
{
    cairo_scaled_font_t *font;

    if (run->count > ARRAY_LENGTH(run->glyphs) - NUM10 ||
        (attr.key != run->attr.key)) {
        if (run->attr.attr.a & (ATTRMASK_BOLD | ATTRMASK_BLINK)) {
            font = run->terminal->font_bold;
        } else {
            font = run->terminal->font_normal;
        }
        cairo_set_scaled_font(run->cr, font);
        terminal_set_color(run->terminal, run->cr,
                           run->attr.attr.fg);

        if (!(run->attr.attr.a & ATTRMASK_CONCEALED)) {
            cairo_show_glyphs (run->cr, run->glyphs, run->count);
        }
        run->g = run->glyphs;
        run->count = 0;
    }
    run->attr = attr;
}

static void glyph_run_add(struct glyph_run *run, int x, int y, union utf8_char *c)
{
    int num_glyphs;
    cairo_scaled_font_t *font;

    num_glyphs = ARRAY_LENGTH(run->glyphs) - run->count;

    if (run->attr.attr.a & (ATTRMASK_BOLD | ATTRMASK_BLINK)) {
        font = run->terminal->font_bold;
    } else {
        font = run->terminal->font_normal;
    }

    cairo_move_to(run->cr, x, y);
    cairo_scaled_font_text_to_glyphs (font, x, y,
                                      (char *) c->byte, NUM4,
                                      &run->g, &num_glyphs,
                                      NULL, NULL, NULL);
    run->g += num_glyphs;
    run->count += num_glyphs;
}


static void redraw_handler(struct widget *widget, void data[])
{
    struct terminal *terminal = data;
    struct rectangle allocation;
    cairo_t *cr;
    int top_margin, side_margin;
    int row, col, cursor_x, cursor_y;
    union utf8_char *p_row;
    union decoded_attr attr;
    int text_x, text_y;
    cairo_surface_t *surface;
    double d;
    struct glyph_run run;
    cairo_font_extents_t extents;
    double average_width;
    double unichar_width;

    surface = window_get_surface(terminal->window);
    widget_get_allocation(terminal->widget, &allocation);
    cr = widget_cairo_create(terminal->widget);
    cairo_rectangle(cr, allocation.x, allocation.y,
                    allocation.width, allocation.height);
    cairo_clip(cr);
    cairo_push_group(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    terminal_set_color(terminal, cr, terminal->color_scheme->border);
    cairo_paint(cr);

    cairo_set_scaled_font(cr, terminal->font_normal);

    extents = terminal->extents;
    average_width = terminal->average_width;
    side_margin = (allocation.width - terminal->width * average_width) / NUM2;
    top_margin = (allocation.height - terminal->height * extents.height) / NUM2;

    cairo_set_line_width(cr, 1.0);
    cairo_translate(cr, allocation.x + side_margin,
                    allocation.y + top_margin);
    /* paint the background */
    for (row = 0; row < terminal->height; row++) {
        p_row = terminal_get_row(terminal, row);
        for (col = 0; col < terminal->width; col++) {
            /* get the attributes for this character cell */
            terminal_decode_attr(terminal, row, col, &attr);

            if (attr.attr.bg == terminal->color_scheme->border) {
                continue;
            }

            if (is_wide(p_row[col])) {
                unichar_width = NUM2 * average_width;
            } else {
                unichar_width = average_width;
            }

            terminal_set_color(terminal, cr, attr.attr.bg);
            cairo_move_to(cr, col * average_width,
                          row * extents.height);
            cairo_rel_line_to(cr, unichar_width, 0);
            cairo_rel_line_to(cr, 0, extents.height);
            cairo_rel_line_to(cr, -unichar_width, 0);
            cairo_close_path(cr);
            cairo_fill(cr);
        }
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* paint the foreground */
    glyph_run_init(&run, terminal, cr);
    for (row = 0; row < terminal->height; row++) {
        p_row = terminal_get_row(terminal, row);
        for (col = 0; col < terminal->width; col++) {
            /* get the attributes for this character cell */
            terminal_decode_attr(terminal, row, col, &attr);

            glyph_run_flush(&run, attr);

            text_x = col * average_width;
            text_y = extents.ascent + row * extents.height;
            if (attr.attr.a & ATTRMASK_UNDERLINE) {
                terminal_set_color(terminal, cr, attr.attr.fg);
                cairo_move_to(cr, text_x, (double)text_y + NUM1);
                cairo_line_to(cr, text_x + average_width, (double) text_y + NUM1);
                cairo_stroke(cr);
            }

                        /* skip space glyph (RLE) we use as a placeholder of
                           the right half of a double-width character,
                           because RLE is not available in every font. */
            if (p_row[col].ch == 0x200B) {
                continue;
            }

            glyph_run_add(&run, text_x, text_y, &p_row[col]);
        }
    }

    attr.key = ~0;
    glyph_run_flush(&run, attr);

    if ((terminal->mode & MODE_SHOW_CURSOR) &&
        !window_has_focus(terminal->window)) {
        d = NUM0;

        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, terminal->column * average_width + d,
                      terminal->row * extents.height + d);
        cairo_rel_line_to(cr, average_width - NUM2 * d, 0);
        cairo_rel_line_to(cr, 0, extents.height - NUM2 * d);
        cairo_rel_line_to(cr, -average_width + NUM2 * d, 0);
        cairo_close_path(cr);

        cairo_stroke(cr);
    }

    cairo_pop_group_to_source(cr);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    if (terminal->send_cursor_position) {
        cursor_x = side_margin + allocation.x +
                terminal->column * average_width;
        cursor_y = top_margin + allocation.y +
                terminal->row * extents.height;
        window_set_text_cursor_position(terminal->window,
                                        cursor_x, cursor_y);
        terminal->send_cursor_position = 0;
    }
}

static void terminal_write(struct terminal *terminal, const char *data, size_t length)
{
    if (write(terminal->master, data, length) < 0) {
        abort();
    }
    terminal->send_cursor_position = 1;
}

static void terminal_data(struct terminal *terminal, const char *data, size_t length);

static void handle_char(struct terminal *terminal, union utf8_char utf8);

static void handle_sgr(struct terminal *terminal, int code);

static void handle_term_parameter(struct terminal *terminal, int code, int sr)
{
    int i;

    if (terminal->escape_flags & ESC_FLAG_WHAT) {
        switch (code) {
            case 1:  /* DECCKM */
                if (sr) {
                    terminal->key_mode = KM_APPLICATION;
                } else {
                    terminal->key_mode = KM_NORMAL;
                }
                break;
            case NUM2:  /* DECANM */
                /* No VT52 support yet */
                terminal->g0 = CS_US;
                terminal->g1 = CS_US;
                terminal->cs = terminal->g0;
                break;
            case NUM3:  /* DECCOLM */
                if (sr) {
                    terminal_resize(terminal, NUM132, NUM24);
                } else {
                    terminal_resize(terminal, NUM80, NUM24);
                }
                /* set columns, but also home cursor and clear screen */
                terminal->row = 0; terminal->column = 0;
                for (i = 0; i < terminal->height; i++) {
                    memset(terminal_get_row(terminal, i),
                        0, terminal->data_pitch);
                    attr_init(terminal_get_attr_row(terminal, i),
                        terminal->curr_attr, terminal->width);
                }
                break;
            case NUM5:  /* DECSCNM */
                if (sr) {
                    terminal->mode |=  MODE_INVERSE;
                } else {
                    terminal->mode &= ~MODE_INVERSE;
                }
                break;
            case NUM6:  /* DECOM */
                terminal->origin_mode = sr;
                if (terminal->origin_mode) {
                    terminal->row = terminal->margin_top;
                } else {
                    terminal->row = 0;
                }
                terminal->column = 0;
                break;
            case NUM7:  /* DECAWM */
                if (sr) {
                    terminal->mode |=  MODE_AUTOWRAP;
                } else {
                    terminal->mode &= ~MODE_AUTOWRAP;
                }
                break;
            case NUM8:  /* DECARM */
                if (sr) {
                    terminal->mode |=  MODE_AUTOREPEAT;
                } else {
                    terminal->mode &= ~MODE_AUTOREPEAT;
                }
                break;
            case NUM12:  /* Very visible cursor (CVVIS) */
                break;
            case NUM25:
                if (sr) {
                    terminal->mode |=  MODE_SHOW_CURSOR;
                } else {
                    terminal->mode &= ~MODE_SHOW_CURSOR;
                }
                break;
            case NUM1034:   /* smm/rmm, meta mode on/off */
                /* ignore */
                break;
            case NUM1037:   /* deleteSendsDel */
                if (sr) {
                    terminal->mode |=  MODE_DELETE_SENDS_DEL;
                } else {
                    terminal->mode &= ~MODE_DELETE_SENDS_DEL;
                }
                break;
            case NUM1039:   /* altSendsEscape */
                if (sr) {
                    terminal->mode |=  MODE_ALT_SENDS_ESC;
                } else {
                    terminal->mode &= ~MODE_ALT_SENDS_ESC;
                }
                break;
            case NUM1049:   /* rmcup/smcup, alternate screen */
                /* Ignore.  Should be possible to implement,
                * but it's kind of annoying. */
                break;
            default:
                fprintf(stderr, "Unknown parameter: ?%d\n", code);
                break;
        }
    } else {
        switch (code) {
            case NUM4:  /* IRM */
                if (sr) {
                    terminal->mode |=  MODE_IRM;
                } else {
                    terminal->mode &= ~MODE_IRM;
                }
                break;
            case NUM20: /* LNM */
                if (sr) {
                    terminal->mode |=  MODE_LF_NEWLINE;
                } else {
                    terminal->mode &= ~MODE_LF_NEWLINE;
                }
                break;
            default:
                fprintf(stderr, "Unknown parameter: %d\n", code);
                break;
        }
    }
}

static void handle_dcs(struct terminal *terminal)
{
}

static void handle_osc(struct terminal *terminal)
{
    char *p;
    int code;

    terminal->escape[terminal->escape_length++] = '\0';
    p = &terminal->escape[NUM2];
    code = strtol(p, &p, NUM10);
    if (*p == ';') {
        p++;
    }

    switch (code) {
        case 0: /* Icon name and window title */
        case 1: /* Icon label */
        case NUM2: /* Window title */
            free(terminal->title);
            terminal->title = strdup(p);
            window_set_title(terminal->window, p);
            break;
        case NUM7: /* shell cwd as uri */
            break;
        case NUM777: /* Desktop notifications */
            break;
        default:
            fprintf(stderr, "Unknown OSC escape code %d, text %s\n",
                    code, p);
            break;
    }
}
void case_m_block(int set[10], int args[10], struct terminal *terminal)
{
    int i;
    for (i = 0; i < NUM10; i++) {
        if (i <= NUM7 && set[i] && set[i + 1] &&
            set[i + NUM2] && args[i + 1] == NUM5) {
            if (args[i] == NUM38) {
                handle_sgr(terminal, args[i + NUM2] + MAX_RESPONSE);
            } else if (args[i] == NUM48) {
                handle_sgr(terminal, args[i + NUM2] + NUM512);
            }
        }
        if (set[i]) {
            handle_sgr(terminal, args[i]);
        } else if (i == 0) {
            handle_sgr(terminal, 0);
        }
    }
    return ;
}
static void handle_escape(struct terminal *terminal)
{
    union utf8_char *row;
    struct attr *attr_row;
    char *p;
    int i, count, x, y, top, bottom;
    int args[10], set[10] = { 0, };
    char response[MAX_RESPONSE] = {0, };
    struct rectangle allocation;

    terminal->escape[terminal->escape_length++] = '\0';
    i = 0;
    p = &terminal->escape[NUM2];
    while ((isdigit(*p) || *p == ';') && i < NUM10) {
        if (*p == ';') {
            if (!set[i]) {
                args[i] = 0;
                set[i] = 1;
            }
            p++;
            i++;
        } else {
            args[i] = strtol(p, &p, NUM10);
            set[i] = 1;
        }
    }

    switch (*p) {
        case '@':    /* ICH - Insert <count> blank characters */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            terminal_shift_line(terminal, count);
            break;
        case 'A':    /* CUU - Move cursor up <count> rows */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if (terminal->row - count >= terminal->margin_top) {
                terminal->row -= count;
            } else {
                terminal->row = terminal->margin_top;
            }
            break;
        case 'B':    /* CUD - Move cursor down <count> rows */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if (terminal->row + count <= terminal->margin_bottom) {
                terminal->row += count;
            } else {
                terminal->row = terminal->margin_bottom;
            }
            break;
        case 'C':    /* CUF - Move cursor right by <count> columns */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if ((terminal->column + count) < terminal->width) {
                terminal->column += count;
            } else {
                terminal->column = terminal->width - 1;
            }
            break;
        case 'D':    /* CUB - Move cursor left <count> columns */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if ((terminal->column - count) >= 0) {
                terminal->column -= count;
            } else {
                terminal->column = 0;
            }
            break;
        case 'E':    /* CNL - Move cursor down <count> rows, to column 1 */
            count = set[0] ? args[0] : 1;
            if (terminal->row + count <= terminal->margin_bottom) {
                terminal->row += count;
            } else {
                terminal->row = terminal->margin_bottom;
            }
            terminal->column = 0;
            break;
        case 'F':    /* CPL - Move cursour up <count> rows, to column 1 */
            count = set[0] ? args[0] : 1;
            if (terminal->row - count >= terminal->margin_top) {
                terminal->row -= count;
            } else {
                terminal->row = terminal->margin_top;
            }
            terminal->column = 0;
            break;
        case 'G':    /* CHA - Move cursor to column <y> in current row */
            y = set[0] ? args[0] : 1;
            y = y <= 0 ? 1 : y > terminal->width ? terminal->width : y;

            terminal->column = y - 1;
            break;
        case 'f':    /* HVP - Move cursor to <x, y> */
        case 'H':    /* CUP - Move cursor to <x, y> (origin at 1,1) */
            x = (set[1] ? args[1] : 1) - 1;
            x = x < 0 ? 0 :
                (x >= terminal->width ? terminal->width - 1 : x);

            y = (set[0] ? args[0] : 1) - 1;
            if (terminal->origin_mode) {
                y += terminal->margin_top;
                y = y < terminal->margin_top ? terminal->margin_top :
                    (y > terminal->margin_bottom ? terminal->margin_bottom : y);
            } else {
                y = y < 0 ? 0 :
                    (y >= terminal->height ? terminal->height - 1 : y);
            }

            terminal->row = y;
            terminal->column = x;
            break;
        case 'I':    /* CHT */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            while (count > 0 && terminal->column < terminal->width) {
                if (terminal->tab_ruler[terminal->column]) {
                    count--;
                }
                terminal->column++;
            }
            terminal->column--;
            break;
        case 'J':    /* ED - Erase display */
            row = terminal_get_row(terminal, terminal->row);
            attr_row = terminal_get_attr_row(terminal, terminal->row);
            if (!set[0] || args[0] == 0 || args[0] > NUM2) {
                memset(&row[terminal->column],
                    0, (terminal->width - terminal->column) * sizeof(union utf8_char));
                attr_init(&attr_row[terminal->column],
                    terminal->curr_attr, terminal->width - terminal->column);
                for (i = terminal->row + 1; i < terminal->height; i++) {
                    memset(terminal_get_row(terminal, i),
                           0, terminal->data_pitch);
                    attr_init(terminal_get_attr_row(terminal, i),
                              terminal->curr_attr, terminal->width);
                }
            } else if (args[0] == 1) {
                memset(row, 0, (terminal->column+1) * sizeof(union utf8_char));
                attr_init(attr_row, terminal->curr_attr, terminal->column+1);
                for (i = 0; i < terminal->row; i++) {
                    memset(terminal_get_row(terminal, i),
                           0, terminal->data_pitch);
                    attr_init(terminal_get_attr_row(terminal, i),
                              terminal->curr_attr, terminal->width);
                }
            } else if (args[0] == NUM2) {
                /* Clear screen by scrolling contents out */
                terminal_scroll_buffer(terminal,
                                       terminal->end - terminal->start);
            }
            break;
        case 'K':    /* EL - Erase line */
            row = terminal_get_row(terminal, terminal->row);
            attr_row = terminal_get_attr_row(terminal, terminal->row);
            if (!set[0] || args[0] == 0 || args[0] > NUM2) {
                memset(&row[terminal->column], 0,
                    (terminal->width - terminal->column) * sizeof(union utf8_char));
                attr_init(&attr_row[terminal->column], terminal->curr_attr,
                          terminal->width - terminal->column);
            } else if (args[0] == 1) {
                memset(row, 0, (terminal->column+1) * sizeof(union utf8_char));
                attr_init(attr_row, terminal->curr_attr, terminal->column+1);
            } else if (args[0] == NUM2) {
                memset(row, 0, terminal->data_pitch);
                attr_init(attr_row, terminal->curr_attr, terminal->width);
            }
            break;
        case 'L':    /* IL - Insert <count> blank lines */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if (terminal->row >= terminal->margin_top &&
                terminal->row < terminal->margin_bottom) {
                top = terminal->margin_top;
                terminal->margin_top = terminal->row;
                terminal_scroll(terminal, 0 - count);
                terminal->margin_top = top;
            } else if (terminal->row == terminal->margin_bottom) {
                memset(terminal_get_row(terminal, terminal->row),
                    0, terminal->data_pitch);
                attr_init(terminal_get_attr_row(terminal, terminal->row),
                    terminal->curr_attr, terminal->width);
            }
            break;
        case 'M':    /* DL - Delete <count> lines */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if (terminal->row >= terminal->margin_top &&
                terminal->row < terminal->margin_bottom) {
                top = terminal->margin_top;
                terminal->margin_top = terminal->row;
                terminal_scroll(terminal, count);
                terminal->margin_top = top;
            } else if (terminal->row == terminal->margin_bottom) {
                memset(terminal_get_row(terminal, terminal->row),
                       0, terminal->data_pitch);
            }
            break;
        case 'P':    /* DCH - Delete <count> characters on current line */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            terminal_shift_line(terminal, 0 - count);
            break;
        case 'S':    /* SU */
            terminal_scroll(terminal, set[0] ? args[0] : 1);
            break;
        case 'T':    /* SD */
            terminal_scroll(terminal, 0 - (set[0] ? args[0] : 1));
            break;
        case 'X':    /* ECH - Erase <count> characters on current line */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if ((terminal->column + count) > terminal->width) {
                count = terminal->width - terminal->column;
            }
            row = terminal_get_row(terminal, terminal->row);
            attr_row = terminal_get_attr_row(terminal, terminal->row);
            memset(&row[terminal->column], 0, count * sizeof(union utf8_char));
            attr_init(&attr_row[terminal->column], terminal->curr_attr, count);
            break;
        case 'Z':    /* CBT */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            while (count > 0 && terminal->column >= 0) {
                if (terminal->tab_ruler[terminal->column]) {
                    count--;
                }
                terminal->column--;
            }
            terminal->column++;
            break;
        case '`':    /* HPA - Move cursor to <y> column in current row */
            y = set[0] ? args[0] : 1;
            y = y <= 0 ? 1 : y > terminal->width ? terminal->width : y;

            terminal->column = y - 1;
            break;
        case 'b':    /* REP */
            count = set[0] ? args[0] : 1;
            if (count == 0) {
                count = 1;
            }
            if (terminal->last_char.byte[0]) {
                for (i = 0; i < count; i++) {
                    handle_char(terminal, terminal->last_char);
                }
            }
            terminal->last_char.byte[0] = 0;
            break;
        case 'c':    /* Primary DA - Answer "I am a VT102" */
            terminal_write(terminal, "\e[?6c", NUM5);
            break;
        case 'd':    /* VPA - Move cursor to <x> row, current column */
            x = set[0] ? args[0] : 1;
            x = x <= 0 ? 1 : x > terminal->height ? terminal->height : x;

            terminal->row = x - 1;
            break;
        case 'g':    /* TBC - Clear tab stop(s) */
            if (!set[0] || args[0] == 0) {
                terminal->tab_ruler[terminal->column] = 0;
            } else if (args[0] == NUM3) {
                memset(terminal->tab_ruler, 0, terminal->width);
            }
            break;
        case 'h':    /* SM - Set mode */
            for (i = 0; i < NUM10 && set[i]; i++) {
                handle_term_parameter(terminal, args[i], 1);
            }
            break;
        case 'l':    /* RM - Reset mode */
            for (i = 0; i < NUM10 && set[i]; i++) {
                handle_term_parameter(terminal, args[i], 0);
            }
            break;
        case 'm':    /* SGR - Set attributes */
            case_m_block(set[NUM10], args[NUM10], terminal);
            break;
        case 'n':    /* DSR - Status report */
            i = set[0] ? args[0] : 0;
            if (i == 0 || i == NUM5) {
                terminal_write(terminal, "\e[0n", NUM4);
            } else if (i == NUM6) {
                snprintf(response, MAX_RESPONSE, "\e[%d;%dR",
                         terminal->origin_mode ?
                         terminal->row+terminal->margin_top : terminal->row+1,
                         terminal->column+1);
                         terminal_write(terminal, response, strlen(response));
            }
            break;
        case 'r':    /* DECSTBM - Set scrolling region */
            if (!set[0]) {
                terminal->margin_top = 0;
                terminal->margin_bottom = terminal->height-1;
                terminal->row = 0;
                terminal->column = 0;
            } else {
                top = (set[0] ? args[0] : 1) - 1;
                top = top < 0 ? 0 :
                    (top >= terminal->height ? terminal->height - 1 : top);
                bottom = (set[1] ? args[1] : 1) - 1;
                bottom = bottom < 0 ? 0 :
                        (bottom >= terminal->height ? terminal->height - 1 : bottom);
                if (bottom > top) {
                    terminal->margin_top = top;
                    terminal->margin_bottom = bottom;
                } else {
                    terminal->margin_top = 0;
                    terminal->margin_bottom = terminal->height-1;
                }
                if (terminal->origin_mode) {
                    terminal->row = terminal->margin_top;
                } else {
                    terminal->row = 0
                };
                terminal->column = 0;
            }
            break;
        case 's':    /* Save cursor location */
            terminal->saved_row = terminal->row;
            terminal->saved_column = terminal->column;
            break;
        case 't':    /* windowOps */
            if (!set[0]) {
                break;
            }
            switch (args[0]) {
                case NUM4:  /* resize px */
                    if (set[1] && set[NUM2]) {
                        widget_schedule_resize(terminal->widget,
                                               args[NUM2], args[1]);
                    }
                    break;
                case NUM8:  /* resize ch */
                    if (set[1] && set[NUM2]) {
                        terminal_resize(terminal, args[NUM2], args[1]);
                    }
                    break;
                case NUM13: /* report position */
                    widget_get_allocation(terminal->widget, &allocation);
                    snprintf(response, MAX_RESPONSE, "\e[3;%d;%dt",
                        allocation.x, allocation.y);
                    terminal_write(terminal, response, strlen(response));
                    break;
                case NUM14: /* report px */
                    widget_get_allocation(terminal->widget, &allocation);
                    snprintf(response, MAX_RESPONSE, "\e[4;%d;%dt",
                        allocation.height, allocation.width);
                    terminal_write(terminal, response, strlen(response));
                    break;
                case NUM18: /* report ch */
                    snprintf(response, MAX_RESPONSE, "\e[9;%d;%dt",
                        terminal->height, terminal->width);
                    terminal_write(terminal, response, strlen(response));
                    break;
                case NUM21: /* report title */
                    snprintf(response, MAX_RESPONSE, "\e]l%s\e\\",
                        window_get_title(terminal->window));
                    terminal_write(terminal, response, strlen(response));
                    break;
                default:
                    if (args[0] >= NUM24) {
                        terminal_resize(terminal, terminal->width, args[0]);
                    } else {
                        fprintf(stderr, "Unimplemented windowOp %d\n", args[0]);
                    }
                    break;
            }
            break;
        case 'u':    /* Restore cursor location */
            terminal->row = terminal->saved_row;
            terminal->column = terminal->saved_column;
            break;
        default:
            fprintf(stderr, "Unknown CSI escape: %c\n", *p);
            break;
        }
}

static void handle_non_csi_escape(struct terminal *terminal, char code)
{
    switch (code) {
        case 'M':    /* RI - Reverse linefeed */
            terminal->row -= 1;
            if (terminal->row < terminal->margin_top) {
                terminal->row = terminal->margin_top;
                terminal_scroll(terminal, -1);
            }
            break;
        case 'E':    /* NEL - Newline */
            terminal->column = 0;
            // fallthrough
        case 'D':    /* IND - Linefeed */
            terminal->row += 1;
            if (terminal->row > terminal->margin_bottom) {
                terminal->row = terminal->margin_bottom;
                terminal_scroll(terminal, +1);
            }
            break;
        case 'c':    /* RIS - Reset */
            terminal_init(terminal);
            break;
        case 'H':    /* HTS - Set tab stop at current column */
            terminal->tab_ruler[terminal->column] = 1;
            break;
        case '7':    /* DECSC - Save current state */
            terminal->saved_row = terminal->row;
            terminal->saved_column = terminal->column;
            terminal->saved_attr = terminal->curr_attr;
            terminal->saved_origin_mode = terminal->origin_mode;
            terminal->saved_cs = terminal->cs;
            terminal->saved_g0 = terminal->g0;
            terminal->saved_g1 = terminal->g1;
        case '8':    /* DECRC - Restore state most recently saved by ESC 7 */
            terminal->row = terminal->saved_row;
            terminal->column = terminal->saved_column;
            terminal->curr_attr = terminal->saved_attr;
            terminal->origin_mode = terminal->saved_origin_mode;
            terminal->cs = terminal->saved_cs;
            terminal->g0 = terminal->saved_g0;
            terminal->g1 = terminal->saved_g1;
            break;
        case '=':    /* DECPAM - Set application keypad mode */
            terminal->key_mode = KM_APPLICATION;
            break;
        case '>':    /* DECPNM - Set numeric keypad mode */
            terminal->key_mode = KM_NORMAL;
            break;
        default:
            fprintf(stderr, "Unknown escape code: %c\n", code);
            break;
        }
}

static void handle_special_escape(struct terminal *terminal, char special, char code)
{
    int i, numChars;

    if (special == '#') {
        switch (code) {
            case '8':
                /* fill with 'E', no cheap way to do this */
                memset(terminal->data, 0, terminal->data_pitch * terminal->height);
                numChars = terminal->width * terminal->height;
                for (i = 0; i < numChars; i++) {
                    terminal->data[i].byte[0] = 'E';
                }
                break;
            default:
                fprintf(stderr, "Unknown HASH escape #%c\n", code);
                break;
        }
    } else if (special == '(' || special == ')') {
        switch (code) {
            case '0':
                if (special == '(') {
                    terminal->g0 = CS_SPECIAL;
                } else {
                    terminal->g1 = CS_SPECIAL;
                }
                break;
            case 'A':
                if (special == '(') {
                    terminal->g0 = CS_UK;
                } else {
                    terminal->g1 = CS_UK;
                }
                break;
            case 'B':
                if (special == '(') {
                    terminal->g0 = CS_US;
                } else {
                    terminal->g1 = CS_US;
                }
                break;
            default:
                fprintf(stderr, "Unknown character set %c\n", code);
                break;
        }
    } else {
        fprintf(stderr, "Unknown special escape %c%c\n", special, code);
    }
}

static void sgr_if(struct terminal *terminal, int dcode)
{
    int code = dcode;
    if (code >= NUM30 && code <= NUM37) {
        terminal->curr_attr.fg = code - NUM30;
        if (terminal->curr_attr.a & ATTRMASK_BOLD) {
            terminal->curr_attr.fg += NUM8;
        }
    } else if (code >= NUM40 && code <= NUM47) {
        terminal->curr_attr.bg = code - NUM40;
    } else if (code >= NUM90 && code <= NUM97) {
        terminal->curr_attr.fg = code - NUM90 + NUM8;
    } else if (code >= NUM100 && code <= NUM107) {
        terminal->curr_attr.bg = code - NUM100 + NUM8;
    } else if (code >= MAX_RESPONSE && code < NUM512) {
        terminal->curr_attr.fg = code - MAX_RESPONSE;
    } else if (code >= NUM512 && code < NUM768) {
        terminal->curr_attr.bg = code - NUM512;
    } else {
        fprintf(stderr, "Unknown SGR code: %d\n", code);
    }
}
static void sgr_case(struct terminal *terminal, int code)
{
    switch (code) {
        case NUM24:
            terminal->curr_attr.a &= ~ATTRMASK_UNDERLINE;
            break;
        case NUM25:
            terminal->curr_attr.a &= ~ATTRMASK_BLINK;
            break;
        case NUM7:
        case NUM26:
            terminal->curr_attr.a |= ATTRMASK_INVERSE;
            break;
        case NUM27:
            terminal->curr_attr.a &= ~ATTRMASK_INVERSE;
            break;
        case NUM28:
            terminal->curr_attr.a &= ~ATTRMASK_CONCEALED;
            break;
        default :
            break;
    }
}
static void handle_sgr(struct terminal *terminal, int code)
{
    if (code < NUM24 || code >NUM28) {
        switch (code) {
            case 0:
                terminal->curr_attr = terminal->color_scheme->default_attr;
                break;
            case 1:
                terminal->curr_attr.a |= ATTRMASK_BOLD;
                if (terminal->curr_attr.fg < NUM8) {
                    terminal->curr_attr.fg += NUM8;
                }
                break;
            case NUM4:
                terminal->curr_attr.a |= ATTRMASK_UNDERLINE;
                break;
            case NUM5:
                terminal->curr_attr.a |= ATTRMASK_BLINK;
                break;
            case NUM8:
                terminal->curr_attr.a |= ATTRMASK_CONCEALED;
                break;
            case NUM2:
            case NUM21:
            case NUM22:
                terminal->curr_attr.a &= ~ATTRMASK_BOLD;
                if (terminal->curr_attr.fg < NUM16 && terminal->curr_attr.fg >= NUM8) {
                    terminal->curr_attr.fg -= NUM8;
                }
                break;
            case NUM39:
                terminal->curr_attr.fg = terminal->color_scheme->default_attr.fg;
                break;
            case NUM49:
                terminal->curr_attr.bg = terminal->color_scheme->default_attr.bg;
                break;
            default:
                sgr_if(terminal, code);
                break;
        }
    } else {
        sgr_case(terminal, code);
    }
}

/* Returns 1 if c was special, otherwise 0 */
static void handle_while(struct terminal *terminal, union utf8_char *row)
{
    union utf8_char *roww;
    roww = row;
    while (terminal->column < terminal->width) {
        if (terminal->mode & MODE_IRM) {
            terminal_shift_line(terminal, +1);
        }

        if (roww[terminal->column].byte[0] == '\0') {
            roww[terminal->column].byte[0] = ' ';
            roww[terminal->column].byte[1] = '\0';
            attr_row[terminal->column] = terminal->curr_attr;
        }
        terminal->column++;
        if (terminal->tab_ruler[terminal->column]) {
            break;
        }
    }
}
/* Returns 1 if c was special, otherwise 0 */
static int handle_special_char(struct terminal *terminal, char c)
{
    union utf8_char *row;
    struct attr *attr_row;

    row = terminal_get_row(terminal, terminal->row);
    attr_row = terminal_get_attr_row(terminal, terminal->row);

    switch (c) {
        case '\r':
            terminal->column = 0;
        case '\n':
            if (terminal->mode & MODE_LF_NEWLINE) {
                terminal->column = 0;
            }
            /* fallthrough */
        case '\v':
        case '\f':
            terminal->row++;
            if (terminal->row > terminal->margin_bottom) {
                terminal->row = terminal->margin_bottom;
                terminal_scroll(terminal, +1);
            }

        case '\t':
            handle_while(terminal, row);
            if (terminal->column >= terminal->width) {
                terminal->column = terminal->width - 1;
            }
        case '\b':
            if (terminal->column >= terminal->width) {
                terminal->column = terminal->width - NUM2;
            } else if (terminal->column > 0) {
                terminal->column--;
            } else if (terminal->mode & MODE_AUTOWRAP) {
                terminal->column = terminal->width - 1;
                terminal->row -= 1;
                if (terminal->row < terminal->margin_top) {
                    terminal->row = terminal->margin_top;
                    terminal_scroll(terminal, -1);
                }
            }
        case '\a':
            /* Bell */
            break;
        case '\x0E': /* SO */
            terminal->cs = terminal->g1;
            break;
        case '\x0F': /* SI */
            terminal->cs = terminal->g0;
            break;
        case '\0':
            break;
        default:
            return 0;
    }
    return 1;
}

static void handle_char(struct terminal *terminal, union utf8_char utf8)
{
    union utf8_char *row;
    struct attr *attr_row;

    if (handle_special_char(terminal, utf8.byte[0])) {
        return;
    }

    apply_char_set(terminal->cs, &utf8);

    /* There are a whole lot of non-characters, control codes,
     * and formatting codes that should probably be ignored,
     * for example: */
    if (strncmp((char*) utf8.byte, "\xEF\xBB\xBF", NUM3) == 0) {
        /* BOM, ignore */
        return;
    }

    /* Some of these non-characters should be translated, e.g.: */
    if (utf8.byte[0] < NUM32) {
        utf8.byte[0] = utf8.byte[0] + NUM64;
    }

    /* handle right margin effects */
    if (terminal->column >= terminal->width) {
        if (terminal->mode & MODE_AUTOWRAP) {
            terminal->column = 0;
            terminal->row += 1;
            if (terminal->row > terminal->margin_bottom) {
                terminal->row = terminal->margin_bottom;
                terminal_scroll(terminal, +1);
            }
        } else {
            terminal->column--;
        }
    }

    row = terminal_get_row(terminal, terminal->row);
    attr_row = terminal_get_attr_row(terminal, terminal->row);

    if (terminal->mode & MODE_IRM) {
        terminal_shift_line(terminal, +1);
    }
    row[terminal->column] = utf8;
    attr_row[terminal->column++] = terminal->curr_attr;

    if (terminal->row + terminal->start + 1 > terminal->end) {
        terminal->end = terminal->row + terminal->start + 1;
    }
    if (terminal->end == terminal->buffer_height) {
        terminal->log_size = terminal->buffer_height;
    } else if (terminal->log_size < terminal->buffer_height) {
        terminal->log_size = terminal->end;
    }

    /* cursor jump for wide character. */
    if (is_wide(utf8)) {
        row[terminal->column++].ch = 0x200B; /* space glyph */
    }

    if (utf8.ch != terminal->last_char.ch) {
        terminal->last_char = utf8;
    }
}

static void escape_append_utf8(struct terminal *terminal, union utf8_char utf8)
{
    int len, i;

    if ((utf8.byte[0] & 0x80) == 0x00) {
        len = 1;
    } else if ((utf8.byte[0] & 0xE0) == 0xC0) {
        len = NUM2;
    } else if ((utf8.byte[0] & 0xF0) == 0xE0) {
        len = NUM3;
    } else if ((utf8.byte[0] & 0xF8) == 0xF0) {
        len = NUM4;
    } else {
        len = 1;  /* Invalid, cannot happen */
    }
    if (terminal->escape_length + len <= MAX_ESCAPE) {
        for (i = 0; i < len; i++) {
            terminal->escape[terminal->escape_length + i] = utf8.byte[i];
        }
        terminal->escape_length += len;
    } else if (terminal->escape_length < MAX_ESCAPE) {
        terminal->escape[terminal->escape_length++] = 0;
    }
}
void case_escape(struct terminal *terminal, union utf8_char *utf8)
{
    if (handle_special_char(terminal, utf8.byte[0]) != 0) {
        /* do nothing */
    } else if (utf8.byte[0] == '?') {
        terminal->escape_flags |= ESC_FLAG_WHAT;
    } else if (utf8.byte[0] == '>') {
        terminal->escape_flags |= ESC_FLAG_GT;
    } else if (utf8.byte[0] == '!') {
        terminal->escape_flags |= ESC_FLAG_BANG;
    } else if (utf8.byte[0] == '$') {
        terminal->escape_flags |= ESC_FLAG_CASH;
    } else if (utf8.byte[0] == '\'') {
        terminal->escape_flags |= ESC_FLAG_SQUOTE;
    } else if (utf8.byte[0] == '"') {
        terminal->escape_flags |= ESC_FLAG_DQUOTE;
    } else if (utf8.byte[0] == ' ') {
        terminal->escape_flags |= ESC_FLAG_SPACE;
    } else {
        escape_append_utf8(terminal, utf8);
        if (terminal->escape_length >= MAX_ESCAPE) {
            terminal->state = escape_state_normal;
        }
    }
    if (isalpha(utf8.byte[0]) || utf8.byte[0] == '@' ||
        utf8.byte[0] == '`') {
        terminal->state = escape_state_normal;
        handle_escape(terminal);
    } else {
    }
    return ;
}
void case_inner_escape(struct terminal *terminal, union utf8_char *utf8)
{
    if (utf8.byte[0] == '\\') {
        terminal->state = escape_state_normal;
        if (terminal->outer_state == escape_state_dcs) {
            handle_dcs(terminal);
        } else if (terminal->outer_state == escape_state_osc) {
            handle_osc(terminal);
        }
    } else if (utf8.byte[0] == '\e') {
        terminal->state = terminal->outer_state;
        escape_append_utf8(terminal, utf8);
        if (terminal->escape_length >= MAX_ESCAPE) {
            terminal->state = escape_state_normal;
        }
    } else {
        terminal->state = terminal->outer_state;
        if (terminal->escape_length < MAX_ESCAPE) {
            terminal->escape[terminal->escape_length++] = '\e';
        }
        escape_append_utf8(terminal, utf8);
        if (terminal->escape_length >= MAX_ESCAPE) {
            terminal->state = escape_state_normal;
        }
    }
}
void case_ignore(struct terminal *terminal, union utf8_char *utf8)
{
    if (utf8.byte[0] == '\e') {
        terminal->outer_state = terminal->state;
        terminal->state = escape_state_inner_escape;
    } else if (utf8.byte[0] == '\a' && terminal->state == escape_state_osc) {
        terminal->state = escape_state_normal;
        handle_osc(terminal);
    } else {
        escape_append_utf8(terminal, utf8);
        if (terminal->escape_length >= MAX_ESCAPE) {
            terminal->state = escape_state_normal;
        }
    }
    return ;
}
void terminal_data_switch1(union utf8_char utf8, struct terminal *terminal)
{
    switch (utf8.byte[0]) {
        case 'P':  /* DCS */
            terminal->state = escape_state_dcs;
            break;
        case '[':  /* CSI */
            terminal->state = escape_state_csi;
            break;
        case ']':  /* OSC */
            terminal->state = escape_state_osc;
            break;
        case '#':
        case '(':
        case ')':  /* special */
            terminal->state = escape_state_special;
            break;
        case '^':  /* PM (not implemented) */
        case '_':  /* APC (not implemented) */
            terminal->state = escape_state_ignore;
            break;
        default:
            terminal->state = escape_state_normal;
            handle_non_csi_escape(terminal, utf8.byte[0]);
            break;
    }
}
void terminal_data_if(union utf8_char utf8, struct terminal *terminal)
{
    if (utf8.byte[0] == '\e') {
        terminal->state = escape_state_escape;
        terminal->outer_state = escape_state_normal;
        terminal->escape[0] = '\e';
        terminal->escape_length = 1;
        terminal->escape_flags = 0;
    } else {
        handle_char(terminal, utf8);
    } /* if */
}
static void terminal_data(struct terminal *terminal, const char *data, size_t length)
{
    unsigned int i;
    union utf8_char utf8;
    enum utf8_state parser_state;

    for (i = 0; i < length; i++) {
        parser_state =
            utf8_next_char(&terminal->state_machine, data[i]);
        switch (parser_state) {
            case utf8state_accept:
                utf8.ch = terminal->state_machine.s.ch;
                break;
            case utf8state_reject:
                /* the unicode replacement character */
                utf8.byte[0] = 0xEF;
                utf8.byte[1] = 0xBF;
                utf8.byte[2] = 0xBD;
                utf8.byte[3] = 0x00;
                break;
            default:
                continue;
        }

        /* assume escape codes never use non-ASCII characters */
        switch (terminal->state) {
            case escape_state_escape:
                escape_append_utf8(terminal, utf8);
                terminal_data_switch1(utf8, terminal);
                continue;
            case escape_state_csi:
                escape_append_utf8(terminal, &utf8);
                continue;
            case escape_state_inner_escape:
                case_inner_escape(terminal, &utf8);
                continue;
            case escape_state_dcs:
            case escape_state_osc:
            case escape_state_ignore:
                case_ignore(terminal, &utf8);
                continue;
            case escape_state_special:
                escape_append_utf8(terminal, utf8);
                terminal->state = escape_state_normal;
                if (isdigit(utf8.byte[0]) || isalpha(utf8.byte[0])) {
                    handle_special_escape(terminal, terminal->escape[1],
                                        utf8.byte[0]);
                }
                continue;
            default:
                break;
        }

        /* this is valid, because ASCII characters are never used to
         * introduce a multibyte sequence in UTF-8 */
        terminal_data_if(utf8, terminal);
    } /* for */

    window_schedule_redraw(terminal->window);
}

static void data_source_target(void data[], struct wl_data_source *source, const char *mime_type)
{
    int ret = fprintf(stderr, "data_source_target, %s\n", mime_type);
    if (ret < 0) {
        printf("fprintf error");
    }
}

static void data_source_send(void data[],
                             struct wl_data_source *source,
                             const char *mime_type, int32_t fd)
{
    struct terminal *terminal = data;

    terminal_send_selection(terminal, fd);
}

static void data_source_cancelled(void data[], struct wl_data_source *source)
{
    wl_data_source_destroy(source);
}

static void data_source_dnd_drop_performed(void data[], struct wl_data_source *source)
{
}

static void data_source_dnd_finished(void data[], struct wl_data_source *source)
{
}

static void data_source_action(void data[],
                               struct wl_data_source *source, uint32_t dnd_action)
{
}

static const struct wl_data_source_listener data_source_listener = {
    data_source_target,
    data_source_send,
    data_source_cancelled,
    data_source_dnd_drop_performed,
    data_source_dnd_finished,
    data_source_action
};

static const char text_mime_type[] = "text/plain;charset=utf-8";

static void data_handler(struct input *input,
                         const char **types)
{
    int i, has_text = 0;

    if (!types) {
        return;
    }
    for (i = 0; types[i]; i++) {
        if (strcmp(types[i], text_mime_type) == 0) {
            has_text = 1;
        }
    }
    if (!has_text) {
        input_accept(input, NULL);
    } else {
        input_accept(input, text_mime_type);
    }
}

static void drop_handler(struct window *window, struct input *input,
                         int32_t x, int32_t y, void data[])
{
    struct terminal *terminal = data;

    input_receive_drag_data_to_fd(input, text_mime_type, terminal->master);
}

static void fullscreen_handler(struct window *window, void data[])
{
    struct terminal *terminal = data;

    window_set_fullscreen(window, !window_is_fullscreen(terminal->window));
}

static void close_handler(void data[])
{
    struct terminal *terminal = data;

    terminal_destroy(terminal);
}

static void terminal_copy(struct terminal *terminal, struct input *input)
{
    terminal->selection =
        display_create_data_source(terminal->display);
    if (!terminal->selection) {
        return;
    }

    wl_data_source_offer(terminal->selection,
                         "text/plain;charset=utf-8");
    wl_data_source_add_listener(terminal->selection,
                                &data_source_listener, terminal);
    input_set_selection(input, terminal->selection,
                        display_get_serial(terminal->display));
}

static void terminal_paste(struct terminal *terminal, struct input *input)
{
    input_receive_selection_data_to_fd(input,
                                       "text/plain;charset=utf-8",
                                       terminal->master);
}

static void terminal_new_instance(struct terminal *terminal)
{
    struct terminal *new_terminal;

    new_terminal = terminal_create(terminal->display);
    if (terminal_run(new_terminal, option_shell)) {
        terminal_destroy(new_terminal);
    }
}

static int handle_bound_key(struct terminal *terminal,
                            struct input *input, uint32_t sym, uint32_t time)
{
    switch (sym) {
        case XKB_KEY_X:
            /* Cut selection; terminal doesn't do cut, fall
             * through to copy. */
        case XKB_KEY_C:
            terminal_copy(terminal, input);
            return 1;
        case XKB_KEY_V:
            terminal_paste(terminal, input);
            return 1;
        case XKB_KEY_N:
            terminal_new_instance(terminal);
            return 1;

        case XKB_KEY_Up:
            if (!terminal->scrolling) {
                terminal->saved_start = terminal->start;
            }
            if (terminal->start == terminal->end - terminal->log_size) {
                return 1;
            }

            terminal->scrolling = 1;
            terminal->start--;
            terminal->row++;
            terminal->selection_start_row++;
            terminal->selection_end_row++;
            widget_schedule_redraw(terminal->widget);
            return 1;

        case XKB_KEY_Down:
            if (!terminal->scrolling) {
                terminal->saved_start = terminal->start;
            }

            if (terminal->start == terminal->saved_start) {
                return 1;
            }

            terminal->scrolling = 1;
            terminal->start++;
            terminal->row--;
            terminal->selection_start_row--;
            terminal->selection_end_row--;
            widget_schedule_redraw(terminal->widget);
            return 1;

        default:
            return 0;
    }
}
void tsSwitch(uint32_t *tsym)
{
    switch (tsym) {
        case XKB_KEY_KP_Space:
            *tsym = XKB_KEY_space;
            break;
        case XKB_KEY_KP_Tab:
            *tsym = XKB_KEY_Tab;
            break;
        case XKB_KEY_KP_Enter:
            *tsym = XKB_KEY_Return;
            break;
        case XKB_KEY_KP_Left:
            *tsym = XKB_KEY_Left;
            break;
        case XKB_KEY_KP_Up:
            *tsym = XKB_KEY_Up;
            break;
        case XKB_KEY_KP_Right:
            *tsym = XKB_KEY_Right;
        case XKB_KEY_KP_Down:
            *tsym = XKB_KEY_Down;
        case XKB_KEY_KP_Equal:
            *tsym = XKB_KEY_equal;
        case XKB_KEY_KP_Multiply:
            *tsym = XKB_KEY_asterisk;
        case XKB_KEY_KP_Add:
            *tsym = XKB_KEY_plus;
        case XKB_KEY_KP_Separator:
            *tsym = XKB_KEY_period;
        case XKB_KEY_KP_Subtract:
            *tsym = XKB_KEY_minus;
        case XKB_KEY_KP_Decimal:
            *tsym = XKB_KEY_period;
        case XKB_KEY_KP_Divide:
            *tsym = XKB_KEY_slash;
        case XKB_KEY_KP_0:
        case XKB_KEY_KP_1:
        case XKB_KEY_KP_2:
        case XKB_KEY_KP_3:
        case XKB_KEY_KP_4:
        case XKB_KEY_KP_5:
        case XKB_KEY_KP_6:
        case XKB_KEY_KP_7:
        case XKB_KEY_KP_8:
        case XKB_KEY_KP_9:
            *tsym = (tsym - XKB_KEY_KP_0) + XKB_KEY_0;
            break;
        default:
            break;
    }
    return ;
}
void defa_block(int *len, uint32_t *tsym, char *ch)
{
    bool convert_utf8 = true;
    int ret;
    *len = apply_key_map(terminal->key_mode, tsym, modifiers, ch);
    if (len != 0) {
        return ;
    }
    if (modifiers & MOD_CONTROL_MASK) {
        if (tsym >= '3' && tsym <= '7') {
            *tsym = (tsym & 0x1f) + NUM8;
        }

        if (!((tsym >= '!' && tsym <= '/') ||
            (tsym >= '8' && tsym <= '?') ||
            (tsym >= '0' && tsym <= '2'))) {
            *tsym = tsym & 0x1f;
        } else if (tsym == '2') {
            *tsym = 0x00;
        } else if (tsym == '/') {
            *tsym = 0x1F;
        } else if (tsym == '8' || tsym == '?') {
            *tsym = 0x7F;
        }
    }
    if (modifiers & MOD_ALT_MASK) {
        if (terminal->mode & MODE_ALT_SENDS_ESC) {
            ch[len++] = 0x1b;
        } else {
            *tsym = tsym | 0x80;
            convert_utf8 = false;
        }
    }

    if ((tsym < NUM128) ||
        (!convert_utf8 && tsym < MAX_RESPONSE)) {
        ch[len++] = tsym;
    } else {
        ret = xkb_keysym_to_utf8(tsym, ch + len,
                                 MAX_RESPONSE - len);
        if (ret < 0) {
            fprintf(stderr, "Warning: buffer too small to encode "
                    "UTF8 character\n");
        } else {
            *len += ret;
        }
    }
}
void stateJudge(enum wl_keyboard_key_state state, int len, struct terminal *terminal, int *d,
                char ch[MAX_RESPONSE], struct input *input)
{
    uint32_t  serial;
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && len > 0) {
        if (terminal->scrolling) {
            d = terminal->saved_start - terminal->start;
            terminal->row -= d;
            terminal->selection_start_row -= d;
            terminal->selection_end_row -= d;
            terminal->start = terminal->saved_start;
            terminal->scrolling = 0;
            widget_schedule_redraw(terminal->widget);
        }

        terminal_write(terminal, ch, len);

        /* Hide cursor, except if this was coming from a
         * repeating key press. */
        serial = display_get_serial(terminal->display);
        if (terminal->hide_cursor_serial != serial) {
            input_set_pointer_image(input, CURSOR_BLANK);
            terminal->hide_cursor_serial = serial;
        }
    }
}
void case_incert(uint32_t *tsym, int *len)
{
    switch (tsym) {
        case XKB_KEY_Insert:
            len = function_key_response('[', NUM2, modifiers, '~', ch);
        case XKB_KEY_Delete:
            if (terminal->mode & MODE_DELETE_SENDS_DEL) {
                ch[len++] = '\x04';
            } else {
                len = function_key_response('[', NUM3, modifiers, '~', ch);
            }
        case XKB_KEY_Page_Up:
            *len = function_key_response('[', NUM5, modifiers, '~', ch);
        case XKB_KEY_Page_Down:
            *len = function_key_response('[', NUM6, modifiers, '~', ch);
        case XKB_KEY_F1:
            *len = function_key_response('O', 1, modifiers, 'P', ch);
        case XKB_KEY_F2:
            *len = function_key_response('O', 1, modifiers, 'Q', ch);
        case XKB_KEY_F3:
            *len = function_key_response('O', 1, modifiers, 'R', ch);
        case XKB_KEY_F4:
            *len = function_key_response('O', 1, modifiers, 'S', ch);
        case XKB_KEY_F5:
            *len = function_key_response('[', NUM15, modifiers, '~', ch);
        case XKB_KEY_F6:
            *len = function_key_response('[', NUM17, modifiers, '~', ch);
        case XKB_KEY_F7:
            *len = function_key_response('[', NUM18, modifiers, '~', ch);
        case XKB_KEY_F8:
            *len = function_key_response('[', NUM19, modifiers, '~', ch);
        case XKB_KEY_F9:
            *len = function_key_response('[', NUM20, modifiers, '~', ch);
        case XKB_KEY_F10:
            *len = function_key_response('[', NUM21, modifiers, '~', ch);
        case XKB_KEY_F12:
            *len = function_key_response('[', NUM24, modifiers, '~', ch);
        default:
            defa_block(&len, &tsym, &ch);
            break;
    }
}
static void key_handler(struct window *window, struct input *input, uint32_t time,
                        uint32_t key, uint32_t sym, enum wl_keyboard_key_state state,
                        void data[])
{
    struct terminal *terminal = data;
    char ch[MAX_RESPONSE];
    uint32_t modifiers, serial tsym = sym;
    int ret, len = 0, d;
    modifiers = input_get_modifiers(input);
    if ((modifiers & MOD_CONTROL_MASK) && (modifiers & MOD_SHIFT_MASK) &&
        state == WL_KEYBOARD_KEY_STATE_PRESSED && handle_bound_key(terminal, input, tsym, time)) {
        return;
    }
    /* Map keypad symbols to 'normal' equivalents before processing */
    tsSwitch(&tsym);
    switch (tsym) {
        case XKB_KEY_BackSpace:
            if (modifiers & MOD_ALT_MASK) {
                ch[len++] = 0x1b;
            }
            ch[len++] = 0x7f;
        case XKB_KEY_Tab:
        case XKB_KEY_Linefeed:
        case XKB_KEY_Clear:
        case XKB_KEY_Pause:
        case XKB_KEY_Scroll_Lock:
        case XKB_KEY_Sys_Req:
        case XKB_KEY_Escape:
            ch[len++] = tsym & 0x7f;
        case XKB_KEY_Return:
            if (terminal->mode & MODE_LF_NEWLINE) {
                ch[len++] = 0x0D;
                ch[len++] = 0x0A;
            } else {
                ch[len++] = 0x0D;
            }
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:
        case XKB_KEY_Hyper_L:
        case XKB_KEY_Hyper_R:
            break;
        default:
            break;
    }
    case_incert(&tsym, &len)
    stateJudge(state, len, terminal, &d, ch[MAX_RESPONSE], input);
}

static void keyboard_focus_handler(struct window *window,
                                   struct input *device, void data[])
{
    struct terminal *terminal = data;

    window_schedule_redraw(terminal->window);
}

static int wordsep(int ch)
{
    const char extra[] = "-,./?%&#:_=+@~";

    if (ch > NUM127 || ch < 0) {
        return 1;
    }

    return ch == 0 || !(isalpha(ch) || isdigit(ch) || strchr(extra, ch));
}

static int recompute_selection(struct terminal *terminal)
{
    struct rectangle allocation;
    int col, x, width, height;
    int start_row, end_row;
    int word_start, eol;
    int side_margin, top_margin;
    int start_x, end_x;
    int cw, ch;
    union utf8_char *data = NULL;

    cw = terminal->average_width;
    ch = terminal->extents.height;
    widget_get_allocation(terminal->widget, &allocation);
    width = terminal->width * cw;
    height = terminal->height * ch;
    side_margin = allocation.x + (allocation.width - width) / NUM2;
    top_margin = allocation.y + (allocation.height - height) / NUM2;

    start_row = (terminal->selection_start_y - top_margin + ch) / ch - 1;
    end_row = (terminal->selection_end_y - top_margin + ch) / ch - 1;

    if (start_row < end_row ||
        (start_row == end_row &&
         terminal->selection_start_x < terminal->selection_end_x)) {
        terminal->selection_start_row = start_row;
        terminal->selection_end_row = end_row;
        start_x = terminal->selection_start_x;
        end_x = terminal->selection_end_x;
    } else {
        terminal->selection_start_row = end_row;
        terminal->selection_end_row = start_row;
        start_x = terminal->selection_end_x;
        end_x = terminal->selection_start_x;
    }

    eol = 0;
    if (terminal->selection_start_row < 0) {
        terminal->selection_start_row = 0;
        terminal->selection_start_col = 0;
    } else {
        x = side_margin + cw / NUM2;
        data = terminal_get_row(terminal,
                    terminal->selection_start_row);
        word_start = 0;
        for (col = 0; col < terminal->width; col++, x += cw) {
            if (col == 0 || wordsep(data[col - 1].ch)) {
                word_start = col;
            }
            if (data[col].ch != 0) {
                eol = col + 1;
            }
            if (start_x < x) {
                break;
            }
        }

        switch (terminal->dragging) {
            case SELECT_LINE:
                terminal->selection_start_col = 0;
                break;
            case SELECT_WORD:
                terminal->selection_start_col = word_start;
                break;
            case SELECT_CHAR:
                terminal->selection_start_col = col;
                break;
            default :
                break;
        }
    }

    if (terminal->selection_end_row >= terminal->height) {
        terminal->selection_end_row = terminal->height;
        terminal->selection_end_col = 0;
    } else {
        x = side_margin + cw / NUM2;
        data = terminal_get_row(terminal, terminal->selection_end_row);
        for (col = 0; col < terminal->width; col++, x += cw) {
            if (terminal->dragging == SELECT_CHAR && end_x < x) {
                break;
            }
            if (terminal->dragging == SELECT_WORD &&
                end_x < x && wordsep(data[col].ch)) {
                break;
            }
        }
        terminal->selection_end_col = col;
    }

    if (terminal->selection_end_col != terminal->selection_start_col ||
        terminal->selection_start_row != terminal->selection_end_row) {
        col = terminal->selection_end_col;
        if (col > 0 && data[col - 1].ch == 0) {
            terminal->selection_end_col = terminal->width;
        }
        data = terminal_get_row(terminal, terminal->selection_start_row);
        if (data[terminal->selection_start_col].ch == 0) {
            terminal->selection_start_col = eol;
        }
    }

    return 1;
}

static void terminal_minimize(struct terminal *terminal)
{
    window_set_minimized(terminal->window);
}

static void menu_func(void data[], struct input *input, int index)
{
    struct window *window = data;
    struct terminal *terminal = window_get_user_data(window);

    int ret = fprintf(stderr, "picked entry %d\n", index);
    if (ret < 0) {
        printf("fprintf error");
    }

    switch (index) {
        case 0:
            terminal_new_instance(terminal);
            break;
        case 1:
            terminal_copy(terminal, input);
            break;
        case NUM2:
            terminal_paste(terminal, input);
            break;
        case NUM3:
            terminal_minimize(terminal);
            break;
        default :
            break;
    }
}

static void show_menu(struct terminal *terminal, struct input *input, uint32_t time)
{
    int32_t x, y;
    static const char *entries[] = {
        "Open Terminal", "Copy", "Paste", "Minimize"
    };

    input_get_position(input, &x, &y);
    window_show_menu(terminal->display, input, time, terminal->window,
                     x - NUM10, y - NUM10, menu_func,
                     entries, ARRAY_LENGTH(entries));
}

static void click_handler(struct widget *widget, struct terminal *terminal,
                          int32_t x, int32_t y,
                          uint32_t time)
{
    if (time - terminal->click_time < NUM500) {
        terminal->click_count++;
    } else {
        terminal->click_count = 1;
    }

    terminal->click_time = time;
    terminal->dragging = (terminal->click_count - 1) % NUM3 + SELECT_CHAR;

    terminal->selection_end_x = terminal->selection_start_x = x;
    terminal->selection_end_y = terminal->selection_start_y = y;
    if (recompute_selection(terminal)) {
            widget_schedule_redraw(widget);
    }
}
uint32_t *button_tb(uint32_t time,
                    uint32_t button,)
{
    uint32_t p[2];
    p[0] = time;
    p[1] = button;
    return p;
}
static void button_handler(struct widget *widget,
                           struct input *input, uint32_t *p,
                           enum wl_pointer_button_state state, void data[])
{
    struct terminal *terminal = data;
    int32_t x, y;
    uint32_t *w;
    w = p;
    switch (w[1]) {
        case BTN_LEFT:
            if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
                input_get_position(input, &x, &y);
                click_handler(widget, terminal, input, x, y, w[0]);
            } else {
                terminal->dragging = SELECT_NONE;
            }
            break;

        case BTN_RIGHT:
            if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
                show_menu(terminal, input, w[0]);
            }
            break;
        default :
            break;
    }
}

static int enter_handler(struct widget *widget,
                         struct input *input, float x, float y, void data[])
{
    return CURSOR_IBEAM;
}

static int motion_handler(struct widget *widget,
                          struct input *input,
                          void data[])
{
    struct terminal *terminal = data;

    if (terminal->dragging) {
        input_get_position(input,
                   &terminal->selection_end_x,
                   &terminal->selection_end_y);

        if (recompute_selection(terminal)) {
            widget_schedule_redraw(widget);
        }
    }

    return CURSOR_IBEAM;
}

/* This magnitude is chosen rather arbitrarily. Really, the scrolling
 * should happen on a (fractional) pixel basis, not a line basis. */
#define AXIS_UNITS_PER_LINE 256

static void axis_handler(struct widget *widget,
                         uint32_t axis,
                         wl_fixed_t value,
                         void data[])
{
    struct terminal *terminal = data;
    int lines;

    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) {
        return;
    }

    terminal->smooth_scroll += value;
    lines = terminal->smooth_scroll / AXIS_UNITS_PER_LINE;
    terminal->smooth_scroll -= lines * AXIS_UNITS_PER_LINE;

    if (lines > 0) {
        if (terminal->scrolling) {
            if ((uint32_t)lines > terminal->saved_start - terminal->start) {
                lines = terminal->saved_start - terminal->start;
            }
        } else {
            lines = 0;
        }
    } else if (lines < 0) {
        uint32_t neg_lines = -lines;

        if (neg_lines > terminal->log_size + terminal->start - terminal->end) {
            lines = terminal->end - terminal->log_size - terminal->start;
        }
    }

    if (lines) {
        if (!terminal->scrolling) {
            terminal->saved_start = terminal->start;
        }
        terminal->scrolling = 1;

        terminal->start += lines;
        terminal->row -= lines;
        terminal->selection_start_row -= lines;
        terminal->selection_end_row -= lines;

        widget_schedule_redraw(widget);
    }
}

static void output_handler(struct window *window, struct output *output, int enter,
                           void data[])
{
    if (enter) {
        window_set_buffer_transform(window, output_get_transform(output));
    }
    window_set_buffer_scale(window, window_get_output_scale(window));
    window_schedule_redraw(window);
}
struct touchti {
    uint32_t time;
    int32_t id;
    float x;
    float y;
};
struct touchti timeidxy(uint32_t time, int32_t id,
                        float x, float y)
{
    struct touchti touchidxy;
    touchidxy.time = time;
    touchidxy.id = id;
    touchidxy.x = x;
    touchidxy.y = y;
    return touchidxy;
}
static void touch_down_handler(struct widget *widget, struct input *input,
                               struct touchti touchtdxy, void data[])
{
    struct terminal *terminal = data;
    struct touchti touchidxy = touchtdxy;
    if (id == 0) {
        click_handler(widget, terminal, input, x, y, time);
    }
}

static void touch_up_handler(int32_t id, void data[])
{
    struct terminal *terminal = data;

    if (id == 0) {
        terminal->dragging = SELECT_NONE;
    }
}

static void touch_motion_handler(struct widget *widget,
                                 int32_t id, float x, float y, void data[])
{
    struct terminal *terminal = data;

    if (terminal->dragging &&
        id == 0) {
        terminal->selection_end_x = (int)x;
        terminal->selection_end_y = (int)y;

        if (recompute_selection(terminal)) {
            widget_schedule_redraw(widget);
        }
    }
}

#ifndef howmany
static double howmany(int x, int y)
{
    return (((x) + ((y) - 1)) / (y));
}

#endif
void progressTer(struct terminal *terminal)
{
    window_set_user_data(terminal->window, terminal);
    window_set_key_handler(terminal->window, key_handler);
    window_set_keyboard_focus_handler(terminal->window,
                                      keyboard_focus_handler);
    window_set_fullscreen_handler(terminal->window, fullscreen_handler);
    window_set_output_handler(terminal->window, output_handler);
    window_set_close_handler(terminal->window, close_handler);
    window_set_state_changed_handler(terminal->window, state_changed_handler);

    window_set_data_handler(terminal->window, data_handler);
    window_set_drop_handler(terminal->window, drop_handler);

    widget_set_redraw_handler(terminal->widget, redraw_handler);
    widget_set_resize_handler(terminal->widget, resize_handler);
    widget_set_button_handler(terminal->widget, button_handler);
    widget_set_enter_handler(terminal->widget, enter_handler);
    widget_set_motion_handler(terminal->widget, motion_handler);
    widget_set_axis_handler(terminal->widget, axis_handler);
    widget_set_touch_up_handler(terminal->widget, touch_up_handler);
    widget_set_touch_down_handler(terminal->widget, touch_down_handler);
    widget_set_touch_motion_handler(terminal->widget, touch_motion_handler);
    return ;
}
static struct terminal *
terminal_create(struct display *display)
{
    struct terminal *terminal;
    cairo_surface_t *surface;
    cairo_t *cr;
    cairo_text_extents_t text_extents;

    terminal = xzalloc(sizeof *terminal);
    terminal->color_scheme = &DEFAULT_COLORS;
    terminal_init(terminal);
    terminal->margin_top = 0;
    terminal->margin_bottom = -1;
    terminal->window = window_create(display);
    terminal->widget = window_frame_create(terminal->window, terminal);
    terminal->title = xstrdup("Wayland Terminal");
    window_set_title(terminal->window, terminal->title);
    widget_set_transparent(terminal->widget, 0);

    init_state_machine(&terminal->state_machine);
    init_color_table(terminal);

    terminal->display = display;
    terminal->margin = NUM5;
    terminal->buffer_height = 1024;  /* height NUM 1024 */
    terminal->end = 1;
    progressTer(terminal);

    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
    cr = cairo_create(surface);
    cairo_set_font_size(cr, option_font_size);
    cairo_select_font_face (cr, option_font, CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_BOLD);
    terminal->font_bold = cairo_get_scaled_font (cr);
    cairo_scaled_font_reference(terminal->font_bold);

    cairo_select_font_face (cr, option_font, CAIRO_FONT_SLANT_NORMAL,
                            CAIRO_FONT_WEIGHT_NORMAL);
    terminal->font_normal = cairo_get_scaled_font (cr);
    cairo_scaled_font_reference(terminal->font_normal);

    cairo_font_extents(cr, &terminal->extents);

    /* Compute the average ascii glyph width */
    cairo_text_extents(cr, TERMINAL_DRAW_SINGLE_WIDE_CHARACTERS, &text_extents);
    terminal->average_width = howmany(text_extents.width,
                                      strlen(TERMINAL_DRAW_SINGLE_WIDE_CHARACTERS));
    terminal->average_width = ceil(terminal->average_width);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    terminal_resize(terminal, NUM20, NUM5); /* Set minimum size first */
    terminal_resize(terminal, NUM80, NUM25);

    wl_list_insert(terminal_list.prev, &terminal->link);
    return terminal;
}

static void terminal_destroy(struct terminal *terminal)
{
    display_unwatch_fd(terminal->display, terminal->master);
    window_destroy(terminal->window);
    close(terminal->master);
    wl_list_remove(&terminal->link);

    if (wl_list_empty(&terminal_list)) {
        display_exit(terminal->display);
    }
    free(terminal->title);
    free(terminal);
}

static void io_handler(struct task *task, uint32_t events)
{
    struct terminal *terminal =
        container_of(task, struct terminal, io_task);
    char buffer[256];
    int len;

    if (events & EPOLLHUP) {
        terminal_destroy(terminal);
        return;
    }

    len = read(terminal->master, buffer, sizeof buffer);
    if (len < 0) {
        terminal_destroy(terminal);
    } else {
        terminal_data(terminal, buffer, len);
    }
}

static int terminal_run(struct terminal *terminal, const char *path)
{
    int master;
    pid_t pid;
    int pipes[2];

    /* Awkwardness: There's a sticky race condition here.  If
     * anything prints after the forkpty() but before the window has
     * a size then we'll segfault.  So we make a pipe and wait on
     * it before actually exec()ing the terminal.  The resize
     * handler closes it in the parent process and the child continues
     * on to launch a shell.
     *
     * The reason we don't just do terminal_run() after the window
     * has a size is that we'd prefer to perform the fork() before
     * the process opens a wayland connection.
     */
    if (pipe(pipes) == -1) {
        fprintf(stderr, "Can't create pipe for pacing.\n");
        exit(EXIT_FAILURE);
    }

    pid = forkpty(&master, NULL, NULL, NULL);
    if (pid == 0) {
        int ret;

        close(pipes[1]);
        do {
            char tmp;
            ret = read(pipes[0], &tmp, 1);
        } while (ret == -1 && errno == EINTR);
        close(pipes[0]);
        setenv("TERM", option_term, 1);
        setenv("COLORTERM", option_term, 1);
        if (execl(path, path, NULL)) {
            printf("exec failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        fprintf(stderr, "failed to fork and create pty (%s).\n",
            strerror(errno));
        return -1;
    }

    close(pipes[0]);
    terminal->master = master;
    terminal->pace_pipe = pipes[1];
    fcntl(master, F_SETFL, O_NONBLOCK);
    terminal->io_task.run = io_handler;
    display_watch_fd(terminal->display, terminal->master,
                     EPOLLIN | EPOLLHUP, &terminal->io_task);

    if (option_fullscreen) {
        window_set_fullscreen(terminal->window, 1);
    } else if (option_maximize) {
        window_set_maximized(terminal->window, 1);
    } else {
        terminal_resize(terminal, NUM80, NUM24);
    }

    return 0;
}

static const struct weston_option terminal_options[] = {
    { WESTON_OPTION_BOOLEAN, "fullscreen", 'f', &option_fullscreen },
    { WESTON_OPTION_BOOLEAN, "maximized", 'm', &option_maximize },
    { WESTON_OPTION_STRING, "font", 0, &option_font },
    { WESTON_OPTION_INTEGER, "font-size", 0, &option_font_size },
    { WESTON_OPTION_STRING, "shell", 0, &option_shell },
};

int main(int argc, char *argv[])
{
    struct display *d;
    struct terminal *terminal;
    const char *config_file;
    struct sigaction sigpipe;
    struct weston_config *config;
    struct weston_config_section *s;

    /* as wcwidth is locale-dependent,
       wcwidth needs setlocale call to function properly. */
    if (setlocale(LC_ALL, "") == NULL) {
        printf("setlocale error");
    }
    option_shell = getenv("SHELL");
    if (!option_shell) {
        option_shell = "/bin/bash";
    }

    config_file = weston_config_get_name_from_env();
    config = weston_config_parse(config_file);
    s = weston_config_get_section(config, "terminal", NULL, NULL);
    weston_config_section_get_string(s, "font", &option_font, "mono");
    weston_config_section_get_int(s, "font-size", &option_font_size, NUM14);
    weston_config_section_get_string(s, "term", &option_term, "xterm");
    weston_config_destroy(config);

    if (parse_options(terminal_options,
                      ARRAY_LENGTH(terminal_options), &argc, argv) > 1) {
        printf("Usage: %s [OPTIONS]\n"
               "  --fullscreen or -f\n"
               "  --maximized or -m\n"
               "  --font=NAME\n"
               "  --font-size=SIZE\n"
               "  --shell=NAME\n", argv[0]);
        return 1;
    }

    /* Disable SIGPIPE so that paste operations do not crash the program
     * when the file descriptor provided to receive data is a pipe or
     * socket whose reading end has been closed */
    sigpipe.sa_handler = SIG_IGN;
    sigemptyset(&sigpipe.sa_mask);
    sigpipe.sa_flags = 0;
    sigaction(SIGPIPE, &sigpipe, NULL);

    d = display_create(&argc, argv);
    if (d == NULL) {
        fprintf(stderr, "failed to create display: %s\n",
            strerror(errno));
        return -1;
    }

    wl_list_init(&terminal_list);
    terminal = terminal_create(d);
    if (terminal_run(terminal, option_shell)) {
        exit(EXIT_FAILURE);
    }

    display_run(d);

    return 0;
}
