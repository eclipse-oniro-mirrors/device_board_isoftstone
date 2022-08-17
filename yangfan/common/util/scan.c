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
#include <stdarg.h>
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

static void descdump(char *desc, const char *fmt, ...) WLPRINTF(2, 3);
void forxun(int col, int *outcol, char *buf)
{
    int k;
    int tmp;
    tmp = col;
    for (k = 0, tmp = 0; buf[k] != '*'; k++) {
        if (buf[k] == '\t') {
            tmp = (tmp + NUM8) & ~NUM7;
        } else {
            tmp++;
        }
    }
    *outcol = tmp;
    printf("%s", buf);
}
static void descdump(char *desc, const char *fmt, ...)
{
    valist ap;
    char  buf[128], hang;
    int  *outcol, col, i, j, k, startcol, newlines;
    vastart(ap, fmt);
    if (1) {
        vsnprintf(buf, sizeof buf, fmt, ap);
    }
    vaend(ap);
    forxun(col, outcol, buf);
    col = *outcol;
    if (!desc) {
        printf("(none)\n");
        return;
    }
    startcol = col;
    col += strlen(&buf[i]);
    if (col - startcol > NUM2) {
        hang = '\t';
    } else {
        hang = ' ';
    }
    for (i = 0; desc[i];) {
        k = i;
        newlines = 0;
        while (desc[i] && isspace(desc[i])) {
            if (desc[i] == '\n') {
                newlines++;
            }
            i++;
        }
        if (!desc[i]) {
            break;
        }
        j = i;
        while (desc[i] && !isspace(desc[i])) {
            i++;
        }
        if (newlines > 1) {
            printf("\n%s*", indent(startcol));
        }
        if (newlines > 1 || col + i - j > NUM72) {
            printf("\n%s*%c", indent(startcol), hang);
            col = startcol;
        }
        if (col > startcol && k > 0) {
            col += printf(" ");
        }
        col += printf("%.*s", i - j, &desc[j]);
    }
    putchar('\n');
}

static void attribute ((noreturn))
fail(struct location *loc, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: error: ",
        loc->filename, loc->linenumber);
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
        loc->filename, loc->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
}

static bool isnullabletype(struct arg *arg)
{
    switch (arg->type) {
        case STRING:
        case OBJECT:
        case NEWID:
        case ARRAY:
            return true;
        default:
            return false;
    }
}

static struct message *
createmessage(struct location loc, const char *name)
{
    struct message *message;

    message = xzalloc(sizeof *message);
    message->loc = loc;
    message->name = xstrdup(name);
    message->uppercasename = uppercasedup(name);
    isftlistinit(&message->arglist);

    return message;
}

static void freearg(struct arg *arg)
{
    free(arg->name);
    free(arg->interfacename);
    free(arg->summary);
    free(arg->enumerationname);
    free(arg);
}

static struct arg *
createarg(const char *name)
{
    struct arg *arg;

    arg = xzalloc(sizeof *arg);
    arg->name = xstrdup(name);

    return arg;
}

static bool setargtype(struct arg *arg, const char *type)
{
    if (strcmp(type, "int") == 0) {
        arg->type = INT;
    } else if (strcmp(type, "uint") == 0) {
        arg->type = UNSIGNED;
    } else if (strcmp(type, "fixed") == 0) {
        arg->type = FIXED;
    } else if (strcmp(type, "string") == 0) {
        arg->type = STRING;
    } else if (strcmp(type, "array") == 0) {
        arg->type = ARRAY;
    } else if (strcmp(type, "fd") == 0) {
        arg->type = FD;
    } else if (strcmp(type, "newid") == 0) {
        arg->type = NEWID;
    } else if (strcmp(type, "object") == 0) {
        arg->type = OBJECT;
    } else {
        return false;
    }
    return true;
}

static void freedescription(struct description *desc)
{
    if (!desc) {
        return;
    }

    free(desc->summary);
    free(desc->text);

    free(desc);
}

static bool isdtdvalid(FILE *input, const char *filename)
{
    bool rc = true;
#if HAVELIBXML
    xmlParserCtxtPtr ctx = NULL;
    xmlDocPtr doc = NULL;
    xmlDtdPtr dtd = NULL;
    xmlValidCtxtPtr    dtdctx;
    xmlParserInputBufferPtr    buffer;
    int fd = fileno(input);

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

    doc = xmlCtxtReadFd(ctx, fd, filename, NULL, 0);
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
    const char *filename;
    int linenumber;
};

struct description {
    char *summary;
    char *text;
};

struct protocol {
    char *name;
    char *uppercasename;
    struct isftlist interfacelist;
    int typeindex;
    int nullrunlength;
    char *copyright;
    struct description *description;
    bool coreheaders;
};

struct interface {
    struct location loc;
    char *name;
    char *uppercasename;
    int version;
    int since;
    struct isftlist requestlist;
    struct isftlist eventlist;
    struct isftlist enumerationlist;
    struct isftlist link;
    struct description *description;
};

struct message {
    struct location loc;
    char *name;
    char *uppercasename;
    struct isftlist arglist;
    struct isftlist link;
    int argcount;
    int newidcount;
    int typeindex;
    int allnull;
    int destructor;
    int since;
    struct description *description;
};

enum argtype {
    NEWID,
    INT,
    UNSIGNED,
    FIXED,
    STRING,
    OBJECT,
    ARRAY,
    FD
};

