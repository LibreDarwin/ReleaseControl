/*
 * submitproject.c - "xbs submitproject"
 *
 * The real Apple submitproject packages a project's sources and submits them
 * to the B&I server; open source has no server, so we produce the same local
 * artifacts the build server would have: a clean source tarball (with optional
 * .sum manifest) staged under a train directory.
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
    const char *srcroot;
    const char *project;
    const char *version;
    const char *release;      /* -release TRAIN; staged under Software/<TRAIN> */
    const char *output;       /* -o DIR; default: ./Submitted/<proj>-<ver> */
    bool verbose;
    bool dryrun;
} subopts;

static const char *
need_value(int argc, char **argv, int *i, const char *flag)
{
    if (*i + 1 >= argc)
        die("option %s requires a value", flag);
    return argv[++*i];
}

static void
usage_submitproject(FILE *f)
{
    fprintf(f,
"usage: xbs submitproject [options] <srcroot>\n"
"\n"
"Package a Darwin project's sources the way B&I's submitproject would: stage\n"
"a pristine copy under Software/<TRAIN>/Sources and write a .sum manifest of\n"
"what was submitted (used by 'xbs buildit' to reproduce builds).\n"
"\n"
"options:\n"
"  -project NAME     project name (default: basename of srcroot)\n"
"  -version VERSION  source version (default: parsed from path)\n"
"  -release TRAIN    release train subdirectory (default: Current)\n"
"  -o DIR            output directory (default: ./Submitted)\n"
"  -verbose          echo what is being done\n"
"  -n                dry run\n"
"  -h                this help\n"
);
}

static void
parse_args(int argc, char **argv, subopts *o)
{
    int i;
    o->srcroot = o->project = o->version = o->release = o->output = NULL;
    o->verbose = false;
    o->dryrun = false;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-project"))
            o->project = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-version"))
            o->version = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-release"))
            o->release = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-o"))
            o->output = need_value(argc, argv, &i, a);
        else if (!strcmp(a, "-verbose"))
            o->verbose = true;
        else if (!strcmp(a, "-n") || !strcmp(a, "-dryrun"))
            o->dryrun = true;
        else if (!strcmp(a, "-h") || !strcmp(a, "-help") ||
                 !strcmp(a, "--help")) {
            usage_submitproject(stdout);
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
        die("missing <srcroot> argument (try 'xbs submitproject -h')");
    g_verbose = o->verbose;
}

int
cmd_submitproject(int argc, char **argv)
{
    subopts o;
    char projbuf[256], verbuf[256];
    const char *proj, *ver, *train;
    char *base, *dest, *sumfile;
    strlist *man;
    FILE *f;
    size_t i;

    parse_args(argc, argv, &o);

    project_from_path(o.srcroot, projbuf, sizeof projbuf);
    if (o.project)
        snprintf(projbuf, sizeof projbuf, "%s", o.project);
    version_from_path(o.srcroot, verbuf, sizeof verbuf);
    if (o.version)
        snprintf(verbuf, sizeof verbuf, "%s", o.version);

    proj = projbuf;
    ver = verbuf;
    train = o.release ? o.release : "Current";
    base = o.output ? xstrdup(o.output) : xstrdup("Submitted");
    dest = xasprintf("%s/Software/%s/Sources/%s-%s", base, train,
                     proj, ver);

    printf("xbs: submitproject: %s -> %s\n", o.srcroot, dest);

    if (o.dryrun) {
        printf("xbs:   (dry-run) copy sources, write %s/%s-%s.sum\n",
               dest, proj, ver);
        return 0;
    }

    if (!dir_exists(o.srcroot))
        die("source directory %s does not exist", o.srcroot);

    g_prune_junk = true;
    rmrf(dest);
    copy_tree(o.srcroot, dest);
    g_prune_junk = false;

    man = manifest_of_dir(dest);
    sl_sort(man);
    sumfile = xasprintf("%s/%s-%s.sum", dest, proj, ver);
    f = fopen(sumfile, "w");
    if (!f)
        die("cannot write %s: %s", sumfile, strerror(errno));
    for (i = 0; i < man->n; i++)
        fprintf(f, "%s\n", man->v[i]);
    fclose(f);

    printf("xbs:   submitted %zu files; sum at %s\n", man->n, sumfile);
    sl_free(man);

    free(sumfile);
    free(base);
    free(dest);
    return 0;
}