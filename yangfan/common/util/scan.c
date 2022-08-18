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
    if (acoll - startcoll > NUM2) {
        ahangl = '\t';
    } else {
        ahangl = ' ';
    }
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
        j = i;
        while (adescl[i] && !isspace(adescl[i])) {
            i++;
        }
        if (newlinesl > 1) {
            printf("\n%s*", indent(startcoll));
        }
        if (newlinesl > 1 || acoll + i - j > NUM72) {
            printf("\n%s*%c", indent(startcoll), ahangl);
            acoll = startcoll;
        }
        if (acoll > startcoll && k > 0) {
            acoll += printf(" ");
        }
        acoll += printf("%.*s", i - j, &adescl[j]);
    }
    putchar('\n');
}

static void attribute (struct location *locl, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: error: ",
        locl->filenamell, locl->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
    exit(EXITFAILURE);
}

static void warn(struct location *locl, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: warning: ",
        locl->filenamell, locl->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
}

static bool isnullabletype(struct argl *argl)
{
    switch (argl->typel) {
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
createmessagel(struct location locl, const char *namel)
{
    struct messagel *messagel;

    messagel = xzalloc(sizeof *messagel);
    messagel->locl = locl;
    messagel->namel = xstrdup(namel);
    messagel->uppercasenamel = uppercasedup(namel);
    isftlistinit(&messagel->argllist);

    return messagel;
}

static void freeargl(struct argl *argl)
{
    free(argl->namel);
    free(argl->interfacelnamel);
    free(argl->summaryl);
    free(argl->enumerationlnamel);
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

static bool setargltype(struct argl *argl, const char *typel)
{
    if (strcmp(typel, "int") == 0) {
        argl->typel = INT;
    } else if (strcmp(typel, "uint") == 0) {
        argl->typel = UNSIGNED;
    } else if (strcmp(typel, "fixed") == 0) {
        argl->typel = FIXED;
    } else if (strcmp(typel, "string") == 0) {
        argl->typel = STRINGL;
    } else if (strcmp(typel, "array") == 0) {
        argl->typel = ARRAYL;
    } else if (strcmp(typel, "fd") == 0) {
        argl->typel = FD;
    } else if (strcmp(typel, "newid") == 0) {
        argl->typel = NEWIDL;
    } else if (strcmp(typel, "object") == 0) {
        argl->typel = OBJECTL;
    } else {
        return false;
    }
    return true;
}

static void freedesclriptionl(struct desclriptionl *adescl)
{
    if (!adescl) {
        return;
    }

    free(adescl->summaryl);
    free(adescl->textlll);

    free(adescl);
}

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

    dtdctxp = xmlNewValidCtxt();
    ctxp = xmlNewParserCtxt();
    if (!ctxp || !dtdctxp) {
        abort();
    }

    bufferp = xmlParserInputBufferCreateMem(&DTDDATAbegin,
                                           DTDDATAlen,
                                           XMLCHARENCODINGUTF8);
    if (!bufferp) {
        fprintf(stderr, "Failed to vv init bufferp for DTD.\n");
        abort();
    }

    dtdp = xmlIOParseDTD(NULL, bufferp, XMLCHARENCODINGUTF8);
    if (!dtdp) {
        fprintf(stderr, "Failed vv to parse DTD.\n");
        abort();
    }

    docp = xmlCtxtReadFd(ctxp, fd, filenamell, NULL, 0);
    if (!docp) {
        fprintf(stderr, "Failed to read XML\n");
        abort();
    }

    rc = xmlValidateDtd(dtdctxp, docp, dtdp);
    xmlFreeDoc(docp);
    xmlFreeParserCtxt(ctxp);
    xmlFreeDtd(dtdp);
    xmlFreeValidCtxt(dtdctxp);

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

struct desclriptionl {
    char *summaryl;
    char *textl;
};

struct protocoll {
    char *namel;
    char *uppercasenamel;
    struct isftlist interfacellistl;
    int typeindex;
    int nullrunlength;
    char *copyright;
    struct desclriptionl *desclriptionl;
    bool coreheaders;
};

struct interfacel {
    struct location locl;
    char *namel;
    char *uppercasenamel;
    int versionl;
    int sincel;
    struct isftlist requestlistl;
    struct isftlist eventlistl;
    struct isftlist enumerationllistl;
    struct isftlist linkl;
    struct desclriptionl *desclriptionl;
};

struct messagel {
    struct location locl;
    char *namel;
    char *uppercasenamel;
    struct isftlist argllist;
    struct isftlist linkl;
    int arglcountl;
    int newidcount;
    int typeindex;
    int allnull;
    int destructor;
    int sincel;
    struct desclriptionl *desclriptionl;
};

enum argltype {
    NEWIDL,
    INT,
    UNSIGNED,
    FIXED,
    STRINGL,
    OBJECTL,
    ARRAYL,
    FD
};

struct argl {
    char *namel;
    enum argltype typel;
    int nullable;
    char *interfacelnamel;
    struct isftlist linkl;
    char *summaryl;
    char *enumerationlnamel;
};

struct enumerationl {
    char *namel;
    char *uppercasenamel;
    struct isftlist entrylist;
    struct isftlist linkl;
    struct desclriptionl *desclriptionl;
    bool bitfield;
    int sincel;
};

struct entryl {
    char *namel;
    char *uppercasenamel;
    char *valuel;
    char *summaryl;
    int sincel;
    struct isftlist linkl;
};

struct parsecontextlll {
    struct location locl;
    XMLParser parser;
    struct protocoll *protocoll;
    struct interfacel *interfacel;
    struct messagel *messagel;
    struct enumerationl *enumerationl;
    struct desclriptionl *desclriptionl;
    char characterdata[8192];
    unsigned int characterdatalength;
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
    freedesclriptionl(messagel->desclriptionl);

    isftlistforeachsafe(a, anext, &messagel->argllist, linkl)
        freeargl(a);

    free(messagel);
}

static struct enumerationl *
createenumerationl(const char *namel)
{
    struct enumerationl *enumerationl;

    enumerationl = xzalloc(sizeof *enumerationl);
    enumerationl->namel = xstrdup(namel);
    enumerationl->uppercasenamel = uppercasedup(namel);
    enumerationl->sincel = 1;

    isftlistinit(&enumerationl->entrylist);

    return enumerationl;
}

static struct entryl *
createentry(const char *namel, const char *valuel)
{
    struct entryl *entryl;

    entryl = xzalloc(sizeof *entryl);
    entryl->namel = xstrdup(namel);
    entryl->uppercasenamel = uppercasedup(namel);
    entryl->valuel = xstrdup(valuel);

    return entryl;
}

static void freeentry(struct entryl *entryl)
{
    free(entryl->namel);
    free(entryl->uppercasenamel);
    free(entryl->valuel);
    free(entryl->summaryl);

    free(entryl);
}

static void freeenumerationl(struct enumerationl *enumerationl)
{
    struct entryl *e, *enext;

    free(enumerationl->namel);
    free(enumerationl->uppercasenamel);
    freedesclriptionl(enumerationl->desclriptionl);

    isftlistforeachsafe(e, enext, &enumerationl->entrylist, linkl)
        freeentry(e);

    free(enumerationl);
}

static struct interfacel *
createinterfacel(struct location locl, const char *namel, int versionl)
{
    struct interfacel *interfacel;

    interfacel = xzalloc(sizeof *interfacel);
    interfacel->locl = locl;
    interfacel->namel = xstrdup(namel);
    interfacel->uppercasenamel = uppercasedup(namel);
    interfacel->versionl = versionl;
    interfacel->sincel = 1;
    isftlistinit(&interfacel->requestlistl);
    isftlistinit(&interfacel->eventlistl);
    isftlistinit(&interfacel->enumerationllistl);

    return interfacel;
}

static void freeinterfacel(struct interfacel *interfacel)
{
    struct messagel *m, *nextm;
    struct enumerationl *e, *nexte;

    free(interfacel->namel);
    free(interfacel->uppercasenamel);
    freedesclriptionl(interfacel->desclriptionl);

    isftlistforeachsafe(m, nextm, &interfacel->requestlistl, linkl)
        freemessagel(m);
    isftlistforeachsafe(m, nextm, &interfacel->eventlistl, linkl)
        freemessagel(m);
    isftlistforeachsafe(e, nexte, &interfacel->enumerationllistl, linkl)
        freeenumerationl(e);

    free(interfacel);
}

static void launchtype(struct argl *a)
{
    switch (a->typel) {
        case INT:
        case FD:
            printf("int32t ");
            break;
        case NEWIDL:
        case UNSIGNED:
            printf("uint32t ");
            break;
        case FIXED:
            printf("isftfixedt ");
            break;
        case STRINGL:
            printf("const char *");
            break;
        case OBJECTL:
            printf("struct %s *", a->interfacelnamel);
            break;
        case ARRAYL:
            printf("struct isftarray *");
            break;
        default:
            break;
    }
}

static void launchstubs(struct isftlist *messagellist, struct interfacel *interfacel)
{
    struct messagel *m;
    struct argl *a, *retl;
    int hasdestructor, hasdestroy;
    printf("/** @ingroup iface%s */\n", interfacel->namel);
    printf("static inline void\n"
           "%ssetuserdata(struct %s *%s, void *userdata)\n"
           "{\n"
           "\tisftproxysetuserdata((struct isftproxy *) %s, userdata);\n"
           "}\n\n",
           interfacel->namel, interfacel->namel, interfacel->namel,
           interfacel->namel);
    printf("/** @ingroup iface%s */\n", interfacel->namel);
    printf("static inline void *\n"
           "%sgetuserdata(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetuserdata((struct isftproxy *) %s);\n"
           "}\n\n",
           interfacel->namel, interfacel->namel, interfacel->namel,
           interfacel->namel);
    printf("static inline uint32t\n"
           "%sgetversion(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetversion((struct isftproxy *) %s);\n"
           "}\n\n",
           interfacel->namel, interfacel->namel, interfacel->namel,
           interfacel->namel);
    hasdestructor = 0;
    hasdestroy = 0;
    isftlistforeach(m, messagellist, linkl) {
        if (m->destructor) {
            hasdestructor = 1;
        }
        if (strcmp(m->namel, "destroy") == 0) {
            hasdestroy = 1;
        }
    }

    if (!hasdestructor && hasdestroy) {
        fail(&interfacel->locl,
             "interfacel '%s' has method nameld destroy "
             "but no destructor",
             interfacel->namel);
        exit(EXITFAILURE);
    }
}
static void launchstubs(struct isftlist *messagellist, struct interfacel *interfacel)
{
    if (!hasdestroy && strcmp(interfacel->namel, "isftdisplay") != 0) {
        printf("/** @ingroup iface%s */\n", interfacel->namel);
        printf("static inline void\n"
               "%sdestroy(struct %s *%s)\n"
               "{\n"
               "\tisftproxydestroy("
               "(struct isftproxy *) %s);\n"
               "}\n\n",
               interfacel->namel, interfacel->namel, interfacel->namel,
               interfacel->namel);
    }

    if (isftlistempty(messagellist)) {
        return;
    }

    isftlistforeach(m, messagellist, linkl) {
        if (m->newidcount > 1) {
            warn(&m->locl,
                 "request '%s::%s' has more than "
                 "one newid argl, not launchting stub\n",
                 interfacel->namel, m->namel);
            continue;
        }

        retl = NULL;
        isftlistforeach(a, &m->argllist, linkl) {
            if (a->typel == NEWIDL) {
                retl = a;
            }
        }

        printf("/**\n"
               " * @ingroup iface%s\n", interfacel->namel);
        if (m->desclriptionl && m->desclriptionl->textlll) {
            formattextllltocomment(m->desclriptionl->textlll, false);
        }
        printf(" */\n");
        if (retl && retl->interfacelnamel == NULL) {
            printf("static inline void *\n");
        } else if (retl) {
            printf("static inline struct %s *\n",
                   retl->interfacelnamel);
        } else {
            printf("static inline void\n");
        }
}
}
static void launchstubs(struct isftlist *messagellist, struct interfacel *interfacel)
{
        printf("%s%s(struct %s *%s",
               interfacel->namel, m->namel,
               interfacel->namel, interfacel->namel);

        isftlistforeach(a, &m->argllist, linkl) {
            if (a->typel == NEWIDL && a->interfacelnamel == NULL) {
                printf(", const struct isftinterfacel *interfacel"
                       ", uint32t versionl");
                continue;
            } else if (a->typel == NEWIDL) {
                continue;
            }
            printf(", ");
            launchtype(a);
            printf("%s", a->namel);
        }

        printf(")\n""{\n");
        if (retl && retl->interfacelnamel == NULL) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructorversioned("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, interfacel, versionl",
                   retl->namel, retl->namel,
                   interfacel->namel,
                   interfacel->uppercasenamel,
                   m->uppercasenamel);
        } else if (retl) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructor("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, &%sinterfacel",
                   retl->namel, retl->namel,
                   interfacel->namel,
                   interfacel->uppercasenamel,
                   m->uppercasenamel,
                   retl->interfacelnamel);
        } else {
            printf("\tisftproxymarshal((struct isftproxy *) %s,\n"
                   "\t\t\t %s%s",
                   interfacel->namel,
                   interfacel->uppercasenamel,
                   m->uppercasenamel);
        }
}
static void launchstubs(struct isftlist *messagellist, struct interfacel *interfacel)
{
        isftlistforeach(a, &m->argllist, linkl) {
            if (a->typel == NEWIDL) {
                if (a->interfacelnamel == NULL)
                    printf(", interfacel->namel, versionl");
                printf(", NULL");
            } else {
                printf(", %s", a->namel);
            }
        }
        printf(");\n");

        if (m->destructor) {
            printf("\n\tisftproxydestroy("
                   "(struct isftproxy *) %s);\n",
                   interfacel->namel);
        }

        if (retl && retl->interfacelnamel == NULL) {
            printf("\n\treturn (void *) %s;\n", retl->namel);
        } else if (retl) {
            printf("\n\treturn (struct %s *) %s;\n",
                   retl->interfacelnamel, retl->namel);
        }

        printf("}\n\n");
    }

static void launcheventwrappers(struct isftlist *messagellist, struct interfacel *interfacel)
{
    struct messagel *m;
    struct argl *a;

    if (strcmp(interfacel->namel, "isftdisplay") == 0) {
        return;
    }

    isftlistforeach(m, messagellist, linkl) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " * Sends an %s event to the client owning the resource.\n",
               interfacel->namel,
               m->namel);
        printf(" * @param resource The client's resource\n");
        isftlistforeach(a, &m->argllist, linkl) {
            if (a->summaryl) {
                printf(" * @param %s %s\n", a->namel, a->summaryl);
            }
        }
        printf(" */\n");
        printf("static inline void\n"
               "%ssend%s(struct isftresource *resource",
               interfacel->namel, m->namel);

        isftlistforeach(a, &m->argllist, linkl) {
            printf(", ");
            switch (a->typel) {
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
               interfacel->uppercasenamel, m->uppercasenamel);

        isftlistforeach(a, &m->argllist, linkl)
            printf(", %s", a->namel);

        printf(");\n");
        printf("}\n\n");
    }
}