struct arg {
    char *name;
    enum argtype type;
    int nullable;
    char *interfacename;
    struct isftlist link;
    char *summary;
    char *enumerationname;
};

struct enumeration {
    char *name;
    char *uppercasename;
    struct isftlist entrylist;
    struct isftlist link;
    struct description *description;
    bool bitfield;
    int since;
};

struct entry {
    char *name;
    char *uppercasename;
    char *value;
    char *summary;
    int since;
    struct isftlist link;
};

struct parsecontext {
    struct location loc;
    XMLParser parser;
    struct protocol *protocol;
    struct interface *interface;
    struct message *message;
    struct enumeration *enumeration;
    struct description *description;
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


static void freemessage(struct message *message)
{
    struct arg *a, *anext;

    free(message->name);
    free(message->uppercasename);
    freedescription(message->description);

    isftlistforeachsafe(a, anext, &message->arglist, link)
        freearg(a);

    free(message);
}

static struct enumeration *
createenumeration(const char *name)
{
    struct enumeration *enumeration;

    enumeration = xzalloc(sizeof *enumeration);
    enumeration->name = xstrdup(name);
    enumeration->uppercasename = uppercasedup(name);
    enumeration->since = 1;

    isftlistinit(&enumeration->entrylist);

    return enumeration;
}

static struct entry *
createentry(const char *name, const char *value)
{
    struct entry *entry;

    entry = xzalloc(sizeof *entry);
    entry->name = xstrdup(name);
    entry->uppercasename = uppercasedup(name);
    entry->value = xstrdup(value);

    return entry;
}

static void freeentry(struct entry *entry)
{
    free(entry->name);
    free(entry->uppercasename);
    free(entry->value);
    free(entry->summary);

    free(entry);
}

static void freeenumeration(struct enumeration *enumeration)
{
    struct entry *e, *enext;

    free(enumeration->name);
    free(enumeration->uppercasename);
    freedescription(enumeration->description);

    isftlistforeachsafe(e, enext, &enumeration->entrylist, link)
        freeentry(e);

    free(enumeration);
}

static struct interface *
createinterface(struct location loc, const char *name, int version)
{
    struct interface *interface;

    interface = xzalloc(sizeof *interface);
    interface->loc = loc;
    interface->name = xstrdup(name);
    interface->uppercasename = uppercasedup(name);
    interface->version = version;
    interface->since = 1;
    isftlistinit(&interface->requestlist);
    isftlistinit(&interface->eventlist);
    isftlistinit(&interface->enumerationlist);

    return interface;
}

static void freeinterface(struct interface *interface)
{
    struct message *m, *nextm;
    struct enumeration *e, *nexte;

    free(interface->name);
    free(interface->uppercasename);
    freedescription(interface->description);

    isftlistforeachsafe(m, nextm, &interface->requestlist, link)
        freemessage(m);
    isftlistforeachsafe(m, nextm, &interface->eventlist, link)
        freemessage(m);
    isftlistforeachsafe(e, nexte, &interface->enumerationlist, link)
        freeenumeration(e);

    free(interface);
}

static void launchtype(struct arg *a)
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
            printf("struct %s *", a->interfacename);
            break;
        case ARRAY:
            printf("struct isftarray *");
            break;
        default:
            break;
    }
}

