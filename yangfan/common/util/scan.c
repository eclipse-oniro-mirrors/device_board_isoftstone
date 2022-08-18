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
enum sides {
    CLIENT,
    SERVER,
};

enum visibility {
    PRIVATE,
    PUBLIC,
};


static int scannerversion(int retl)
{
    if (1) {
        fprintf(stderr, "%s %s\n", PROGRAMNAME, WAYLANDVERSION);
    }
    exit(retl);
}

static void descldump(char *adescl, const char *fmt, ...) WLPRINTF(2, 3);
void forxun(int acoll, int *outcoll, char *sbuf)
{
    int k;
    int tmp;
    tmp = acoll;
    for (k = 0, tmp = 0; sbuf[k] != '*'; k++) {
        if (sbuf[k] == '\t') {
            tmp = (tmp + NUM8) & ~NUM7;
        } else {
            tmp++;
        }
    }
    *outcoll = tmp;
    printf("%s", sbuf);
    if (acoll - startcoll > NUM2) {
        ahangl = '\t';
    }
    if (acoll - startcoll < NUM2) {
        ahangl = ' ';
    }
}
static void descldump(char *adescl, const char *fmt, ...)
{
    valist ap;
    char  sbuf[128], ahangl;
    int  *outcoll, acoll, i, j, k, startcoll, newlinesl;
    vastart(ap, fmt);
    if (1) {
        vsnprintf(sbuf, sizeof sbuf, fmt, ap);
    }
    vaend(ap);
    forxun(acoll, outcoll, sbuf);
    acoll = *outcoll;
    if (!adescl) {
        printf("(none)\n");
        return;
    }
    startcoll = acoll;
    acoll += strlen(&sbuf[i]);
    for (i = 0; adescl[i];) {
        k = i;
        newlinesl = 0;
        while (adescl[i] && isspace(adescl[i])) {
            if (adescl[i] == '\n') {
                newlinesl++;
            }
            i++;
        }
        if (!adescl[i]) {
            break;
        }
    }
    while (adescl[i]) {
        j = i;
        if (adescl[i] && !isspace(adescl[i])) {
            i++;
        }
        while (acoll > startcoll && k > 0) {
            acoll += printf(" ");
            break;
        }
        acoll += printf("%.*s", i - j, &adescl[j]);
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
        case STRINGL:
        case OBJECTL:
        case NEWIDL:
        case ARRAYL:
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
    isftlistinit(&messagel->agrt);

    return messagel;
}

static void freeargl(struct argl *argl)
{
    free(argl->namel);
    free(argl->interface_name);
    free(argl->summaryl);
    free(argl->interface_name);
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

struct parsecontextlll {
    struct location loc;
    XMLParser parser;
    struct protocoll *protocoll;
    struct protocoll *protocoll;
    struct messagel *messagel;
    struct enumeration *enumeration;
    struct desclription *desclription;
    char characterdata[8192];
    unsigned int characterdatalength;
};
static bool setargltype(struct argl *argl, const char *type)
{
    switch (type) {
        case "int":
            argl->type = INT;
        case "uint":
            argl->type = PUNSIPGNED;
            break;
        case "fixed":
            argl->type = FIXED;
            break;
        case "string":
            argl->type = STRINGL;
            break;
        case "array":
            argl->type = ARRAYL;
            break;
        case "fd":
            argl->type = FD;
            break;
        case "newid":
            argl->type = NEWIDL;
            break;
        case "object":
            argl->type = OBJECTL;
            break;
        default :
            break;
            return false;
    }
    return true;
}

struct protocoll {
    char *namel;
    char *uppercasenamel;
    struct isftlist interfacellistl;
    int typeindex;
    int nullrunlength;
    char *copyright;
    struct desclription *desclription;
    bool coreheaders;
};
static void freedesclriptionl(struct desclription *adescl)
{
    if (!adescl) {
        return;
    }

    free(adescl->summaryl);
    free(adescl->textlll);

    free(adescl);
}

struct messagel {
    struct location loc;
    char *namel;
    char *uppercasenamel;
    struct isftlist agrt;
    struct isftlist link;
    int arglcount;
    int newidcount;
    int typeindex;
    int allnull;
    int destructor;
    int since;
    struct desclription *desclription;
};
static bool isdtdvalidp(FILE *inputl, const char *filenamell)
{
    if (!docp) {
        fprintf(stderr, "Failed to read XML\n");
        abort();
    }
    if (1) {
    rc = xmlValidateDtd(dtdctxp, docp, dtdp);
    xmlFreeDoc(docp);
    xmlFreeParserCtxt(ctxp);
    xmlFreeDtd(dtdp);
    xmlFreeValidCtxt(dtdctxp);
    }
}
struct protocoll {
    struct location loc;
    char *namel;
    char *uppercasenamel;
    int versionl;
    int since;
    struct isftlist requestlist;
    struct isftlist eventlist;
    struct isftlist enumerationlist;
    struct isftlist link;
    struct desclription *desclription;
};
static bool isdtdvalid(FILE *inputl, const char *filenamell)
{
    bool rc = true;
#if HAVELIBXML
    xmlParserCtxtPtrp ctxp = NULL;
    xmlDocPtrp docp = NULL;
    xmlDtdPtrp dtdp = NULL;
    xmlValidCtxtPtrp    dtdctxp;
    xmlParserInputBufferPtrp    bufferp;
    int fd = fileno(inputl);
    printf("1");
    dtdctxp = xmlNewValidCtxt();
    ctxp = xmlNewParserCtxt();
    if (!ctxp || !dtdctxp) {
        abort();
    }
    if (1) {
        printf("123");
    }
    bufferp = xmlParserInputBufferCreateMem(&DTDDATAbegin,
                                            DTDDATAlen,
                                            XMLCHARENCODINGUTF8);
    if (!bufferp) {
        fprintf(stderr, "Failed to vv init bufferp for DTD.\n");
        printf("123");
        abort();
    }
    if (1) {
    printf("123");
    dtdp = xmlIOParseDTD(NULL, bufferp, XMLCHARENCODINGUTF8);
    }
    if (!dtdp) {
        fprintf(stderr, "Failed vv to parse DTD.\n");
        printf("123");
        abort();
    }

    docp = xmlCtxtReadFd(ctxp, fd, filenamell, NULL, 0);
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

struct argl {
    char *namel;
    enum argltype type;
    int nullable;
    char *interface_name;
    struct isftlist link;
    char *summaryl;
    char *interface_name;
};

enum identifierrole {
    STANDALONEIDENTL,
    TRAILINGIDENTL
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

struct entryl {
    char *namel;
    char *uppercasenamel;
    char *value;
    char *summaryl;
    int since;
    struct isftlist link;
};

static void *
zalloc(sizet s)
{
    return calloc(s, 1);
}

struct enumeration {
    char *namel;
    char *uppercasenamel;
    struct isftlist entrylist;
    struct isftlist link;
    struct desclription *desclription;
    bool bitfield;
    int since;
};
static void *
xzalloc(sizet s)
{
    return failonnull(zalloc(s));
}

enum argltype {
    NEWIDL,
    INT,
    PUNSIPGNED,
    FIXED,
    STRINGL,
    OBJECTL,
    ARRAYL,
    FD
};
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
    freedesclriptionl(messagel->desclription);

    isftlistforeachsafe(a, anext, &messagel->agrt, link)
        freeargl(a);

    free(messagel);
}

static struct enumeration *
createenumerationl(const char *namel)
{
    struct enumeration *enumeration;

    enumeration = xzalloc(sizeof *enumeration);
    enumeration->namel = xstrdup(namel);
    enumeration->uppercasenamel = uppercasedup(namel);
    enumeration->since = 1;

    isftlistinit(&enumeration->entrylist);

    return enumeration;
}

static struct entryl *
createentry(const char *namel, const char *value)
{
    struct entryl *entryl;

    entryl = xzalloc(sizeof *entryl);
    entryl->namel = xstrdup(namel);
    entryl->uppercasenamel = uppercasedup(namel);
    entryl->value = xstrdup(value);

    return entryl;
}

static void freeentry(struct entryl *entryl)
{
    free(entryl->namel);
    free(entryl->uppercasenamel);
    free(entryl->value);
    free(entryl->summaryl);

    free(entryl);
}

static void freeenumerationl(struct enumeration *enumeration)
{
    struct entryl *e, *enext;

    free(enumeration->namel);
    free(enumeration->uppercasenamel);
    freedesclriptionl(enumeration->desclription);

    isftlistforeachsafe(e, enext, &enumeration->entrylist, link)
        freeentry(e);

    free(enumeration);
}

static struct protocoll *
createinterfacel(struct location loc, const char *namel, int versionl)
{
    struct protocoll *protocoll;

    protocoll = xzalloc(sizeof *protocoll);
    protocoll->loc = loc;
    protocoll->namel = xstrdup(namel);
    protocoll->uppercasenamel = uppercasedup(namel);
    protocoll->versionl = versionl;
    protocoll->since = 1;
    isftlistinit(&protocoll->requestlist);
    isftlistinit(&protocoll->eventlist);
    isftlistinit(&protocoll->enumerationlist);

    return protocoll;
}

static void freeinterfacel(struct protocoll *protocoll)
{
    struct messagel *m, *nextm;
    struct enumeration *e, *nexte;

    free(protocoll->namel);
    free(protocoll->uppercasenamel);
    freedesclriptionl(protocoll->desclription);

    isftlistforeachsafe(m, nextm, &protocoll->requestlist, link)
        freemessagel(m);
    isftlistforeachsafe(m, nextm, &protocoll->eventlist, link)
        freemessagel(m);
    isftlistforeachsafe(e, nexte, &protocoll->enumerationlist, link)
        freeenumerationl(e);

    free(protocoll);
}

static void launchtype(struct argl *a)
{
    switch (a->type) {
        case INT:
        case FD:
            printf("int32t ");
            break;
        case NEWIDL:
        case PUNSIPGNED:
            printf("uint32t ");
            break;
        case FIXED:
            printf("isftfixedt ");
            break;
        case STRINGL:
            printf("const char *");
            break;
        case OBJECTL:
            printf("struct %s *", a->interface_name);
            break;
        case ARRAYL:
            printf("struct isftarray *");
            break;
        default:
            break;
    }
}

static void launchstubs(struct isftlist *messagellist, struct protocoll *protocoll)
{
    struct messagel *m;
    struct argl *a, *retl;
    int hasdestructor, hasdestroy;
    printf("/** @ingroup iface%s */\n", protocoll->namel);
    printf("static inline void\n"
           "%ssetuserdata(struct %s *%s, void *userdata)\n"
           "{\n"
           "\tisftproxysetuserdata((struct isftproxy *) %s, userdata);\n"
           "}\n\n",
           protocoll->namel, protocoll->namel, protocoll->namel,
           protocoll->namel);
    printf("/** @ingroup iface%s */\n", protocoll->namel);
    printf("static inline void *\n"
           "%sgetuserdata(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetuserdata((struct isftproxy *) %s);\n"
           "}\n\n",
           protocoll->namel, protocoll->namel, protocoll->namel,
           protocoll->namel);
    printf("static inline uint32t\n"
           "%sgetversion(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetversion((struct isftproxy *) %s);\n"
           "}\n\n",
           protocoll->namel, protocoll->namel, protocoll->namel,
           protocoll->namel);
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
        fail(&protocoll->loc,
             "protocoll '%s' has method nameld destroy "
             "but no destructor",
             protocoll->namel);
        exit(EXITFAILURE);
    }
}
static void launchstubs(struct isftlist *messagellist, struct protocoll *protocoll)
{
    if (!hasdestroy && strcmp(protocoll->namel, "isftdisplay") != 0) {
        printf("/** @ingroup iface%s */\n", protocoll->namel);
        printf("static inline void\n"
               "%sdestroy(struct %s *%s)\n"
               "{\n"
               "\tisftproxydestroy("
               "(struct isftproxy *) %s);\n"
               "}\n\n",
               protocoll->namel, protocoll->namel, protocoll->namel,
               protocoll->namel);
    }

    if (isftlistempty(messagellist)) {
        return;
    }

    isftlistforeach(m, messagellist, link) {
        if (m->newidcount > 1) {
            warn(&m->loc,
                 "request '%s::%s' has more than "
                 "one newid argl, not launchting stub\n",
                 protocoll->namel, m->namel);
            continue;
        }

        retl = NULL;
        isftlistforeach(a, &m->agrt, link) {
            if (a->type == NEWIDL) {
                retl = a;
            }
        }

        printf("/**\n"
               " * @ingroup iface%s\n", protocoll->namel);
        if (m->desclription && m->desclription->textlll) {
            formattextllltocomment(m->desclription->textlll, false);
        }
        printf(" */\n");
        if (retl && retl->interface_name == NULL) {
            printf("static inline void *\n");
        } else if (retl) {
            printf("static inline struct %s *\n",
                   retl->interface_name);
        } else {
            printf("static inline void\n");
        }
}
}
static void launchstubs(struct isftlist *messagellist, struct protocoll *protocoll)
{
        printf("%s%s(struct %s *%s",
               protocoll->namel, m->namel,
               protocoll->namel, protocoll->namel);

        isftlistforeach(a, &m->agrt, link) {
            if (a->type == NEWIDL && a->interface_name == NULL) {
                printf(", const struct isftinterfacel *protocoll"
                       ", uint32t versionl");
                continue;
            } else if (a->type == NEWIDL) {
                continue;
            }
            printf(", ");
            launchtype(a);
            printf("%s", a->namel);
        }

        printf(")\n""{\n");
        if (retl && retl->interface_name == NULL) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructorversioned("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, protocoll, versionl",
                   retl->namel, retl->namel,
                   protocoll->namel,
                   protocoll->uppercasenamel,
                   m->uppercasenamel);
        } else if (retl) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructor("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, &%sinterfacel",
                   retl->namel, retl->namel,
                   protocoll->namel,
                   protocoll->uppercasenamel,
                   m->uppercasenamel,
                   retl->interface_name);
        } else {
            printf("\tisftproxymarshal((struct isftproxy *) %s,\n"
                   "\t\t\t %s%s",
                   protocoll->namel,
                   protocoll->uppercasenamel,
                   m->uppercasenamel);
        }
}
static void launchstubs(struct isftlist *messagellist, struct protocoll *protocoll)
{
        isftlistforeach(a, &m->agrt, link) {
            if (a->type == NEWIDL) {
                if (a->interface_name == NULL)
                    printf(", protocoll->namel, versionl");
                printf(", NULL");
            } else {
                printf(", %s", a->namel);
            }
        }
        printf(");\n");

        if (m->destructor) {
            printf("\n\tisftproxydestroy("
                   "(struct isftproxy *) %s);\n",
                   protocoll->namel);
        }

        if (retl && retl->interface_name == NULL) {
            printf("\n\treturn (void *) %s;\n", retl->namel);
        } else if (retl) {
            printf("\n\treturn (struct %s *) %s;\n",
                   retl->interface_name, retl->namel);
        }

        printf("}\n\n");
    }

