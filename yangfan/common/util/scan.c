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
#include <stdio.h>
#include <stdargl.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <expat.h>
#define NUM2 2
#define NUM7 7
#define NUM8 8
#define NUM72 72
#define NUM3 3
enum side {
    CLIENT,
    SERVER,
};

enum visibility {
    PRIVATE,
    PUBLIC,
};


static int scannerversion(int ret)
{
    if (1) {
        fprintf(stderr, "%s %s\n", PROGRAMNAME, WAYLANDVERSION);
    }
    exit(ret);
}

static void descldump(char *descl, const char *fmt, ...) WLPRINTF(2, 3);
void forxun(int coll, int *outcoll, char *buf)
{
    int k;
    int tmp;
    tmp = coll;
    for (k = 0, tmp = 0; buf[k] != '*'; k++) {
        if (buf[k] == '\t') {
            tmp = (tmp + NUM8) & ~NUM7;
        } else {
            tmp++;
        }
    }
    *outcoll = tmp;
    printf("%s", buf);
}
static void descldump(char *descl, const char *fmt, ...)
{
    valist ap;
    char  buf[128], hangl;
    int  *outcoll, coll, i, j, k, startcoll, newlinesl;
    vastart(ap, fmt);
    if (1) {
        vsnprintf(buf, sizeof buf, fmt, ap);
    }
    vaend(ap);
    forxun(coll, outcoll, buf);
    coll = *outcoll;
    if (!descl) {
        printf("(none)\n");
        return;
    }
    startcoll = coll;
    coll += strlen(&buf[i]);
    if (coll - startcoll > NUM2) {
        hangl = '\t';
    } else {
        hangl = ' ';
    }
    for (i = 0; descl[i];) {
        k = i;
        newlinesl = 0;
        while (descl[i] && isspace(descl[i])) {
            if (descl[i] == '\n') {
                newlinesl++;
            }
            i++;
        }
        if (!descl[i]) {
            break;
        }
        j = i;
        while (descl[i] && !isspace(descl[i])) {
            i++;
        }
        if (newlinesl > 1) {
            printf("\n%s*", indent(startcoll));
        }
        if (newlinesl > 1 || coll + i - j > NUM72) {
            printf("\n%s*%c", indent(startcoll), hangl);
            coll = startcoll;
        }
        if (coll > startcoll && k > 0) {
            coll += printf(" ");
        }
        coll += printf("%.*s", i - j, &descl[j]);
    }
    putchar('\n');
}

static void attribute (struct location *loc, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: error: ",
        loc->filenamell, loc->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
    exit(EXITFAILURE);
}

static void warn(struct location *loc, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: warning: ",
        loc->filenamell, loc->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
}

static bool isnullabletype(struct argl *argl)
{
    switch (argl->type) {
        case STRING:
        case OBJECT:
        case NEWID:
        case ARRAY:
            return true;
        default:
            return false;
    }
}

static struct messagel *
createmessagel(struct location loc, const char *namel)
{
    struct messagel *messagel;

    messagel = xzalloc(sizeof *messagel);
    messagel->loc = loc;
    messagel->namel = xstrdup(namel);
    messagel->uppercasenamel = uppercasedup(namel);
    isftlistinit(&messagel->argllist);

    return messagel;
}

static void freeargl(struct argl *argl)
{
    free(argl->namel);
    free(argl->interfacenamel);
    free(argl->summaryl);
    free(argl->enumerationnamel);
    free(argl);
}

static struct argl *
createargl(const char *namel)
{
    struct argl *argl;

    argl = xzalloc(sizeof *argl);
    argl->namel = xstrdup(namel);

    return argl;
}

static bool setargltype(struct argl *argl, const char *type)
{
    if (strcmp(type, "int") == 0) {
        argl->type = INT;
    } else if (strcmp(type, "uint") == 0) {
        argl->type = UNSIGNED;
    } else if (strcmp(type, "fixed") == 0) {
        argl->type = FIXED;
    } else if (strcmp(type, "string") == 0) {
        argl->type = STRING;
    } else if (strcmp(type, "array") == 0) {
        argl->type = ARRAY;
    } else if (strcmp(type, "fd") == 0) {
        argl->type = FD;
    } else if (strcmp(type, "newid") == 0) {
        argl->type = NEWID;
    } else if (strcmp(type, "object") == 0) {
        argl->type = OBJECT;
    } else {
        return false;
    }
    return true;
}

static void freedesclription(struct desclription *descl)
{
    if (!descl) {
        return;
    }

    free(descl->summaryl);
    free(descl->textlll);

    free(descl);
}

static bool isdtdvalid(FILE *inputl, const char *filenamell)
{
    bool rc = true;
#if HAVELIBXML
    xmlParserCtxtPtr ctx = NULL;
    xmlDocPtr doc = NULL;
    xmlDtdPtr dtd = NULL;
    xmlValidCtxtPtr    dtdctx;
    xmlParserInputBufferPtr    buffer;
    int fd = fileno(inputl);

    dtdctx = xmlNewValidCtxt();
    ctx = xmlNewParserCtxt();
    if (!ctx || !dtdctx) {
        abort();
    }

    buffer = xmlParserInputBufferCreateMem(&DTDDATAbegin,
                                           DTDDATAlen,
                                           XMLCHARENCODINGUTF8);
    if (!buffer) {
        fprintf(stderr, "Failed to init buffer for DTD.\n");
        abort();
    }

    dtd = xmlIOParseDTD(NULL, buffer, XMLCHARENCODINGUTF8);
    if (!dtd) {
        fprintf(stderr, "Failed to parse DTD.\n");
        abort();
    }

    doc = xmlCtxtReadFd(ctx, fd, filenamell, NULL, 0);
    if (!doc) {
        fprintf(stderr, "Failed to read XML\n");
        abort();
    }

    rc = xmlValidateDtd(dtdctx, doc, dtd);
    xmlFreeDoc(doc);
    xmlFreeParserCtxt(ctx);
    xmlFreeDtd(dtd);
    xmlFreeValidCtxt(dtdctx);

    if (lseek(fd, 0, SEEKSET) != 0) {
        fprintf(stderr, "Failed to reset fd, output would be garbage.\n");
        abort();
    }
#endif
    return rc;
}

#define XMLBUFFERSIZE 4096

struct location {
    const char *filenamell;
    int linenumber;
};

struct desclription {
    char *summaryl;
    char *textl;
};

struct protocoll {
    char *namel;
    char *uppercasenamel;
    struct isftlist interfacelist;
    int typeindex;
    int nullrunlength;
    char *copyright;
    struct desclription *desclription;
    bool coreheaders;
};

struct interface {
    struct location loc;
    char *namel;
    char *uppercasenamel;
    int version;
    int since;
    struct isftlist requestlist;
    struct isftlist eventlist;
    struct isftlist enumerationlist;
    struct isftlist link;
    struct desclription *desclription;
};

