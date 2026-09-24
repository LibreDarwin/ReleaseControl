/*
 * buildit.c - "xbs buildit": build a Darwin project the Apple B&I way.
 *
 * Implements the phase pipeline ~rc/bin/buildit / XBS "buildit" expose to
 * make-based (CoreOSMakefiles/xnu) and Xcode-project (libkdd/libsyscall/
 * libkmod) Darwin projects, using the RC_* environment contract:
 *
 *   installsrc -> clean -> install -> verify -> sum [-> merge] [-> archive]
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 LibreDarwin
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _DARWIN_C_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xbs.h"

typedef struct {
    strlist archs;
    strlist envs;             /* -env KEY=VALUE passthrough */
    const char *conf;         /* -conf FILE (project database) */
    const char *project;      /* -project / -buildAlias */
    const char *release;      /* -release train */
    const char *version;      /* -version */
    const char *rootsdir;     /* -rootsDirectory */
    const char *merge;        /* -merge dir */
    const char *sdk;          /* -sdk */
    const char *othercflags;  /* -othercflags */
    const char *target;       /* -target (make target override) */
    const char *rc_os;        /* -os */
    const char *config;       /* -configuration (RC_TARGET_CONFIG) */
    const char *srcroot;      /* positional arg */
    bool clean;
    bool noinstallsrc;
    bool nosum;
    bool noverify;
    bool archive;
    bool verbose;
    int parallel;
} opts;

/* ---- argument parsing ---- */

static const char *
need_value(int argc, char **argv, int *i, const char *flag)
{
    if (*i + 1 >= argc)
        die("option %s requires a value", flag);
    return argv[++*i];
}

static void
usage_buildit(FILE *f)
{
    fprintf(f,
"usage: xbs buildit [options] <srcroot>\n"
"\n"
"Build a Darwin project the Apple B&I way.  Sets the RC_* environment and\n"
"runs: installsrc -> clean -> install -> verify -> sum [-> merge] [-> archive].\n"
"\n"
"options:\n"
"  -arch NAME            architecture to build (repeatable; default: host)\n"
"  -project NAME         project / build alias name (default: basename of srcroot)\n"
"  -buildAlias NAME      xnu-style alias name (alias for -project)\n"
"  -conf FILE            project database (aliases/trains); default: $XBS_CONF,\n"
"                        else <bindir>/conf or <bindir>/../share/xbs/conf\n"
"  -release TRAIN        release train (RC_RELEASE)\n"
"  -version VERSION      explicit source version (default: parsed from the path,\n"
"                        then from git describe when the path has no version)\n"
"  -rootsDirectory DIR   write roots under DIR (default: $TMPDIR)\n"
"  -sdk SDKROOT          override SDK root\n"
"  -othercflags FLAGS    extra compile flags (RC_NONARCH_CFLAGS)\n"
"  -target NAME          override make target (default: install)\n"
"  -os NAME              RC_OS override (default: MacOSX)\n"
"  -configuration NAME   RC_TARGET_CONFIG (default: release)\n"
"  -env KEY=VALUE        pass through to the build environment (repeatable)\n"
"  -parallel N           make -jN (default: number of CPUs)\n"
"  -merge DST            copy built root on top of DST after install\n"
"  -update TRAIN         accepted for compatibility (no-op)\n"
"  -archive              also leave <proj>-<ver>~dst.tgz of the built root\n"
"  -noinstallsrc         build in place, do not shadow-copy sources\n"
"  -noclean              do not clean before building\n"
"  -nosum                do not compute/store the build sum\n"
"  -noverify             do not compare the build sum against the previous\n"
"  -verbose              echo every command\n"
"  -n                   dry run: print what would run, change nothing\n"
);
}