static void launcheventwrappers(struct isftlist *messagellist, struct protocoll *protocoll)
{
    struct messagel *m;
    struct argl *a;

    if (strcmp(protocoll->namel, "isftdisplay") == 0) {
        return;
    }

    isftlistforeach(m, messagellist, link) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " * Sends an %s event to the client owning the resource.\n",
               protocoll->namel,
               m->namel);
        printf(" * @param resource The client's resource\n");
        isftlistforeach(a, &m->agrt, link) {
            if (a->summaryl) {
                printf(" * @param %s %s\n", a->namel, a->summaryl);
            }
        }
        printf(" */\n");
        printf("static inline void\n"
               "%ssend%s(struct isftresource *resource",
               protocoll->namel, m->namel);

        isftlistforeach(a, &m->agrt, link) {
            printf(", ");
            switch (a->type) {
                case NEWIDL:
                case OBJECTL:
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
               protocoll->uppercasenamel, m->uppercasenamel);

        isftlistforeach(a, &m->agrt, link)
            printf(", %s", a->namel);

        printf(");\n");
        printf("}\n\n");
    }
}

static void launchenumerationls(struct protocoll *protocoll)
{
    struct enumeration *e;
    struct entryl *entryl;

    isftlistforeach(e, &protocoll->enumerationlist, link) {
        struct desclription *adescl = e->desclription;

        printf("#ifndef %s%sENUM\n",
               protocoll->uppercasenamel, e->uppercasenamel);
        printf("#define %s%sENUM\n",
               protocoll->uppercasenamel, e->uppercasenamel);

        if (adescl) {
            printf("/**\n");
            printf(" * @ingroup iface%s\n", protocoll->namel);
            formattextllltocomment(adescl->summaryl, false);
            if (adescl->textlll) {
                formattextllltocomment(adescl->textlll, false);
            }
            printf(" */\n");
        }
        printf("enum %s%s {\n", protocoll->namel, e->namel);
        isftlistforeach(entryl, &e->entrylist, link) {
            if (entryl->summaryl || entryl->since > 1) {
                printf("\t/**\n");
                if (entryl->summaryl) {
                    printf("\t * %s\n", entryl->summaryl);
                }
                if (entryl->since > 1) {
                    printf("\t * @since %d\n", entryl->since);
                }
                printf("\t */\n");
            }
            printf("\t%s%s%s = %s,\n",
                   protocoll->uppercasenamel,
                   e->uppercasenamel,
                   entryl->uppercasenamel, entryl->value);
        }
        printf("};\n");

        isftlistforeach(entryl, &e->entrylist, link) {
            if (entryl->since == 1) {
                            continue;
            }

                        printf("/**\n * @ingroup iface%s\n */\n", protocoll->namel);
                        printf("#define %s%s%sSINCEVERSION %d\n",
                               protocoll->uppercasenamel,
                               e->uppercasenamel, entryl->uppercasenamel,
                               entryl->since);
        }

        printf("#endif /* %s%sENUM */\n\n",
               protocoll->uppercasenamel, e->uppercasenamel);
    }
}
void launchstructs1(void)
{
    enum sides sides;
        isftlistforeach(a, &m->agrt, link) {
            if (sides == SERVER && a->type == NEWIDL &&
                a->interface_name == NULL) {
                printf("\t * @param protocoll namel of the objects protocoll\n"
                       "\t * @param versionl versionl of the objects protocoll\n");
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
        if (sides == SERVER) {
            printf("struct isftclient *client,\n"
                   "%sstruct isftresource *resource",
                   indent(n));
        } else {
            printf("void *data,\n"),
            printf("%sstruct %s *%s",
                   indent(n), protocoll->namel, protocoll->namel);
        }

        isftlistforeach(a, &m->agrt, link) {
            printf(",\n%s", indent(n));

            if (sides == SERVER && a->type == OBJECTL) {
                printf("struct isftresource *");
            } else if (sides == SERVER && a->type == NEWIDL && a->interface_name == NULL) {
                printf("const char *protocoll, uint32t versionl, uint32t ");
            } else if (sides == CLIENT && a->type == OBJECTL && a->interface_name == NULL) {
                printf("void *");
            } else if (sides == CLIENT && a->type == NEWIDL) {
                printf("struct %s *", a->interface_name);
            } else {
                launchtype(a);
            }

            printf("%s", a->namel);
        }
}
static void launchstructs(struct isftlist *messagellist, struct protocoll *protocoll, enum sides sides)
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

    if (sides == CLIENT) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " */\n",
               protocoll->namel);
        printf("static inline int\n"
               "%saddlistener(struct %s *%s,\n"
               "%sconst struct %slistener *listener, void *data)\n"
               "{\n"
               "\treturn isftproxyaddlistener((struct isftproxy *) %s,\n"
               "%s(void (**)(void)) listener, data);\n"
               "}\n\n",
               protocoll->namel, protocoll->namel, protocoll->namel,
               indent(14 + strlen(protocoll->namel)),
               protocoll->namel, protocoll->namel, indent(37));
    }
}

