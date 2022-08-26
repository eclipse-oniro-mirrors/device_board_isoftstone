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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <cairo.h>
#include <linux/importing.h>

#define NUM03 0.3
#define NUM2 2
#define NUM10 10
#define NUM20 20
#define NUM40 40
#define NUM500 500
#define NUM400 400
#define NUM65535 65535

struct contententry {
    struct part *part;
    struct window *window;
    char *text;
    int active;
    bool panelvisible;
    unsigned int cursor;
    unsigned int anchor;
    struct {
        char *text;
        unsigned int cursor;
        char *commit;
        PangoAttrList *attrlist;
    } preedit;
    struct {
        PangoAttrList *attrlist;
        unsigned int cursor;
    } preeditinfo;
    struct {
        unsigned int cursor;
        unsigned int anchor;
        unsigned int deleteindex;
        unsigned int deletelength;
        bool invaliddelete;
    } pendingcommit;
    struct zwpcontentimportingv1 *contentimporting;
    PangoLayout *layout;
    struct {
        xkbmodmaskt shiftmask;
    } keysym;
    unsigned int serial;
    unsigned int resetserial;
    unsigned int contentpurpose;
    bool clicktoshow;
    char *preferredlanguage;
    bool buttonpressed;
};

struct editor {
    struct zwpcontentimportingmanagerv1 *contentimportingmanager;
    struct isftdatasource *selection;
    char *selectedtext;
    struct display *display;
    struct window *window;
    struct part *part;
    struct contententry *entry;
    struct contententry *editor;
    struct contententry *activeentry;
};

static void contententryredrawhandler(struct part *part, void data[]);
static void contententrybuttonhandler(struct part *part, struct importing *importing, unsigned int button,
                                      enum isftpointerbuttonstate state, void data[]);
static void contententrytouchhandler(struct part *part, struct importing *importing, float tx, float ty, void data[]);
static int contententrymotionhandler(struct part *part, unsigned int time, float x, float y, void data[]);
static void contententryinsertatcursor(struct contententry *entry, const char *text,
                                       unsigned int cursor, unsigned int anchor);
static void contententrysetpreedit(struct contententry *entry, const char *preedittext, int preeditcursor);
static void contententrydeletetext(struct contententry *entry, unsigned int index, unsigned int length);
static void contententrydeleteselectedtext(struct contententry *entry);
static void contententryresetpreedit(struct contententry *entry);
static void contententrycommitandreset(struct contententry *entry);
static void contententrygetcursorrectangle(struct contententry *entry, struct rectangle *rectangle);
static void contententryupdate(struct contententry *entry);

static const char *mb4endchar(const char *p)
{
    char *q;
    *q = *p;
    while ((*q & 0xc0) == 0x80) {
        q++;
    }
    return q;
}

static const char *mb4prevchar(const char *s, const char *p)
{
    char *q;
    *q = *p;
    for (--q; q >= s; --q) {
        if ((*q & 0xc0) != 0x80) {
            return q;
        }
    }
    return NULL;
}

static const char *mb4nextchar(const char *p)
{
    char *q;
    *q = *p;
    if (*q != 0) {
        return mb4endchar(++q);
    }
    return NULL;
}

static void ationup(const char *p, unsigned int *cursor)
{
    const char *posr, *posri;
    char text[16];

    xkbkeysymtomb4(XKBKEYReturn, text, sizeof(text));

    posr = strstr(p, text);
    while (posr) {
        if (*cursor > (unsigned)(posr-p)) {
            posri = strstr(posr+1, text);
            if (!posri || !(*cursor > (unsigned)(posri-p))) {
                *cursor = posr-p;
                break;
            }
            posr = posri;
        } else {
            break;
        }
    }
}

static void contentimportingcommitstring(void data[], struct zwpcontentimportingv1 *contentimporting,
                                         unsigned int serial, const char *text)
{
    struct contententry *entry = data;

    if ((entry->serial - serial) > (entry->serial - entry->resetserial)) {
        fprintf(stderr, "Ignore commit. Serial: %u, Current: %u, Reset: %u\n",
                serial, entry->serial, entry->resetserial);
    }
        return;
    }

    if (entry->pendingcommit.invaliddelete) {
        fprintf(stderr, "Ignore commit. Invalid previous deletesurrounding event.\n");
        memset(&entry->pendingcommit, 0, sizeof entry->pendingcommit);
        return;
    }

    contententryresetpreedit(entry);

    if (entry->pendingcommit.deletelength) {
        contententrydeletetext(entry, entry->pendingcommit.deleteindex, entry->pendingcommit.deletelength);
    } else {
        contententrydeleteselectedtext(entry);
    }

    contententryinsertatcursor(entry, text, entry->pendingcommit.cursor, entry->pendingcommit.anchor);
    memset(&entry->pendingcommit, 0, sizeof entry->pendingcommit);
    partscheduleredraw(entry->part);
}

static void clearpendingpreedit(struct contententry *entry)
{
    memset(&entry->pendingcommit, 0, sizeof entry->pendingcommit);

    pangoattrlistunref(entry->preeditinfo.attrlist);

    entry->preeditinfo.cursor = 0;
    entry->preeditinfo.attrlist = NULL;

    memset(&entry->preeditinfo, 0, sizeof entry->preeditinfo);
}

static void ationdown(const char *p, unsigned int *cursor)
{
    const char *posr;
    char text[16];

    xkbkeysymtomb4(XKBKEYReturn, text, sizeof(text));

    posr = strstr(p, text);
    while (posr) {
        if (*cursor <= (unsigned)(posr-p)) {
            *cursor = posr-p + 1;
            break;
        }
        posr = strstr(posr+1, text);
    }
}