static void
parse_args(int argc, char **argv, opts *o)
{
    int i;

    sl_init(&o->archs);
    sl_init(&o->envs);
    o->project = o->release = o->version = o->rootsdir = NULL;
    o->merge = o->sdk = o->othercflags = o->target = o->rc_os = NULL;
    o->config = NULL;
    o->conf = NULL;
    o->srcroot = NULL;
    o->clean = true;
    o->noinstallsrc = o->nosum = o->noverify = false;
    o->archive = o->verbose = false;
    o->parallel = 0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-arch"))
            sl_add(&o->archs, need_value(argc, argv, &i, a));
        else if (!strcmp(a, "-project") || !strcmp(a, "-buildAlias") ||
                 !strcmp(a, "-buildProject"))   /* legacy naming */
            o->project = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-release"))
            o->release = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-version"))
            o->version = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-rootsDirectory"))
            o->rootsdir = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-sdk"))
            o->sdk = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-othercflags"))
            o->othercflags = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-target"))
            o->target = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-os"))
            o->rc_os = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-parallel"))
            o->parallel = atoi(need_value(argc, argv, &i, a));
        else if (!strcmp(a, "-env"))
            sl_add(&o->envs, need_value(argc, argv, &i, a));
        else if (!strcmp(a, "-configuration"))
            o->config = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-conf"))
            o->conf = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-merge"))
            o->merge = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-update"))
            (void)need_value(argc, argv, &i, a);      /* compat no-op */
        else if (!strcmp(a, "-archive"))
            o->archive = true;
        else if (!strcmp(a, "-dsymsInDstroot") || !strcmp(a, "-dsymsInDstRoot"))
            (void)0;                                   /* accepted, default keep */
        else if (!strcmp(a, "-noinstallsrc"))
            o->noinstallsrc = true;
        else if (!strcmp(a, "-noclean"))
            o->clean = false;
        else if (!strcmp(a, "-nosum"))
            o->nosum = true;
        else if (!strcmp(a, "-noverify"))
            o->noverify = true;
        else if (!strcmp(a, "-verbose"))
            o->verbose = true;
        else if (!strcmp(a, "-n") || !strcmp(a, "-dryrun") ||
                 !strcmp(a, "-dry-run"))
            g_dryrun = true;
        else if (!strcmp(a, "-h") || !strcmp(a, "-help") ||
                 !strcmp(a, "--help")) {
            usage_buildit(stdout);
            exit(0);
        }
        else if (a[0] == '-' && a[1])
            die("unknown option %s", a);
        else if (!o->srcroot)
            o->srcroot = a;
        else
            die("unexpected argument %s", a);
    }

    if (!o->srcroot)
        die("missing <srcroot> argument (try 'xbs buildit -h')");
    g_verbose = o->verbose;
}

/* ---- Xcode-project aliases from the project database (conf/) ---- */

typedef struct {
    char *prefix;
    char *subdir;
    char *config;
    char *target;
} xcode_alias;

static xcode_alias *g_aliases = NULL;
static size_t g_naliases = 0, g_capaliases = 0;

static void
alias_add(const char *prefix, const char *subdir,
          const char *config, const char *target)
{
    xcode_alias *a;
    if (g_naliases == g_capaliases) {
        g_capaliases = g_capaliases ? g_capaliases * 2 : 8;
        g_aliases = realloc(g_aliases, g_capaliases * sizeof(xcode_alias));
        if (!g_aliases)
            die("out of memory");
    }
    a = &g_aliases[g_naliases++];
    memset(a, 0, sizeof *a);
    a->prefix = xstrdup(prefix);
    a->subdir = xstrdup(subdir);
    a->config = config ? xstrdup(config) : NULL;
    a->target = target ? xstrdup(target) : NULL;
}

static void
parse_conf_line(char *line)
{
    char *tokens[16];
    int nt = 0, j;
    char *q = line;

    while (nt < 16) {
        while (*q == ' ' || *q == '\t')
            q++;
        if (!*q)
            break;
        tokens[nt++] = q;
        while (*q && *q != ' ' && *q != '\t')
            q++;
        if (*q)
            *q++ = '\0';
    }
    if (!nt || !strcmp(tokens[0], "#"))
        return;

    if (!strcmp(tokens[0], "alias")) {
        const char *config = NULL, *target = NULL;
        if (nt < 3) {
            warn("conf: malformed alias directive at '%s'", line);
            return;
        }
        for (j = 3; j + 1 < nt; j++) {
            if (!strcmp(tokens[j], "configuration"))
                config = tokens[++j];
            else if (!strcmp(tokens[j], "target"))
                target = tokens[++j];
            else
                warn("conf: unknown alias keyword '%s'", tokens[j]);
        }
        alias_add(tokens[1], tokens[2], config, target);
    } else {
        warn("conf: ignoring unknown directive '%s'", tokens[0]);
    }
}

/* Find the project database: -conf, then $XBS_CONF, then relative to the
 * executable (dev trees: <bindir>/conf; installed: <bindir>/../share/xbs/conf). */
