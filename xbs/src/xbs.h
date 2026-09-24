/*
 * xbs.h - open-source Apple Build & Integration (B&I) tool
 *
 * Reimplements the public contract of Apple's proprietary XBS/buildit
 * tools for driving Apple-OSS Darwin projects (xnu, CoreOSMakefiles,
 * libkdd/libsyscall/libkmod aliases).  Not affiliated with Apple.
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
int  sys_ncpus(void);
int  spawnv(char *const argv[], bool verbose);           /* wait + return status */
int  spawnv_in(const char *cwd, char *const argv[]);     /* spawn in cwd */
int  run_cmd(char *const argv[]);                        /* spawnv + die on signal */
const char *host_arch(void);

/* ---- manifest (build-sum) evaluation ---- */
strlist *manifest_of_dir(const char *root);
char **sl_argv(const strlist *l);                        /* NULL-terminated copy */

/* ---- version heuristics ---- */
/* Given srcroot path, guess "<project>" and "<version>-suffixed" bits. */
const char *project_from_path(const char *srcroot, char *buf, size_t n);
const char *version_from_path(const char *srcroot, char *buf, size_t n);

/* ---- xbs subcommands (main.c entry points) ---- */
int cmd_buildit(int argc, char **argv);
int cmd_submitproject(int argc, char **argv);

#endif