static void contentimportingpreeditstring(void data[], struct zwpcontentimportingv1 *contentimporting,
                                          unsigned int serial, const char *text, const char *commit)
{
    struct contententry *entry = data;

    if ((entry->serial - serial) > (entry->serial - entry->resetserial)) {
        fprintf(stderr, "Ignore preeditstring. Serial: %u, Current: %u, Reset: %u\n",
                serial, entry->serial, entry->resetserial);
        clearpendingpreedit(entry);
        return;
    }

    if (entry->pendingcommit.invaliddelete) {
        fprintf(stderr, "Ignore preeditstring. Invalid previous deletesurrounding event.\n");
        clearpendingpreedit(entry);
        return;
    }

    if (entry->pendingcommit.deletelength) {
        contententrydeletetext(entry, entry->pendingcommit.deleteindex, entry->pendingcommit.deletelength);
    } else {
        contententrydeleteselectedtext(entry);
    }

    contententrysetpreedit(entry, text, entry->preeditinfo.cursor);
    entry->preedit.commit = strdup(commit);
    entry->preedit.attrlist = pangoattrlistref(entry->preeditinfo.attrlist);

    clearpendingpreedit(entry);
    contententryupdate(entry);
    partscheduleredraw(entry->part);
}

static void contentimportingdeletesurroundingtext(void data[], struct zwpcontentimportingv1 *contentimporting,
                                                  unsigned int index, unsigned int length)
{
    struct contententry *entry = data;
    unsigned int contentlength;

    entry->pendingcommit.deleteindex = entry->cursor + index;
    entry->pendingcommit.deletelength = length;
    entry->pendingcommit.invaliddelete = false;
    contentlength = strlen(entry->text);
    if (entry->pendingcommit.deleteindex > contentlength || length > contentlength ||
        entry->pendingcommit.deleteindex + length > contentlength) {
        fprintf(stderr, "deletesurroundingtext: Invalid index: %d," \
                "length %u'; cursor: %u text length: %u\n", index, length, entry->cursor, contentlength);
        entry->pendingcommit.invaliddelete = true;
        return;
    }
}

static void contentimportingcursorposition(void data[], struct zwpcontentimportingv1 *contentimporting,
                                           unsigned int index, unsigned int anchor)
{
    struct contententry *entry = data;

    entry->pendingcommit.cursor = index;
    entry->pendingcommit.anchor = anchor;
}

static void contentimportingenter(void data[], struct zwpcontentimportingv1 *contentimporting,
                                  struct isftsurface *surface)
{
    struct contententry *entry = data;

    if (surface != windowgetisftsurface(entry->window)) {
        return;
    }
    entry->active++;

    contententryupdate(entry);
    entry->resetserial = entry->serial;

    partscheduleredraw(entry->part);
}

static void contentimportingleave(void data[], struct zwpcontentimportingv1 *contentimporting)
{
    struct contententry *entry = data;

    contententrycommitandreset(entry);
    entry->active--;

    if (!entry->active) {
        zwpcontentimportingv1hideimportingpanel(contentimporting);
        entry->panelvisible = false;
    }

    partscheduleredraw(entry->part);
}

static void contentimportingimportingpanelstate(void data[], struct zwpcontentimportingv1 *contentimporting,
                                                unsigned int state)
{
}

static void contentimportinglanguage(void data[], struct zwpcontentimportingv1 *contentimporting,
                                     unsigned int serial, const char *language)
{
    int ret = fprintf(stderr, "importing language is %s \n", language);
    if (ret < 0) {
        printf("fail to fprintf");
    }
}

static void contentimportingcontentdirection(void data[], struct zwpcontentimportingv1 *contentimporting,
                                             unsigned int serial, unsigned int direction)
{
    struct contententry *entry = data;
    PangoContext *context = pangolayoutgetcontext(entry->layout);
    PangoDirection pangodirection;

    switch (direction) {
        case ZWPcontentimportingV1contentDIRECTIONLTR:
            pangodirection = PANGODIRECTIONLTR;
            break;
        case ZWPcontentimportingV1contentDIRECTIONRTL:
            pangodirection = PANGODIRECTIONRTL;
            break;
        case ZWPcontentimportingV1contentDIRECTIONAUTO:
        default:
            pangodirection = PANGODIRECTIONNEUTRAL;
    }

    pangoconcontentsetbasedir(context, pangodirection);
}

static const struct zwpcontentimportingv1listener contentimportinglistener = {
    contentimportingenter,
    contentimportingleave,
    contentimportingmodifiersmap,
    contentimportingimportingpanelstate,
    contentimportingpreeditstring,
    contentimportingpreeditstyling,
    contentimportingpreeditcursor,
    contentimportingcommitstring,
    contentimportingcursorposition,
    contentimportingdeletesurroundingtext,
    contentimportingkeysym,
    contentimportinglanguage,
    contentimportingcontentdirection
};

static void contentimportingpreeditstyling(void data[], struct zwpcontentimportingv1 *contentimporting,
                                           unsigned int index, unsigned int length, unsigned int style)
{
    struct contententry *entry = data;
    PangoAttribute *attr1 = NULL;
    PangoAttribute *attr2 = NULL;

    if (!entry->preeditinfo.attrlist) {
        entry->preeditinfo.attrlist = pangoattrlistnew();
    }
    switch (style) {
        case ZWPcontentimportingV1PREEDITSTYLEDEFAULT:
        case ZWPcontentimportingV1PREEDITSTYLEUNDERLINE:
            attr1 = pangoattrunderlinenew(PANGOUNDERLINESINGLE);
            break;
        case ZWPcontentimportingV1PREEDITSTYLEINCORRECT:
            attr1 = pangoattrunderlinenew(PANGOUNDERLINEERROR);
            attr2 = pangoattrunderlinecolornew(NUM65535, 0, 0);
            break;
        case ZWPcontentimportingV1PREEDITSTYLESELECTION:
            attr1 = pangoattrbackgroundnew(NUM03 * NUM65535, NUM03 * NUM65535, NUM65535);
            attr2 = pangoattrforegroundnew(NUM65535, NUM65535, NUM65535);
            break;
        case ZWPcontentimportingV1PREEDITSTYLEHIGHLIGHT:
        case ZWPcontentimportingV1PREEDITSTYLEACTIVE:
            attr1 = pangoattrunderlinenew(PANGOUNDERLINESINGLE);
            attr2 = pangoattrweightnew(PANGOWEIGHTBOLD);
            break;
        case ZWPcontentimportingV1PREEDITSTYLEINACTIVE:
            attr1 = pangoattrunderlinenew(PANGOUNDERLINESINGLE);
            attr2 = pangoattrforegroundnew(NUM03 * NUM65535, NUM03 * NUM65535, NUM03 * NUM65535);
            break;
        default:
            break;
    }

    if (attr1) {
        attr1->startindex = entry->cursor + index;
        attr1->endindex = entry->cursor + index + length;
        pangoattrlistinsert(entry->preeditinfo.attrlist, attr1);
    }

    if (attr2) {
        attr2->startindex = entry->cursor + index;
        attr2->endindex = entry->cursor + index + length;
        pangoattrlistinsert(entry->preeditinfo.attrlist, attr2);
    }
}