static char *
conf_path(const opts *o)
{
    char *dir, *p, *up;

    if (o->conf && *o->conf)
        return xstrdup(o->conf);
    if (getenv("XBS_CONF") && *getenv("XBS_CONF"))
        return xstrdup(getenv("XBS_CONF"));
    if (!g_argv0 || !*g_argv0)
        return NULL;
    dir = path_dirname(g_argv0);
    p = path_join(dir, "conf/projects.conf");
    if (file_exists(p)) {
        free(dir);
        return p;
    }
    free(p);
    up = path_join(dir, "../share/xbs/conf/projects.conf");
    free(dir);
    if (file_exists(up))
        return up;
    free(up);
    return NULL;
}

static void
load_conf(const opts *o)
{
    char *path = conf_path(o), *data, *line, *next;

    if (!path) {
        warn("no project database found (set XBS_CONF); using make-only builds");
        return;
    }
    data = slurp(path);
    if (!data) {
        warn("project database %s unreadable", path);
        free(path);
        return;
    }
    line = data;
    while ((next = strchr(line, '\n')) != NULL) {
        *next = '\0';
        parse_conf_line(trim(line));
        line = next + 1;
    }
    parse_conf_line(trim(line));
    free(data);
    printf("xbs: conf: %zu alias(es) from %s\n", g_naliases, path);
    free(path);
}

static void
free_aliases(void)
{
    size_t i;
    for (i = 0; i < g_naliases; i++) {
        free(g_aliases[i].prefix);
        free(g_aliases[i].subdir);
        free(g_aliases[i].config);
        free(g_aliases[i].target);
    }
    free(g_aliases);
    g_aliases = NULL;
    g_naliases = g_capaliases = 0;
}

static const xcode_alias *
find_alias(const char *project)
{
    const xcode_alias *hit = NULL;
    size_t i, best = 0;
    for (i = 0; i < g_naliases; i++) {
        size_t n = strlen(g_aliases[i].prefix);
        if (strncmp(project, g_aliases[i].prefix, n) == 0 && n >= best) {
            best = n;
            hit = &g_aliases[i];
        }
    }
    return hit;
}

/* ---- command execution (verbose + dry-run aware) ---- */

/* Pretty-print cwd + argv as a single line, e.g. (cd dir; make install). */
static const char *
cmdline_of(const char *cwd, const strlist *args)
{
    static char line[8192];
    size_t off = 0;
    size_t i;

    if (cwd && *cwd && strcmp(cwd, "."))
        off += snprintf(line + off, sizeof line - off, "(cd %s; ", cwd);
    for (i = 0; i < args->n && off < sizeof line; i++) {
        const char *s = args->v[i];
        bool q = s && strpbrk(s, " \t\"'\\$;|&<>") != NULL;
        off += snprintf(line + off, sizeof line - off, "%s%s%s%s",
                        i ? " " : "", q ? "\"" : "", s ? s : "", q ? "\"" : "");
    }
    if (off >= sizeof line)
        line[sizeof line - 1] = '\0';
    if (cwd && *cwd && strcmp(cwd, ".")
        && off + 1 < sizeof line) {
        off += snprintf(line + off, sizeof line - off, ")");
    }
    return line;
}

/* Run args in cwd; print the command when verbose; honor -n. */
static void
xrun(const char *cwd, const strlist *args)
{
    char **argv;
    int i, status;

    printf("xbs: %s\n", cmdline_of(cwd, args));

    if (g_dryrun) {
        printf("xbs:   (dry-run)\n");
        return;
    }

    argv = sl_argv(args);
    status = spawnv_in(cwd, argv);
    for (i = 0; argv[i]; i++)
        free(argv[i]);
    free(argv);
    if (status != 0)
        die("command failed (status %d)", status);
}

/* ---- build pipeline ---- */

typedef struct {
    const char *project;
    const char *version;
    const char *rootdir;     /* <roots>/<project>-<version>.roots */
    const char *srcroot;
    const char *buildsrc;    /* where make sees the sources */
    const char *objroot;
    const char *symroot;
    const char *dstroot;
} buildctx;