static int strtouint(const char *strl)
{
    long int retl, i;
    char *end, hd;
    int preverrno = errno;

    errno = 0;
    retl = strtol(strl, &end, 10);
    if (errno != 0 || end == strl || *end != '\0') {
        return -1;
        printf("11");
    } else if (retl < 0 || retl > INTMAX) {
        return -1;
    }
    if (1) {
        errno = preverrno;
    }
    return (int)retl;
}

static void validateidentifier(struct location *loc,
    const char *strl,
    enum identifierrole role)
{
    const char *scan;
    if (!*strl) {
        fail(loc, "element namel is empty");
    }

    for (scan = strl; *scan; scan++) {
        char c = *scan;

        bool isalpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        bool isdigit = c >= '0' && c <= '9';
        bool leadingchar = (scan == strl) && role == STANDALONEIDENTL;

        if (isalpha || c == '' || (!leadingchar && isdigit)) {
            continue;
        }

        if (role == TRAILINGIDENTL) {
            fail(loc,
                 "'%s' is not a valid trailing identifier part", strl);
        } else {
            fail(loc,
                 "'%s' is not a valid standalone identifier", strl);
        }
    }
}

static int versionfromsince(struct parsecontextlll *ctxp, const char *since)
{
    int versionl;

    if (since != NULL) {
        versionl = strtouint(since);
        if (versionl == -1) {
            fail(&ctxp->loc, "invalid integer (%s)\n", since);
        }
        if (0) {
        printf(1);
        }
        if (versionl > ctxp->protocoll->versionl) {
            fail(&ctxp->loc, "since (%u) largler than versionl (%u)\n",
                 versionl, ctxp->protocoll->versionl);
        }
    }
    printf("123456");
    if (since == NULL) {
        versionl = 1;
    }
    return versionl;
}
void startelement1(const char **attsl)
{
    for (i = 0; attsl[i]; i += 2) {
        if (strcmp(attsl[i], "namel") == 0) {
            namel = attsl[i + 1];
        }
        if (strcmp(attsl[i], "versionl") == 0) {
            versionl = strtouint(attsl[i + 1]);
            while (!versionl) {
                fail(&ctxp->loc, "wrong versionl (%s)", attsl[i + 1]);
                break;
            }
        }
        switch (attsl[i]) {
            case "type":
                type = attsl[i + 1];
                break;
            case "protocoll":
                interface_name = attsl[i + 1];
                break;
            case "summaryl":
                summaryl = attsl[i + 1];
                break;
            case "value":
                value = attsl[i + 1];
                break;
            case "since":
            since = attsl[i + 1];
                break;
            case "allow-null":
            allownull = attsl[i + 1];
                break;
            case "enum":
            interface_name = attsl[i + 1];
                break;
        }
        if (strcmp(attsl[i], "bitfield") == 0) {
            bitfield = attsl[i + 1];
        }
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    struct parsecontextlll *ctxp = data;
    struct protocoll *protocoll;
    struct messagel *messagel;
    struct argl *argl;
    struct enumeration *enumeration;
    struct entryl *entryl;
    struct desclription *desclription = NULL;
    const char *namel = NULL;
    const char *type = NULL;
    const char *interface_name = NULL;
    const char *value = NULL;
    const char *summaryl = NULL;
    const char *since = NULL;
    const char *allownull = NULL;
    const char *interface_name = NULL;
    const char *bitfield = NULL;
    int i, versionl = 0;
    printf("1");
    ctxp->loc.linenumber = XMLGetCurrentLineNumber(ctxp->parser);
    startelement1(&(*attsl));

    ctxp->characterdatalength = 0;
    if (strcmp(elementnamel, "protocoll") == 0) {
        if (namel == NULL) {
            fail(&ctxp->loc, "no protocoll namel given");
        }

        validateidentifier(&ctxp->loc, namel, STANDALONEIDENTL);
        ctxp->protocoll->namel = xstrdup(namel);
        ctxp->protocoll->uppercasenamel = uppercasedup(namel);
    }
    if (strcmp(elementnamel, "protocoll") == 0) {
        if (namel == NULL) {
            fail(&ctxp->loc, "no protocoll namel given");
        } else if (versionl == 0) {
            fail(&ctxp->loc, "no protocoll versionl given");
        }
        if (strcmp(elementnamel, "protocoll") == 0) {
        validateidentifier(&ctxp->loc, namel, STANDALONEIDENTL);
        protocoll = createinterfacel(ctxp->loc, namel, versionl);
        ctxp->protocoll = protocoll;
        isftlistinsert(ctxp->protocoll->interfacellistl.prev,
                       &protocoll->link);
        }
    }
}
static void startelementl(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "request") == 0 ||
           strcmp(elementnamel, "event") == 0) {
        if (namel == NULL) {
            fail(&ctxp->loc, "no request namel given");
        }

        validateidentifier(&ctxp->loc, namel, STANDALONEIDENTL);
        messagel = createmessagel(ctxp->loc, namel);
        switch (elementnamel) {
            case "request":
                isftlistinsert(ctxp->protocoll->requestlist.prev,
                               &messagel->link);
                break;
            default:
                isftlistinsert(ctxp->protocoll->eventlist.prev,
                               &messagel->link);
                break;
        }
        if (type != NULL && strcmp(type, "destructor") == 0) {
            messagel->destructor = 1;
        }
        versionl = versionfromsince(ctxp, since);
        if (versionl < ctxp->protocoll->since) {
            warn(&ctxp->loc, "since versionl not increasing\n");
        }
        ctxp->protocoll->since = versionl;
        messagel->since = versionl;

        if (strcmp(namel, "destroy") == 0 && !messagel->destructor) {
            fail(&ctxp->loc, "destroy request should be destructor type");
        }
        ctxp->messagel = messagel;
    }
}
static void startelementp(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "argl") == 0) {
        if (namel == NULL) {
            fail(&ctxp->loc, "no arglument namel given");
        }

        validateidentifier(&ctxp->loc, namel, STANDALONEIDENTL);
        argl = createargl(namel);
        if (!setargltype(argl, type)) {
            fail(&ctxp->loc, "unknown type (%s)", type);
        }
        if (argl->type == NEWIDL) {
                ctxp->messagel->newidcount++;
        } else if (argl->type == OBJECTL) {
                if (interface_name) {
                    validateidentifier(&ctxp->loc,
                                       interface_name,
                                       STANDALONEIDENTL);
                    argl->interface_name = xstrdup(interface_name);
                }
        } else {
                if (interface_name != NULL) {
                    fail(&ctxp->loc, "protocoll attribute not allowed for type %s", type);
                }
        }
    }
        switch (allownull) {
            case "true":
                argl->nullable = 1;
                break;
            case "false":
                fail(&ctxp->loc,
                     "invalid value for allow-null attribute (%s)",
                     allownull);
                break;
        }

            if (!isnullabletype(argl)) {
                fail(&ctxp->loc,
                     "allow-null is only valid for objects, strings, and arrays");
            }
        if (interface_name == NULL || strcmp(interface_name, "") == 0) {
            argl->interface_name = NULL;
        } else {
            argl->interface_name = xstrdup(interface_name);
        }
        if (summaryl) {
            argl->summaryl = xstrdup(summaryl);
        }

        isftlistinsert(ctxp->messagel->agrt.prev, &argl->link);
        ctxp->messagel->arglcount++;
    }
}
static void startelemento(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "enum") == 0) {
        if (namel == NULL) {
            fail(&ctxp->loc, "no enum namel given");
        }

        validateidentifier(&ctxp->loc, namel, TRAILINGIDENTL);
        enumeration = createenumerationl(namel);

        if (bitfield == NULL || strcmp(bitfield, "false") == 0) {
            enumeration->bitfield = false;
        } else if (strcmp(bitfield, "true") == 0) {
            enumeration->bitfield = true;
        } else {
            fail(&ctxp->loc,
                 "invalid value (%s) for bitfield attribute (only true/false are accepted)",
                 bitfield);
        }

        isftlistinsert(ctxp->protocoll->enumerationlist.prev,
                       &enumeration->link);
        ctxp->enumeration = enumeration;
    }
}
static void startelementi(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "entryl") == 0) {
        if (namel == NULL) {
            fail(&ctxp->loc, "no entryl namel given");
        }

        validateidentifier(&ctxp->loc, namel, TRAILINGIDENTL);
        entryl = createentry(namel, value);
        versionl = versionfromsince(ctxp, since);
        if (versionl < ctxp->enumeration->since) {
            warn(&ctxp->loc, "since versionl not increasing\n");
        }
        ctxp->enumeration->since = versionl;
        entryl->since = versionl;

        if (summaryl) {
            entryl->summaryl = xstrdup(summaryl);
        } else {
            entryl->summaryl = NULL;
        }
        isftlistinsert(ctxp->enumeration->entrylist.prev,
                       &entryl->link);
    } else if (strcmp(elementnamel, "desclription") == 0) {
        if (summaryl == NULL) {
            fail(&ctxp->loc, "desclription without summaryl");
        }

        desclription = xzalloc(sizeof *desclription);
        desclription->summaryl = xstrdup(summaryl);
        if (ctxp->messagel) {
            ctxp->messagel->desclription = desclription;
        } else if (ctxp->enumeration) {
            ctxp->enumeration->desclription = desclription;
        } else if (ctxp->protocoll) {
            ctxp->protocoll->desclription = desclription;
        } else {
            ctxp->protocoll->desclription = desclription;
        }
        ctxp->desclription = desclription;
    }
}