static void contentimportingpreeditcursor(void data[], struct zwpcontentimportingv1 *contentimporting,
                                          unsigned int index)
{
    struct contententry *entry = data;

    entry->preeditinfo.cursor = index;
}

static void contentimportingmodifiersmap(void data[], struct zwpcontentimportingv1 *contentimporting,
                                         struct isftarray *map)
{
    struct contententry *entry = data;

    entry->keysym.shiftmask = keysymmodifiersgetmask(map, "Shift");
}

static void contentimportingkeysym1(void data[], unsigned int serial, unsigned int key,
                                    unsigned int state, unsigned int modifiers)
{
    struct contententry *entry = data;
    const char *newchar;

    if (key == XKBKEYLeft || key == XKBKEYRight) {
        if (state != isftKEYBOARDKEYSTATERELEASED) {
            return;
        }
        if (key == XKBKEYLeft) {
            newchar = mb4prevchar(entry->text, entry->text + entry->cursor);
        } else {
            newchar = mb4nextchar(entry->text + entry->cursor);
        }
        if (newchar != NULL) {
            entry->cursor = newchar - entry->text;
        }

        if (!(modifiers & entry->keysym.shiftmask)) {
            entry->anchor = entry->cursor;
        }
        partscheduleredraw(entry->part);

        return;
    }

    if (key == XKBKEYUp || key == XKBKEYDown) {
        if (state != isftKEYBOARDKEYSTATERELEASED) {
            return;
        }
        if (key == XKBKEYUp) {
            ationup(entry->text, &entry->cursor);
        } else {
            ationdown(entry->text, &entry->cursor);
        }
        if (!(modifiers & entry->keysym.shiftmask)) {
            entry->anchor = entry->cursor;
        }
        partscheduleredraw(entry->part);

        return;
    }
}
static void contentimportingkeysym2(void data[], unsigned int serial, unsigned int key,
                                    unsigned int state, unsigned int modifiers)
{
    struct contententry *entry = data;

    if (key == XKBKEYBackSpace) {
        const char *start, *end;

        if (state != isftKEYBOARDKEYSTATERELEASED) {
            return;
        }
        contententrycommitandreset(entry);

        start = mb4prevchar(entry->text, entry->text + entry->cursor);
        if (start == NULL) {
            return;
        }
        end = mb4nextchar(start);

        contententrydeletetext(entry, start - entry->text, end - start);

        return;
    }

    if (key == XKBKEYTab || key == XKBKEYKPEnter || key == XKBKEYReturn) {
        char text[16];
        if (state != isftKEYBOARDKEYSTATERELEASED) {
            return;
        }
        xkbkeysymtomb4(key, text, sizeof(text));
        contententryinsertatcursor(entry, text, 0, 0);
        return;
    }
}
static void datasourcesend(void data[], struct isftdatasource *source, const char *mimetype, unsigned int fd)
{
    struct editor *editor = data;

    if (write(fd, editor->selectedtext, strlen(editor->selectedtext) + 1) < 0) {
        fprintf(stderr, "write failed: %s\n", strerror(errno));
    }
    close(fd);
}

static void datasourcecancelled(void data[], struct isftdatasource *source)
{
    isftdatasourcedestroy(source);
}

static const struct isftdatasourcelistener datasourcelistener = {
    datasourcetarget,
    datasourcesend,
    datasourcecancelled
};

static void pastefunc(void buffer[], int len, unsigned int x, unsigned int y, void data[])
{
    struct editor *editor = data;
    struct contententry *entry = editor->activeentry;
    char *pastedtext;

    if (!entry) {
        return;
    }
    pastedtext = malloc(len + 1);
    strncpy(pastedtext, buffer, len);
    pastedtext[len] = '\0';

    contententryinsertatcursor(entry, pastedtext, 0, 0);

    free(pastedtext);
}

static void editorpaste(struct editor *editor, struct importing *importing)
{
    importingreceiveselectiondata(importing, "text/plain;charset=utf-8", pastefunc, editor);
}

static void menufunc(void data[], struct importing *importing, int index)
{
    struct window *window = data;
    struct editor *editor = windowgetuserdata(window);

    int ret = fprintf(stderr, "picked entry %d\n", index);
    if (ret < 0) {
        printf("fail to fprintf");
    }
    switch (index) {
        case 0:
            editorcopycut(editor, importing, true);
            break;
        case 1:
            editorcopycut(editor, importing, false);
            break;
        case NUM2:
            editorpaste(editor, importing);
            break;
        default:
            break;
    }
}

static void showmenu(struct editor *editor, struct importing *importing, unsigned int time)
{
    unsigned int x, y;
    static const char *entries[] = {
        "Cut", "Copy", "Paste"
    };

    importinggetposition(importing, &x, &y);
    windowshowmenu(editor->display, importing, time, editor->window, x + NUM10, y + NUM20, menufunc,
                   entries, ARRAYLENGTH(entries));
}

static struct contententry* contententrycreate(struct editor *editor, const char *text)
{
    struct contententry *entry;

    entry = xzalloc(sizeof *entry);