static void
set_env(const opts *o, const buildctx *c)
{
    strlist *archs = (strlist *)&o->archs;
    strlist acc;
    size_t i;

    /* Explicit -env KEY=VALUE passthroughs override any of the above. */
    for (i = 0; i < o->envs.n; i++) {
        const char *kv = o->envs.v[i];
        const char *eq = strchr(kv, '=');
        char *name;
        if (!eq || eq == kv)
            die("-env expects KEY=VALUE, got '%s'", kv);
        name = xasprintf("%.*s", (int)(eq - kv), kv);
        setenv(name, eq + 1, 1);
        free(name);
    }

    sl_init(&acc);
    if (archs->n == 0) {
        sl_add(&acc, host_arch());
    } else {
        for (i = 0; i < archs->n; i++)
            sl_add(&acc, archs->v[i]);
    }
    setenv("RC_ARCHS", acc.v[0], 1);
    if (acc.n > 1) {
        size_t used = 0;
        for (i = 0; i < acc.n; i++)
            used += strlen(acc.v[i]) + 1;
        char *all = calloc(used + 1, 1);
        for (i = 0; i < acc.n; i++) {
            if (i)
                strcat(all, " ");
            strcat(all, acc.v[i]);
        }
        setenv("RC_ARCHS", all, 1);
        free(all);
    }
    sl_free(&acc);

    setenv("RC_XBS", "YES", 1);
    setenvf("RC_ProjectName", "%s", c->project);
    setenvf("RC_ProjectSourceVersion", "%s", c->version);
    setenvf("RC_ProjectNameAndSourceVersion", "%s-%s", c->project, c->version);
    setenv("RC_ProjectSourceSubversion", "0", 1);
    setenv("RC_BUILDING_INTERNAL", "0", 1);
    if (o->rc_os)
        setenvf("RC_OS", "%s", o->rc_os);
    else if (!getenv("RC_OS"))
        setenv("RC_OS", "MacOSX", 1);
    if (o->release)
        setenvf("RC_RELEASE", "%s", o->release);
    if (o->config)
        setenvf("RC_TARGET_CONFIG", "%s", o->config);
    else if (!getenv("RC_TARGET_CONFIG"))
        setenv("RC_TARGET_CONFIG", "release", 1);
    {
        const char *flags = o->othercflags ?
            o->othercflags : getenv("XBS_CFLAGS");
        char *cf = xasprintf("-pipe %s", flags ? flags : "");
        setenv("RC_CFLAGS", cf, 1);
        setenv("RC_NONARCH_CFLAGS", flags ? flags : "", 1);
        free(cf);
    }
    setenvf("SRCROOT", "%s", c->buildsrc);
    setenvf("OBJROOT", "%s", c->objroot);
    setenvf("SYMROOT", "%s", c->symroot);
    setenvf("DSTROOT", "%s", c->dstroot);
    if (o->sdk)
        setenvf("SDKROOT", "%s", o->sdk);
    setenvf("MAKEFLAGS", "-j%d", o->parallel > 0 ? o->parallel : sys_ncpus());
    setenv("INSTALL_DIR", "/", 1);
}

static void
phase_installsrc(const opts *o, const buildctx *c)
{
    if (o->noinstallsrc) {
        printf("xbs: %s: skip installsrc (-noinstallsrc)\n", c->project);
        return;
    }
    printf("xbs: %s: installsrc %s -> %s\n", c->project, c->srcroot, c->buildsrc);
    if (g_dryrun) {
        printf("xbs:   (dry-run)\n");
        return;
    }
    g_prune_junk = true;
    rmrf(c->buildsrc);
    copy_tree(c->srcroot, c->buildsrc);
    g_prune_junk = false;
}

static void
phase_clean(const opts *o, const buildctx *c, const xcode_alias *al)
{
    printf("xbs: %s: clean\n", c->project);
    (void)o;
    rmrf(c->objroot);
    if (!al) {
        strlist args;
        sl_init(&args);
        sl_add(&args, "make");
        sl_add(&args, "-C");
        sl_add(&args, c->buildsrc);
        sl_add(&args, "clean");
        xrun(c->buildsrc, &args);
        sl_free(&args);
    }
}

static void
phase_install_make(const opts *o, const buildctx *c)
{
    strlist args;
    sl_init(&args);
    sl_add(&args, "make");
    sl_add(&args, "-C");
    sl_add(&args, c->buildsrc);
    sl_add(&args, o->target ? o->target : "install");
    printf("xbs: %s: %s\n", c->project, o->target ? o->target : "install");
    xrun(c->buildsrc, &args);
    sl_free(&args);
}