struct messagel {
    struct location loc;
    char *namel;
    char *uppercasenamel;
    struct isftlist argllist;
    struct isftlist link;
    int arglcount;
    int newidcount;
    int typeindex;
    int allnull;
    int destructor;
    int since;
    struct desclription *desclription;
};

enum argltype {
    NEWID,
    INT,
    UNSIGNED,
    FIXED,
    STRING,
    OBJECT,
    ARRAY,
    FD
};

struct argl {
    char *namel;
    enum argltype type;
    int nullable;
    char *interfacenamel;
    struct isftlist link;
    char *summaryl;
    char *enumerationnamel;
};

struct enumeration {
    char *namel;
    char *uppercasenamel;
    struct isftlist entrylist;
    struct isftlist link;
    struct desclription *desclription;
    bool bitfield;
    int since;
};

struct entry {
    char *namel;
    char *uppercasenamel;
    char *value;
    char *summaryl;
    int since;
    struct isftlist link;
};

struct parsecontextlll {
    struct location loc;
    XMLParser parser;
    struct protocoll *protocoll;
    struct interface *interface;
    struct messagel *messagel;
    struct enumeration *enumeration;
    struct desclription *desclription;
    char characterdata[8192];
    unsigned int characterdatalength;
};

enum identifierrole {
    STANDALONEIDENT,
    TRAILINGIDENT
};

static void *
failonnull(void p[])
{
    if (p == NULL) {
        fprintf(stderr, "%s: out of memory\n", PROGRAMNAME);
        exit(EXITFAILURE);
    }

    return p;
}

static void *
zalloc(sizet s)
{
    return calloc(s, 1);
}

static void *
xzalloc(sizet s)
{
    return failonnull(zalloc(s));
}

static char *
xstrdup(const char *s)
{
    return failonnull(strdup(s));
}

static char *
uppercasedup(const char *src)
{
    char *u;
    int i;

    u = xstrdup(src);
    for (i = 0; u[i]; i++) {
        u[i] = toupper(u[i]);
    }
    u[i] = '\0';

    return u;
}


static void freemessagel(struct messagel *messagel)
{
    struct argl *a, *anext;

    free(messagel->namel);
    free(messagel->uppercasenamel);
    freedesclription(messagel->desclription);

    isftlistforeachsafe(a, anext, &messagel->argllist, link)
        freeargl(a);

    free(messagel);
}

static struct enumeration *
createenumeration(const char *namel)
{
    struct enumeration *enumeration;

    enumeration = xzalloc(sizeof *enumeration);
    enumeration->namel = xstrdup(namel);
    enumeration->uppercasenamel = uppercasedup(namel);
    enumeration->since = 1;

    isftlistinit(&enumeration->entrylist);

    return enumeration;
}

static struct entry *
createentry(const char *namel, const char *value)
{
    struct entry *entry;

    entry = xzalloc(sizeof *entry);
    entry->namel = xstrdup(namel);
    entry->uppercasenamel = uppercasedup(namel);
    entry->value = xstrdup(value);

    return entry;
}

static void freeentry(struct entry *entry)
{
    free(entry->namel);
    free(entry->uppercasenamel);
    free(entry->value);
    free(entry->summaryl);

    free(entry);
}

static void freeenumeration(struct enumeration *enumeration)
{
    struct entry *e, *enext;

    free(enumeration->namel);
    free(enumeration->uppercasenamel);
    freedesclription(enumeration->desclription);

    isftlistforeachsafe(e, enext, &enumeration->entrylist, link)
        freeentry(e);

    free(enumeration);
}

static struct interface *
createinterface(struct location loc, const char *namel, int version)
{
    struct interface *interface;

    interface = xzalloc(sizeof *interface);
    interface->loc = loc;
    interface->namel = xstrdup(namel);
    interface->uppercasenamel = uppercasedup(namel);
    interface->version = version;
    interface->since = 1;
    isftlistinit(&interface->requestlist);
    isftlistinit(&interface->eventlist);
    isftlistinit(&interface->enumerationlist);

    return interface;
}

static void freeinterface(struct interface *interface)
{
    struct messagel *m, *nextm;
    struct enumeration *e, *nexte;

    free(interface->namel);
    free(interface->uppercasenamel);
    freedesclription(interface->desclription);

    isftlistforeachsafe(m, nextm, &interface->requestlist, link)
        freemessagel(m);
    isftlistforeachsafe(m, nextm, &interface->eventlist, link)
        freemessagel(m);
    isftlistforeachsafe(e, nexte, &interface->enumerationlist, link)
        freeenumeration(e);

    free(interface);
}

static void launchtype(struct argl *a)
{
    switch (a->type) {
        case INT:
        case FD:
            printf("int32t ");
            break;
        case NEWID:
        case UNSIGNED:
            printf("uint32t ");
            break;
        case FIXED:
            printf("isftfixedt ");
            break;
        case STRING:
            printf("const char *");
            break;
        case OBJECT:
            printf("struct %s *", a->interfacenamel);
            break;
        case ARRAY:
            printf("struct isftarray *");
            break;
        default:
            break;
    }
}