static void launchenumerationls(struct interfacel *interfacel)
{
    struct enumerationl *e;
    struct entryl *entryl;

    isftlistforeach(e, &interfacel->enumerationllistl, linkl) {
        struct desclriptionl *adescl = e->desclriptionl;

        printf("#ifndef %s%sENUM\n",
               interfacel->uppercasenamel, e->uppercasenamel);
        printf("#define %s%sENUM\n",
               interfacel->uppercasenamel, e->uppercasenamel);

        if (adescl) {
            printf("/**\n");
            printf(" * @ingroup iface%s\n", interfacel->namel);
            formattextllltocomment(adescl->summaryl, false);
            if (adescl->textlll) {
                formattextllltocomment(adescl->textlll, false);
            }
            printf(" */\n");
        }
        printf("enum %s%s {\n", interfacel->namel, e->namel);
        isftlistforeach(entryl, &e->entrylist, linkl) {
            if (entryl->summaryl || entryl->sincel > 1) {
                printf("\t/**\n");
                if (entryl->summaryl) {
                    printf("\t * %s\n", entryl->summaryl);
                }
                if (entryl->sincel > 1) {
                    printf("\t * @sincel %d\n", entryl->sincel);
                }
                printf("\t */\n");
            }
            printf("\t%s%s%s = %s,\n",
                   interfacel->uppercasenamel,
                   e->uppercasenamel,
                   entryl->uppercasenamel, entryl->valuel);
        }
        printf("};\n");

        isftlistforeach(entryl, &e->entrylist, linkl) {
            if (entryl->sincel == 1) {
                            continue;
            }

                        printf("/**\n * @ingroup iface%s\n */\n", interfacel->namel);
                        printf("#define %s%s%sSINCEVERSION %d\n",
                               interfacel->uppercasenamel,
                               e->uppercasenamel, entryl->uppercasenamel,
                               entryl->sincel);
        }

        printf("#endif /* %s%sENUM */\n\n",
               interfacel->uppercasenamel, e->uppercasenamel);
    }
}
void launchstructs1(void)
{
    enum sides sides;
        isftlistforeach(a, &m->argllist, linkl) {
            if (sides == SERVER && a->typel == NEWIDL &&
                a->interfacelnamel == NULL) {
                printf("\t * @param interfacel namel of the objects interfacel\n"
                       "\t * @param versionl versionl of the objects interfacel\n");
            }

            if (a->summaryl) {
                printf("\t * @param %s %s\n", a->namel,
                       a->summaryl);
            }
        }
        if (m->sincel > 1) {
            printf("\t * @sincel %d\n", m->sincel);
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
                   indent(n), interfacel->namel, interfacel->namel);
        }

        isftlistforeach(a, &m->argllist, linkl) {
            printf(",\n%s", indent(n));

            if (sides == SERVER && a->typel == OBJECTL) {
                printf("struct isftresource *");
            } else if (sides == SERVER && a->typel == NEWIDL && a->interfacelnamel == NULL) {
                printf("const char *interfacel, uint32t versionl, uint32t ");
            } else if (sides == CLIENT && a->typel == OBJECTL && a->interfacelnamel == NULL) {
                printf("void *");
            } else if (sides == CLIENT && a->typel == NEWIDL) {
                printf("struct %s *", a->interfacelnamel);
            } else {
                launchtype(a);
            }

            printf("%s", a->namel);
        }
}
static void launchstructs(struct isftlist *messagellist, struct interfacel *interfacel, enum sides sides)
{
    struct messagel *m;
    struct argl *a;
    int n;

    if (isftlistempty(messagellist)) {
        return;
    }
    isftlistforeach(m, messagellist, linkl) {
        struct desclriptionl *mdescl = m->desclriptionl;

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
               interfacel->namel);
        printf("static inline int\n"
               "%saddlistener(struct %s *%s,\n"
               "%sconst struct %slistener *listener, void *data)\n"
               "{\n"
               "\treturn isftproxyaddlistener((struct isftproxy *) %s,\n"
               "%s(void (**)(void)) listener, data);\n"
               "}\n\n",
               interfacel->namel, interfacel->namel, interfacel->namel,
               indent(14 + strlen(interfacel->namel)),
               interfacel->namel, interfacel->namel, indent(37));
    }
}

