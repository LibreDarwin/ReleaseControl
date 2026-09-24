/*
 * common.c - shared helpers for the open-source xbs tool.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/utsname.h>

#include "xbs.h"

/* Global verbosity knob, read by spawnv(). */
bool g_verbose = false;
bool g_dryrun = false;
bool g_prune_junk = false;
const char *g_argv0 = NULL;    /* real executable path, set by main() */

/* Entries never worth shadow-copying: SCM metadata, Finder cruft. */
static bool
junk_name(const char *name)
{
    return !strcmp(name, ".git") || !strcmp(name, ".svn") ||
           !strcmp(name, "CVS") || !strcmp(name, ".hg") ||
           !strcmp(name, ".DS_Store") || !strcmp(name, "__MACOSX") ||
           !strcmp(name, ".build") || !strcmp(name, ".Roots");
}

void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("xbs: error: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

void
warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("xbs: warning: ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

char *
xstrdup(const char *s)
{
    char *p = strdup(s ? s : "");
    if (!p)
        die("out of memory");
    return p;
}

char *
xasprintf(const char *fmt, ...)
{
    va_list ap;
    char *p = NULL;
    va_start(ap, fmt);
    if (vasprintf(&p, fmt, ap) < 0)
        die("out of memory");
    va_end(ap);
    return p;
}

char *
trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' ||
                     e[-1] == '\n' || e[-1] == '\r'))
        *--e = '\0';
    return s;
}

char *
path_join(const char *a, const char *b)
{
    size_t la;
    if (!a || !*a)
        return xstrdup(b);
    la = strlen(a);
    if (a[la - 1] == '/')
        return xasprintf("%s%s", a, b);
    return xasprintf("%s/%s", a, b);
}

/* Parent directory of p, as a malloc'd string (".", "/", "a/b" style). */
char *
path_dirname(const char *p)
{
    const char *s = strrchr(p, '/');
    if (!s)
        return xstrdup(".");
    if (s == p)
        return xstrdup("/");
    return xasprintf("%.*s", (int)(s - p), p);
}

char *
slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long sz;
    size_t got;
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

void
setenvf(const char *name, const char *fmt, ...)
{
    va_list ap;
    char *val;
    va_start(ap, fmt);
    if (vasprintf(&val, fmt, ap) < 0)
        die("out of memory");
    va_end(ap);
    setenv(name, val, 1);
    free(val);
}

void
sl_init(strlist *l)
{
    l->v = NULL;
    l->n = l->cap = 0;
}

void
sl_add(strlist *l, const char *s)
{
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 8;
        l->v = realloc(l->v, l->cap * sizeof(char *));
        if (!l->v)
            die("out of memory");
    }
    l->v[l->n++] = xstrdup(s);
}

static int
sl_cmp(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

void
sl_sort(strlist *l)
{
    if (l->n > 1)
        qsort(l->v, l->n, sizeof(char *), sl_cmp);
}

void
sl_free(strlist *l)
{
    size_t i;
    for (i = 0; i < l->n; i++)
        free(l->v[i]);
    free(l->v);
    l->v = NULL;
    l->n = l->cap = 0;
}

/* Convert a strlist into a NULL-terminated argv array (deep copy). */
char **
sl_argv(const strlist *l)
{
    char **out = calloc(l->n + 1, sizeof(char *));
    size_t i;
    if (!out)
        die("out of memory");
    for (i = 0; i < l->n; i++)
        out[i] = xstrdup(l->v[i]);
    out[l->n] = NULL;
    return out;
}

bool
file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool
dir_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void
mkdirs(const char *path)
{
    char *p, *q;
    struct stat st;

    if (!path || !*path)
        return;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return;
    p = xstrdup(path);
    for (q = p + 1; *q; q++) {
        if (*q == '/') {
            *q = '\0';
            if (mkdir(p, 0755) != 0 && errno != EEXIST)
                die("cannot create directory %s: %s", p, strerror(errno));
            *q = '/';
        }
    }
    if (mkdir(p, 0755) != 0 && errno != EEXIST &&
        !(stat(p, &st) == 0 && S_ISDIR(st.st_mode)))
        die("cannot create directory %s: %s", p, strerror(errno));
    free(p);
}

static enum { RM_KEEP, RM_ALL } rm_mode = RM_ALL;

static void
rmr(const char *path)
{
    struct stat st;
    DIR *d;
    struct dirent *e;
    char *child;

    if (lstat(path, &st) != 0)
        return;
    if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
        d = opendir(path);
        if (!d)
            return;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;
            child = path_join(path, e->d_name);
            rmr(child);
            free(child);
        }
        closedir(d);
        if (rm_mode == RM_ALL)
            rmdir(path);
        else
            unlink(path);
    } else {
        unlink(path);
    }
}

void
rmrf(const char *path)
{
    if (!path || !*path || strcmp(path, "/") == 0 || strcmp(path, ".") == 0)
        die("refusing to remove unsafe path '%s'", path);
    rm_mode = RM_ALL;
    rmr(path);
}

int
sys_ncpus(void)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 2;
}