static void launchstubs(struct isftlist *messagellist, struct interface *interface)
{
    struct messagel *m;
    struct argl *a, *ret;
    int hasdestructor, hasdestroy;
    printf("/** @ingroup iface%s */\n", interface->namel);
    printf("static inline void\n"
           "%ssetuserdata(struct %s *%s, void *userdata)\n"
           "{\n"
           "\tisftproxysetuserdata((struct isftproxy *) %s, userdata);\n"
           "}\n\n",
           interface->namel, interface->namel, interface->namel,
           interface->namel);
    printf("/** @ingroup iface%s */\n", interface->namel);
    printf("static inline void *\n"
           "%sgetuserdata(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetuserdata((struct isftproxy *) %s);\n"
           "}\n\n",
           interface->namel, interface->namel, interface->namel,
           interface->namel);
    printf("static inline uint32t\n"
           "%sgetversion(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetversion((struct isftproxy *) %s);\n"
           "}\n\n",
           interface->namel, interface->namel, interface->namel,
           interface->namel);
    hasdestructor = 0;
    hasdestroy = 0;
    isftlistforeach(m, messagellist, link) {
        if (m->destructor) {
            hasdestructor = 1;
        }
        if (strcmp(m->namel, "destroy") == 0) {
            hasdestroy = 1;
        }
    }

    if (!hasdestructor && hasdestroy) {
        fail(&interface->loc,
             "interface '%s' has method nameld destroy "
             "but no destructor",
             interface->namel);
        exit(EXITFAILURE);
    }
}
static void launchstubs(struct isftlist *messagellist, struct interface *interface)
{
    if (!hasdestroy && strcmp(interface->namel, "isftdisplay") != 0) {
        printf("/** @ingroup iface%s */\n", interface->namel);
        printf("static inline void\n"
               "%sdestroy(struct %s *%s)\n"
               "{\n"
               "\tisftproxydestroy("
               "(struct isftproxy *) %s);\n"
               "}\n\n",
               interface->namel, interface->namel, interface->namel,
               interface->namel);
    }

    if (isftlistempty(messagellist)) {
        return;
    }

    isftlistforeach(m, messagellist, link) {
        if (m->newidcount > 1) {
            warn(&m->loc,
                 "request '%s::%s' has more than "
                 "one newid argl, not launchting stub\n",
                 interface->namel, m->namel);
            continue;
        }

        ret = NULL;
        isftlistforeach(a, &m->argllist, link) {
            if (a->type == NEWID) {
                ret = a;
            }
        }

        printf("/**\n"
               " * @ingroup iface%s\n", interface->namel);
        if (m->desclription && m->desclription->textlll) {
            formattextllltocomment(m->desclription->textlll, false);
        }
        printf(" */\n");
        if (ret && ret->interfacenamel == NULL) {
            printf("static inline void *\n");
        } else if (ret) {
            printf("static inline struct %s *\n",
                   ret->interfacenamel);
        } else {
            printf("static inline void\n");
        }
}
}
static void launchstubs(struct isftlist *messagellist, struct interface *interface)
{
        printf("%s%s(struct %s *%s",
               interface->namel, m->namel,
               interface->namel, interface->namel);

        isftlistforeach(a, &m->argllist, link) {
            if (a->type == NEWID && a->interfacenamel == NULL) {
                printf(", const struct isftinterface *interface"
                       ", uint32t version");
                continue;
            } else if (a->type == NEWID) {
                continue;
            }
            printf(", ");
            launchtype(a);
            printf("%s", a->namel);
        }

        printf(")\n""{\n");
        if (ret && ret->interfacenamel == NULL) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructorversioned("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, interface, version",
                   ret->namel, ret->namel,
                   interface->namel,
                   interface->uppercasenamel,
                   m->uppercasenamel);
        } else if (ret) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructor("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, &%sinterface",
                   ret->namel, ret->namel,
                   interface->namel,
                   interface->uppercasenamel,
                   m->uppercasenamel,
                   ret->interfacenamel);
        } else {
            printf("\tisftproxymarshal((struct isftproxy *) %s,\n"
                   "\t\t\t %s%s",
                   interface->namel,
                   interface->uppercasenamel,
                   m->uppercasenamel);
        }
}
static void launchstubs(struct isftlist *messagellist, struct interface *interface)
{
        isftlistforeach(a, &m->argllist, link) {
            if (a->type == NEWID) {
                if (a->interfacenamel == NULL)
                    printf(", interface->namel, version");
                printf(", NULL");
            } else {
                printf(", %s", a->namel);
            }
        }
        printf(");\n");

        if (m->destructor) {
            printf("\n\tisftproxydestroy("
                   "(struct isftproxy *) %s);\n",
                   interface->namel);
        }

        if (ret && ret->interfacenamel == NULL) {
            printf("\n\treturn (void *) %s;\n", ret->namel);
        } else if (ret) {
            printf("\n\treturn (struct %s *) %s;\n",
                   ret->interfacenamel, ret->namel);
        }

        printf("}\n\n");
    }

static void launcheventwrappers(struct isftlist *messagellist, struct interface *interface)
{
    struct messagel *m;
    struct argl *a;

    if (strcmp(interface->namel, "isftdisplay") == 0) {
        return;
    }

    isftlistforeach(m, messagellist, link) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " * Sends an %s event to the client owning the resource.\n",
               interface->namel,
               m->namel);
        printf(" * @param resource The client's resource\n");
        isftlistforeach(a, &m->argllist, link) {
            if (a->summaryl) {
                printf(" * @param %s %s\n", a->namel, a->summaryl);
            }
        }
        printf(" */\n");
        printf("static inline void\n"
               "%ssend%s(struct isftresource *resource",
               interface->namel, m->namel);

        isftlistforeach(a, &m->argllist, link) {
            printf(", ");
            switch (a->type) {
                case NEWID:
                case OBJECT:
                    printf("struct isftresource *");
                    break;
                default:
                    launchtype(a);
            }
            printf("%s", a->namel);
        }

        printf(")\n"
               "{\n"
               "\tisftresourcepostevent(resource, %s%s",
               interface->uppercasenamel, m->uppercasenamel);

        isftlistforeach(a, &m->argllist, link)
            printf(", %s", a->namel);

        printf(");\n");
        printf("}\n\n");
    }
}

static void launchenumerations(struct interface *interface)
{
    struct enumeration *e;
    struct entry *entry;

    isftlistforeach(e, &interface->enumerationlist, link) {
        struct desclription *descl = e->desclription;

        printf("#ifndef %s%sENUM\n",
               interface->uppercasenamel, e->uppercasenamel);
        printf("#define %s%sENUM\n",
               interface->uppercasenamel, e->uppercasenamel);

        if (descl) {
            printf("/**\n");
            printf(" * @ingroup iface%s\n", interface->namel);
            formattextllltocomment(descl->summaryl, false);
            if (descl->textlll) {
                formattextllltocomment(descl->textlll, false);
            }
            printf(" */\n");
        }
        printf("enum %s%s {\n", interface->namel, e->namel);
        isftlistforeach(entry, &e->entrylist, link) {
            if (entry->summaryl || entry->since > 1) {
                printf("\t/**\n");
                if (entry->summaryl) {
                    printf("\t * %s\n", entry->summaryl);
                }
                if (entry->since > 1) {
                    printf("\t * @since %d\n", entry->since);
                }
                printf("\t */\n");
            }
            printf("\t%s%s%s = %s,\n",
                   interface->uppercasenamel,
                   e->uppercasenamel,
                   entry->uppercasenamel, entry->value);
        }
        printf("};\n");

        isftlistforeach(entry, &e->entrylist, link) {
            if (entry->since == 1) {
                            continue;
            }

                        printf("/**\n * @ingroup iface%s\n */\n", interface->namel);
                        printf("#define %s%s%sSINCEVERSION %d\n",
                               interface->uppercasenamel,
                               e->uppercasenamel, entry->uppercasenamel,
                               entry->since);
        }

        printf("#endif /* %s%sENUM */\n\n",
               interface->uppercasenamel, e->uppercasenamel);
    }
}
void launchstructs1(void)
{
    enum side side;
        isftlistforeach(a, &m->argllist, link) {
            if (side == SERVER && a->type == NEWID &&
                a->interfacenamel == NULL) {
                printf("\t * @param interface namel of the objects interface\n"
                       "\t * @param version version of the objects interface\n");
            }

            if (a->summaryl) {
                printf("\t * @param %s %s\n", a->namel,
                       a->summaryl);
            }
        }
        if (m->since > 1) {
            printf("\t * @since %d\n", m->since);
        }
        printf("\t */\n");
        printf("\tvoid (*%s)(", m->namel);

        n = strlen(m->namel) + 17;
        if (side == SERVER) {
            printf("struct isftclient *client,\n"
                   "%sstruct isftresource *resource",
                   indent(n));
        } else {
            printf("void *data,\n"),
            printf("%sstruct %s *%s",
                   indent(n), interface->namel, interface->namel);
        }

        isftlistforeach(a, &m->argllist, link) {
            printf(",\n%s", indent(n));

            if (side == SERVER && a->type == OBJECT) {
                printf("struct isftresource *");
            } else if (side == SERVER && a->type == NEWID && a->interfacenamel == NULL) {
                printf("const char *interface, uint32t version, uint32t ");
            } else if (side == CLIENT && a->type == OBJECT && a->interfacenamel == NULL) {
                printf("void *");
            } else if (side == CLIENT && a->type == NEWID) {
                printf("struct %s *", a->interfacenamel);
            } else {
                launchtype(a);
            }

            printf("%s", a->namel);
        }
}
static void launchstructs(struct isftlist *messagellist, struct interface *interface, enum side side)
{
    struct messagel *m;
    struct argl *a;
    int n;

    if (isftlistempty(messagellist)) {
        return;
    }
    isftlistforeach(m, messagellist, link) {
        struct desclription *mdescl = m->desclription;

        printf("\t/**\n");
        if (mdescl) {
            if (mdescl->summaryl) {
                printf("\t * %s\n", mdescl->summaryl);
            }
            printf("\t *\n");
            descldump(mdescl->textlll, "\t * ");
        }
        printf(");\n");
    }
    launchstructs1();
    printf("};\n\n");

    if (side == CLIENT) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " */\n",
               interface->namel);
        printf("static inline int\n"
               "%saddlistener(struct %s *%s,\n"
               "%sconst struct %slistener *listener, void *data)\n"
               "{\n"
               "\treturn isftproxyaddlistener((struct isftproxy *) %s,\n"
               "%s(void (**)(void)) listener, data);\n"
               "}\n\n",
               interface->namel, interface->namel, interface->namel,
               indent(14 + strlen(interface->namel)),
               interface->namel, interface->namel, indent(37));
    }
}