static int strtouint(const char *strl)
{
    long int retl;
    char *end;
    int preverrno = errno;

    errno = 0;
    retl = strtol(strl, &end, 10);
    if (errno != 0 || end == strl || *end != '\0') {
        return -1;
    }

    if (retl < 0 || retl > INTMAX) {
        return -1;
    }

    errno = preverrno;
    return (int)retl;
}

static void validateidentifier(struct location *locl,
    const char *strl,
    enum identifierrole role)
{
    const char *scan;
    if (!*strl) {
        fail(locl, "element namel is empty");
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
            fail(locl,
                 "'%s' is not a valid trailing identifier part", strl);
        } else {
            fail(locl,
                 "'%s' is not a valid standalone identifier", strl);
        }
    }
}

static int versionfromsince(struct parsecontextlll *ctxp, const char *sincel)
{
    int versionl;

    if (sincel != NULL) {
        versionl = strtouint(sincel);
        if (versionl == -1) {
            fail(&ctxp->locl, "invalid integer (%s)\n", sincel);
        } else if (versionl > ctxp->interfacel->versionl) {
            fail(&ctxp->locl, "sincel (%u) largler than versionl (%u)\n",
                 versionl, ctxp->interfacel->versionl);
        }
    } else {
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
            if (versionl == -1) {
                fail(&ctxp->locl, "wrong versionl (%s)", attsl[i + 1]);
            }
        }
        if (strcmp(attsl[i], "typel") == 0) {
            typel = attsl[i + 1];
        }
        if (strcmp(attsl[i], "valuel") == 0) {
            valuel = attsl[i + 1];
        }
        if (strcmp(attsl[i], "interfacel") == 0) {
            interfacelnamel = attsl[i + 1];
        }
        if (strcmp(attsl[i], "summaryl") == 0) {
            summaryl = attsl[i + 1];
        }
        if (strcmp(attsl[i], "sincel") == 0) {
            sincel = attsl[i + 1];
        }
        if (strcmp(attsl[i], "allow-null") == 0) {
            allownull = attsl[i + 1];
        }
        if (strcmp(attsl[i], "enum") == 0) {
            enumerationlnamel = attsl[i + 1];
        }
        if (strcmp(attsl[i], "bitfield") == 0) {
            bitfield = attsl[i + 1];
        }
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    struct parsecontextlll *ctxp = data;
    struct interfacel *interfacel;
    struct messagel *messagel;
    struct argl *argl;
    struct enumerationl *enumerationl;
    struct entryl *entryl;
    struct desclriptionl *desclriptionl = NULL;
    const char *namel = NULL;
    const char *typel = NULL;
    const char *interfacelnamel = NULL;
    const char *valuel = NULL;
    const char *summaryl = NULL;
    const char *sincel = NULL;
    const char *allownull = NULL;
    const char *enumerationlnamel = NULL;
    const char *bitfield = NULL;
    int i, versionl = 0;

    ctxp->locl.linenumber = XMLGetCurrentLineNumber(ctxp->parser);
    startelement1(&(*attsl));

    ctxp->characterdatalength = 0;
    if (strcmp(elementnamel, "protocoll") == 0) {
        if (namel == NULL) {
            fail(&ctxp->locl, "no protocoll namel given");
        }

        validateidentifier(&ctxp->locl, namel, STANDALONEIDENTL);
        ctxp->protocoll->namel = xstrdup(namel);
        ctxp->protocoll->uppercasenamel = uppercasedup(namel);
    } else if (strcmp(elementnamel, "copyright") == 0) {
    } else if (strcmp(elementnamel, "interfacel") == 0) {
        if (namel == NULL) {
            fail(&ctxp->locl, "no interfacel namel given");
        }

        if (versionl == 0) {
            fail(&ctxp->locl, "no interfacel versionl given");
        }

        validateidentifier(&ctxp->locl, namel, STANDALONEIDENTL);
        interfacel = createinterfacel(ctxp->locl, namel, versionl);
        ctxp->interfacel = interfacel;
        isftlistinsert(ctxp->protocoll->interfacellistl.prev,
                       &interfacel->linkl);
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "request") == 0 ||
           strcmp(elementnamel, "event") == 0) {
        if (namel == NULL) {
            fail(&ctxp->locl, "no request namel given");
        }

        validateidentifier(&ctxp->locl, namel, STANDALONEIDENTL);
        messagel = createmessagel(ctxp->locl, namel);

        if (strcmp(elementnamel, "request") == 0) {
            isftlistinsert(ctxp->interfacel->requestlistl.prev,
                           &messagel->linkl);
        } else {
            isftlistinsert(ctxp->interfacel->eventlistl.prev,
                           &messagel->linkl);
        }
        if (typel != NULL && strcmp(typel, "destructor") == 0) {
            messagel->destructor = 1;
        }
        versionl = versionfromsince(ctxp, sincel);
        if (versionl < ctxp->interfacel->sincel) {
            warn(&ctxp->locl, "sincel versionl not increasing\n");
        }
        ctxp->interfacel->sincel = versionl;
        messagel->sincel = versionl;

        if (strcmp(namel, "destroy") == 0 && !messagel->destructor) {
            fail(&ctxp->locl, "destroy request should be destructor typel");
        }
        ctxp->messagel = messagel;
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "argl") == 0) {
        if (namel == NULL) {
            fail(&ctxp->locl, "no arglument namel given");
        }

        validateidentifier(&ctxp->locl, namel, STANDALONEIDENTL);
        argl = createargl(namel);
        if (!setargltype(argl, typel)) {
            fail(&ctxp->locl, "unknown typel (%s)", typel);
        }

        switch (argl->typel) {
            case NEWIDL:
                ctxp->messagel->newidcount++;
            case OBJECTL:
                if (interfacelnamel) {
                    validateidentifier(&ctxp->locl,
                                       interfacelnamel,
                                       STANDALONEIDENTL);
                    argl->interfacelnamel = xstrdup(interfacelnamel);
                }
                break;
            default:
                if (interfacelnamel != NULL) {
                    fail(&ctxp->locl, "interfacel attribute not allowed for typel %s", typel);
                }
                break;
        }

        if (allownull) {
            if (strcmp(allownull, "true") == 0) {
                argl->nullable = 1;
            } else if (strcmp(allownull, "false") != 0) {
                fail(&ctxp->locl,
                     "invalid valuel for allow-null attribute (%s)",
                     allownull);
            }

            if (!isnullabletype(argl)) {
                fail(&ctxp->locl,
                     "allow-null is only valid for objects, strings, and arrays");
            }
        }

        if (enumerationlnamel == NULL || strcmp(enumerationlnamel, "") == 0) {
            argl->enumerationlnamel = NULL;
        } else {
            argl->enumerationlnamel = xstrdup(enumerationlnamel);
        }
        if (summaryl) {
            argl->summaryl = xstrdup(summaryl);
        }

        isftlistinsert(ctxp->messagel->argllist.prev, &argl->linkl);
        ctxp->messagel->arglcountl++;
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "enum") == 0) {
        if (namel == NULL) {
            fail(&ctxp->locl, "no enum namel given");
        }

        validateidentifier(&ctxp->locl, namel, TRAILINGIDENTL);
        enumerationl = createenumerationl(namel);

        if (bitfield == NULL || strcmp(bitfield, "false") == 0) {
            enumerationl->bitfield = false;
        } else if (strcmp(bitfield, "true") == 0) {
            enumerationl->bitfield = true;
        } else {
            fail(&ctxp->locl,
                 "invalid valuel (%s) for bitfield attribute (only true/false are accepted)",
                 bitfield);
        }

        isftlistinsert(ctxp->interfacel->enumerationllistl.prev,
                       &enumerationl->linkl);
        ctxp->enumerationl = enumerationl;
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "entryl") == 0) {
        if (namel == NULL) {
            fail(&ctxp->locl, "no entryl namel given");
        }

        validateidentifier(&ctxp->locl, namel, TRAILINGIDENTL);
        entryl = createentry(namel, valuel);
        versionl = versionfromsince(ctxp, sincel);
        if (versionl < ctxp->enumerationl->sincel) {
            warn(&ctxp->locl, "sincel versionl not increasing\n");
        }
        ctxp->enumerationl->sincel = versionl;
        entryl->sincel = versionl;

        if (summaryl) {
            entryl->summaryl = xstrdup(summaryl);
        } else {
            entryl->summaryl = NULL;
        }
        isftlistinsert(ctxp->enumerationl->entrylist.prev,
                       &entryl->linkl);
    } else if (strcmp(elementnamel, "desclriptionl") == 0) {
        if (summaryl == NULL) {
            fail(&ctxp->locl, "desclriptionl without summaryl");
        }

        desclriptionl = xzalloc(sizeof *desclriptionl);
        desclriptionl->summaryl = xstrdup(summaryl);
        if (ctxp->messagel) {
            ctxp->messagel->desclriptionl = desclriptionl;
        } else if (ctxp->enumerationl) {
            ctxp->enumerationl->desclriptionl = desclriptionl;
        } else if (ctxp->interfacel) {
            ctxp->interfacel->desclriptionl = desclriptionl;
        } else {
            ctxp->protocoll->desclriptionl = desclriptionl;
        }
        ctxp->desclriptionl = desclriptionl;
    }
}

