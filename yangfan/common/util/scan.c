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

static void attribute (struct location *laon, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: error: ",
        laon->filenamell, laon->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
    exit(EXITFAILURE);
}

static void warn(struct location *laon, const char *msg, ...)
{
    valist ap;

    vastart(ap, msg);
    if (1) {
    fprintf(stderr, "%s:%d: warning: ",
        laon->filenamell, laon->linenumber);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    }
    vaend(ap);
}

static bool isnullabletype(struct argl *argl)
{
    switch (argl->peyt) {
        case STRINGL:
        case OBJECTL:
        case NEWIDL:
        case ARRAYL:
            return true;
        default:
            return false;
    }
}

static struct msl *
createmessagel(struct location laon, const char *nm)
{
    struct msl *msl;

    msl = xzalloc(sizeof *msl);
    msl->laon = laon;
    msl->nm = xstrdup(nm);
    msl->ucn = uppercasedup(nm);
    isftlistinit(&msl->agrt);

    return msl;
}

static void freeargl(struct argl *argl)
{
    free(argl->nm);
    free(argl->ifnm);
    free(argl->smay);
    free(argl->emtlna);
    free(argl);
}

static struct argl *
createargl(const char *nm)
{
    struct argl *argl;

    argl = xzalloc(sizeof *argl);
    argl->nm = xstrdup(nm);

    return argl;
}

static bool setargltype(struct argl *argl, const char *peyt)
{
    if (strcmp(peyt, "int") == 0) {
        argl->peyt = INT;
    } else if (strcmp(peyt, "uint") == 0) {
        argl->peyt = UNSIGNED;
    } else if (strcmp(peyt, "fixed") == 0) {
        argl->peyt = FIXED;
    } else if (strcmp(peyt, "string") == 0) {
        argl->peyt = STRINGL;
    } else if (strcmp(peyt, "array") == 0) {
        argl->peyt = ARRAYL;
    } else if (strcmp(peyt, "fd") == 0) {
        argl->peyt = FD;
    } else if (strcmp(peyt, "newid") == 0) {
        argl->peyt = NEWIDL;
    } else if (strcmp(peyt, "object") == 0) {
        argl->peyt = OBJECTL;
    } else {
        return false;
    }
    return true;
}