    entry->part = partaddpart(editor->part, entry);
    entry->window = editor->window;
    entry->text = strdup(text);
    entry->active = 0;
    entry->panelvisible = false;
    entry->cursor = strlen(text);
    entry->anchor = entry->cursor;
    entry->contentimporting =
        zwpcontentimportingmanagerv1createcontentimporting(editor->contentimportingmanager);
        zwpcontentimportingv1addlistener(entry->contentimporting, &contentimportinglistener, entry);

    partsetredrawhandler(entry->part, contententryredrawhandler);
    partsetbuttonhandler(entry->part, contententrybuttonhandler);
    partsetmotionhandler(entry->part, contententrymotionhandler);
    partsettouchdownhandler(entry->part, contententrytouchhandler);

    return entry;
}

static void contententrydestroy(struct contententry *entry)
{
    partdestroy(entry->part);
    zwpcontentimportingv1destroy(entry->contentimporting);
    gclearobject(&entry->layout);
    free(entry->text);
    free(entry->preferredlanguage);
    free(entry);
}

static void redrawhandler(struct part *part, void data[])
{
    struct editor *editor = data;
    cairosurfacet *surface;
    struct rectangle allocation;
    cairot *cr;

    surface = windowgetsurface(editor->window);
    partgetallocation(editor->part, &allocation);

    cr = cairocreate(surface);
    cairorectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
    cairoclip(cr);

    cairotranslate(cr, allocation.x, allocation.y);

    /* Draw background */
    cairopushgroup(cr);
    cairosetoperator(cr, CAIROOPERATORSOURCE);
    cairosetsourcergba(cr, 1, 1, 1, 1);
    cairorectangle(cr, 0, 0, allocation.width, allocation.height);
    cairofill(cr);

    cairopopgrouptosource(cr);
    cairopaint(cr);

    cairodestroy(cr);
    cairosurfacedestroy(surface);
}

static void editorcopycut(struct editor *editor, struct importing *importing, bool cut)
{
    struct contententry *entry = editor->activeentry;

    if (!entry) {
        return;
    }
    if (entry->cursor != entry->anchor) {
        int startindex = MIN(entry->cursor, entry->anchor);
        int endindex = MAX(entry->cursor, entry->anchor);
        int len = endindex - startindex;

        editor->selectedtext = realloc(editor->selectedtext, len + 1);
        strncpy(editor->selectedtext, &entry->text[startindex], len);
        editor->selectedtext[len] = '\0';

        if (cut) {
            contententrydeletetext(entry, startindex, len);
        }
        editor->selection =
            displaycreatedatasource(editor->display);
        if (!editor->selection) {
            return;
        }
        isftdatasourceoffer(editor->selection, "text/plain;charset=utf-8");
        isftdatasourceaddlistener(editor->selection, &datasourcelistener, editor);
        importingsetselection(importing, editor->selection, displaygetserial(editor->display));
    }
}

static void contententryallocate(struct contententry *entry, unsigned int x, unsigned int y,
                                 unsigned int width, unsigned int height)
{
    partsetallocation(entry->part, x, y, width, height);
}

static void resizehandler(struct part *part, unsigned int width, unsigned int height, void data[])
{
    struct editor *editor = data;
    struct rectangle allocation;

    partgetallocation(editor->part, &allocation);
    contententryallocate(editor->entry, allocation.x + NUM20, allocation.y + NUM20,
                         width - NUM40, height / NUM2 - NUM40);
    contententryallocate(editor->editor, allocation.x + NUM20, allocation.y + height / NUM2 + NUM20,
                         width - NUM40, height / NUM2 - NUM40);
}

static void contententrydeactivate(struct contententry *entry, struct isftseat *seat)
{
    zwpcontentimportingv1deactivate(entry->contentimporting, seat);
}

static void contententryupdatelayout(struct contententry *entry)
{
    char *text;
    PangoAttrList *attrlist;

    assert(entry->cursor <= (strlen(entry->text) +
           (entry->preedit.text ? strlen(entry->preedit.text) : 0)));

    if (entry->preedit.text) {
        text = xmalloc(strlen(entry->text) + strlen(entry->preedit.text) + 1);
        strncpy(text, entry->text, entry->cursor);
        strcpy(text + entry->cursor, entry->preedit.text);
        strcpy(text + entry->cursor + strlen(entry->preedit.text),
               entry->text + entry->cursor);
    } else {
        text = strdup(entry->text);
    }

    if (entry->cursor != entry->anchor) {
        int startindex = MIN(entry->cursor, entry->anchor);
        int endindex = MAX(entry->cursor, entry->anchor);
        PangoAttribute *attr;
        attrlist = pangoattrlistcopy(entry->preedit.attrlist);
        if (!attrlist) {
            attrlist = pangoattrlistnew();
        }
        attr = pangoattrbackgroundnew(NUM03 * NUM65535, NUM03 * NUM65535, NUM65535);
        attr->startindex = startindex;
        attr->endindex = endindex;
        pangoattrlistinsert(attrlist, attr);

        attr = pangoattrforegroundnew(NUM65535, NUM65535, NUM65535);
        attr->startindex = startindex;
        attr->endindex = endindex;
        pangoattrlistinsert(attrlist, attr);
    } else {
        attrlist = pangoattrlistref(entry->preedit.attrlist);
    }

    if (entry->preedit.text && !entry->preedit.attrlist) {
        PangoAttribute *attr;

        if (!attrlist) {
            attrlist = pangoattrlistnew();
        }
        attr = pangoattrunderlinenew(PANGOUNDERLINESINGLE);
        attr->startindex = entry->cursor;
        attr->endindex = entry->cursor + strlen(entry->preedit.text);
        pangoattrlistinsert(attrlist, attr);
    }

    if (entry->layout) {
        pangolayoutsettext(entry->layout, text, -1);
        pangolayoutsetattributes(entry->layout, attrlist);
    }

    free(text);
    pangoattrlistunref(attrlist);
}