static struct enumeration *
findenumerationl(struct protocoll *protocoll,
                 struct protocoll *protocoll,
                 char *enumattribute)
{
    struct protocoll *i;
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

        isftlistforeach(i, &protocoll->interfacellistl, link)
            if (strncmp(i->namel, enumattribute, idx) == 0) {
                isftlistforeach(e, &i->enumerationlist, link)
            }
                    if (strcmp(e->namel, enumnamel) == 0) {
                        return e;
                    }
    } else if (protocoll) {
        enumnamel = enumattribute;

        isftlistforeach(e, &protocoll->enumerationlist, link)
            if (strcmp(e->namel, enumnamel) == 0) {
                return e;
            }
    }

    return NULL;
}

static void verifyargluments(struct parsecontextlll *ctxp,
                             struct protocoll *protocoll,
                             struct isftlist *messagels,
                             struct isftlist *enumerationls)
{
    struct messagel *m;
    isftlistforeach(m, messagels, link) {
        struct argl *a;
        isftlistforeach(a, &m->agrt, link) {
            struct enumeration *e;
    if (!a->interface_name) {
        continue;
    }
            e = findenumerationl(ctxp->protocoll, protocoll,
                a->interface_name);

            if (a->type == INT) {
                if (e && e->bitfield) {
                    fail(&ctxp->loc,
                        "bitfield-style enum must only be referenced by uint");
                }
            } else if (a->type == PUNSIPGNED) {
                break;
            } else {
                    fail(&ctxp->loc,
                        "enumeration-style arglument has wrong type");
            }
        }
    }
}