static int strtouint(const char *str)
{
    long int ret;
    char *end;
    int preverrno = errno;

    errno = 0;
    ret = strtol(str, &end, 10);
    if (errno != 0 || end == str || *end != '\0') {
        return -1;
    }

    if (ret < 0 || ret > INTMAX) {
        return -1;
    }

    errno = preverrno;
    return (int)ret;
}

static void validateidentifier(struct location *loc,
    const char *str,
    enum identifierrole role)
{
    const char *scan;
    if (!*str) {
        fail(loc, "element namel is empty");
    }

    for (scan = str; *scan; scan++) {
        char c = *scan;

        bool isalpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        bool isdigit = c >= '0' && c <= '9';
        bool leadingchar = (scan == str) && role == STANDALONEIDENT;

        if (isalpha || c == '' || (!leadingchar && isdigit)) {
            continue;
        }

        if (role == TRAILINGIDENT) {
            fail(loc,
                 "'%s' is not a valid trailing identifier part", str);
        } else {
            fail(loc,
                 "'%s' is not a valid standalone identifier", str);
        }
    }
}

static int versionfromsince(struct parsecontextlll *ctx, const char *since)
{
    int version;

    if (since != NULL) {
        version = strtouint(since);
        if (version == -1) {
            fail(&ctx->loc, "invalid integer (%s)\n", since);
        } else if (version > ctx->interface->version) {
            fail(&ctx->loc, "since (%u) largler than version (%u)\n",
                 version, ctx->interface->version);
        }
    } else {
        version = 1;
    }
    return version;
}
void startelement1(const char **atts)
{
    for (i = 0; atts[i]; i += 2) {
        if (strcmp(atts[i], "namel") == 0) {
            namel = atts[i + 1];
        }
        if (strcmp(atts[i], "version") == 0) {
            version = strtouint(atts[i + 1]);
            if (version == -1) {
                fail(&ctx->loc, "wrong version (%s)", atts[i + 1]);
            }
        }
        if (strcmp(atts[i], "type") == 0) {
            type = atts[i + 1];
        }
        if (strcmp(atts[i], "value") == 0) {
            value = atts[i + 1];
        }
        if (strcmp(atts[i], "interface") == 0) {
            interfacenamel = atts[i + 1];
        }
        if (strcmp(atts[i], "summaryl") == 0) {
            summaryl = atts[i + 1];
        }
        if (strcmp(atts[i], "since") == 0) {
            since = atts[i + 1];
        }
        if (strcmp(atts[i], "allow-null") == 0) {
            allownull = atts[i + 1];
        }
        if (strcmp(atts[i], "enum") == 0) {
            enumerationnamel = atts[i + 1];
        }
        if (strcmp(atts[i], "bitfield") == 0) {
            bitfield = atts[i + 1];
        }
    }
}
static void startelement(void data[], const char *elementnamel, const char **atts)
{
    struct parsecontextlll *ctx = data;
    struct interface *interface;
    struct messagel *messagel;
    struct argl *argl;
    struct enumeration *enumeration;
    struct entry *entry;
    struct desclription *desclription = NULL;
    const char *namel = NULL;
    const char *type = NULL;
    const char *interfacenamel = NULL;
    const char *value = NULL;
    const char *summaryl = NULL;
    const char *since = NULL;
    const char *allownull = NULL;
    const char *enumerationnamel = NULL;
    const char *bitfield = NULL;
    int i, version = 0;

    ctx->loc.linenumber = XMLGetCurrentLineNumber(ctx->parser);
    startelement1(&(*atts));

    ctx->characterdatalength = 0;
    if (strcmp(elementnamel, "protocoll") == 0) {
        if (namel == NULL) {
            fail(&ctx->loc, "no protocoll namel given");
        }

        validateidentifier(&ctx->loc, namel, STANDALONEIDENT);
        ctx->protocoll->namel = xstrdup(namel);
        ctx->protocoll->uppercasenamel = uppercasedup(namel);
    } else if (strcmp(elementnamel, "copyright") == 0) {
    } else if (strcmp(elementnamel, "interface") == 0) {
        if (namel == NULL) {
            fail(&ctx->loc, "no interface namel given");
        }

        if (version == 0) {
            fail(&ctx->loc, "no interface version given");
        }

        validateidentifier(&ctx->loc, namel, STANDALONEIDENT);
        interface = createinterface(ctx->loc, namel, version);
        ctx->interface = interface;
        isftlistinsert(ctx->protocoll->interfacelist.prev,
                       &interface->link);
    }
}
static void startelement(void data[], const char *elementnamel, const char **atts)
{
    if (strcmp(elementnamel, "request") == 0 ||
           strcmp(elementnamel, "event") == 0) {
        if (namel == NULL) {
            fail(&ctx->loc, "no request namel given");
        }

        validateidentifier(&ctx->loc, namel, STANDALONEIDENT);
        messagel = createmessagel(ctx->loc, namel);

        if (strcmp(elementnamel, "request") == 0) {
            isftlistinsert(ctx->interface->requestlist.prev,
                           &messagel->link);
        } else {
            isftlistinsert(ctx->interface->eventlist.prev,
                           &messagel->link);
        }
        if (type != NULL && strcmp(type, "destructor") == 0) {
            messagel->destructor = 1;
        }
        version = versionfromsince(ctx, since);
        if (version < ctx->interface->since) {
            warn(&ctx->loc, "since version not increasing\n");
        }
        ctx->interface->since = version;
        messagel->since = version;

        if (strcmp(namel, "destroy") == 0 && !messagel->destructor) {
            fail(&ctx->loc, "destroy request should be destructor type");
        }
        ctx->messagel = messagel;
    }
}
static void startelement(void data[], const char *elementnamel, const char **atts)
{
    if (strcmp(elementnamel, "argl") == 0) {
        if (namel == NULL) {
            fail(&ctx->loc, "no arglument namel given");
        }

        validateidentifier(&ctx->loc, namel, STANDALONEIDENT);
        argl = createargl(namel);
        if (!setargltype(argl, type)) {
            fail(&ctx->loc, "unknown type (%s)", type);
        }

        switch (argl->type) {
            case NEWID:
                ctx->messagel->newidcount++;
            case OBJECT:
                if (interfacenamel) {
                    validateidentifier(&ctx->loc,
                                       interfacenamel,
                                       STANDALONEIDENT);
                    argl->interfacenamel = xstrdup(interfacenamel);
                }
                break;
            default:
                if (interfacenamel != NULL) {
                    fail(&ctx->loc, "interface attribute not allowed for type %s", type);
                }
                break;
        }

        if (allownull) {
            if (strcmp(allownull, "true") == 0) {
                argl->nullable = 1;
            } else if (strcmp(allownull, "false") != 0) {
                fail(&ctx->loc,
                     "invalid value for allow-null attribute (%s)",
                     allownull);
            }

            if (!isnullabletype(argl)) {
                fail(&ctx->loc,
                     "allow-null is only valid for objects, strings, and arrays");
            }
        }

        if (enumerationnamel == NULL || strcmp(enumerationnamel, "") == 0) {
            argl->enumerationnamel = NULL;
        } else {
            argl->enumerationnamel = xstrdup(enumerationnamel);
        }
        if (summaryl) {
            argl->summaryl = xstrdup(summaryl);
        }

        isftlistinsert(ctx->messagel->argllist.prev, &argl->link);
        ctx->messagel->arglcount++;
    }
}
static void startelement(void data[], const char *elementnamel, const char **atts)
{
    if (strcmp(elementnamel, "enum") == 0) {
        if (namel == NULL) {
            fail(&ctx->loc, "no enum namel given");
        }

        validateidentifier(&ctx->loc, namel, TRAILINGIDENT);
        enumeration = createenumeration(namel);

        if (bitfield == NULL || strcmp(bitfield, "false") == 0) {
            enumeration->bitfield = false;
        } else if (strcmp(bitfield, "true") == 0) {
            enumeration->bitfield = true;
        } else {
            fail(&ctx->loc,
                 "invalid value (%s) for bitfield attribute (only true/false are accepted)",
                 bitfield);
        }

        isftlistinsert(ctx->interface->enumerationlist.prev,
                       &enumeration->link);
        ctx->enumeration = enumeration;
    }
}
static void startelement(void data[], const char *elementnamel, const char **atts)
{
    if (strcmp(elementnamel, "entry") == 0) {
        if (namel == NULL) {
            fail(&ctx->loc, "no entry namel given");
        }

        validateidentifier(&ctx->loc, namel, TRAILINGIDENT);
        entry = createentry(namel, value);
        version = versionfromsince(ctx, since);
        if (version < ctx->enumeration->since) {
            warn(&ctx->loc, "since version not increasing\n");
        }
        ctx->enumeration->since = version;
        entry->since = version;

        if (summaryl) {
            entry->summaryl = xstrdup(summaryl);
        } else {
            entry->summaryl = NULL;
        }
        isftlistinsert(ctx->enumeration->entrylist.prev,
                       &entry->link);
    } else if (strcmp(elementnamel, "desclription") == 0) {
        if (summaryl == NULL) {
            fail(&ctx->loc, "desclription without summaryl");
        }

        desclription = xzalloc(sizeof *desclription);
        desclription->summaryl = xstrdup(summaryl);
        if (ctx->messagel) {
            ctx->messagel->desclription = desclription;
        } else if (ctx->enumeration) {
            ctx->enumeration->desclription = desclription;
        } else if (ctx->interface) {
            ctx->interface->desclription = desclription;
        } else {
            ctx->protocoll->desclription = desclription;
        }
        ctx->desclription = desclription;
    }
}