static void
phase_install_xcode(const opts *o, const buildctx *c, const xcode_alias *al)
{
    strlist args;
    char *subdir = path_join(c->buildsrc, al->subdir);
    char *tmp;

    sl_init(&args);
    sl_add(&args, "xcodebuild");
    sl_add(&args, "install");
    if (al->config) {
        sl_add(&args, "-configuration");
        sl_add(&args, al->config);
    }
    if (al->target) {
        sl_add(&args, "-target");
        sl_add(&args, al->target);
    }
    tmp = xasprintf("SRCROOT=%s", subdir);
    sl_add(&args, tmp);
    free(tmp);
    tmp = xasprintf("OBJROOT=%s", c->objroot);
    sl_add(&args, tmp);
    free(tmp);
    tmp = xasprintf("SYMROOT=%s", c->symroot);
    sl_add(&args, tmp);
    free(tmp);
    tmp = xasprintf("DSTROOT=%s", c->dstroot);
    sl_add(&args, tmp);
    free(tmp);
    if (o->sdk) {
        tmp = xasprintf("SDKROOT=%s", o->sdk);
        sl_add(&args, tmp);
        free(tmp);
    }

    printf("xbs: %s: xcodebuild install (alias %s)\n", c->project, al->prefix);
    xrun(subdir, &args);
    sl_free(&args);
    free(subdir);
}

static strlist *
load_manifest(const char *path)
{
    strlist *l = calloc(1, sizeof(strlist));
    char *buf = slurp(path), *p, *nl;
    if (!buf) {
        free(l);
        return NULL;
    }
    sl_init(l);
    p = buf;
    while ((nl = strchr(p, '\n'))) {
        *nl = '\0';
        if (*p)
            sl_add(l, p);
        p = nl + 1;
    }
    free(buf);
    return l;
}

/* Sum stores a manifest of DSTROOT; verify compares it to the stored one.
 * Manifests are sorted so rebuilds compare identically regardless of
 * filesystem readdir order. */
static void
phase_verify_sum(const opts *o, const buildctx *c)
{
    char *sumfile = xasprintf("%s/%s-%s.sum", c->rootdir, c->project, c->version);
    strlist *fresh = manifest_of_dir(c->dstroot);
    strlist *stored = o->noverify ? NULL : load_manifest(sumfile);
    bool same = true;
    size_t ndiff = 0;

    sl_sort(fresh);
    if (stored)
        sl_sort(stored);

    if (o->noverify) {
        printf("xbs: %s: verify skipped (-noverify)\n", c->project);
    } else if (stored) {
        size_t i;
        if (stored->n != fresh->n) {
            same = false;
            ndiff = stored->n > fresh->n ? stored->n - fresh->n
                                         : fresh->n - stored->n;
        } else {
            for (i = 0; i < stored->n; i++)
                if (strcmp(stored->v[i], fresh->v[i])) {
                    same = false;
                    ndiff++;
                    if (ndiff <= 20)
                        printf("xbs:   diff: %s\n", fresh->v[i]);
                }
        }
        printf("xbs: %s: verify: %s (%zu %s, %zu differing)\n",
               c->project, same ? "IDENTICAL" : "DIFFERS", fresh->n,
               fresh->n == 1 ? "file" : "files", ndiff);
    } else {
        printf("xbs: %s: verify: no previous sum, recording baseline\n",
               c->project);
    }

    if ((!o->nosum) && g_dryrun) {
        printf("xbs:   (dry-run) store %s\n", sumfile);
    } else if (!o->nosum) {
        FILE *f = fopen(sumfile, "w");
        size_t i;
        if (!f)
            die("cannot write %s: %s", sumfile, strerror(errno));
        for (i = 0; i < fresh->n; i++)
            fprintf(f, "%s\n", fresh->v[i]);
        fclose(f);
        printf("xbs: %s: sum recorded: %s\n", c->project, sumfile);
    }

    if (!same)
        warn("%s: build not reproducible vs previous sum", c->project);

    if (stored)
        sl_free(stored);
    sl_free(fresh);
    free(sumfile);
}

static void
phase_merge(const opts *o, const buildctx *c)
{
    printf("xbs: %s: merge %s over %s\n", c->project, c->dstroot, o->merge);
    if (g_dryrun)
        return;
    copy_tree(c->dstroot, o->merge);
}