static void contententryupdate(struct contententry *entry)
{
    struct rectangle cursorrectangle;
    zwpcontentimportingv1setcontenttype(entry->contentimporting, ZWPcontentimportingV1CONTENTHINTNONE,
                                        entry->contentpurpose);

    zwpcontentimportingv1setsurroundingtext(entry->contentimporting, entry->text, entry->cursor, entry->anchor);

    if (entry->preferredlanguage) {
        zwpcontentimportingv1setpreferredlanguage(entry->contentimporting, entry->preferredlanguage);
    }
    contententrygetcursorrectangle(entry, &cursorrectangle);
    zwpcontentimportingv1setcursorrectangle(entry->contentimporting, cursorrectangle.x, cursorrectangle.y,
                                            cursorrectangle.width, cursorrectangle.height);

    zwpcontentimportingv1commitstate(entry->contentimporting, ++entry->serial);
}

static void contententryactivate(struct contententry *entry, struct isftseat *seat)
{
    struct isftsurface *surface = windowgetisftsurface(entry->window);

    if (entry->clicktoshow && entry->active) {
        entry->panelvisible = !entry->panelvisible;

        if (entry->panelvisible) {
            zwpcontentimportingv1showimportingpanel(entry->contentimporting);
        } else {
            zwpcontentimportingv1hideimportingpanel(entry->contentimporting);
        }
        return;
    }

    if (!entry->clicktoshow) {
        zwpcontentimportingv1showimportingpanel(entry->contentimporting);
    }
    zwpcontentimportingv1activate(entry->contentimporting, seat, surface);
}

static void contententrydeletetext(struct contententry *entry, unsigned int index, unsigned int length)
{
    unsigned int l;

    assert(index <= strlen(entry->text));
    assert(index + length <= strlen(entry->text));
    assert(index + length >= length);

    l = strlen(entry->text + index + length);
    memmove(entry->text + index, entry->text + index + length, l + 1);

    if (entry->cursor > (index + length)) {
        entry->cursor -= length;
    } else if (entry->cursor > index) {
        entry->cursor = index;
    }
    entry->anchor = entry->cursor;
    contententryupdatelayout(entry);
    partscheduleredraw(entry->part);
    contententryupdate(entry);
}

static void contententrydeleteselectedtext(struct contententry *entry)
{
    unsigned int startindex = entry->anchor < entry->cursor ? entry->anchor : entry->cursor;
    unsigned int endindex = entry->anchor < entry->cursor ? entry->cursor : entry->anchor;

    if (entry->anchor == entry->cursor) {
        return;
    }
    contententrydeletetext(entry, startindex, endindex - startindex);

    entry->anchor = entry->cursor;
}

static void contententrygetcursorrectangle(struct contententry *entry, struct rectangle *rectangle)
{
    struct rectangle allocation;
    PangoRectangle extents;
    PangoRectangle cursorpos;

    partgetallocation(entry->part, &allocation);

    if (entry->preedit.text && entry->preedit.cursor < 0) {
        rectangle->x = 0;
        rectangle->y = 0;
        rectangle->width = 0;
        rectangle->height = 0;
        return;
    }

    pangolayoutgetextents(entry->layout, &extents, NULL);
    pangolayoutgetcursorpos(entry->layout, entry->cursor + entry->preedit.cursor, &cursorpos, NULL);

    rectangle->x = allocation.x + (allocation.height / NUM2) + PANGOPIXELS(cursorpos.x);
    rectangle->y = allocation.y + NUM10 + PANGOPIXELS(cursorpos.y);
    rectangle->width = PANGOPIXELS(cursorpos.width);
    rectangle->height = PANGOPIXELS(cursorpos.height);
}

static void contententrydrawcursor(struct contententry *entry, cairot *cr)
{
    PangoRectangle extents;
    PangoRectangle cursorpos;

    if (entry->preedit.text && entry->preedit.cursor < 0) {
        return;
    }
    pangolayoutgetextents(entry->layout, &extents, NULL);
    pangolayoutgetcursorpos(entry->layout, entry->cursor + entry->preedit.cursor, &cursorpos, NULL);

    cairosetlinewidth(cr, 1.0);
    cairoationto(cr, PANGOPIXELS(cursorpos.x), PANGOPIXELS(cursorpos.y));
    cairolineto(cr, PANGOPIXELS(cursorpos.x), PANGOPIXELS(cursorpos.y) + PANGOPIXELS(cursorpos.height));
    cairostroke(cr);
}

static int contentoffsetleft(struct rectangle *allocation)
{
    return NUM10;
}

static void contententryinsertatcursor(struct contententry *entry, const char *text,
                                       unsigned int cursor, unsigned int anchor)
{
    char *newtext = xmalloc(strlen(entry->text) + strlen(text) + 1);

    strncpy(newtext, entry->text, entry->cursor);
    strcpy(newtext + entry->cursor, text);
    strcpy(newtext + entry->cursor + strlen(text), entry->text + entry->cursor);

    free(entry->text);
    entry->text = newtext;
    if (anchor >= 0) {
        entry->anchor = entry->cursor + strlen(text) + anchor;
    } else {
        entry->anchor = entry->cursor + 1 + anchor;
    }
    if (cursor >= 0) {
        entry->cursor += strlen(text) + cursor;
    } else {
        entry->cursor += 1 + cursor;
    }
    contententryupdatelayout(entry);
    partscheduleredraw(entry->part);
    contententryupdate(entry);
}

static void contententryresetpreedit(struct contententry *entry)
{
    entry->preedit.cursor = 0;

    free(entry->preedit.text);
    entry->preedit.text = NULL;

    free(entry->preedit.commit);
    entry->preedit.commit = NULL;

    pangoattrlistunref(entry->preedit.attrlist);
    entry->preedit.attrlist = NULL;
}

static void contententrycommitandreset(struct contententry *entry)
{
    char *commit = NULL;

    if (entry->preedit.commit) {
        commit = strdup(entry->preedit.commit);
    }
    contententryresetpreedit(entry);
    if (commit) {
        contententryinsertatcursor(entry, commit, 0, 0);
        free(commit);
    }

    zwpcontentimportingv1reset(entry->contentimporting);
    contententryupdate(entry);
    entry->resetserial = entry->serial;
}