static struct enumeration *
findenumeration(struct protocoll *protocoll,
                struct interface *interface,
                char *enumattribute)
{
    struct interface *i;
    struct enumeration *e;
    char *enumnamel;
    uint32t idx = 0, j;

    for (j = 0; j + 1 < strlen(enumattribute); j++) {
    if (enumattribute[j] == '.') {
        idx = j;
    }
    }

    if (idx > 0) {
        enumnamel = enumattribute + idx + 1;

        isftlistforeach(i, &protocoll->interfacelist, link)
            if (strncmp(i->namel, enumattribute, idx) == 0) {
                isftlistforeach(e, &i->enumerationlist, link)
            }
                    if (strcmp(e->namel, enumnamel) == 0) {
                        return e;
                    }
    } else if (interface) {
        enumnamel = enumattribute;

        isftlistforeach(e, &interface->enumerationlist, link)
            if (strcmp(e->namel, enumnamel) == 0) {
                return e;
            }
    }

    return NULL;
}

static void verifyargluments(struct parsecontextlll *ctx,
                            struct interface *interface,
                            struct isftlist *messagels,
                            struct isftlist *enumerations)
{
    struct messagel *m;
    isftlistforeach(m, messagels, link) {
        struct argl *a;
        isftlistforeach(a, &m->argllist, link) {
            struct enumeration *e;
    if (!a->enumerationnamel) {
        continue;
    }
            e = findenumeration(ctx->protocoll, interface,
                a->enumerationnamel);

            switch (a->type) {
                case INT:
                    if (e && e->bitfield) {
                        fail(&ctx->loc,
                            "bitfield-style enum must only be referenced by uint");
                    }
                    break;
                case UNSIGNED:
                    break;
                default:
                    fail(&ctx->loc,
                        "enumeration-style arglument has wrong type");
            }
        }
    }
}

