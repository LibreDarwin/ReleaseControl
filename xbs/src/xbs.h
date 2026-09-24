/*
 * xbs.h - open-source Apple Build & Integration (B&I) tool
 *
 * Reimplements the public contract of Apple's proprietary XBS/buildit
 * tools for driving Apple-OSS Darwin projects (xnu, CoreOSMakefiles,
 * libkdd/libsyscall/libkmod aliases).  Not affiliated with Apple.
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

#ifndef XBS_H
#define XBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

/* ---- diagnostics ---- */
void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---- string / list helpers ---- */
char *xstrdup(const char *s);
char *xasprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *trim(char *s);
char *path_join(const char *a, const char *b);
char *path_dirname(const char *p);
char *slurp(const char *path);      /* malloc'd file contents or NULL */
void setenvf(const char *name, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

typedef struct strlist {
    char **v;
    size_t n, cap;
} strlist;

void sl_init(strlist *l);
void sl_add(strlist *l, const char *s);
void sl_sort(strlist *l);              /* qsort by strcmp */
void sl_free(strlist *l);

/* ---- filesystem ---- */
bool file_exists(const char *path);
bool dir_exists(const char *path);
void mkdirs(const char *path);      /* mkdir -p */
void rmrf(const char *path);        /* rm -rf, safe against "" and "/" */
void copy_tree(const char *src, const char *dst);
void cp_r(const char *src, const char *dst);

/* ---- process / env ---- */
extern bool g_verbose;                 /* echo commands */
extern bool g_dryrun;                  /* -n: print only */
extern bool g_prune_junk;              /* skip .git/.DS_Store/... when copying */
extern const char *g_argv0;            /* real argv[0], for config lookup */
int  sys_ncpus(void);
int  spawnv(char *const argv[], bool verbose);           /* wait + return status */
int  spawnv_in(const char *cwd, char *const argv[]);     /* spawn in cwd */
int  run_cmd(char *const argv[]);                        /* spawnv + die on signal */
int  run_capture(char *const argv[], char *buf, size_t n); /* spawn + capture stdout */
int  run_capture_quiet(char *const argv[], char *buf, size_t n); /* ...stderr to /dev/null */
const char *host_arch(void);

/* ---- manifest (build-sum) evaluation ---- */
strlist *manifest_of_dir(const char *root);
char **sl_argv(const strlist *l);                        /* NULL-terminated copy */

/* ---- version heuristics ---- */
/* Given srcroot path, guess "<project>" and "<version>-suffixed" bits. */
const char *project_from_path(const char *srcroot, char *buf, size_t n);
const char *version_from_path(const char *srcroot, char *buf, size_t n);
/* Apple-style source version from the git state of dir (1 on success). */
int git_source_version(const char *dir, char *buf, size_t n);

/* ---- xbs subcommands (main.c entry points) ---- */
int cmd_buildit(int argc, char **argv);
int cmd_submitproject(int argc, char **argv);

#endif