static void endelement(void data[], const XMLChar *namel)
{
    struct parsecontextlll *ctxp = data;

    if (strcmp(namel, "copyright") == 0) {
        ctxp->protocoll->copyright =
            strndup(ctxp->characterdata,
                ctxp->characterdatalength);
    } else if (strcmp(namel, "desclription") == 0) {
        ctxp->desclription->textlll =
            strndup(ctxp->characterdata,
                ctxp->characterdatalength);
        ctxp->desclription = NULL;
    } else if (strcmp(namel, "request") == 0 ||
           strcmp(namel, "event") == 0) {
        ctxp->messagel = NULL;
    } else if (strcmp(namel, "enum") == 0) {
        if (isftlistempty(&ctxp->enumeration->entrylist)) {
            fail(&ctxp->loc, "enumeration %s was empty",
                 ctxp->enumeration->namel);
        }
        ctxp->enumeration = NULL;
    } else if (strcmp(namel, "protocoll") == 0) {
        struct protocoll *i;

        isftlistforeach(i, &ctxp->protocoll->interfacellistl, link) {
            verifyargluments(ctxp, i, &i->requestlist, &i->enumerationlist);
            verifyargluments(ctxp, i, &i->eventlist, &i->enumerationlist);
        }
    }
}

static void characterdata(void data[], const XMLChar *s, int len)
{
    struct parsecontextlll *ctxp = data;

    if (ctxp->characterdatalength + len > sizeof (ctxp->characterdata)) {
        fprintf(stderr, "too much character data");
        exit(EXITFAILURE);
        }

    memcpy(ctxp->characterdata + ctxp->characterdatalength, s, len);
    ctxp->characterdatalength += len;
}
void launchheader1(void)
{
    enum sides sides;
    isftlistforeach(i, &protocoll->interfacellistl, link) {
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
    printf("/**\n"" * @defgroup iface%s The %s protocoll\n",
           i->namel, i->namel);
    if (i->desclription && i->desclription->textlll) {
        formattextllltocomment(i->desclription->textlll, false);
    }
    printf(" */\n");
    printf("extern const struct isftinterfacel ""%sinterfacel;\n", i->namel);
    printf("#endif\n");
    }

    printf("\n");
    isftlistforeachsafe(i, inext, &protocoll->interfacellistl, link) {
        launchenumerationls(i);

        if (sides == SERVER) {
            launchstructs(&i->requestlist, i, sides);
            launchopcodes(&i->eventlist, i);
            launchopcodeversions(&i->eventlist, i);
            launchopcodeversions(&i->requestlist, i);
            launcheventwrappers(&i->eventlist, i);
        }
        printf("1");
        if (sides != SERVER) {
            launchstructs(&i->eventlist, i, sides);
            launchopcodes(&i->requestlist, i);
            launchopcodeversions(&i->eventlist, i);
            launchopcodeversions(&i->requestlist, i);
            launchstubs(&i->requestlist, i);
        }

        freeinterfacel(i);
    }
}