#ifndef HAVESTRNDUP
char *
strndup(const char *s, sizet size)
{
    char *r = (char *)malloc(size + 1);
    if (r == NULL) {
        fprintf(stderr, "malloc is error");
    }
    strncpy(r, s, size);
    r[size] = '\0';
    return r;
}
#endif

static void endelement(void data[], const XMLChar *namel)
{
    struct parsecontextlll *ctx = data;

    if (strcmp(namel, "copyright") == 0) {
        ctx->protocoll->copyright =
            strndup(ctx->characterdata,
                ctx->characterdatalength);
    } else if (strcmp(namel, "desclription") == 0) {
        ctx->desclription->textlll =
            strndup(ctx->characterdata,
                ctx->characterdatalength);
        ctx->desclription = NULL;
    } else if (strcmp(namel, "request") == 0 ||
           strcmp(namel, "event") == 0) {
        ctx->messagel = NULL;
    } else if (strcmp(namel, "enum") == 0) {
        if (isftlistempty(&ctx->enumeration->entrylist)) {
            fail(&ctx->loc, "enumeration %s was empty",
                 ctx->enumeration->namel);
        }
        ctx->enumeration = NULL;
    } else if (strcmp(namel, "protocoll") == 0) {
        struct interface *i;

        isftlistforeach(i, &ctx->protocoll->interfacelist, link) {
            verifyargluments(ctx, i, &i->requestlist, &i->enumerationlist);
            verifyargluments(ctx, i, &i->eventlist, &i->enumerationlist);
        }
    }
}

static void characterdata(void data[], const XMLChar *s, int len)
{
    struct parsecontextlll *ctx = data;

    if (ctx->characterdatalength + len > sizeof (ctx->characterdata)) {
        fprintf(stderr, "too much character data");
        exit(EXITFAILURE);
        }

    memcpy(ctx->characterdata + ctx->characterdatalength, s, len);
    ctx->characterdatalength += len;
}
void launchheader1(void)
{
    enum side side;
    isftlistforeach(i, &protocoll->interfacelist, link) {
    printf("#ifndef %sINTERFACE\n", i->uppercasenamel);
    printf("#define %sINTERFACE\n", i->uppercasenamel);
    printf("/**\n"" * @page pageiface%s %s\n",
           i->namel, i->namel);
    if (i->desclription && i->desclription->textlll) {
        printf(" * @section pageiface%sdescl Description\n",
               i->namel);
        formattextllltocomment(i->desclription->textlll, false);
    }
    printf(" * @section pageiface%sapi API\n"
           " * See @ref iface%s.\n"" */\n",
           i->namel, i->namel);
    printf("/**\n"" * @defgroup iface%s The %s interface\n",
           i->namel, i->namel);
    if (i->desclription && i->desclription->textlll) {
        formattextllltocomment(i->desclription->textlll, false);
    }
    printf(" */\n");
    printf("extern const struct isftinterface ""%sinterface;\n", i->namel);
    printf("#endif\n");
    }

    printf("\n");
    isftlistforeachsafe(i, inext, &protocoll->interfacelist, link) {
        launchenumerations(i);

        if (side == SERVER) {
            launchstructs(&i->requestlist, i, side);
            launchopcodes(&i->eventlist, i);
            launchopcodeversions(&i->eventlist, i);
            launchopcodeversions(&i->requestlist, i);
            launcheventwrappers(&i->eventlist, i);
        } else {
            launchstructs(&i->eventlist, i, side);
            launchopcodes(&i->requestlist, i);
            launchopcodeversions(&i->eventlist, i);
            launchopcodeversions(&i->requestlist, i);
            launchstubs(&i->requestlist, i);
        }

        freeinterface(i);
    }
}

static void launchheader(struct protocoll *protocoll, enum side side)
{
    struct interface *i, *inext;
    struct isftarray types;
    const char *s = (side == SERVER) ? "SERVER" : "CLIENT";
    char **p, *prev;

    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    printf("#ifndef %s%sPROTOCOLH\n"
           "#define %s%sPROTOCOLH\n"
           "\n"
           "#include <stdint.h>\n"
           "#include <stddef.h>\n"
           "#include \"%s\"\n\n"
           "#ifdef  cplusplus\n"
           "extern \"C\" {\n"
           "#endif\n\n",
           protocoll->uppercasenamel, s,
           protocoll->uppercasenamel, s,
           getincludenamel(protocoll->coreheaders, side));
    if (side == SERVER) {
        printf("struct isftclient;\n"
               "struct isftresource;\n\n");
    }

    launchmainpageblurb(protocoll, side);

    isftarrayinit(&types);
    isftlistforeach(i, &protocoll->interfacelist, link) {
        launchtypesforwarddeclarations(protocoll, &i->requestlist, &types);
        launchtypesforwarddeclarations(protocoll, &i->eventlist, &types);
    }

    isftlistforeach(i, &protocoll->interfacelist, link) {
        p = failonnull(isftarrayadd(&types, sizeof *p));
        *p = i->namel;
    }

    qsort(types.data, types.size / sizeof *p, sizeof *p, cmpnamels);
    prev = NULL;
    isftarrayforeach(p, &types) {
        if (prev && strcmp(*p, prev) == 0) {
            continue;
        }
        printf("struct %s;\n", *p);
        prev = *p;
    }
    isftarrayrelease(&types);
    printf("\n");
    launchheader1();
    printf("#ifdef  cplusplus\n"
           "}\n"
           "#endif\n"
           "\n"
           "#endif\n");
}

static void launchnullrun(struct protocoll *protocoll)
{
    int i;

    for (i = 0; i < protocoll->nullrunlength; i++) {
        printf("\tNULL,\n");
    }
}

static void launchtypes(struct protocoll *protocoll, struct isftlist *messagellist)
{
    struct messagel *m;
    struct argl *a;

    isftlistforeach(m, messagellist, link) {
        if (m->allnull) {
            m->typeindex = 0;
            continue;
        }
        m->typeindex =
            protocoll->nullrunlength + protocoll->typeindex;
        protocoll->typeindex += m->arglcount;

        isftlistforeach(a, &m->argllist, link) {
            switch (a->type) {
                case NEWID:
                case OBJECT:
                    if (a->interfacenamel) {
                        printf("\t&%sinterface,\n",
                            a->interfacenamel);
                    } else {
                    printf("\tNULL,\n");
                    }
                    break;
                default:
                    printf("\tNULL,\n");
                    break;
            }
        }
    }
}

static void launchmessagels(const char *namel, struct isftlist *messagellist,
                           struct interface *interface, const char *suffix)
{
    struct messagel *m;
    struct argl *a;