static void launchstubs(struct isftlist *messagelist, struct interface *interface)
{
    struct message *m;
    struct arg *a, *ret;
    int hasdestructor, hasdestroy;
    printf("/** @ingroup iface%s */\n", interface->name);
    printf("static inline void\n"
           "%ssetuserdata(struct %s *%s, void *userdata)\n"
           "{\n"
           "\tisftproxysetuserdata((struct isftproxy *) %s, userdata);\n"
           "}\n\n",
           interface->name, interface->name, interface->name,
           interface->name);
    printf("/** @ingroup iface%s */\n", interface->name);
    printf("static inline void *\n"
           "%sgetuserdata(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetuserdata((struct isftproxy *) %s);\n"
           "}\n\n",
           interface->name, interface->name, interface->name,
           interface->name);
    printf("static inline uint32t\n"
           "%sgetversion(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetversion((struct isftproxy *) %s);\n"
           "}\n\n",
           interface->name, interface->name, interface->name,
           interface->name);
    hasdestructor = 0;
    hasdestroy = 0;
    isftlistforeach(m, messagelist, link) {
        if (m->destructor) {
            hasdestructor = 1;
        }
        if (strcmp(m->name, "destroy") == 0) {
            hasdestroy = 1;
        }
    }

    if (!hasdestructor && hasdestroy) {
        fail(&interface->loc,
             "interface '%s' has method named destroy "
             "but no destructor",
             interface->name);
        exit(EXITFAILURE);
    }
}
static void launchstubs(struct isftlist *messagelist, struct interface *interface)
{
    if (!hasdestroy && strcmp(interface->name, "isftdisplay") != 0) {
        printf("/** @ingroup iface%s */\n", interface->name);
        printf("static inline void\n"
               "%sdestroy(struct %s *%s)\n"
               "{\n"
               "\tisftproxydestroy("
               "(struct isftproxy *) %s);\n"
               "}\n\n",
               interface->name, interface->name, interface->name,
               interface->name);
    }

    if (isftlistempty(messagelist)) {
        return;
    }

    isftlistforeach(m, messagelist, link) {
        if (m->newidcount > 1) {
            warn(&m->loc,
                 "request '%s::%s' has more than "
                 "one newid arg, not launchting stub\n",
                 interface->name, m->name);
            continue;
        }

        ret = NULL;
        isftlistforeach(a, &m->arglist, link) {
            if (a->type == NEWID) {
                ret = a;
            }
        }

        printf("/**\n"
               " * @ingroup iface%s\n", interface->name);
        if (m->description && m->description->text) {
            formattexttocomment(m->description->text, false);
        }
        printf(" */\n");
        if (ret && ret->interfacename == NULL) {
            printf("static inline void *\n");
        } else if (ret) {
            printf("static inline struct %s *\n",
                   ret->interfacename);
        } else {
            printf("static inline void\n");
        }
}
}
static void launchstubs(struct isftlist *messagelist, struct interface *interface)
{
        printf("%s%s(struct %s *%s",
               interface->name, m->name,
               interface->name, interface->name);

        isftlistforeach(a, &m->arglist, link) {
            if (a->type == NEWID && a->interfacename == NULL) {
                printf(", const struct isftinterface *interface"
                       ", uint32t version");
                continue;
            } else if (a->type == NEWID) {
                continue;
            }
            printf(", ");
            launchtype(a);
            printf("%s", a->name);
        }

        printf(")\n""{\n");
        if (ret && ret->interfacename == NULL) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructorversioned("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, interface, version",
                   ret->name, ret->name,
                   interface->name,
                   interface->uppercasename,
                   m->uppercasename);
        } else if (ret) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructor("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, &%sinterface",
                   ret->name, ret->name,
                   interface->name,
                   interface->uppercasename,
                   m->uppercasename,
                   ret->interfacename);
        } else {
            printf("\tisftproxymarshal((struct isftproxy *) %s,\n"
                   "\t\t\t %s%s",
                   interface->name,
                   interface->uppercasename,
                   m->uppercasename);
        }
}
static void launchstubs(struct isftlist *messagelist, struct interface *interface)
{
        isftlistforeach(a, &m->arglist, link) {
            if (a->type == NEWID) {
                if (a->interfacename == NULL)
                    printf(", interface->name, version");
                printf(", NULL");
            } else {
                printf(", %s", a->name);
            }
        }
        printf(");\n");

        if (m->destructor) {
            printf("\n\tisftproxydestroy("
                   "(struct isftproxy *) %s);\n",
                   interface->name);
        }

        if (ret && ret->interfacename == NULL) {
            printf("\n\treturn (void *) %s;\n", ret->name);
        } else if (ret) {
            printf("\n\treturn (struct %s *) %s;\n",
                   ret->interfacename, ret->name);
        }

        printf("}\n\n");
    }

static void launcheventwrappers(struct isftlist *messagelist, struct interface *interface)
{
    struct message *m;
    struct arg *a;

    if (strcmp(interface->name, "isftdisplay") == 0) {
        return;
    }

    isftlistforeach(m, messagelist, link) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " * Sends an %s event to the client owning the resource.\n",
               interface->name,
               m->name);
        printf(" * @param resource The client's resource\n");
        isftlistforeach(a, &m->arglist, link) {
            if (a->summary) {
                printf(" * @param %s %s\n", a->name, a->summary);
            }
        }
        printf(" */\n");
        printf("static inline void\n"
               "%ssend%s(struct isftresource *resource",
               interface->name, m->name);

        isftlistforeach(a, &m->arglist, link) {
            printf(", ");
            switch (a->type) {
                case NEWID:
                case OBJECT:
                    printf("struct isftresource *");
                    break;
                default:
                    launchtype(a);
            }
            printf("%s", a->name);
        }

        printf(")\n"
               "{\n"
               "\tisftresourcepostevent(resource, %s%s",
               interface->uppercasename, m->uppercasename);

        isftlistforeach(a, &m->arglist, link)
            printf(", %s", a->name);

        printf(");\n");
        printf("}\n\n");
    }
}