const char *
host_arch(void)
{
    static char buf[32];
    struct utsname un;
    const char *m;

    if (uname(&un) == 0) {
        m = un.machine;
    } else {
        m = "";
    }
    if (strcmp(m, "arm64") == 0 || strcmp(m, "aarch64") == 0)
        snprintf(buf, sizeof buf, "arm64");
    else if (strcmp(m, "x86_64") == 0 || strcmp(m, "i686") == 0 ||
             strcmp(m, "i386") == 0)
        snprintf(buf, sizeof buf, "x86_64");
    else
        snprintf(buf, sizeof buf, "%s", m[0] ? m : "x86_64");
    return buf;
}

int
spawnv(char *const argv[], bool verbose)
{
    pid_t pid;
    int status;

    (void)verbose; /* buildit-level printing is handled by xrun() */
    fflush(NULL);
    pid = fork();
    if (pid < 0)
        die("fork: %s", strerror(errno));
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "xbs: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            die("waitpid: %s", strerror(errno));
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        die("command '%s' died with signal %d", argv[0], WTERMSIG(status));
    return 1;
}

int
spawnv_in(const char *cwd, char *const argv[])
{
    pid_t pid;
    int status;

    fflush(NULL);
    pid = fork();
    if (pid < 0)
        die("fork: %s", strerror(errno));
    if (pid == 0) {
        if (cwd && *cwd && chdir(cwd) != 0)
            _exit(126);
        execvp(argv[0], argv);
        fprintf(stderr, "xbs: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            die("waitpid: %s", strerror(errno));
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        die("command '%s' died with signal %d", argv[0], WTERMSIG(status));
    return 1;
}

int
run_cmd(char *const argv[])
{
    int st = spawnv(argv, g_verbose);
    if (st != 0)
        die("command failed (status %d): %s", st, argv[0]);
    return 0;
}

/* Run argv, capturing stdout into buf (NUL-terminated, trailing newline
 * stripped).  Returns the child exit status, or 1 on failure. */
static int
run_capture_ex(char *const argv[], char *buf, size_t n, bool quiet_err)
{
    int fds[2];
    pid_t pid;
    int status;
    ssize_t got;
    size_t total = 0;

    if (n == 0)
        return 1;
    if (pipe(fds) != 0)
        die("pipe: %s", strerror(errno));
    fflush(NULL);
    pid = fork();
    if (pid < 0)
        die("fork: %s", strerror(errno));
    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], 1) != 1)
            _exit(127);
        close(fds[1]);
        if (quiet_err) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, 2);
                close(devnull);
            }
        }
        execvp(argv[0], argv);
        dprintf(2, "xbs: exec %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }
    close(fds[1]);
    while (total + 1 < n &&
           (got = read(fds[0], buf + total, n - total - 1)) > 0)
        total += (size_t)got;
    close(fds[0]);
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            die("waitpid: %s", strerror(errno));
    }
    buf[total] = '\0';
    if (total && buf[total - 1] == '\n')
        buf[total - 1] = '\0';
    if (WIFSIGNALED(status))
        return 1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int
run_capture(char *const argv[], char *buf, size_t n)
{
    return run_capture_ex(argv, buf, n, false);
}

int
run_capture_quiet(char *const argv[], char *buf, size_t n)
{
    return run_capture_ex(argv, buf, n, true);
}

/* git describe --tags --long output -> Apple-style source version.
 *   "xnu-7195.141.6-42-gabc1234" -> "7195.141.6.42"
 *   "xnu-7195.141.6-0-gabc1234"  -> "7195.141.6"
 *   "v2.0.0-0-gabc1234"          -> "2.0.0"
 * Returns 1 and fills buf on success, 0 if dir is not a git checkout. */
int
git_source_version(const char *dir, char *buf, size_t n)
{
    char *av[] = { (char *)"git", (char *)"-C", (char *)dir,
                   (char *)"describe", (char *)"--tags", (char *)"--long",
                   (char *)"--always", NULL };
    char out[512];
    char *tagend, *countstart, *v;
    long commits = 0;
    const char *p;
    int ok;

    if (run_capture_quiet(av, out, sizeof out) != 0 || !out[0])
        return 0;

    /* Trim the trailing "-g<sha>"; take commits between tag and sha. */
    tagend = strstr(out, "-g");
    if (tagend) {
        *tagend = '\0';                       /* "...-<count>" remains */
        countstart = tagend;
        while (countstart > out && countstart[-1] != '-')
            countstart--;
        if (countstart > out) {
            commits = atol(countstart);
            countstart--;
            *countstart = '\0';               /* drop "-<count>" */
        }
    }

    /* Drop any leading non-numeric prefix ("xnu-", "v") when digits follow. */
    v = out;
    while (*v && !isdigit((unsigned char)*v))
        v++;
    if (!*v)
        v = out;

    ok = *v != '\0';
    for (p = v; *p; p++)
        if (!(isdigit((unsigned char)*p) || *p == '.'))
            ok = 0;

    if (ok)
        snprintf(buf, n, commits > 0 ? "%s.%ld" : "%s", v, commits);
    else
        snprintf(buf, n, "%s", out);
    return 1;
}