static void contententrysetpreedit(struct contententry *entry, const char *preedittext, int preeditcursor)
{
    contententryresetpreedit(entry);

    if (!preedittext) {
        return;
    }
    entry->preedit.text = strdup(preedittext);
    entry->preedit.cursor = preeditcursor;
    contententryupdatelayout(entry);
    partscheduleredraw(entry->part);
}

static unsigned int contententrytryinvokepreeditaction(struct contententry *entry, unsigned int x, unsigned int y,
                                                       unsigned int button, enum isftpointerbuttonstate state)
{
    int index, trailing;
    unsigned int cursor;
    const char *text;

    if (!entry->preedit.text) {
        return 0;
    }
    pangolayoutxytoindex(entry->layout, x * PANGOSCALE, y * PANGOSCALE, &index, &trailing);
    text = pangolayoutgettext(entry->layout);
    cursor = gmb4offsettopointer(text + index, trailing) - text;
    if (cursor < entry->cursor || cursor > entry->cursor + strlen(entry->preedit.text)) {
        return 0;
    }

    if (state == isftPOINTERBUTTONSTATERELEASED) {
        zwpcontentimportingv1invokeaction(entry->contentimporting, button, cursor - entry->cursor);
    }
    return 1;
}

static bool contententryhaspreedit(struct contententry *entry)
{
    return entry->preedit.text && (strlen(entry->preedit.text) > 0);
}

static void contententrysetcursorposition(struct contententry *entry, unsigned int x, unsigned int y, bool ationanchor)
{
    int index, trailing;
    const char *text;
    unsigned int cursor;

    pangolayoutxytoindex(entry->layout, x * PANGOSCALE, y * PANGOSCALE, &index, &trailing);
    text = pangolayoutgettext(entry->layout);

    cursor = gmb4offsettopointer(text + index, trailing) - text;

    if (ationanchor) {
        entry->anchor = cursor;
    }
    if (contententryhaspreedit(entry)) {
        contententrycommitandreset(entry);

        assert(!contententryhaspreedit(entry));
    }

    if (entry->cursor == cursor) {
        return;
    }
    entry->cursor = cursor;
    contententryupdatelayout(entry);
    partscheduleredraw(entry->part);
    contententryupdate(entry);
}

static int contentoffsettop(struct rectangle *allocation)
{
    return allocation->height / NUM2;
}

static void contententrybuttonhandler(struct part *part, struct importing *importing, unsigned int button,
                                      enum isftpointerbuttonstate state, void data[])
{
    unsigned int time;
    struct contententry *entry = data;
    struct rectangle allocation;
    struct editor *editor;
    unsigned int x, y;
    unsigned int result;

    partgetallocation(entry->part, &allocation);
    importinggetposition(importing, &x, &y);

    x -= allocation.x + contentoffsetleft(&allocation);
    y -= allocation.y + contentoffsettop(&allocation);

    editor = windowgetuserdata(entry->window);

    switch (button) {
        case BTNLEFT:
            entry->buttonpressed = (state == isftPOINTERBUTTONSTATEPRESSED);
            if (state == isftPOINTERBUTTONSTATEPRESSED) {
                importinggrab(importing, entry->part, button);
            } else {
                importingungrab(importing);
            }
            break;
        case BTNRIGHT:
            if (state == isftPOINTERBUTTONSTATEPRESSED) {
                showmenu(editor, importing, time);
            }
            break;
        default:
            break;
    }

    if (contententryhaspreedit(entry)) {
        result = contententrytryinvokepreeditaction(entry, x, y, button, state);
        if (result) {
            return;
        }
    }
    if (state == isftPOINTERBUTTONSTATEPRESSED && button == BTNLEFT) {
        struct isftseat *seat = importinggetseat(importing);

        contententryactivate(entry, seat);
        editor->activeentry = entry;
        contententrysetcursorposition(entry, x, y, true);
    }
}

static void contententrytouchhandler(struct part *part, struct importing *importing, float tx, float ty, void data[])
{
    struct contententry *entry = data;
    struct isftseat *seat = importinggetseat(importing);
    struct rectangle allocation;
    struct editor *editor;
    unsigned int x, y;

    partgetallocation(entry->part, &allocation);

    x = tx - (allocation.x + contentoffsetleft(&allocation));
    y = ty - (allocation.y + contentoffsettop(&allocation));

    editor = windowgetuserdata(entry->window);
    contententryactivate(entry, seat);
    editor->activeentry = entry;

    contententrysetcursorposition(entry, x, y, true);
}

static void editorbuttonhandler(struct part *part, struct importing *importing, unsigned int button,
                                enum isftpointerbuttonstate state, void data[])
{
    struct editor *editor = data;

    if (button != BTNLEFT) {
        return;
    }

    if (state == isftPOINTERBUTTONSTATEPRESSED) {
        struct isftseat *seat = importinggetseat(importing);

        contententrydeactivate(editor->entry, seat);
        contententrydeactivate(editor->editor, seat);
        editor->activeentry = NULL;
    }
}

static void contententryredrawhandler(struct part *part, void data[])
{
    struct contententry *entry = data;
    cairosurfacet *surface;
    struct rectangle allocation;
    cairot *cr;

    surface = windowgetsurface(entry->window);
    partgetallocation(entry->part, &allocation);

    cr = cairocreate(surface);
    cairorectangle(cr, allocation.x, allocation.y, allocation.width, allocation.height);
    cairoclip(cr);

    cairosetoperator(cr, CAIROOPERATORSOURCE);

    cairopushgroup(cr);
    cairotranslate(cr, allocation.x, allocation.y);

    cairosetsourcergba(cr, 1, 1, 1, 1);
    cairorectangle(cr, 0, 0, allocation.width, allocation.height);
    cairofill(cr);

    cairosetoperator(cr, CAIROOPERATOROVER);

    if (entry->active) {
        cairorectangle(cr, 0, 0, allocation.width, allocation.height);
        cairosetlinewidth (cr, NUM3);
        cairosetsourcergba(cr, 0, 0, 1, 1.0);
        cairostroke(cr);
    }

    cairosetsourcergba(cr, 0, 0, 0, 1);

    cairotranslate(cr, ontentoffsetleft(&allocation), contentoffsettop(&allocation));

    if (!entry->layout) {
        entry->layout = pangocairocreatelayout(cr);
    } else {
        pangocairoupdatelayout(cr, entry->layout);
    }
    contententryupdatelayout(entry);

    pangocairoshowlayout(cr, entry->layout);

    contententrydrawcursor(entry, cr);

    cairopopgrouptosource(cr);
    cairopaint(cr);

    cairodestroy(cr);
    cairosurfacedestroy(surface);
}