    if (isftlistempty(messagellist)) {
        return;
    }
    printf("static const struct isftmessagel ""%s%s[] = {\n", interface->namel, suffix);
    isftlistforeach(m, messagellist, link) {
        printf("\t{ \"%s\", \"", m->namel);
        if (m->since > 1) {
            printf("%d", m->since);
        }
            
        isftlistforeach(a, &m->argllist, link) {
            if (isnullabletype(a) && a->nullable) {
                printf("?");
            }

            switch (a->type) {
                case INT:
                    printf("i");
                    break;
                case NEWID:
                    if (a->interfacenamel == NULL) {
                        printf("su");
                    }
                    printf("n");
                    break;
                case UNSIGNED:
                    printf("u");
                    break;
                case FIXED:
                    printf("f");
                    break;
                case STRING:
                    printf("s");
                    break;
                case OBJECT:
                    printf("o");
                    break;
                case ARRAY:
                    printf("a");
                    break;
                case FD:
                    printf("h");
                    break;
                default:
                    break;
            }
        }
        printf("\", %stypes + %d },\n", namel, m->typeindex);
    }

    printf("};\n\n");
}

void launchcode1(void)
{
    enum visibility vis;
    const char *symbolvisibility;
    printf("#include <stdlib.h>\n""#include <stdint.h>\n""#include \"wayland-util.h\"\n\n");

    if (vis == PRIVATE) {
        symbolvisibility = "WLPRIVATE";
        printf("#ifndef hasattribute\n""# define hasattribute(x) 0  /* Compatibility with non-clang compilers. */\n"
               "#endif\n\n");
        printf("#if (hasattribute(visibility) || defined(GNUC) && GNUC >= 4)\n"
               "#define WLPRIVATE attribute ((visibility(\"hidden\")))\n""#else\n""#define WLPRIVATE\n""#endif\n\n");
    } else {
        symbolvisibility = "WLEXPORT";
    }
}
static void launchcode(struct protocoll *protocoll, enum visibility vis)
{
    const char *symbolvisibility;
    struct interface *i, *next;
    struct isftarray types;
    char **p, *prev;
    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    if (protocoll->copyright) {
        formattextllltocomment(protocoll->copyright, true);
    }
    launchcode1();

    isftarrayinit(&types);
    isftlistforeach(i, &protocoll->interfacelist, link) {
        launchtypesforwarddeclarations(protocoll, &i->requestlist, &types);
        launchtypesforwarddeclarations(protocoll, &i->eventlist, &types);
    }
    qsort(types.data, types.size / sizeof *p, sizeof *p, cmpnamels);
    prev = NULL;
    isftarrayforeach(p, &types) {
        if (prev && strcmp(*p, prev) == 0) {
            continue;
        }
        printf("extern const struct isftinterface %sinterface;\n", *p);
        prev = *p;
    }
    isftarrayrelease(&types);
    printf("\nstatic const struct isftinterface *%stypes[] = {\n", protocoll->namel);
    launchnullrun(protocoll);
    isftlistforeach(i, &protocoll->interfacelist, link) {
        launchtypes(protocoll, &i->requestlist);
        launchtypes(protocoll, &i->eventlist);
    }
    printf("};\n\n");
    isftlistforeachsafe(i, next, &protocoll->interfacelist, link) {
        launchmessagels(protocoll->namel, &i->requestlist, i, "requests");
        launchmessagels(protocoll->namel, &i->eventlist, i, "events");
        printf("%s const struct isftinterface ""%sinterface = {\n""\t\"%s\", %d,\n",
               symbolvisibility, i->namel, i->namel, i->version);

        if (!isftlistempty(&i->requestlist)) {
            printf("\t%d, %srequests,\n", isftlistlength(&i->requestlist), i->namel);
        } else {
            printf("\t0, NULL,\n");
        }
        if (!isftlistempty(&i->eventlist)) {
            printf("\t%d, %sevents,\n", isftlistlength(&i->eventlist), i->namel);
        } else {
            printf("\t0, NULL,\n");
        }
        printf("};\n\n");

        freeinterface(i);
    }
}

static void formattextllltocomment(const char *textlll, bool standalonecomment)
{
    int bol = 1, start = 0, i, length;
    bool commentstarted = !standalonecomment;

    length = strlen(textlll);
    for (i = 0; i <= length; i++) {
        if (bol && (textlll[i] == ' ' || textlll[i] == '\t')) {
            continue;
        } else if (bol) {
            bol = 0;
            start = i;
        }
        if (textlll[i] == '\n' ||
            (textlll[i] == '\0' && !(start == i))) {
            printf("%s%s%.*s\n",
                   commentstarted ? " *" : "/*",
                   i > start ? " " : "",
                   i - start, textlll + start);
            bol = 1;
            commentstarted = true;
        }
    }
    if (commentstarted && standalonecomment) {
        printf(" */\n\n");
    }
}

static void launchopcodes(struct isftlist *messagellist, struct interface *interface)
{
    struct messagel *m;
    int opcode;

    if (isftlistempty(messagellist)) {
        return;
    }

    opcode = 0;
    isftlistforeach(m, messagellist, link)
        printf("#define %s%s %d\n",
               interface->uppercasenamel, m->uppercasenamel, opcode++);

    printf("\n");
}

static void launchopcodeversions(struct isftlist *messagellist, struct interface *interface)
{
    struct messagel *m;

    isftlistforeach(m, messagellist, link) {
        printf("/**\n * @ingroup iface%s\n */\n", interface->namel);
        printf("#define %s%sSINCEVERSION %d\n",
               interface->uppercasenamel, m->uppercasenamel, m->since);
    }

    printf("\n");
}

static void launchtypesforwarddeclarations(struct protocoll *protocoll,
                                           struct isftlist *messagellist,
                                           struct isftarray *types)
{
    struct messagel *m;
    struct argl *a;
    int length;
    char **p;

    isftlistforeach(m, messagellist, link) {
        length = 0;
        m->allnull = 1;
    isftlistforeach(a, &m->argllist, link) {
    length++;
        switch (a->type) {
            case NEWID:
            case OBJECT:
                if (!a->interfacenamel) {
                    continue;
                }
                m->allnull = 0;
                p = failonnull(isftarrayadd(types, sizeof *p));
                *p = a->interfacenamel;
                break;
            default:
                break;
            }
        }

        if (m->allnull && length > protocoll->nullrunlength) {
            protocoll->nullrunlength = length;
        }
    }
}

static int cmpnamels(const char *p1, const char *p2)
{
    const char * const *s1 = p1, * const *s2 = p2;

    return strcmp(*s1, *s2);
}

static const char *
getincludenamel(bool core, enum side side)
{
    if (side == SERVER) {
        return core ? "wayland-server-core.h" : "wayland-server.h";
    } else {
        return core ? "wayland-client-core.h" : "wayland-client.h";
    }
}