static void launchenumerations(struct interface *interface)
{
    struct enumeration *e;
    struct entry *entry;

    isftlistforeach(e, &interface->enumerationlist, link) {
        struct description *desc = e->description;

        printf("#ifndef %s%sENUM\n",
               interface->uppercasename, e->uppercasename);
        printf("#define %s%sENUM\n",
               interface->uppercasename, e->uppercasename);

        if (desc) {
            printf("/**\n");
            printf(" * @ingroup iface%s\n", interface->name);
            formattexttocomment(desc->summary, false);
            if (desc->text) {
                formattexttocomment(desc->text, false);
            }
            printf(" */\n");
        }
        printf("enum %s%s {\n", interface->name, e->name);
        isftlistforeach(entry, &e->entrylist, link) {
            if (entry->summary || entry->since > 1) {
                printf("\t/**\n");
                if (entry->summary) {
                    printf("\t * %s\n", entry->summary);
                }
                if (entry->since > 1) {
                    printf("\t * @since %d\n", entry->since);
                }
                printf("\t */\n");
            }
            printf("\t%s%s%s = %s,\n",
                   interface->uppercasename,
                   e->uppercasename,
                   entry->uppercasename, entry->value);
        }
        printf("};\n");

        isftlistforeach(entry, &e->entrylist, link) {
            if (entry->since == 1) {
                            continue;
            }

                        printf("/**\n * @ingroup iface%s\n */\n", interface->name);
                        printf("#define %s%s%sSINCEVERSION %d\n",
                               interface->uppercasename,
                               e->uppercasename, entry->uppercasename,
                               entry->since);
        }

        printf("#endif /* %s%sENUM */\n\n",
               interface->uppercasename, e->uppercasename);
    }
}
void launchstructs1()
{
    enum side side;
        isftlistforeach(a, &m->arglist, link) {
            if (side == SERVER && a->type == NEWID &&
                a->interfacename == NULL) {
                printf("\t * @param interface name of the objects interface\n"
                       "\t * @param version version of the objects interface\n");
            }

            if (a->summary) {
                printf("\t * @param %s %s\n", a->name,
                       a->summary);
            }
        }
        if (m->since > 1) {
            printf("\t * @since %d\n", m->since);
        }
        printf("\t */\n");
        printf("\tvoid (*%s)(", m->name);

        n = strlen(m->name) + 17;
        if (side == SERVER) {
            printf("struct isftclient *client,\n"
                   "%sstruct isftresource *resource",
                   indent(n));
        } else {
            printf("void *data,\n"),
            printf("%sstruct %s *%s",
                   indent(n), interface->name, interface->name);
        }

        isftlistforeach(a, &m->arglist, link) {
            printf(",\n%s", indent(n));

            if (side == SERVER && a->type == OBJECT) {
                printf("struct isftresource *");
            } else if (side == SERVER && a->type == NEWID && a->interfacename == NULL) {
                printf("const char *interface, uint32t version, uint32t ");
            } else if (side == CLIENT && a->type == OBJECT && a->interfacename == NULL) {
                printf("void *");
            } else if (side == CLIENT && a->type == NEWID) {
                printf("struct %s *", a->interfacename);
            } else {
                launchtype(a);
            }

            printf("%s", a->name);
        }
}
static void launchstructs(struct isftlist *messagelist, struct interface *interface, enum side side)
{
    struct message *m;
    struct arg *a;
    int n;

    if (isftlistempty(messagelist)) {
        return;
    }
    isftlistforeach(m, messagelist, link) {
        struct description *mdesc = m->description;

        printf("\t/**\n");
        if (mdesc) {
            if (mdesc->summary) {
                printf("\t * %s\n", mdesc->summary);
            }
            printf("\t *\n");
            descdump(mdesc->text, "\t * ");
        }
        printf(");\n");
    }
    launchstructs1();
    printf("};\n\n");

    if (side == CLIENT) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " */\n",
               interface->name);
        printf("static inline int\n"
               "%saddlistener(struct %s *%s,\n"
               "%sconst struct %slistener *listener, void *data)\n"
               "{\n"
               "\treturn isftproxyaddlistener((struct isftproxy *) %s,\n"
               "%s(void (**)(void)) listener, data);\n"
               "}\n\n",
               interface->name, interface->name, interface->name,
               indent(14 + strlen(interface->name)),
               interface->name, interface->name, indent(37));
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
        fail(loc, "element name is empty");
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