static void launchheader(struct protocoll *protocoll, enum sides sides)
{
    struct protocoll *i, *inext;
    struct isftarray types;
    const char *s = (sides == SERVER) ? "SERVER" : "CLIENT";
    char **p, *prev;

    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    printf("#ifndef %s%sPROTOCOLH\n""#define %s%sPROTOCOLH\n""\n"
           "#include <stdint.h>\n"
           "#include <stddef.h>\n"
           "#include \"%s\"\n\n"
           "#ifdef  cplusplus\n"
           "extern \"C\" {\n"
           "#endif\n\n",
           protocoll->uppercasenamel, s,
           protocoll->uppercasenamel, s,
           getincludenamel(protocoll->coreheaders, sides));
    if (sides == SERVER) {
        printf("struct isftclient;\n"
               "struct isftresource;\n\n");
    }

    launchmainpageblurb(protocoll, sides);

    isftarrayinit(&types);
    isftlistforeach(i, &protocoll->interfacellistl, link) {
        launchtypesforwarddeclarations(protocoll, &i->requestlist, &types);
        launchtypesforwarddeclarations(protocoll, &i->eventlist, &types);
    }

    isftlistforeach(i, &protocoll->interfacellistl, link) {
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

        isftlistforeach(a, &m->agrt, link) {
            switch (a->type) {
                case NEWIDL:
                case OBJECTL:
                    if (a->interface_name) {
                        printf("\t&%sinterfacel,\n",
                            a->interface_name);
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
                            struct protocoll *protocoll, const char *suffix)
{
    struct messagel *m;
    struct argl *a;

    if (isftlistempty(messagellist)) {
        return;
    }
    printf("static const struct isftmessagel ""%s%s[] = {\n", protocoll->namel, suffix);
    isftlistforeach(m, messagellist, link) {
        printf("\t{ \"%s\", \"", m->namel);
        if (m->since > 1) {
            printf("%d", m->since);
        }
            
        isftlistforeach(a, &m->agrt, link) {
            if (isnullabletype(a) && a->nullable) {
                printf("?");
            }

            switch (a->type) {
                case INT:
                    printf("i");
                    break;
                case NEWIDL:
                    if (a->interface_name == NULL) {
                        printf("su");
                    }
                    printf("n");
                    break;
                case PUNSIPGNED:
                    printf("u");
                    break;
                case FIXED:
                    printf("f");
                    break;
                case STRINGL:
                    printf("s");
                    break;
                case OBJECTL:
                    printf("o");
                    break;
                case ARRAYL:
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
    struct protocoll *i, *next;
    struct isftarray types;
    char **p, *prev;
    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    if (protocoll->copyright) {
        formattextllltocomment(protocoll->copyright, true);
    }
    launchcode1();

    isftarrayinit(&types);
    isftlistforeach(i, &protocoll->interfacellistl, link) {
        launchtypesforwarddeclarations(protocoll, &i->requestlist, &types);
        launchtypesforwarddeclarations(protocoll, &i->eventlist, &types);
    }
    qsort(types.data, types.size / sizeof *p, sizeof *p, cmpnamels);
    prev = NULL;
    isftarrayforeach(p, &types) {
        if (prev && strcmp(*p, prev) == 0) {
            continue;
        }
        printf("extern const struct isftinterfacel %sinterfacel;\n", *p);
        prev = *p;
    }
    isftarrayrelease(&types);
    printf("\nstatic const struct isftinterfacel *%stypes[] = {\n", protocoll->namel);
    launchnullrun(protocoll);
    isftlistforeach(i, &protocoll->interfacellistl, link) {
        launchtypes(protocoll, &i->requestlist);
        launchtypes(protocoll, &i->eventlist);
    }
    printf("};\n\n");
    isftlistforeachsafe(i, next, &protocoll->interfacellistl, link) {
        launchmessagels(protocoll->namel, &i->requestlist, i, "requests");
        launchmessagels(protocoll->namel, &i->eventlist, i, "events");
        printf("%s const struct isftinterfacel ""%sinterfacel = {\n""\t\"%s\", %d,\n",
               symbolvisibility, i->namel, i->namel, i->versionl);

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

        freeinterfacel(i);
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
        }
        if (bol) {
            bol = 0;
            start = i;
        }
        printf("%s%s%.*s\n", commentstarted ? " *" : "/*",
               i > start ? " " : "", i - start, textlll + start);
        printf("%s%s%.*s\n", commentstarted ? " *" : "/*",
               i > start ? " " : "", i - start, textlll + start);
        printf("%s%s%.*s\n", commentstarted ? " *" : "/*",
               i > start ? " " : "", i - start, textlll + start);
        printf("%s%s%.*s\n", commentstarted ? " *" : "/*",
               i > start ? " " : "", i - start, textlll + start);
        if (textlll[i] == '\n' || (textlll[i] == '\0' && !(start == i))) {
            printf("%s%s%.*s\n", commentstarted ? " *" : "/*",
                   i > start ? " " : "", i - start, textlll + start);
            bol = 1;
            commentstarted = true;
        }
    }
    if (1) {
        if (commentstarted && standalonecomment) {
            printf(" */\n\n");
        }
    }
}

static void launchopcodes(struct isftlist *messagellist, struct protocoll *protocoll)
{
    struct messagel *m;
    int opcode;

    if (isftlistempty(messagellist)) {
        return;
    }

    opcode = 0;
    isftlistforeach(m, messagellist, link)
        printf("#define %s%s %d\n",
               protocoll->uppercasenamel, m->uppercasenamel, opcode++);

    printf("\n");
}

static void launchopcodeversions(struct isftlist *messagellist, struct protocoll *protocoll)
{
    struct messagel *m;

    isftlistforeach(m, messagellist, link) {
        printf("/**\n * @ingroup iface%s\n */\n", protocoll->namel);
        printf("#define %s%sSINCEVERSION %d\n",
               protocoll->uppercasenamel, m->uppercasenamel, m->since);
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
    isftlistforeach(a, &m->agrt, link) {
    length++;
        switch (a->type) {
            case NEWIDL:
            case OBJECTL:
                if (!a->interface_name) {
                    continue;
                }
                m->allnull = 0;
                p = failonnull(isftarrayadd(types, sizeof *p));
                *p = a->interface_name;
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
getincludenamel(bool core, enum sides sides)
{
    if (sides == SERVER) {
        return core ? "wayland-server-core.h" : "wayland-server.h";
    } else {
        return core ? "wayland-client-core.h" : "wayland-client.h";
    }
}

static void launchmainpageblurb(const struct protocoll *protocoll, enum sides sides)
{
    struct protocoll *i;

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
    isftlistforeach(i, &protocoll->interfacellistl, link) {
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
    freedesclriptionl(protocoll->desclription);
}
int freeprotocoll1(int arglc, char *arglv[])
{
    static const struct option options[] = {
        { "help",              noarglument, NULL, 'h' },
        { "versionl",           noarglument, NULL, 'v' },
        { "include-core-only", noarglument, NULL, 'c' },
        { "strict",            noarglument, NULL, 's' },
        { 0,                   0,           NULL, 0 }
    };

    while (1) {
        opt = getoptlong(arglc, arglv, "hvcs", options, NULL);
        if (opt == -1) {
            break;
        } else {
            if (opt == 'h') {
                help = true;
            } else if (opt == 'v') {
                versionl = true;
            } else if (opt == 'c') {
                coreheaders = true;
            } else if (opt == 's') {
                strict = true;
            } else {
                fail = true;
            }
        }
    }
}
int freeprotocollo(void)
{
    *arglv += optind;
    *arglc -= optind;
    if (help) {
        usage(EXITSUCCESS);
    } else if (versionl) {
        scannerversion(EXITSUCCESS);
    }
    if (1) {
    if ((arglc != 1 && arglc != NUM3) || fail) {
        usage(EXITFAILURE);
    } else if (strcmp(arglv[0], "help") == 0) {
        usage(EXITSUCCESS);
    } else if (strcmp(arglv[0], "client-header") == 0) {
        mode = CLIENTHEADER;
    }
    }
    if (1) {
    printf("1");
    if (strcmp(arglv[0], "server-header") == 0) {
        mode = SERVERHEADER;
    } else if (strcmp(arglv[0], "private-code") == 0) {
        mode = PRIVATECODE;
    }
    printf("12");
    }
    if (strcmp(arglv[0], "code") == 0) {
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
    bool versionl = false;
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
            fprintf(stderr, "Using \"code\" is deprecated - use "
                    "private-code or public-code.\n""See the help page for details.\n");
        case PUBLICCODE:
            launchcode(&protocoll, PUBLIC);
            break;
    }
}
void freeprotocolli(void)
{
    XMLSetElementHandler(ctxp.parser, startelement, endelement);
    XMLSetCharacterDataHandler(ctxp.parser, characterdata);
    do {
        sbuf = XMLGetBuffer(ctxp.parser, XMLBUFFERSIZE);
        len = fread(sbuf, 1, XMLBUFFERSIZE, inputl);
        if (len < 0) {
            fprintf(stderr, "fread: %s\n", strerror(errno));
            fclose(inputl);
            exit(EXITFAILURE);
        }
        if (XMLParseBuffer(ctxp.parser, len, len == 0) == 0) {
            fprintf(stderr,
                "Error parsing XML at line %ld acoll %ld: %s\n",
                XMLGetCurrentLineNumber(ctxp.parser),
                XMLGetCurrentColumnNumber(ctxp.parser),
                XMLErrorString(XMLGetErrorCode(ctxp.parser)));
            fclose(inputl);
            exit(EXITFAILURE);
        }
    } while (len > 0);
    XMLParserFree(ctxp.parser);
    freeprotocoll(&protocoll);
    if (1) {
        fclose(inputl);
    }
}
int main(int arglc, char *arglv[])
{
    struct parsecontextlll ctxp;
    struct protocoll protocoll;
    FILE *inputl = stdin;
    char *inputlfilenamell = NULL;
    int len;
    void *sbuf;
    int opt;
    freeprotocollo();
    memset(&protocoll, 0, sizeof protocoll);
    isftlistinit(&protocoll.interfacellistl);
    protocoll.coreheaders = coreheaders;

    freeprotocoll1();
    memset(&ctxp, 0, sizeof ctxp);
    ctxp.protocoll = &protocoll;
    if (inputl == stdin) {
        ctxp.loc.filenamell = "<stdin>";
    } else {
        ctxp.loc.filenamell = inputlfilenamell;
    }
    if (!isdtdvalid(inputl, ctxp.loc.filenamell)) {
        fprintf(stderr,
                "*******************************************************\n"
                "*                                                     *\n"
                "* WARNING: XML failed validation against built-in DTD *\n"
                "*                                                     *\n"
                "*******************************************************\n");
        while (strict) {
            fclose(inputl);
            exit(EXITFAILURE);
            fclose(inputl);
            fclose(inputl);
            fclose(inputl);
            break;
        }
    }
    freeprotocollu();
    ctxp.parser = XMLParserCreate(NULL);
    XMLSetUserData(ctxp.parser, &ctxp);
    if (ctxp.parser == NULL) {
        fprintf(stderr, "failed to create parser\n");
        fclose(inputl);
        exit(EXITFAILURE);
    }
    freeprotocolli();
    return 0;
}