static int contententrymotionhandler(struct part *part, unsigned int time, float x, float y, void data[])
{
    struct contententry *entry = data;
    struct rectangle allocation;
    int tx, ty;

    if (!entry->buttonpressed) {
        return CURSORIBEAM;
    }

    partgetallocation(entry->part, &allocation);

    tx = x - allocation.x - contentoffsetleft(&allocation);
    ty = y - allocation.y - contentoffsettop(&allocation);

    contententrysetcursorposition(entry, tx, ty, false);

    return CURSORIBEAM;
}

static void globalhandler(struct display *display, unsigned int name, const char *interface,
                          unsigned int version, void data[])
{
    struct editor *editor = data;

    if (!strcmp(interface, "zwpcontentimportingmanagerv1")) {
        editor->contentimportingmanager =
            displaybind(display, name,
                     &zwpcontentimportingmanagerv1interface, 1);
    }
}

/** Display help for command line options, and exit */
static bool opthelp = false;

/** Require a distinct click to show the importing panel (virtual keyboard) */
static bool optclicktoshow = false;

/** Set a specific (RFC-3066) language.  Used for the virtual keyboard, etc. */
static const char *optpreferredlanguage = NULL;

/**
 * \brief command line options for editor
 */
static const struct westonoption editoroptions[] = {
    { WESTONOPTIONBOOLEAN, "help", 'h', &opthelp },
    { WESTONOPTIONBOOLEAN, "click-to-show", 'C', &optclicktoshow },
    { WESTONOPTIONSTRING, "preferred-language", 'L', &optpreferredlanguage },
};

static void usage(const char *programname, int exitcode)
{
    unsigned k;

    int ret = fprintf(stderr, "Usage: %s [OPTIONS] [FILENAME]\n\n", programname);
    if (ret < 0) {
        printf("fail to fprintf");
    }
    for (k = 0; k < ARRAYLENGTH(editoroptions); k++) {
        const struct westonoption *p = &editoroptions[k];
        if (p->name) {
            fprintf(stderr, "  --%s", p->name);
            if (p->type != WESTONOPTIONBOOLEAN) {
                fprintf(stderr, "=VALUE");
            }
            fprintf(stderr, "\n");
        }
        if (p->shortname) {
            fprintf(stderr, "  -%c", p->shortname);
            if (p->type != WESTONOPTIONBOOLEAN) {
                fprintf(stderr, "VALUE");
            }
            fprintf(stderr, "\n");
        }
    }
    exit(exitcode);
}

void error(int err, FILE *fin, char *buffer)
{
    int errsv = err;
    errsv = errno;
    if (fin) {
        int ret = fclose(fin);
        if (ret != 0) {
            printf("fail to fclose");
        }
    }
    free(buffer);
    errno = errsv ? errsv : EINVAL;
    return NULL;
}

static char *readfile(char *filename)
{
    char *buffer = NULL;
    int bufsize, readsize;
    FILE *fin;
    int errsv;

    fin = fopen(filename, "r");
    if (fin == NULL) {
        error(errsv, fin, buffer);
    }
    
    if (fseek(fin, 0, SEEKEND) != 0) {
        error(errsv, fin, buffer);
    }
    bufsize = ftell(fin);
    if (bufsize < 0) {
        error(errsv, fin, buffer);
    }
    rewind(fin);
    buffer = (char*) malloc(sizeof(char) * (bufsize + 1));
    if (buffer == NULL) {
        error(errsv, fin, buffer);
    }
    readsize = fread(buffer, sizeof(char), bufsize, fin);
    int ret = fclose(fin);
    if (ret != 0) {
        printf("fail to fclose");
    }
    if (bufsize != readsize) {
        error(errsv, fin, buffer);
    }
    buffer[bufsize] = '\0';
    return buffer;
}

static void editortouchhandler(struct part *part, struct importing *importing, void data[])
{
    struct editor *editor = data;
    struct isftseat *seat = importinggetseat(importing);
    contententrydeactivate(editor->entry, seat);
    contententrydeactivate(editor->editor, seat);
    editor->activeentry = NULL;
}

static void keyboardfocushandler(struct window *window, struct importing *device, void data[])
{
    struct editor *editor = data;
    windowscheduleredraw(editor->window);
}

static int handleboundkey(struct editor *editor, struct importing *importing, unsigned int sym, unsigned int time)
{
    switch (sym) {
        case XKBKEYX:
            editorcopycut(editor, importing, true);
            return 1;
        case XKBKEYC:
            editorcopycut(editor, importing, false);
            return 1;
        case XKBKEYV:
            editorpaste(editor, importing);
            return 1;
        default:
            return 0;
    }
}