/* Copy one file/any-plain object dir tree. */
static void
copy_one(const char *src, const char *dst)
{
    struct stat st;
    DIR *d;
    struct dirent *e;
    char *sc, *dc;

    if (lstat(src, &st) != 0)
        die("copytree: cannot stat %s: %s", src, strerror(errno));

    if (S_ISLNK(st.st_mode)) {
        char link[PATH_MAX];
        ssize_t n = readlink(src, link, sizeof link - 1);
        if (n < 0)
            die("copytree: readlink %s: %s", src, strerror(errno));
        link[n] = '\0';
        symlink(link, dst);
        return;
    }
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST)
            die("copytree: mkdir %s: %s", dst, strerror(errno));
        d = opendir(src);
        if (!d)
            die("copytree: opendir %s: %s", src, strerror(errno));
        while ((e = readdir(d)) != NULL) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                continue;
            if (g_prune_junk && junk_name(e->d_name))
                continue;
            sc = path_join(src, e->d_name);
            dc = path_join(dst, e->d_name);
            copy_one(sc, dc);
            free(sc);
            free(dc);
        }
        closedir(d);
    } else {
        FILE *in = fopen(src, "rb");
        FILE *out;
        char buf[65536];
        size_t n;
        if (!in)
            die("copytree: open %s: %s", src, strerror(errno));
        out = fopen(dst, "wb");
        if (!out)
            die("copytree: open %s: %s", dst, strerror(errno));
        while ((n = fread(buf, 1, sizeof buf, in)) > 0)
            fwrite(buf, 1, n, out);
        fclose(in);
        fclose(out);
        chmod(dst, st.st_mode & 07777);
    }
}

void
cp_r(const char *src, const char *dst)
{
    copy_one(src, dst);
}

void
copy_tree(const char *src, const char *dst)
{
    /* Src is a dir; dst will be created if missing. */
    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode))
        die("installsrc: %s is not a directory", src);
    if (!dir_exists(dst))
        mkdirs(dst);
    copy_one(src, dst);
}

/* Manifest walk: emit "size<TAB>path" lines for every file under root. */
strlist *
manifest_of_dir(const char *root)
{
    strlist *out = calloc(1, sizeof(strlist));
    char **stack = NULL;
    size_t top = 0, cap = 0;
    size_t plen;

    if (!out)
        die("out of memory");
    sl_init(out);
    plen = strlen(root);

    if (top == cap)
        { cap = 16; stack = malloc(cap * sizeof(char *)); }
    stack[top++] = xstrdup(root);

    while (top) {
        char *cur = stack[--top];
        DIR *d = opendir(cur);
        struct dirent *e;
        if (d) {
            strlist children;
            sl_init(&children);
            while ((e = readdir(d)) != NULL) {
                struct stat st;
                char *p;
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
                    continue;
                p = path_join(cur, e->d_name);
                if (lstat(p, &st) == 0) {
                    if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
                        sl_add(&children, p);
                    } else {
                        char *rel = p + plen;
                        if (*rel == '/')
                            rel++;
                        sl_add(out, xasprintf("%lld\t%s",
                                              (long long)st.st_size, rel));
                    }
                }
                free(p);
            }
            closedir(d);
            /* Push dirs (owned copies) so later pops survive sl_free(). */
            for (size_t i = children.n; i > 0; i--) {
                if (top == cap) {
                    cap = cap ? cap * 2 : 16;
                    stack = realloc(stack, cap * sizeof(char *));
                    if (!stack)
                        die("out of memory");
                }
                stack[top++] = xstrdup(children.v[i - 1]);
            }
            sl_free(&children);
        }
        free(cur);
    }
    free(stack);
    return out;
}

/* Version detection: "xnu-7195.141.6" -> "7195.141.6". */
const char *
version_from_path(const char *srcroot, char *buf, size_t n)
{
    const char *base = strrchr(srcroot, '/');
    const char *dash;
    base = base ? base + 1 : srcroot;
    dash = strrchr(base, '-');
    if (dash && dash[1]) {
        const char *digit = dash + 1;
        bool ok = (*digit >= '0' && *digit <= '9');
        size_t i;
        for (i = 0; ok && digit[i]; i++) {
            char c = digit[i];
            if (!((c >= '0' && c <= '9') || c == '.' || c == '_'))
                ok = false;
        }
        if (ok) {
            snprintf(buf, n, "%s", digit);
            return buf;
        }
    }
    snprintf(buf, n, "1");
    return buf;
}

const char *
project_from_path(const char *srcroot, char *buf, size_t n)
{
    const char *base = strrchr(srcroot, '/');
    const char *dash;
    base = base ? base + 1 : srcroot;
    dash = strrchr(base, '-');
    if (dash && dash[1] >= '0' && dash[1] <= '9') {
        size_t len = (size_t)(dash - base);
        if (len >= n)
            len = n - 1;
        memcpy(buf, base, len);
        buf[len] = '\0';
        return buf;
    }
    snprintf(buf, n, "%s", base);
    return buf;
}