static int versionfromsince(struct parsecontext *ctx, const char *since)
{
    int version;

    if (since != NULL) {
        version = strtouint(since);
        if (version == -1) {
            fail(&ctx->loc, "invalid integer (%s)\n", since);
        } else if (version > ctx->interface->version) {
            fail(&ctx->loc, "since (%u) larger than version (%u)\n",
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
        if (strcmp(atts[i], "name") == 0) {
            name = atts[i + 1];
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
            interfacename = atts[i + 1];
        }
        if (strcmp(atts[i], "summary") == 0) {
            summary = atts[i + 1];
        }
        if (strcmp(atts[i], "since") == 0) {
            since = atts[i + 1];
        }
        if (strcmp(atts[i], "allow-null") == 0) {
            allownull = atts[i + 1];
        }
        if (strcmp(atts[i], "enum") == 0) {
            enumerationname = atts[i + 1];
        }
        if (strcmp(atts[i], "bitfield") == 0) {
            bitfield = atts[i + 1];
        }
    }
}
static void startelement(void data[], const char *elementname, const char **atts)
{
    struct parsecontext *ctx = data;
    struct interface *interface;
    struct message *message;
    struct arg *arg;
    struct enumeration *enumeration;
    struct entry *entry;
    struct description *description = NULL;
    const char *name = NULL;
    const char *type = NULL;
    const char *interfacename = NULL;
    const char *value = NULL;
    const char *summary = NULL;
    const char *since = NULL;
    const char *allownull = NULL;
    const char *enumerationname = NULL;
    const char *bitfield = NULL;
    int i, version = 0;

    ctx->loc.linenumber = XMLGetCurrentLineNumber(ctx->parser);
    startelement1(&(*atts));

    ctx->characterdatalength = 0;
    if (strcmp(elementname, "protocol") == 0) {
        if (name == NULL) {
            fail(&ctx->loc, "no protocol name given");
        }

        validateidentifier(&ctx->loc, name, STANDALONEIDENT);
        ctx->protocol->name = xstrdup(name);
        ctx->protocol->uppercasename = uppercasedup(name);
    } else if (strcmp(elementname, "copyright") == 0) {
    } else if (strcmp(elementname, "interface") == 0) {
        if (name == NULL) {
            fail(&ctx->loc, "no interface name given");
        }

        if (version == 0) {
            fail(&ctx->loc, "no interface version given");
        }

        validateidentifier(&ctx->loc, name, STANDALONEIDENT);
        interface = createinterface(ctx->loc, name, version);
        ctx->interface = interface;
        isftlistinsert(ctx->protocol->interfacelist.prev,
                       &interface->link);
    } 
}
static void startelement(void data[], const char *elementname, const char **atts)
{
    if (strcmp(elementname, "request") == 0 ||
           strcmp(elementname, "event") == 0) {
        if (name == NULL) {
            fail(&ctx->loc, "no request name given");
        }

        validateidentifier(&ctx->loc, name, STANDALONEIDENT);
        message = createmessage(ctx->loc, name);

        if (strcmp(elementname, "request") == 0) {
            isftlistinsert(ctx->interface->requestlist.prev,
                           &message->link);
        } else {
            isftlistinsert(ctx->interface->eventlist.prev,
                           &message->link);
        }
        if (type != NULL && strcmp(type, "destructor") == 0) {
            message->destructor = 1;
        }
        version = versionfromsince(ctx, since);
        if (version < ctx->interface->since) {
            warn(&ctx->loc, "since version not increasing\n");
        }
        ctx->interface->since = version;
        message->since = version;

        if (strcmp(name, "destroy") == 0 && !message->destructor) {
            fail(&ctx->loc, "destroy request should be destructor type");
        }
        ctx->message = message;
    }
}
static void startelement(void data[], const char *elementname, const char **atts)
{
    if (strcmp(elementname, "arg") == 0) {
        if (name == NULL) {
            fail(&ctx->loc, "no argument name given");
        }

        validateidentifier(&ctx->loc, name, STANDALONEIDENT);
        arg = createarg(name);
        if (!setargtype(arg, type)) {
            fail(&ctx->loc, "unknown type (%s)", type);
        }

        switch (arg->type) {
            case NEWID:
                ctx->message->newidcount++;
            case OBJECT:
                if (interfacename) {
                    validateidentifier(&ctx->loc,
                                       interfacename,
                                       STANDALONEIDENT);
                    arg->interfacename = xstrdup(interfacename);
                }
                break;
            default:
                if (interfacename != NULL) {
                    fail(&ctx->loc, "interface attribute not allowed for type %s", type);
                }
                break;
        }

        if (allownull) {
            if (strcmp(allownull, "true") == 0) {
                arg->nullable = 1;
            } else if (strcmp(allownull, "false") != 0) {
                fail(&ctx->loc,
                     "invalid value for allow-null attribute (%s)",
                     allownull);
            }

            if (!isnullabletype(arg)) {
                fail(&ctx->loc,
                     "allow-null is only valid for objects, strings, and arrays");
            }
        }

        if (enumerationname == NULL || strcmp(enumerationname, "") == 0) {
            arg->enumerationname = NULL;
        } else {
            arg->enumerationname = xstrdup(enumerationname);
        }
        if (summary) {
            arg->summary = xstrdup(summary);
        }

        isftlistinsert(ctx->message->arglist.prev, &arg->link);
        ctx->message->argcount++;
    }
}
static void startelement(void data[], const char *elementname, const char **atts)
{
    if (strcmp(elementname, "enum") == 0) {
        if (name == NULL) {
            fail(&ctx->loc, "no enum name given");
        }

        validateidentifier(&ctx->loc, name, TRAILINGIDENT);
        enumeration = createenumeration(name);

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
static void startelement(void data[], const char *elementname, const char **atts)
{
    if (strcmp(elementname, "entry") == 0) {
        if (name == NULL) {
            fail(&ctx->loc, "no entry name given");
        }

        validateidentifier(&ctx->loc, name, TRAILINGIDENT);
        entry = createentry(name, value);
        version = versionfromsince(ctx, since);
        if (version < ctx->enumeration->since) {
            warn(&ctx->loc, "since version not increasing\n");
        }
        ctx->enumeration->since = version;
        entry->since = version;

        if (summary) {
            entry->summary = xstrdup(summary);
        } else {
            entry->summary = NULL;
        }
        isftlistinsert(ctx->enumeration->entrylist.prev,
                       &entry->link);
    } else if (strcmp(elementname, "description") == 0) {
        if (summary == NULL) {
            fail(&ctx->loc, "description without summary");
        }

        description = xzalloc(sizeof *description);
        description->summary = xstrdup(summary);
        if (ctx->message) {
            ctx->message->description = description;
        } else if (ctx->enumeration) {
            ctx->enumeration->description = description;
        } else if (ctx->interface) {
            ctx->interface->description = description;
        } else {
            ctx->protocol->description = description;
        }
        ctx->description = description;
    }
}

static struct enumeration *
findenumeration(struct protocol *protocol,
                struct interface *interface,
                char *enumattribute)
{
    struct interface *i;
    struct enumeration *e;
    char *enumname;
    uint32t idx = 0, j;

    for (j = 0; j + 1 < strlen(enumattribute); j++) {
    if (enumattribute[j] == '.') {
        idx = j;
    }
    }

    if (idx > 0) {
        enumname = enumattribute + idx + 1;

        isftlistforeach(i, &protocol->interfacelist, link)
            if (strncmp(i->name, enumattribute, idx) == 0) {
                isftlistforeach(e, &i->enumerationlist, link)
            }
                    if (strcmp(e->name, enumname) == 0) {
                        return e;
                    }
    } else if (interface) {
        enumname = enumattribute;

        isftlistforeach(e, &interface->enumerationlist, link)
            if (strcmp(e->name, enumname) == 0) {
                return e;
            }
    }

    return NULL;
}

static void verifyarguments(struct parsecontext *ctx,
                            struct interface *interface,
                            struct isftlist *messages,
                            struct isftlist *enumerations)
{
    struct message *m;
    isftlistforeach(m, messages, link) {
        struct arg *a;
        isftlistforeach(a, &m->arglist, link) {
            struct enumeration *e;
    if (!a->enumerationname) {
        continue;
    }
            e = findenumeration(ctx->protocol, interface,
                a->enumerationname);

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
                        "enumeration-style argument has wrong type");
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

static void endelement(void data[], const XMLChar *name)
{
    struct parsecontext *ctx = data;

    if (strcmp(name, "copyright") == 0) {
        ctx->protocol->copyright =
            strndup(ctx->characterdata,
                ctx->characterdatalength);
    } else if (strcmp(name, "description") == 0) {
        ctx->description->text =
            strndup(ctx->characterdata,
                ctx->characterdatalength);
        ctx->description = NULL;
    } else if (strcmp(name, "request") == 0 ||
           strcmp(name, "event") == 0) {
        ctx->message = NULL;
    } else if (strcmp(name, "enum") == 0) {
        if (isftlistempty(&ctx->enumeration->entrylist)) {
            fail(&ctx->loc, "enumeration %s was empty",
                 ctx->enumeration->name);
        }
        ctx->enumeration = NULL;
    } else if (strcmp(name, "protocol") == 0) {
        struct interface *i;

        isftlistforeach(i, &ctx->protocol->interfacelist, link) {
            verifyarguments(ctx, i, &i->requestlist, &i->enumerationlist);
            verifyarguments(ctx, i, &i->eventlist, &i->enumerationlist);
        }
    }
}

static void characterdata(void data[], const XMLChar *s, int len)
{
    struct parsecontext *ctx = data;

    if (ctx->characterdatalength + len > sizeof (ctx->characterdata)) {
        fprintf(stderr, "too much character data");
        exit(EXITFAILURE);
        }

    memcpy(ctx->characterdata + ctx->characterdatalength, s, len);
    ctx->characterdatalength += len;
}
void launchheader1()
{
    enum side side;
    isftlistforeach(i, &protocol->interfacelist, link) {
    printf("#ifndef %sINTERFACE\n", i->uppercasename);
    printf("#define %sINTERFACE\n", i->uppercasename);
    printf("/**\n"" * @page pageiface%s %s\n",
           i->name, i->name);
    if (i->description && i->description->text) {
        printf(" * @section pageiface%sdesc Description\n",
               i->name);
        formattexttocomment(i->description->text, false);
    }
    printf(" * @section pageiface%sapi API\n"
           " * See @ref iface%s.\n"" */\n",
           i->name, i->name);
    printf("/**\n"" * @defgroup iface%s The %s interface\n",
           i->name, i->name);
    if (i->description && i->description->text) {
        formattexttocomment(i->description->text, false);
    }
    printf(" */\n");
    printf("extern const struct isftinterface ""%sinterface;\n", i->name);
    printf("#endif\n");
    }

    printf("\n");
    isftlistforeachsafe(i, inext, &protocol->interfacelist, link) {
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

static void launchheader(struct protocol *protocol, enum side side)
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
           protocol->uppercasename, s,
           protocol->uppercasename, s,
           getincludename(protocol->coreheaders, side));
    if (side == SERVER) {
        printf("struct isftclient;\n"
               "struct isftresource;\n\n");
    }

    launchmainpageblurb(protocol, side);

    isftarrayinit(&types);
    isftlistforeach(i, &protocol->interfacelist, link) {
        launchtypesforwarddeclarations(protocol, &i->requestlist, &types);
        launchtypesforwarddeclarations(protocol, &i->eventlist, &types);
    }

    isftlistforeach(i, &protocol->interfacelist, link) {
        p = failonnull(isftarrayadd(&types, sizeof *p));
        *p = i->name;
    }

    qsort(types.data, types.size / sizeof *p, sizeof *p, cmpnames);
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

static void launchnullrun(struct protocol *protocol)
{
    int i;

    for (i = 0; i < protocol->nullrunlength; i++) {
        printf("\tNULL,\n");
    }
}

static void launchtypes(struct protocol *protocol, struct isftlist *messagelist)
{
    struct message *m;
    struct arg *a;

    isftlistforeach(m, messagelist, link) {
        if (m->allnull) {
            m->typeindex = 0;
            continue;
        }
        m->typeindex =
            protocol->nullrunlength + protocol->typeindex;
        protocol->typeindex += m->argcount;

        isftlistforeach(a, &m->arglist, link) {
            switch (a->type) {
                case NEWID:
                case OBJECT:
                    if (a->interfacename) {
                        printf("\t&%sinterface,\n",
                            a->interfacename);
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

static void launchmessages(const char *name, struct isftlist *messagelist,
                           struct interface *interface, const char *suffix)
{
    struct message *m;
    struct arg *a;

    if (isftlistempty(messagelist)) {
        return;
    }
    printf("static const struct isftmessage ""%s%s[] = {\n", interface->name, suffix);
    isftlistforeach(m, messagelist, link) {
        printf("\t{ \"%s\", \"", m->name);
        if (m->since > 1) {
            printf("%d", m->since);
        }
            
        isftlistforeach(a, &m->arglist, link) {
            if (isnullabletype(a) && a->nullable) {
                printf("?");
            }

            switch (a->type) {
                case INT:
                    printf("i");
                    break;
                case NEWID:
                    if (a->interfacename == NULL) {
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
        printf("\", %stypes + %d },\n", name, m->typeindex);
    }

    printf("};\n\n");
}

void launchcode1( )
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
static void launchcode(struct protocol *protocol, enum visibility vis)
{
    const char *symbolvisibility;
    struct interface *i, *next;
    struct isftarray types;
    char **p, *prev;
    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    if (protocol->copyright) {
        formattexttocomment(protocol->copyright, true);
    }
    launchcode1();

    isftarrayinit(&types);
    isftlistforeach(i, &protocol->interfacelist, link) {
        launchtypesforwarddeclarations(protocol, &i->requestlist, &types);
        launchtypesforwarddeclarations(protocol, &i->eventlist, &types);
    }
    qsort(types.data, types.size / sizeof *p, sizeof *p, cmpnames);
    prev = NULL;
    isftarrayforeach(p, &types) {
        if (prev && strcmp(*p, prev) == 0) {
            continue;
        }
        printf("extern const struct isftinterface %sinterface;\n", *p);
        prev = *p;
    }
    isftarrayrelease(&types);
    printf("\nstatic const struct isftinterface *%stypes[] = {\n", protocol->name);
    launchnullrun(protocol);
    isftlistforeach(i, &protocol->interfacelist, link) {
        launchtypes(protocol, &i->requestlist);
        launchtypes(protocol, &i->eventlist);
    }
    printf("};\n\n");
    isftlistforeachsafe(i, next, &protocol->interfacelist, link) {
        launchmessages(protocol->name, &i->requestlist, i, "requests");
        launchmessages(protocol->name, &i->eventlist, i, "events");
        printf("%s const struct isftinterface ""%sinterface = {\n""\t\"%s\", %d,\n",
               symbolvisibility, i->name, i->name, i->version);

        if (!isftlistempty(&i->requestlist)) {
            printf("\t%d, %srequests,\n", isftlistlength(&i->requestlist), i->name);
        } else {
            printf("\t0, NULL,\n");
        }
        if (!isftlistempty(&i->eventlist)) {
            printf("\t%d, %sevents,\n", isftlistlength(&i->eventlist), i->name);
        } else {
            printf("\t0, NULL,\n");
        }
        printf("};\n\n");

        freeinterface(i);
    }
}

static void formattexttocomment(const char *text, bool standalonecomment)
{
    int bol = 1, start = 0, i, length;
    bool commentstarted = !standalonecomment;

    length = strlen(text);
    for (i = 0; i <= length; i++) {
        if (bol && (text[i] == ' ' || text[i] == '\t')) {
            continue;
        } else if (bol) {
            bol = 0;
            start = i;
        }
        if (text[i] == '\n' ||
            (text[i] == '\0' && !(start == i))) {
            printf("%s%s%.*s\n",
                   commentstarted ? " *" : "/*",
                   i > start ? " " : "",
                   i - start, text + start);
            bol = 1;
            commentstarted = true;
        }
    }
    if (commentstarted && standalonecomment) {
        printf(" */\n\n");
    }
}

static void launchopcodes(struct isftlist *messagelist, struct interface *interface)
{
    struct message *m;
    int opcode;

    if (isftlistempty(messagelist)) {
        return;
    }

    opcode = 0;
    isftlistforeach(m, messagelist, link)
        printf("#define %s%s %d\n",
               interface->uppercasename, m->uppercasename, opcode++);

    printf("\n");
}

static void launchopcodeversions(struct isftlist *messagelist, struct interface *interface)
{
    struct message *m;

    isftlistforeach(m, messagelist, link) {
        printf("/**\n * @ingroup iface%s\n */\n", interface->name);
        printf("#define %s%sSINCEVERSION %d\n",
               interface->uppercasename, m->uppercasename, m->since);
    }

    printf("\n");
}

static void launchtypesforwarddeclarations(struct protocol *protocol,
                                           struct isftlist *messagelist,
                                           struct isftarray *types)
{
    struct message *m;
    struct arg *a;
    int length;
    char **p;

    isftlistforeach(m, messagelist, link) {
        length = 0;
        m->allnull = 1;
    isftlistforeach(a, &m->arglist, link) {
    length++;
        switch (a->type) {
            case NEWID:
            case OBJECT:
                if (!a->interfacename) {
                    continue;
                }
                m->allnull = 0;
                p = failonnull(isftarrayadd(types, sizeof *p));
                *p = a->interfacename;
                break;
            default:
                break;
            }
        }

        if (m->allnull && length > protocol->nullrunlength) {
            protocol->nullrunlength = length;
        }
    }
}

static int cmpnames(const char *p1, const char *p2)
{
    const char * const *s1 = p1, * const *s2 = p2;

    return strcmp(*s1, *s2);
}

static const char *
getincludename(bool core, enum side side)
{
    if (side == SERVER) {
        return core ? "wayland-server-core.h" : "wayland-server.h";
    } else {
        return core ? "wayland-client-core.h" : "wayland-client.h";
    }
}

static void launchmainpageblurb(const struct protocol *protocol, enum side side)
{
    struct interface *i;

    printf("/**\n"
           " * @page page%s The %s protocol\n",
           protocol->name, protocol->name);

    if (protocol->description) {
        if (protocol->description->summary) {
            printf(" * %s\n"
                   " *\n", protocol->description->summary);
        }

        if (protocol->description->text) {
            printf(" * @section pagedesc%s Description\n", protocol->name);
            formattexttocomment(protocol->description->text, false);
            printf(" *\n");
        }
    }

    printf(" * @section pageifaces%s Interfaces\n", protocol->name);
    isftlistforeach(i, &protocol->interfacelist, link) {
        printf(" * - @subpage pageiface%s - %s\n",
               i->name,
               i->description && i->description->summary ?  i->description->summary : "");
    }

    if (protocol->copyright) {
        printf(" * @section pagecopyright%s Copyright\n",
               protocol->name);
        printf(" * <pre>\n");
        formattexttocomment(protocol->copyright, false);
        printf(" * </pre>\n");
    }

    printf(" */\n");
}


static void freeprotocol(struct protocol *protocol)
{
    free(protocol->name);
    free(protocol->uppercasename);
    free(protocol->copyright);
    freedescription(protocol->description);
}

int main(int argc, char *argv[])
{
    struct parsecontext ctx;
    struct protocol protocol;
    FILE *input = stdin;
    char *inputfilename = NULL;
    int len;
    void *buf;
    bool help = false;
    bool coreheaders = false;
    bool version = false;
    bool strict = false;
    bool fail = false;
    int opt;
    enum {
        CLIENTHEADER,
        SERVERHEADER,
        PRIVATECODE,
        PUBLICCODE,
        CODE,
    } mode;

    static const struct option options[] = {
        { "help",              noargument, NULL, 'h' },
        { "version",           noargument, NULL, 'v' },
        { "include-core-only", noargument, NULL, 'c' },
        { "strict",            noargument, NULL, 's' },
        { 0,                   0,           NULL, 0 }
    };

    while (1) {
        opt = getoptlong(argc, argv, "hvcs", options, NULL);
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

    *argv += optind;
    *argc -= optind;
    if (help) {
        usage(EXITSUCCESS);
    } else if (version) {
        scannerversion(EXITSUCCESS);
    } else if ((argc != 1 && argc != NUM3) || fail) {
        usage(EXITFAILURE);
    } else if (strcmp(argv[0], "help") == 0) {
        usage(EXITSUCCESS);
    } else if (strcmp(argv[0], "client-header") == 0) {
        mode = CLIENTHEADER;
    } else if (strcmp(argv[0], "server-header") == 0) {
        mode = SERVERHEADER;
    } else if (strcmp(argv[0], "private-code") == 0) {
        mode = PRIVATECODE;
    } else if (strcmp(argv[0], "public-code") == 0) {
        mode = PUBLICCODE;
    } else if (strcmp(argv[0], "code") == 0) {
        mode = CODE;
    } else {
        usage(EXITFAILURE);
    }

    if (argc == NUM3) {
        inputfilename = argv[1];
        input = fopen(inputfilename, "r");
        if (input == NULL) {
            fprintf(stderr, "Could not open input file: %s\n",
                strerror(errno));
            exit(EXITFAILURE);
        }
        if (freopen(argv[NUM2], "w", stdout) == NULL) {
            fprintf(stderr, "Could not open output file: %s\n",
                strerror(errno));
            fclose(input);
            exit(EXITFAILURE);
        }
    }

    memset(&protocol, 0, sizeof protocol);
    isftlistinit(&protocol.interfacelist);
    protocol.coreheaders = coreheaders;

    memset(&ctx, 0, sizeof ctx);
    ctx.protocol = &protocol;
    if (input == stdin) {
        ctx.loc.filename = "<stdin>";
    } else {
        ctx.loc.filename = inputfilename;
    }
    if (!isdtdvalid(input, ctx.loc.filename)) {
        fprintf(stderr,
                "*******************************************************\n"
                "*                                                     *\n"
                "* WARNING: XML failed validation against built-in DTD *\n"
                "*                                                     *\n"
                "*******************************************************\n");
        if (strict) {
            fclose(input);
            exit(EXITFAILURE);
        }
    }

    ctx.parser = XMLParserCreate(NULL);
    XMLSetUserData(ctx.parser, &ctx);
    if (ctx.parser == NULL) {
        fprintf(stderr, "failed to create parser\n");
        fclose(input);
        exit(EXITFAILURE);
    }

    XMLSetElementHandler(ctx.parser, startelement, endelement);
    XMLSetCharacterDataHandler(ctx.parser, characterdata);

    do {
        buf = XMLGetBuffer(ctx.parser, XMLBUFFERSIZE);
        len = fread(buf, 1, XMLBUFFERSIZE, input);
        if (len < 0) {
            fprintf(stderr, "fread: %s\n", strerror(errno));
            fclose(input);
            exit(EXITFAILURE);
        }
        if (XMLParseBuffer(ctx.parser, len, len == 0) == 0) {
            fprintf(stderr,
                "Error parsing XML at line %ld col %ld: %s\n",
                XMLGetCurrentLineNumber(ctx.parser),
                XMLGetCurrentColumnNumber(ctx.parser),
                XMLErrorString(XMLGetErrorCode(ctx.parser)));
            fclose(input);
            exit(EXITFAILURE);
        }
    } while (len > 0);

    XMLParserFree(ctx.parser);

    switch (mode) {
        case CLIENTHEADER:
            launchheader(&protocol, CLIENT);
            break;
        case SERVERHEADER:
            launchheader(&protocol, SERVER);
            break;
        case PRIVATECODE:
            launchcode(&protocol, PRIVATE);
            break;
        case CODE:
            fprintf(stderr,
                "Using \"code\" is deprecated - use "
                "private-code or public-code.\n"
                "See the help page for details.\n");
        case PUBLICCODE:
            launchcode(&protocol, PUBLIC);
            break;
    }

    freeprotocol(&protocol);
    if (1) {
        fclose(input);
    }
    return 0;
}