static struct enumerationl *
findenumerationl(struct protocoll *protocoll,
                 struct interfacel *interfacel,
                 char *enumattribute)
{
    struct interfacel *i;
    struct enumerationl *e;
    char *enumnamel;
    uint32t idx = 0, j;

    for (j = 0; j + 1 < strlen(enumattribute); j++) {
    if (enumattribute[j] == '.') {
        idx = j;
    }
    }

    if (idx > 0) {
        enumnamel = enumattribute + idx + 1;

        isftlistforeach(i, &protocoll->interfacellistl, linkl)
            if (strncmp(i->namel, enumattribute, idx) == 0) {
                isftlistforeach(e, &i->enumerationllistl, linkl)
            }
                    if (strcmp(e->namel, enumnamel) == 0) {
                        return e;
                    }
    } else if (interfacel) {
        enumnamel = enumattribute;

        isftlistforeach(e, &interfacel->enumerationllistl, linkl)
            if (strcmp(e->namel, enumnamel) == 0) {
                return e;
            }
    }

    return NULL;
}

static void verifyargluments(struct parsecontextlll *ctxp,
                             struct interfacel *interfacel,
                             struct isftlist *messagels,
                             struct isftlist *enumerationls)
{
    struct messagel *m;
    isftlistforeach(m, messagels, linkl) {
        struct argl *a;
        isftlistforeach(a, &m->argllist, linkl) {
            struct enumerationl *e;
    if (!a->enumerationlnamel) {
        continue;
    }
            e = findenumerationl(ctxp->protocoll, interfacel,
                a->enumerationlnamel);

            switch (a->typel) {
                case INT:
                    if (e && e->bitfield) {
                        fail(&ctxp->locl,
                            "bitfield-style enum must only be referenced by uint");
                    }
                    break;
                case UNSIGNED:
                    break;
                default:
                    fail(&ctxp->locl,
                        "enumerationl-style arglument has wrong typel");
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
    } else if (strcmp(namel, "desclriptionl") == 0) {
        ctxp->desclriptionl->textlll =
            strndup(ctxp->characterdata,
                ctxp->characterdatalength);
        ctxp->desclriptionl = NULL;
    } else if (strcmp(namel, "request") == 0 ||
           strcmp(namel, "event") == 0) {
        ctxp->messagel = NULL;
    } else if (strcmp(namel, "enum") == 0) {
        if (isftlistempty(&ctxp->enumerationl->entrylist)) {
            fail(&ctxp->locl, "enumerationl %s was empty",
                 ctxp->enumerationl->namel);
        }
        ctxp->enumerationl = NULL;
    } else if (strcmp(namel, "protocoll") == 0) {
        struct interfacel *i;

        isftlistforeach(i, &ctxp->protocoll->interfacellistl, linkl) {
            verifyargluments(ctxp, i, &i->requestlistl, &i->enumerationllistl);
            verifyargluments(ctxp, i, &i->eventlistl, &i->enumerationllistl);
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
    isftlistforeach(i, &protocoll->interfacellistl, linkl) {
    printf("#ifndef %sINTERFACE\n", i->uppercasenamel);
    printf("#define %sINTERFACE\n", i->uppercasenamel);
    printf("/**\n"" * @page pageiface%s %s\n",
           i->namel, i->namel);
    if (i->desclriptionl && i->desclriptionl->textlll) {
        printf(" * @section pageiface%sdescl Description\n",
               i->namel);
        formattextllltocomment(i->desclriptionl->textlll, false);
    }
    printf(" * @section pageiface%sapi API\n"
           " * See @ref iface%s.\n"" */\n",
           i->namel, i->namel);
    printf("/**\n"" * @defgroup iface%s The %s interfacel\n",
           i->namel, i->namel);
    if (i->desclriptionl && i->desclriptionl->textlll) {
        formattextllltocomment(i->desclriptionl->textlll, false);
    }
    printf(" */\n");
    printf("extern const struct isftinterfacel ""%sinterfacel;\n", i->namel);
    printf("#endif\n");
    }

    printf("\n");
    isftlistforeachsafe(i, inext, &protocoll->interfacellistl, linkl) {
        launchenumerationls(i);

        if (sides == SERVER) {
            launchstructs(&i->requestlistl, i, sides);
            launchopcodes(&i->eventlistl, i);
            launchopcodeversions(&i->eventlistl, i);
            launchopcodeversions(&i->requestlistl, i);
            launcheventwrappers(&i->eventlistl, i);
        } else {
            launchstructs(&i->eventlistl, i, sides);
            launchopcodes(&i->requestlistl, i);
            launchopcodeversions(&i->eventlistl, i);
            launchopcodeversions(&i->requestlistl, i);
            launchstubs(&i->requestlistl, i);
        }

        freeinterfacel(i);
    }
}

static void launchheader(struct protocoll *protocoll, enum sides sides)
{
    struct interfacel *i, *inext;
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
    isftlistforeach(i, &protocoll->interfacellistl, linkl) {
        launchtypesforwarddeclarations(protocoll, &i->requestlistl, &types);
        launchtypesforwarddeclarations(protocoll, &i->eventlistl, &types);
    }

    isftlistforeach(i, &protocoll->interfacellistl, linkl) {
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

    isftlistforeach(m, messagellist, linkl) {
        if (m->allnull) {
            m->typeindex = 0;
            continue;
        }
        m->typeindex =
            protocoll->nullrunlength + protocoll->typeindex;
        protocoll->typeindex += m->arglcountl;

        isftlistforeach(a, &m->argllist, linkl) {
            switch (a->typel) {
                case NEWIDL:
                case OBJECTL:
                    if (a->interfacelnamel) {
                        printf("\t&%sinterfacel,\n",
                            a->interfacelnamel);
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
                            struct interfacel *interfacel, const char *suffix)
{
    struct messagel *m;
    struct argl *a;

    if (isftlistempty(messagellist)) {
        return;
    }
    printf("static const struct isftmessagel ""%s%s[] = {\n", interfacel->namel, suffix);
    isftlistforeach(m, messagellist, linkl) {
        printf("\t{ \"%s\", \"", m->namel);
        if (m->sincel > 1) {
            printf("%d", m->sincel);
        }
            
        isftlistforeach(a, &m->argllist, linkl) {
            if (isnullabletype(a) && a->nullable) {
                printf("?");
            }

            switch (a->typel) {
                case INT:
                    printf("i");
                    break;
                case NEWIDL:
                    if (a->interfacelnamel == NULL) {
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
    struct interfacel *i, *next;
    struct isftarray types;
    char **p, *prev;
    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    if (protocoll->copyright) {
        formattextllltocomment(protocoll->copyright, true);
    }
    launchcode1();

    isftarrayinit(&types);
    isftlistforeach(i, &protocoll->interfacellistl, linkl) {
        launchtypesforwarddeclarations(protocoll, &i->requestlistl, &types);
        launchtypesforwarddeclarations(protocoll, &i->eventlistl, &types);
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
    isftlistforeach(i, &protocoll->interfacellistl, linkl) {
        launchtypes(protocoll, &i->requestlistl);
        launchtypes(protocoll, &i->eventlistl);
    }
    printf("};\n\n");
    isftlistforeachsafe(i, next, &protocoll->interfacellistl, linkl) {
        launchmessagels(protocoll->namel, &i->requestlistl, i, "requests");
        launchmessagels(protocoll->namel, &i->eventlistl, i, "events");
        printf("%s const struct isftinterfacel ""%sinterfacel = {\n""\t\"%s\", %d,\n",
               symbolvisibility, i->namel, i->namel, i->versionl);

        if (!isftlistempty(&i->requestlistl)) {
            printf("\t%d, %srequests,\n", isftlistlength(&i->requestlistl), i->namel);
        } else {
            printf("\t0, NULL,\n");
        }
        if (!isftlistempty(&i->eventlistl)) {
            printf("\t%d, %sevents,\n", isftlistlength(&i->eventlistl), i->namel);
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

static void launchopcodes(struct isftlist *messagellist, struct interfacel *interfacel)
{
    struct messagel *m;
    int opcode;

    if (isftlistempty(messagellist)) {
        return;
    }

    opcode = 0;
    isftlistforeach(m, messagellist, linkl)
        printf("#define %s%s %d\n",
               interfacel->uppercasenamel, m->uppercasenamel, opcode++);

    printf("\n");
}

static void launchopcodeversions(struct isftlist *messagellist, struct interfacel *interfacel)
{
    struct messagel *m;

    isftlistforeach(m, messagellist, linkl) {
        printf("/**\n * @ingroup iface%s\n */\n", interfacel->namel);
        printf("#define %s%sSINCEVERSION %d\n",
               interfacel->uppercasenamel, m->uppercasenamel, m->sincel);
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

    isftlistforeach(m, messagellist, linkl) {
        length = 0;
        m->allnull = 1;
    isftlistforeach(a, &m->argllist, linkl) {
    length++;
        switch (a->typel) {
            case NEWIDL:
            case OBJECTL:
                if (!a->interfacelnamel) {
                    continue;
                }
                m->allnull = 0;
                p = failonnull(isftarrayadd(types, sizeof *p));
                *p = a->interfacelnamel;
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
    struct interfacel *i;

    printf("/**\n"
           " * @page page%s The %s protocoll\n",
           protocoll->namel, protocoll->namel);

    if (protocoll->desclriptionl) {
        if (protocoll->desclriptionl->summaryl) {
            printf(" * %s\n"
                   " *\n", protocoll->desclriptionl->summaryl);
        }

        if (protocoll->desclriptionl->textlll) {
            printf(" * @section pagedescl%s Description\n", protocoll->namel);
            formattextllltocomment(protocoll->desclriptionl->textlll, false);
            printf(" *\n");
        }
    }

    printf(" * @section pageifaces%s Interfaces\n", protocoll->namel);
    isftlistforeach(i, &protocoll->interfacellistl, linkl) {
        printf(" * - @subpage pageiface%s - %s\n",
               i->namel,
               i->desclriptionl && i->desclriptionl->summaryl ?  i->desclriptionl->summaryl : "");
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
    freedesclriptionl(protocoll->desclriptionl);
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
        }
        switch (opt) {
            case 'h':
                help = true;
                break;
            case 'v':
                versionl = true;
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
    } else if (versionl) {
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
        ctxp.locl.filenamell = "<stdin>";
    } else {
        ctxp.locl.filenamell = inputlfilenamell;
    }
    if (!isdtdvalid(inputl, ctxp.locl.filenamell)) {
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