static void
phase_archive(const opts *o, const buildctx *c)
{
    strlist args;
    char *tgz = xasprintf("%s/%s-%s~dst.tgz", c->rootdir, c->project, c->version);
    (void)o;
    sl_init(&args);
    sl_add(&args, "tar");
    sl_add(&args, "-C");
    sl_add(&args, c->rootdir);
    sl_add(&args, "-czf");
    sl_add(&args, tgz);
    sl_add(&args, "~dst");
    xrun(c->rootdir, &args);
    sl_free(&args);
    printf("xbs: %s: archived %s\n", c->project, tgz);
    free(tgz);
}

int
cmd_buildit(int argc, char **argv)
{
    opts o;
    const xcode_alias *al;
    buildctx c;
    char projbuf[256], verbuf[256];
    char *tmp = NULL;
    char *rootdir;
    char *srcshadow, *objroot, *symroot, *dstroot;
    const char *rootsbase;

    parse_args(argc, argv, &o);
    load_conf(&o);

    project_from_path(o.srcroot, projbuf, sizeof projbuf);
    if (o.project)
        snprintf(projbuf, sizeof projbuf, "%s", o.project);
    version_from_path(o.srcroot, verbuf, sizeof verbuf);
    if (!o.version && !strcmp(verbuf, "1")) {
        char gbuf[256];
        if (git_source_version(o.srcroot, gbuf, sizeof gbuf)) {
            snprintf(verbuf, sizeof verbuf, "%s", gbuf);
            printf("xbs: version from git: %s\n", verbuf);
        }
    }
    if (o.version)
        snprintf(verbuf, sizeof verbuf, "%s", o.version);

    /* Roots base: normalize away any trailing slash so joins come out clean.
     * Precedence: -rootsDirectory, then $XBS_ROOTS_DIR, then $TMPDIR. */
    {
        const char *envroots = getenv("XBS_ROOTS_DIR");
        const char *r = o.rootsdir ? o.rootsdir
                     : envroots && *envroots ? envroots
                     : (const char *)NULL;
        if (!r) {
            const char *t = getenv("TMPDIR");
            r = t && *t ? t : "/tmp";
        }
        rootsbase = xstrdup(r);
        while (*rootsbase && rootsbase[strlen(rootsbase) - 1] == '/')
            ((char *)rootsbase)[strlen(rootsbase) - 1] = '\0';
        tmp = (char *)rootsbase;
    }

    rootdir = xasprintf("%s/%s-%s.roots", rootsbase, projbuf, verbuf);
    if (!g_dryrun)
        mkdirs(rootdir);
    srcshadow = xasprintf("%s/~src/%s-%s", rootdir, projbuf, verbuf);
    objroot = path_join(rootdir, "~obj");
    symroot = path_join(rootdir, "~sym");
    dstroot = path_join(rootdir, "~dst");
    if (!g_dryrun) {
        mkdirs(objroot);
        mkdirs(symroot);
        mkdirs(dstroot);
    }

    c.project = projbuf;
    c.version = verbuf;
    c.rootdir = rootdir;
    c.srcroot = o.srcroot;
    c.buildsrc = o.noinstallsrc ? o.srcroot : srcshadow;
    c.objroot = objroot;
    c.symroot = symroot;
    c.dstroot = dstroot;

    printf("xbs: project %s version %s\n", c.project, c.version);
    printf("xbs: roots: %s\n", rootdir);

    set_env(&o, &c);
    al = find_alias(c.project);

    phase_installsrc(&o, &c);
    if (o.clean)
        phase_clean(&o, &c, al);
    if (al)
        phase_install_xcode(&o, &c, al);
    else
        phase_install_make(&o, &c);
    phase_verify_sum(&o, &c);
    if (o.merge)
        phase_merge(&o, &c);
    if (o.archive)
        phase_archive(&o, &c);

    printf("xbs: %s: BUILD SUCCEEDED\n", c.project);
    printf("xbs:   DSTROOT=%s\n", c.dstroot);
    free(rootdir);
    free(srcshadow);
    free(objroot);
    free(symroot);
    free(dstroot);
    free(tmp);
    sl_free(&o.archs);
    sl_free(&o.envs);
    free_aliases();
    return 0;
}