void keyhandlerswitch1(struct importing *importing, unsigned int sym)
{
    switch (sym) {
        case XKBKEYBackSpace:
            contententrycommitandreset(entry);

            newchar = mb4prevchar(entry->text, entry->text + entry->cursor);
            if (newchar != NULL) {
                contententrydeletetext(entry, newchar - entry->text, (entry->text + entry->cursor) - newchar);
            }
            break;
        case XKBKEYDelete:
            contententrycommitandreset(entry);

            newchar = mb4nextchar(entry->text + entry->cursor);
            if (newchar != NULL) {
                contententrydeletetext(entry, entry->cursor, newchar - (entry->text + entry->cursor));
            }
            break;
        case XKBKEYLeft:
            contententrycommitandreset(entry);

            newchar = mb4prevchar(entry->text, entry->text + entry->cursor);
            if (newchar != NULL) {
                entry->cursor = newchar - entry->text;
                if (!(importinggetmodifiers(importing) & MODSHIFTMASK))
                    entry->anchor = entry->cursor;
                partscheduleredraw(entry->part);
            }
            break;
        default:
            if (xkbkeysymtomb4(sym, text, sizeof(text)) <= 0) {
                break;
            }
            contententrycommitandreset(entry);
            contententryinsertatcursor(entry, text, 0, 0);
            break;
    }
}

void keyhandlerswitch2(struct importing *importing, unsigned int sym)
{
    switch (sym) {
        case XKBKEYRight:
            contententrycommitandreset(entry);

            newchar = mb4nextchar(entry->text + entry->cursor);
            if (newchar != NULL) {
                entry->cursor = newchar - entry->text;
                if (!(importinggetmodifiers(importing) & MODSHIFTMASK)) {
                    entry->anchor = entry->cursor;
                }
                partscheduleredraw(entry->part);
            }
            break;
        case XKBKEYUp:
            contententrycommitandreset(entry);

            ationup(entry->text, &entry->cursor);
            if (!(importinggetmodifiers(importing) & MODSHIFTMASK)) {
                entry->anchor = entry->cursor;
            }
            partscheduleredraw(entry->part);
            break;
        case XKBKEYDown:
            contententrycommitandreset(entry);

            ationdown(entry->text, &entry->cursor);
            if (!(importinggetmodifiers(importing) & MODSHIFTMASK)) {
                entry->anchor = entry->cursor;
            }
            partscheduleredraw(entry->part);
            break;
        case XKBKEYEscape:
            break;
        default:
            if (xkbkeysymtomb4(sym, text, sizeof(text)) <= 0) {
                break;
            }
            contententrycommitandreset(entry);
            contententryinsertatcursor(entry, text, 0, 0);
            break;
    }
}

static void keyhandler(struct importing *importing, unsigned int time, unsigned int sym,
                       enum isftkeyboardkeystate state, void data[])
{
    struct editor *editor = data;
    struct contententry *entry;
    const char *newchar;
    char text[16];
    unsigned int modifiers;

    if (!editor->activeentry) {
        return;
    }
    entry = editor->activeentry;

    if (state != isftKEYBOARDKEYSTATEPRESSED) {
        return;
    }
    modifiers = importinggetmodifiers(importing);
    if ((modifiers & MODCONTROLMASK) && (modifiers & MODSHIFTMASK) && handleboundkey(editor, importing, sym, time)) {
        return;
    }
    keyhandlerswitch1(importing, sym);
    keyhandlerswitch2(importing, sym);

    partscheduleredraw(entry->part);
}

void maineditor(struct editor editor, char *contentbuffer)
{
    char *contentbuffer = NULL;

    if (contentbuffer) {
        editor.entry = contententrycreate(&editor, contentbuffer);
    } else {
        editor.entry = contententrycreate(&editor, "Entry");
    }
    editor.entry->clicktoshow = optclicktoshow;
    if (optpreferredlanguage) {
        editor.entry->preferredlanguage = strdup(optpreferredlanguage);
    }
    editor.editor = contententrycreate(&editor, "Numeric");
    editor.editor->contentpurpose = ZWPcontentimportingV1CONTENTPURPOSENUMBER;
    editor.editor->clicktoshow = optclicktoshow;
    editor.selection = NULL;
    editor.selectedtext = NULL;

    windowsettitle(editor.window, "Text Editor");
    windowsetkeyhandler(editor.window, keyhandler);
    windowsetkeyboardfocushandler(editor.window, keyboardfocushandler);
    windowsetuserdata(editor.window, &editor);

    partsetredrawhandler(editor.part, redrawhandler);
    partsetresizehandler(editor.part, resizehandler);
    partsetbuttonhandler(editor.part, editorbuttonhandler);
    partsettouchdownhandler(editor.part, editortouchhandler);

    windowscheduleresize(editor.window, NUM500, NUM400);

    displayrun(editor.display);

    if (editor.selectedtext) {
        free(editor.selectedtext);
    }
    if (editor.selection) {
        isftdatasourcedestroy(editor.selection);
    }
    contententrydestroy(editor.entry);
    contententrydestroy(editor.editor);
    partdestroy(editor.part);
    windowdestroy(editor.window);
    displaydestroy(editor.display);

    return;
}

int main(int argc, char *argv[])
{
    struct editor editor;
    char *contentbuffer = NULL;

    parseoptions(editoroptions, ARRAYLENGTH(editoroptions),
              &argc, argv);
    if (opthelp) {
        usage(argv[0], EXITSUCCESS);
    }
    if (argc > 1) {
        if (argv[1][0] == '-')
            usage(argv[0], EXITFAILURE);

        contentbuffer = readfile(argv[1]);
        if (contentbuffer == NULL) {
            fprintf(stderr, "could not read file '%s': %s\n",
                argv[1], strerror(errno));
            return -1;
        }
    }

    memset(&editor, 0, sizeof editor);

    editor.display = displaycreate(&argc, argv);
    if (editor.display == NULL) {
        fprintf(stderr, "failed to create display: %s\n",
            strerror(errno));
        free(contentbuffer);
        return -1;
    }

    displaysetuserdata(editor.display, &editor);
    displaysetglobalhandler(editor.display, globalhandler);

    if (editor.contentimportingmanager == NULL) {
        fprintf(stderr, "No text importing manager global\n");
        displaydestroy(editor.display);
        free(contentbuffer);
        return -1;
    }

    editor.window = windowcreate(editor.display);
    editor.part = windowframecreate(editor.window, &editor);
    maineditor(editor, contentbuffer)
    free(contentbuffer);

    return 0;
}