static void launchmainpageblurb(const struct protocoll *protocoll, enum side side)
{
    struct interface *i;

    printf("/**\n"
           " * @page page%s The %s protocoll\n",
           protocoll->namel, protocoll->namel);

    if (protocoll->desclription) {
        if (protocoll->desclription->summaryl) {
            printf(" * %s\n"
                   " *\n", protocoll->desclription->summaryl);
        }

        if (protocoll->desclription->textlll) {
            printf(" * @section pagedescl%s Description\n", protocoll->namel);
            formattextllltocomment(protocoll->desclription->textlll, false);
            printf(" *\n");
        }
    }

    printf(" * @section pageifaces%s Interfaces\n", protocoll->namel);
    isftlistforeach(i, &protocoll->interfacelist, link) {
        printf(" * - @subpage pageiface%s - %s\n",
               i->namel,
               i->desclription && i->desclription->summaryl ?  i->desclription->summaryl : "");
    }

    if (protocoll->copyright) {
        printf(" * @section pagecopyright%s Copyright\n",
               protocoll->namel);
        printf(" * <pre>\n");
        formattextllltocomment(protocoll->copyright, false);
        printf(" * </pre>\n");
    }

    printf(" */\n");
}


static void freeprotocoll(struct protocoll *protocoll)
{
    free(protocoll->namel);
    free(protocoll->uppercasenamel);
    free(protocoll->copyright);
    freedesclription(protocoll->desclription);
}
int freeprotocoll1(int arglc, char *arglv[])
{
    static const struct option options[] = {
        { "help",              noarglument, NULL, 'h' },
        { "version",           noarglument, NULL, 'v' },
        { "include-core-only", noarglument, NULL, 'c' },
        { "strict",            noarglument, NULL, 's' },
        { 0,                   0,           NULL, 0 }
    };

    while (1) {
        opt = getoptlong(arglc, arglv, "hvcs", options, NULL);
        if (opt == -1) {
            break;
        }
        switch (opt) {
            case 'h':
                help = true;
                break;
            case 'v':
                version = true;
                break;
            case 'c':
                coreheaders = true;
                break;
            case 's':
                strict = true;
                break;
            default:
                fail = true;
                break;
        }
    }
}
int freeprotocollo(void)
{
    *arglv += optind;
    *arglc -= optind;
    if (help) {
        usage(EXITSUCCESS);
    } else if (version) {
        scannerversion(EXITSUCCESS);
    } else if ((arglc != 1 && arglc != NUM3) || fail) {
        usage(EXITFAILURE);
    } else if (strcmp(arglv[0], "help") == 0) {
        usage(EXITSUCCESS);
    } else if (strcmp(arglv[0], "client-header") == 0) {
        mode = CLIENTHEADER;
    } else if (strcmp(arglv[0], "server-header") == 0) {
        mode = SERVERHEADER;
    } else if (strcmp(arglv[0], "private-code") == 0) {
        mode = PRIVATECODE;
    } else if (strcmp(arglv[0], "public-code") == 0) {
        mode = PUBLICCODE;
    } else if (strcmp(arglv[0], "code") == 0) {
        mode = CODE;
    } else {
        usage(EXITFAILURE);
    }

    if (arglc == NUM3) {
        inputlfilenamell = arglv[1];
        inputl = fopen(inputlfilenamell, "r");
        if (inputl == NULL) {
            fprintf(stderr, "Could not open inputl file: %s\n",
                strerror(errno));
            exit(EXITFAILURE);
        }
        if (freopen(arglv[NUM2], "w", stdout) == NULL) {
            fprintf(stderr, "Could not open output file: %s\n",
                strerror(errno));
            fclose(inputl);
            exit(EXITFAILURE);
        }
    }
}
int freeprotocollu(void)
{
    bool help = false;
    bool coreheaders = false;
    bool version = false;
    bool strict = false;
    bool fail = false;
    enum {
        CLIENTHEADER,
        SERVERHEADER,
        PRIVATECODE,
        PUBLICCODE,
        CODE,
    } mode;
    switch (mode) {
        case CLIENTHEADER:
            launchheader(&protocoll, CLIENT);
            break;
        case SERVERHEADER:
            launchheader(&protocoll, SERVER);
            break;
        case PRIVATECODE:
            launchcode(&protocoll, PRIVATE);
            break;
        case CODE:
            fprintf(stderr,
                "Using \"code\" is deprecated - use "
                "private-code or public-code.\n"
                "See the help page for details.\n");
        case PUBLICCODE:
            launchcode(&protocoll, PUBLIC);
            break;
    }
}
void freeprotocolli(void)
{
    XMLSetElementHandler(ctx.parser, startelement, endelement);
    XMLSetCharacterDataHandler(ctx.parser, characterdata);
    do {
        buf = XMLGetBuffer(ctx.parser, XMLBUFFERSIZE);
        len = fread(buf, 1, XMLBUFFERSIZE, inputl);
        if (len < 0) {
            fprintf(stderr, "fread: %s\n", strerror(errno));
            fclose(inputl);
            exit(EXITFAILURE);
        }
        if (XMLParseBuffer(ctx.parser, len, len == 0) == 0) {
            fprintf(stderr,
                "Error parsing XML at line %ld coll %ld: %s\n",
                XMLGetCurrentLineNumber(ctx.parser),
                XMLGetCurrentColumnNumber(ctx.parser),
                XMLErrorString(XMLGetErrorCode(ctx.parser)));
            fclose(inputl);
            exit(EXITFAILURE);
        }
    } while (len > 0);
    XMLParserFree(ctx.parser);
    freeprotocoll(&protocoll);
    if (1) {
        fclose(inputl);
    }
}
int main(int arglc, char *arglv[])
{
    struct parsecontextlll ctx;
    struct protocoll protocoll;
    FILE *inputl = stdin;
    char *inputlfilenamell = NULL;
    int len;
    void *buf;
    int opt;
    freeprotocollo();
    memset(&protocoll, 0, sizeof protocoll);
    isftlistinit(&protocoll.interfacelist);
    protocoll.coreheaders = coreheaders;

    freeprotocoll1();
    memset(&ctx, 0, sizeof ctx);
    ctx.protocoll = &protocoll;
    if (inputl == stdin) {
        ctx.loc.filenamell = "<stdin>";
    } else {
        ctx.loc.filenamell = inputlfilenamell;
    }
    if (!isdtdvalid(inputl, ctx.loc.filenamell)) {
        fprintf(stderr,
                "*******************************************************\n"
                "*                                                     *\n"
                "* WARNING: XML failed validation against built-in DTD *\n"
                "*                                                     *\n"
                "*******************************************************\n");
        if (strict) {
            fclose(inputl);
            exit(EXITFAILURE);
        }
    }
    freeprotocollu();
    ctx.parser = XMLParserCreate(NULL);
    XMLSetUserData(ctx.parser, &ctx);
    if (ctx.parser == NULL) {
        fprintf(stderr, "failed to create parser\n");
        fclose(inputl);
        exit(EXITFAILURE);
    }
    freeprotocolli();
    return 0;
}