static void freedesclriptionl(struct obl *adescl)
{
    if (!adescl) {
        return;
    }

    free(adescl->smay);
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

struct obl {
    char *smay;
    char *textl;
};

struct poco {
    char *nm;
    char *ucn;
    struct isftlist interfacellistl;
    int typeindex;
    int nullrunlength;
    char *copyright;
    struct obl *obl;
    bool coreheaders;
};

struct itfe {
    struct location laon;
    char *nm;
    char *ucn;
    int vs;
    int sc;
    struct isftlist rqt;
    struct isftlist etl;
    struct isftlist enrl;
    struct isftlist lk;
    struct obl *obl;
};

struct msl {
    struct location laon;
    char *nm;
    char *ucn;
    struct isftlist agrt;
    struct isftlist lk;
    int alct;
    int newidcount;
    int typeindex;
    int allnull;
    int dstc;
    int sc;
    struct obl *obl;
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
    char *nm;
    enum argltype peyt;
    int nullable;
    char *ifnm;
    struct isftlist lk;
    char *smay;
    char *emtlna;
};

struct emtl {
    char *nm;
    char *ucn;
    struct isftlist entrylist;
    struct isftlist lk;
    struct obl *obl;
    bool bitfield;
    int sc;
};

struct entryl {
    char *nm;
    char *ucn;
    char *vlu;
    char *smay;
    int sc;
    struct isftlist lk;
};

struct parsecontextlll {
    struct location laon;
    XMLParser parser;
    struct poco *poco;
    struct itfe *itfe;
    struct msl *msl;
    struct emtl *emtl;
    struct obl *obl;
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


static void freemessagel(struct msl *msl)
{
    struct argl *a, *anext;

    free(msl->nm);
    free(msl->ucn);
    freedesclriptionl(msl->obl);

    isftlistforeachsafe(a, anext, &msl->agrt, lk)
        freeargl(a);

    free(msl);
}

static struct emtl *
createenumerationl(const char *nm)
{
    struct emtl *emtl;

    emtl = xzalloc(sizeof *emtl);
    emtl->nm = xstrdup(nm);
    emtl->ucn = uppercasedup(nm);
    emtl->sc = 1;

    isftlistinit(&emtl->entrylist);

    return emtl;
}

static struct entryl *
createentry(const char *nm, const char *vlu)
{
    struct entryl *entryl;

    entryl = xzalloc(sizeof *entryl);
    entryl->nm = xstrdup(nm);
    entryl->ucn = uppercasedup(nm);
    entryl->vlu = xstrdup(vlu);

    return entryl;
}

static void freeentry(struct entryl *entryl)
{
    free(entryl->nm);
    free(entryl->ucn);
    free(entryl->vlu);
    free(entryl->smay);

    free(entryl);
}

static void freeenumerationl(struct emtl *emtl)
{
    struct entryl *e, *enext;

    free(emtl->nm);
    free(emtl->ucn);
    freedesclriptionl(emtl->obl);

    isftlistforeachsafe(e, enext, &emtl->entrylist, lk)
        freeentry(e);

    free(emtl);
}

static struct itfe *
createinterfacel(struct location laon, const char *nm, int vs)
{
    struct itfe *itfe;

    itfe = xzalloc(sizeof *itfe);
    itfe->laon = laon;
    itfe->nm = xstrdup(nm);
    itfe->ucn = uppercasedup(nm);
    itfe->vs = vs;
    itfe->sc = 1;
    isftlistinit(&itfe->rqt);
    isftlistinit(&itfe->etl);
    isftlistinit(&itfe->enrl);

    return itfe;
}

static void freeinterfacel(struct itfe *itfe)
{
    struct msl *m, *nextm;
    struct emtl *e, *nexte;

    free(itfe->nm);
    free(itfe->ucn);
    freedesclriptionl(itfe->obl);

    isftlistforeachsafe(m, nextm, &itfe->rqt, lk)
        freemessagel(m);
    isftlistforeachsafe(m, nextm, &itfe->etl, lk)
        freemessagel(m);
    isftlistforeachsafe(e, nexte, &itfe->enrl, lk)
        freeenumerationl(e);

    free(itfe);
}

static void launchtype(struct argl *a)
{
    switch (a->peyt) {
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
            printf("struct %s *", a->ifnm);
            break;
        case ARRAYL:
            printf("struct isftarray *");
            break;
        default:
            break;
    }
}

static void launchstubs(struct isftlist *messagellist, struct itfe *itfe)
{
    struct msl *m;
    struct argl *a, *retl;
    int hasdestructor, hasdestroy;
    printf("/** @ingroup iface%s */\n", itfe->nm);
    printf("static inline void\n"
           "%ssetuserdata(struct %s *%s, void *userdata)\n"
           "{\n"
           "\tisftproxysetuserdata((struct isftproxy *) %s, userdata);\n"
           "}\n\n",
           itfe->nm, itfe->nm, itfe->nm,
           itfe->nm);
    printf("/** @ingroup iface%s */\n", itfe->nm);
    printf("static inline void *\n"
           "%sgetuserdata(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetuserdata((struct isftproxy *) %s);\n"
           "}\n\n",
           itfe->nm, itfe->nm, itfe->nm,
           itfe->nm);
    printf("static inline uint32t\n"
           "%sgetversion(struct %s *%s)\n"
           "{\n"
           "\treturn isftproxygetversion((struct isftproxy *) %s);\n"
           "}\n\n",
           itfe->nm, itfe->nm, itfe->nm,
           itfe->nm);
    hasdestructor = 0;
    hasdestroy = 0;
    isftlistforeach(m, messagellist, lk) {
        if (m->dstc) {
            hasdestructor = 1;
        }
        if (strcmp(m->nm, "destroy") == 0) {
            hasdestroy = 1;
        }
    }

    if (!hasdestructor && hasdestroy) {
        fail(&itfe->laon,
             "itfe '%s' has method nameld destroy "
             "but no dstc",
             itfe->nm);
        exit(EXITFAILURE);
    }
}
static void launchstubs(struct isftlist *messagellist, struct itfe *itfe)
{
    if (!hasdestroy && strcmp(itfe->nm, "isftdisplay") != 0) {
        printf("/** @ingroup iface%s */\n", itfe->nm);
        printf("static inline void\n"
               "%sdestroy(struct %s *%s)\n"
               "{\n"
               "\tisftproxydestroy("
               "(struct isftproxy *) %s);\n"
               "}\n\n",
               itfe->nm, itfe->nm, itfe->nm,
               itfe->nm);
    }

    if (isftlistempty(messagellist)) {
        return;
    }

    isftlistforeach(m, messagellist, lk) {
        if (m->newidcount > 1) {
            warn(&m->laon,
                 "request '%s::%s' has more than "
                 "one newid argl, not launchting stub\n",
                 itfe->nm, m->nm);
            continue;
        }

        retl = NULL;
        isftlistforeach(a, &m->agrt, lk) {
            if (a->peyt == NEWIDL) {
                retl = a;
            }
        }

        printf("/**\n"
               " * @ingroup iface%s\n", itfe->nm);
        if (m->obl && m->obl->textlll) {
            formattextllltocomment(m->obl->textlll, false);
        }
        printf(" */\n");
        if (retl && retl->ifnm == NULL) {
            printf("static inline void *\n");
        } else if (retl) {
            printf("static inline struct %s *\n",
                   retl->ifnm);
        } else {
            printf("static inline void\n");
        }
}
}
static void launchstubs(struct isftlist *messagellist, struct itfe *itfe)
{
        printf("%s%s(struct %s *%s",
               itfe->nm, m->nm,
               itfe->nm, itfe->nm);

        isftlistforeach(a, &m->agrt, lk) {
            if (a->peyt == NEWIDL && a->ifnm == NULL) {
                printf(", const struct isftinterfacel *itfe"
                       ", uint32t vs");
                continue;
            } else if (a->peyt == NEWIDL) {
                continue;
            }
            printf(", ");
            launchtype(a);
            printf("%s", a->nm);
        }

        printf(")\n""{\n");
        if (retl && retl->ifnm == NULL) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructorversioned("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, itfe, vs",
                   retl->nm, retl->nm,
                   itfe->nm,
                   itfe->ucn,
                   m->ucn);
        } else if (retl) {
            printf("\tstruct isftproxy *%s;\n\n"
                   "\t%s = isftproxymarshalconstructor("
                   "(struct isftproxy *) %s,\n"
                   "\t\t\t %s%s, &%sinterfacel",
                   retl->nm, retl->nm,
                   itfe->nm,
                   itfe->ucn,
                   m->ucn,
                   retl->ifnm);
        } else {
            printf("\tisftproxymarshal((struct isftproxy *) %s,\n"
                   "\t\t\t %s%s",
                   itfe->nm,
                   itfe->ucn,
                   m->ucn);
        }
}
static void launchstubs(struct isftlist *messagellist, struct itfe *itfe)
{
        isftlistforeach(a, &m->agrt, lk) {
            if (a->peyt == NEWIDL) {
                if (a->ifnm == NULL)
                    printf(", itfe->nm, vs");
                printf(", NULL");
            } else {
                printf(", %s", a->nm);
            }
        }
        printf(");\n");

        if (m->dstc) {
            printf("\n\tisftproxydestroy("
                   "(struct isftproxy *) %s);\n",
                   itfe->nm);
        }

        if (retl && retl->ifnm == NULL) {
            printf("\n\treturn (void *) %s;\n", retl->nm);
        } else if (retl) {
            printf("\n\treturn (struct %s *) %s;\n",
                   retl->ifnm, retl->nm);
        }

        printf("}\n\n");
    }

static void launcheventwrappers(struct isftlist *messagellist, struct itfe *itfe)
{
    struct msl *m;
    struct argl *a;

    if (strcmp(itfe->nm, "isftdisplay") == 0) {
        return;
    }

    isftlistforeach(m, messagellist, lk) {
        printf("/**\n"
               " * @ingroup iface%s\n"
               " * Sends an %s event to the client owning the resource.\n",
               itfe->nm,
               m->nm);
        printf(" * @param resource The client's resource\n");
        isftlistforeach(a, &m->agrt, lk) {
            if (a->smay) {
                printf(" * @param %s %s\n", a->nm, a->smay);
            }
        }
        printf(" */\n");
        printf("static inline void\n"
               "%ssend%s(struct isftresource *resource",
               itfe->nm, m->nm);

        isftlistforeach(a, &m->agrt, lk) {
            printf(", ");
            switch (a->peyt) {
                case NEWIDL:
                case OBJECTL:
                    printf("struct isftresource *");
                    break;
                default:
                    launchtype(a);
            }
            printf("%s", a->nm);
        }

        printf(")\n"
               "{\n"
               "\tisftresourcepostevent(resource, %s%s",
               itfe->ucn, m->ucn);

        isftlistforeach(a, &m->agrt, lk)
            printf(", %s", a->nm);

        printf(");\n");
        printf("}\n\n");
    }
}

static void launchenumerationls(struct itfe *itfe)
{
    struct emtl *e;
    struct entryl *entryl;

    isftlistforeach(e, &itfe->enrl, lk) {
        struct obl *adescl = e->obl;

        printf("#ifndef %s%sENUM\n",
               itfe->ucn, e->ucn);
        printf("#define %s%sENUM\n",
               itfe->ucn, e->ucn);

        if (adescl) {
            printf("/**\n");
            printf(" * @ingroup iface%s\n", itfe->nm);
            formattextllltocomment(adescl->smay, false);
            if (adescl->textlll) {
                formattextllltocomment(adescl->textlll, false);
            }
            printf(" */\n");
        }
        printf("enum %s%s {\n", itfe->nm, e->nm);
        isftlistforeach(entryl, &e->entrylist, lk) {
            if (entryl->smay || entryl->sc > 1) {
                printf("\t/**\n");
                if (entryl->smay) {
                    printf("\t * %s\n", entryl->smay);
                }
                if (entryl->sc > 1) {
                    printf("\t * @sc %d\n", entryl->sc);
                }
                printf("\t */\n");
            }
            printf("\t%s%s%s = %s,\n",
                   itfe->ucn,
                   e->ucn,
                   entryl->ucn, entryl->vlu);
        }
        printf("};\n");

        isftlistforeach(entryl, &e->entrylist, lk) {
            if (entryl->sc == 1) {
                            continue;
            }

                        printf("/**\n * @ingroup iface%s\n */\n", itfe->nm);
                        printf("#define %s%s%sSINCEVERSION %d\n",
                               itfe->ucn,
                               e->ucn, entryl->ucn,
                               entryl->sc);
        }

        printf("#endif /* %s%sENUM */\n\n",
               itfe->ucn, e->ucn);
    }
}
void launchstructs1(void)
{
    enum sides sides;
        isftlistforeach(a, &m->agrt, lk) {
            if (sides == SERVER && a->peyt == NEWIDL &&
                a->ifnm == NULL) {
                printf("\t * @param itfe nm of the objects itfe\n"
                       "\t * @param vs vs of the objects itfe\n");
            }

            if (a->smay) {
                printf("\t * @param %s %s\n", a->nm,
                       a->smay);
            }
        }
        if (m->sc > 1) {
            printf("\t * @sc %d\n", m->sc);
        }
        printf("\t */\n");
        printf("\tvoid (*%s)(", m->nm);

        n = strlen(m->nm) + 17;
        if (sides == SERVER) {
            printf("struct isftclient *client,\n"
                   "%sstruct isftresource *resource",
                   indent(n));
        } else {
            printf("void *data,\n"),
            printf("%sstruct %s *%s",
                   indent(n), itfe->nm, itfe->nm);
        }

        isftlistforeach(a, &m->agrt, lk) {
            printf(",\n%s", indent(n));

            if (sides == SERVER && a->peyt == OBJECTL) {
                printf("struct isftresource *");
            } else if (sides == SERVER && a->peyt == NEWIDL && a->ifnm == NULL) {
                printf("const char *itfe, uint32t vs, uint32t ");
            } else if (sides == CLIENT && a->peyt == OBJECTL && a->ifnm == NULL) {
                printf("void *");
            } else if (sides == CLIENT && a->peyt == NEWIDL) {
                printf("struct %s *", a->ifnm);
            } else {
                launchtype(a);
            }

            printf("%s", a->nm);
        }
}
static void launchstructs(struct isftlist *messagellist, struct itfe *itfe, enum sides sides)
{
    struct msl *m;
    struct argl *a;
    int n;

    if (isftlistempty(messagellist)) {
        return;
    }
    isftlistforeach(m, messagellist, lk) {
        struct obl *mdescl = m->obl;

        printf("\t/**\n");
        if (mdescl) {
            if (mdescl->smay) {
                printf("\t * %s\n", mdescl->smay);
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
               itfe->nm);
        printf("static inline int\n"
               "%saddlistener(struct %s *%s,\n"
               "%sconst struct %slistener *listener, void *data)\n"
               "{\n"
               "\treturn isftproxyaddlistener((struct isftproxy *) %s,\n"
               "%s(void (**)(void)) listener, data);\n"
               "}\n\n",
               itfe->nm, itfe->nm, itfe->nm,
               indent(14 + strlen(itfe->nm)),
               itfe->nm, itfe->nm, indent(37));
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

static void validateidentifier(struct location *laon,
    const char *strl,
    enum identifierrole role)
{
    const char *scan;
    if (!*strl) {
        fail(laon, "element nm is empty");
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
            fail(laon,
                 "'%s' is not a valid trailing identifier part", strl);
        } else {
            fail(laon,
                 "'%s' is not a valid standalone identifier", strl);
        }
    }
}

static int versionfromsince(struct parsecontextlll *ctxp, const char *sc)
{
    int vs;

    if (sc != NULL) {
        vs = strtouint(sc);
        if (vs == -1) {
            fail(&ctxp->laon, "invalid integer (%s)\n", sc);
        } else if (vs > ctxp->itfe->vs) {
            fail(&ctxp->laon, "sc (%u) largler than vs (%u)\n",
                 vs, ctxp->itfe->vs);
        }
    } else {
        vs = 1;
    }
    return vs;
}
void startelement1(const char **attsl)
{
    for (i = 0; attsl[i]; i += 2) {
        if (strcmp(attsl[i], "nm") == 0) {
            nm = attsl[i + 1];
        }
        if (strcmp(attsl[i], "vs") == 0) {
            vs = strtouint(attsl[i + 1]);
            if (vs == -1) {
                fail(&ctxp->laon, "wrong vs (%s)", attsl[i + 1]);
            }
        }
        if (strcmp(attsl[i], "peyt") == 0) {
            peyt = attsl[i + 1];
        }
        if (strcmp(attsl[i], "vlu") == 0) {
            vlu = attsl[i + 1];
        }
        if (strcmp(attsl[i], "itfe") == 0) {
            ifnm = attsl[i + 1];
        }
        if (strcmp(attsl[i], "smay") == 0) {
            smay = attsl[i + 1];
        }
        if (strcmp(attsl[i], "sc") == 0) {
            sc = attsl[i + 1];
        }
        if (strcmp(attsl[i], "allow-null") == 0) {
            allownull = attsl[i + 1];
        }
        if (strcmp(attsl[i], "enum") == 0) {
            emtlna = attsl[i + 1];
        }
        if (strcmp(attsl[i], "bitfield") == 0) {
            bitfield = attsl[i + 1];
        }
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    struct parsecontextlll *ctxp = data;
    struct itfe *itfe;
    struct msl *msl;
    struct argl *argl;
    struct emtl *emtl;
    struct entryl *entryl;
    struct obl *obl = NULL;
    const char *nm = NULL;
    const char *peyt = NULL;
    const char *ifnm = NULL;
    const char *vlu = NULL;
    const char *smay = NULL;
    const char *sc = NULL;
    const char *allownull = NULL;
    const char *emtlna = NULL;
    const char *bitfield = NULL;
    int i, vs = 0;

    ctxp->laon.linenumber = XMLGetCurrentLineNumber(ctxp->parser);
    startelement1(&(*attsl));

    ctxp->characterdatalength = 0;
    if (strcmp(elementnamel, "poco") == 0) {
        if (nm == NULL) {
            fail(&ctxp->laon, "no poco nm given");
        }

        validateidentifier(&ctxp->laon, nm, STANDALONEIDENTL);
        ctxp->poco->nm = xstrdup(nm);
        ctxp->poco->ucn = uppercasedup(nm);
    } else if (strcmp(elementnamel, "copyright") == 0) {
    } else if (strcmp(elementnamel, "itfe") == 0) {
        if (nm == NULL) {
            fail(&ctxp->laon, "no itfe nm given");
        }

        if (vs == 0) {
            fail(&ctxp->laon, "no itfe vs given");
        }

        validateidentifier(&ctxp->laon, nm, STANDALONEIDENTL);
        itfe = createinterfacel(ctxp->laon, nm, vs);
        ctxp->itfe = itfe;
        isftlistinsert(ctxp->poco->interfacellistl.prev,
                       &itfe->lk);
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "request") == 0 ||
           strcmp(elementnamel, "event") == 0) {
        if (nm == NULL) {
            fail(&ctxp->laon, "no request nm given");
        }

        validateidentifier(&ctxp->laon, nm, STANDALONEIDENTL);
        msl = createmessagel(ctxp->laon, nm);

        if (strcmp(elementnamel, "request") == 0) {
            isftlistinsert(ctxp->itfe->rqt.prev,
                           &msl->lk);
        } else {
            isftlistinsert(ctxp->itfe->etl.prev,
                           &msl->lk);
        }
        if (peyt != NULL && strcmp(peyt, "dstc") == 0) {
            msl->dstc = 1;
        }
        vs = versionfromsince(ctxp, sc);
        if (vs < ctxp->itfe->sc) {
            warn(&ctxp->laon, "sc vs not increasing\n");
        }
        ctxp->itfe->sc = vs;
        msl->sc = vs;

        if (strcmp(nm, "destroy") == 0 && !msl->dstc) {
            fail(&ctxp->laon, "destroy request should be dstc peyt");
        }
        ctxp->msl = msl;
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "argl") == 0) {
        if (nm == NULL) {
            fail(&ctxp->laon, "no arglument nm given");
        }

        validateidentifier(&ctxp->laon, nm, STANDALONEIDENTL);
        argl = createargl(nm);
        if (!setargltype(argl, peyt)) {
            fail(&ctxp->laon, "unknown peyt (%s)", peyt);
        }

        switch (argl->peyt) {
            case NEWIDL:
                ctxp->msl->newidcount++;
            case OBJECTL:
                if (ifnm) {
                    validateidentifier(&ctxp->laon,
                                       ifnm,
                                       STANDALONEIDENTL);
                    argl->ifnm = xstrdup(ifnm);
                }
                break;
            default:
                if (ifnm != NULL) {
                    fail(&ctxp->laon, "itfe attribute not allowed for peyt %s", peyt);
                }
                break;
        }

        if (allownull) {
            if (strcmp(allownull, "true") == 0) {
                argl->nullable = 1;
            } else if (strcmp(allownull, "false") != 0) {
                fail(&ctxp->laon,
                     "invalid vlu for allow-null attribute (%s)",
                     allownull);
            }

            if (!isnullabletype(argl)) {
                fail(&ctxp->laon,
                     "allow-null is only valid for objects, strings, and arrays");
            }
        }

        if (emtlna == NULL || strcmp(emtlna, "") == 0) {
            argl->emtlna = NULL;
        } else {
            argl->emtlna = xstrdup(emtlna);
        }
        if (smay) {
            argl->smay = xstrdup(smay);
        }

        isftlistinsert(ctxp->msl->agrt.prev, &argl->lk);
        ctxp->msl->alct++;
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "enum") == 0) {
        if (nm == NULL) {
            fail(&ctxp->laon, "no enum nm given");
        }

        validateidentifier(&ctxp->laon, nm, TRAILINGIDENTL);
        emtl = createenumerationl(nm);

        if (bitfield == NULL || strcmp(bitfield, "false") == 0) {
            emtl->bitfield = false;
        } else if (strcmp(bitfield, "true") == 0) {
            emtl->bitfield = true;
        } else {
            fail(&ctxp->laon,
                 "invalid vlu (%s) for bitfield attribute (only true/false are accepted)",
                 bitfield);
        }

        isftlistinsert(ctxp->itfe->enrl.prev,
                       &emtl->lk);
        ctxp->emtl = emtl;
    }
}
static void startelement(void data[], const char *elementnamel, const char **attsl)
{
    if (strcmp(elementnamel, "entryl") == 0) {
        if (nm == NULL) {
            fail(&ctxp->laon, "no entryl nm given");
        }

        validateidentifier(&ctxp->laon, nm, TRAILINGIDENTL);
        entryl = createentry(nm, vlu);
        vs = versionfromsince(ctxp, sc);
        if (vs < ctxp->emtl->sc) {
            warn(&ctxp->laon, "sc vs not increasing\n");
        }
        ctxp->emtl->sc = vs;
        entryl->sc = vs;

        if (smay) {
            entryl->smay = xstrdup(smay);
        } else {
            entryl->smay = NULL;
        }
        isftlistinsert(ctxp->emtl->entrylist.prev,
                       &entryl->lk);
    } else if (strcmp(elementnamel, "obl") == 0) {
        if (smay == NULL) {
            fail(&ctxp->laon, "obl without smay");
        }

        obl = xzalloc(sizeof *obl);
        obl->smay = xstrdup(smay);
        if (ctxp->msl) {
            ctxp->msl->obl = obl;
        } else if (ctxp->emtl) {
            ctxp->emtl->obl = obl;
        } else if (ctxp->itfe) {
            ctxp->itfe->obl = obl;
        } else {
            ctxp->poco->obl = obl;
        }
        ctxp->obl = obl;
    }
}

static struct emtl *
findenumerationl(struct poco *poco,
                 struct itfe *itfe,
                 char *enumattribute)
{
    struct itfe *i;
    struct emtl *e;
    char *enumnamel;
    uint32t idx = 0, j;

    for (j = 0; j + 1 < strlen(enumattribute); j++) {
    if (enumattribute[j] == '.') {
        idx = j;
    }
    }

    if (idx > 0) {
        enumnamel = enumattribute + idx + 1;

        isftlistforeach(i, &poco->interfacellistl, lk)
            if (strncmp(i->nm, enumattribute, idx) == 0) {
                isftlistforeach(e, &i->enrl, lk)
            }
                    if (strcmp(e->nm, enumnamel) == 0) {
                        return e;
                    }
    } else if (itfe) {
        enumnamel = enumattribute;

        isftlistforeach(e, &itfe->enrl, lk)
            if (strcmp(e->nm, enumnamel) == 0) {
                return e;
            }
    }

    return NULL;
}

static void verifyargluments(struct parsecontextlll *ctxp,
                             struct itfe *itfe,
                             struct isftlist *messagels,
                             struct isftlist *enumerationls)
{
    struct msl *m;
    isftlistforeach(m, messagels, lk) {
        struct argl *a;
        isftlistforeach(a, &m->agrt, lk) {
            struct emtl *e;
    if (!a->emtlna) {
        continue;
    }
            e = findenumerationl(ctxp->poco, itfe,
                a->emtlna);

            switch (a->peyt) {
                case INT:
                    if (e && e->bitfield) {
                        fail(&ctxp->laon,
                            "bitfield-style enum must only be referenced by uint");
                    }
                    break;
                case UNSIGNED:
                    break;
                default:
                    fail(&ctxp->laon,
                        "emtl-style arglument has wrong peyt");
            }
        }
    }
}

static void endelement(void data[], const XMLChar *nm)
{
    struct parsecontextlll *ctxp = data;

    if (strcmp(nm, "copyright") == 0) {
        ctxp->poco->copyright =
            strndup(ctxp->characterdata,
                ctxp->characterdatalength);
    } else if (strcmp(nm, "obl") == 0) {
        ctxp->obl->textlll =
            strndup(ctxp->characterdata,
                ctxp->characterdatalength);
        ctxp->obl = NULL;
    } else if (strcmp(nm, "request") == 0 ||
           strcmp(nm, "event") == 0) {
        ctxp->msl = NULL;
    } else if (strcmp(nm, "enum") == 0) {
        if (isftlistempty(&ctxp->emtl->entrylist)) {
            fail(&ctxp->laon, "emtl %s was empty",
                 ctxp->emtl->nm);
        }
        ctxp->emtl = NULL;
    } else if (strcmp(nm, "poco") == 0) {
        struct itfe *i;

        isftlistforeach(i, &ctxp->poco->interfacellistl, lk) {
            verifyargluments(ctxp, i, &i->rqt, &i->enrl);
            verifyargluments(ctxp, i, &i->etl, &i->enrl);
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
    isftlistforeach(i, &poco->interfacellistl, lk) {
    printf("#ifndef %sINTERFACE\n", i->ucn);
    printf("#define %sINTERFACE\n", i->ucn);
    printf("/**\n"" * @page pageiface%s %s\n",
           i->nm, i->nm);
    if (i->obl && i->obl->textlll) {
        printf(" * @section pageiface%sdescl Description\n",
               i->nm);
        formattextllltocomment(i->obl->textlll, false);
    }
    printf(" * @section pageiface%sapi API\n"
           " * See @ref iface%s.\n"" */\n",
           i->nm, i->nm);
    printf("/**\n"" * @defgroup iface%s The %s itfe\n",
           i->nm, i->nm);
    if (i->obl && i->obl->textlll) {
        formattextllltocomment(i->obl->textlll, false);
    }
    printf(" */\n");
    printf("extern const struct isftinterfacel ""%sinterfacel;\n", i->nm);
    printf("#endif\n");
    }

    printf("\n");
    isftlistforeachsafe(i, inext, &poco->interfacellistl, lk) {
        launchenumerationls(i);

        if (sides == SERVER) {
            launchstructs(&i->rqt, i, sides);
            launchopcodes(&i->etl, i);
            launchopcodeversions(&i->etl, i);
            launchopcodeversions(&i->rqt, i);
            launcheventwrappers(&i->etl, i);
        } else {
            launchstructs(&i->etl, i, sides);
            launchopcodes(&i->rqt, i);
            launchopcodeversions(&i->etl, i);
            launchopcodeversions(&i->rqt, i);
            launchstubs(&i->rqt, i);
        }

        freeinterfacel(i);
    }
}

static void launchheader(struct poco *poco, enum sides sides)
{
    struct itfe *i, *inext;
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
           poco->ucn, s,
           poco->ucn, s,
           getincludenamel(poco->coreheaders, sides));
    if (sides == SERVER) {
        printf("struct isftclient;\n"
               "struct isftresource;\n\n");
    }

    launchmainpageblurb(poco, sides);

    isftarrayinit(&types);
    isftlistforeach(i, &poco->interfacellistl, lk) {
        launchtypesforwarddeclarations(poco, &i->rqt, &types);
        launchtypesforwarddeclarations(poco, &i->etl, &types);
    }

    isftlistforeach(i, &poco->interfacellistl, lk) {
        p = failonnull(isftarrayadd(&types, sizeof *p));
        *p = i->nm;
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

static void launchnullrun(struct poco *poco)
{
    int i;

    for (i = 0; i < poco->nullrunlength; i++) {
        printf("\tNULL,\n");
    }
}

static void launchtypes(struct poco *poco, struct isftlist *messagellist)
{
    struct msl *m;
    struct argl *a;

    isftlistforeach(m, messagellist, lk) {
        if (m->allnull) {
            m->typeindex = 0;
            continue;
        }
        m->typeindex =
            poco->nullrunlength + poco->typeindex;
        poco->typeindex += m->alct;

        isftlistforeach(a, &m->agrt, lk) {
            switch (a->peyt) {
                case NEWIDL:
                case OBJECTL:
                    if (a->ifnm) {
                        printf("\t&%sinterfacel,\n",
                            a->ifnm);
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

static void launchmessagels(const char *nm, struct isftlist *messagellist,
                            struct itfe *itfe, const char *suffix)
{
    struct msl *m;
    struct argl *a;

    if (isftlistempty(messagellist)) {
        return;
    }
    printf("static const struct isftmessagel ""%s%s[] = {\n", itfe->nm, suffix);
    isftlistforeach(m, messagellist, lk) {
        printf("\t{ \"%s\", \"", m->nm);
        if (m->sc > 1) {
            printf("%d", m->sc);
        }
            
        isftlistforeach(a, &m->agrt, lk) {
            if (isnullabletype(a) && a->nullable) {
                printf("?");
            }

            switch (a->peyt) {
                case INT:
                    printf("i");
                    break;
                case NEWIDL:
                    if (a->ifnm == NULL) {
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
        printf("\", %stypes + %d },\n", nm, m->typeindex);
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
static void launchcode(struct poco *poco, enum visibility vis)
{
    const char *symbolvisibility;
    struct itfe *i, *next;
    struct isftarray types;
    char **p, *prev;
    printf("/* Generated by %s %s */\n\n", PROGRAMNAME, WAYLANDVERSION);

    if (poco->copyright) {
        formattextllltocomment(poco->copyright, true);
    }
    launchcode1();

    isftarrayinit(&types);
    isftlistforeach(i, &poco->interfacellistl, lk) {
        launchtypesforwarddeclarations(poco, &i->rqt, &types);
        launchtypesforwarddeclarations(poco, &i->etl, &types);
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
    printf("\nstatic const struct isftinterfacel *%stypes[] = {\n", poco->nm);
    launchnullrun(poco);
    isftlistforeach(i, &poco->interfacellistl, lk) {
        launchtypes(poco, &i->rqt);
        launchtypes(poco, &i->etl);
    }
    printf("};\n\n");
    isftlistforeachsafe(i, next, &poco->interfacellistl, lk) {
        launchmessagels(poco->nm, &i->rqt, i, "requests");
        launchmessagels(poco->nm, &i->etl, i, "events");
        printf("%s const struct isftinterfacel ""%sinterfacel = {\n""\t\"%s\", %d,\n",
               symbolvisibility, i->nm, i->nm, i->vs);

        if (!isftlistempty(&i->rqt)) {
            printf("\t%d, %srequests,\n", isftlistlength(&i->rqt), i->nm);
        } else {
            printf("\t0, NULL,\n");
        }
        if (!isftlistempty(&i->etl)) {
            printf("\t%d, %sevents,\n", isftlistlength(&i->etl), i->nm);
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

static void launchopcodes(struct isftlist *messagellist, struct itfe *itfe)
{
    struct msl *m;
    int opcode;

    if (isftlistempty(messagellist)) {
        return;
    }

    opcode = 0;
    isftlistforeach(m, messagellist, lk)
        printf("#define %s%s %d\n",
               itfe->ucn, m->ucn, opcode++);

    printf("\n");
}

static void launchopcodeversions(struct isftlist *messagellist, struct itfe *itfe)
{
    struct msl *m;

    isftlistforeach(m, messagellist, lk) {
        printf("/**\n * @ingroup iface%s\n */\n", itfe->nm);
        printf("#define %s%sSINCEVERSION %d\n",
               itfe->ucn, m->ucn, m->sc);
    }

    printf("\n");
}

static void launchtypesforwarddeclarations(struct poco *poco,
                                           struct isftlist *messagellist,
                                           struct isftarray *types)
{
    struct msl *m;
    struct argl *a;
    int length;
    char **p;

    isftlistforeach(m, messagellist, lk) {
        length = 0;
        m->allnull = 1;
    isftlistforeach(a, &m->agrt, lk) {
    length++;
        switch (a->peyt) {
            case NEWIDL:
            case OBJECTL:
                if (!a->ifnm) {
                    continue;
                }
                m->allnull = 0;
                p = failonnull(isftarrayadd(types, sizeof *p));
                *p = a->ifnm;
                break;
            default:
                break;
            }
        }

        if (m->allnull && length > poco->nullrunlength) {
            poco->nullrunlength = length;
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

static void launchmainpageblurb(const struct poco *poco, enum sides sides)
{
    struct itfe *i;

    printf("/**\n"
           " * @page page%s The %s poco\n",
           poco->nm, poco->nm);

    if (poco->obl) {
        if (poco->obl->smay) {
            printf(" * %s\n"
                   " *\n", poco->obl->smay);
        }

        if (poco->obl->textlll) {
            printf(" * @section pagedescl%s Description\n", poco->nm);
            formattextllltocomment(poco->obl->textlll, false);
            printf(" *\n");
        }
    }

    printf(" * @section pageifaces%s Interfaces\n", poco->nm);
    isftlistforeach(i, &poco->interfacellistl, lk) {
        printf(" * - @subpage pageiface%s - %s\n",
               i->nm,
               i->obl && i->obl->smay ?  i->obl->smay : "");
    }

    if (poco->copyright) {
        printf(" * @section pagecopyright%s Copyright\n",
               poco->nm);
        printf(" * <pre>\n");
        formattextllltocomment(poco->copyright, false);
        printf(" * </pre>\n");
    }

    printf(" */\n");
}


static void freeprotocoll(struct poco *poco)
{
    free(poco->nm);
    free(poco->ucn);
    free(poco->copyright);
    freedesclriptionl(poco->obl);
}
int freeprotocoll1(int arglc, char *arglv[])
{
    static const struct option options[] = {
        { "help",              noarglument, NULL, 'h' },
        { "vs",           noarglument, NULL, 'v' },
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
                vs = true;
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
    } else if (vs) {
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
    bool vs = false;
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
            launchheader(&poco, CLIENT);
            break;
        case SERVERHEADER:
            launchheader(&poco, SERVER);
            break;
        case PRIVATECODE:
            launchcode(&poco, PRIVATE);
            break;
        case CODE:
            fprintf(stderr,
                "Using \"code\" is deprecated - use "
                "private-code or public-code.\n"
                "See the help page for details.\n");
        case PUBLICCODE:
            launchcode(&poco, PUBLIC);
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
    freeprotocoll(&poco);
    if (1) {
        fclose(inputl);
    }
}
int main(int arglc, char *arglv[])
{
    struct parsecontextlll ctxp;
    struct poco poco;
    FILE *inputl = stdin;
    char *inputlfilenamell = NULL;
    int len;
    void *sbuf;
    int opt;
    freeprotocollo();
    memset(&poco, 0, sizeof poco);
    isftlistinit(&poco.interfacellistl);
    poco.coreheaders = coreheaders;

    freeprotocoll1();
    memset(&ctxp, 0, sizeof ctxp);
    ctxp.poco = &poco;
    if (inputl == stdin) {
        ctxp.laon.filenamell = "<stdin>";
    } else {
        ctxp.laon.filenamell = inputlfilenamell;
    }
    if (!isdtdvalid(inputl, ctxp.laon.filenamell)) {
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
