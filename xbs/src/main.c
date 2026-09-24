/*
 * main.c - "xbs": open-source Apple Build & Integration command-line tool.
 *
 *  xbs buildit       build a Darwin project (install/clean/verify/sum/...)
 *  xbs submitproject package sources for a B&I-style build
 *  xbs vers          compute an Apple-style source version from git
 *  xbs --help        this help
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xbs.h"

#define XBS_VERSION "0.3.0"

static void
usage_main(FILE *f)
{
    fprintf(f,
"xbs - open-source Apple Build & Integration (B&I) tool  v%s\n"
"\n"
"usage: xbs <command> [options]\n"
"\n"
"commands:\n"
"  buildit <srcroot>       build a Darwin project (see 'xbs buildit -h')\n"
"  submitproject <srcroot> prepare a source package for submission\n"
"  vers [path]             derive the Apple-style source version of path\n"
"  version                 print the tool version\n"
"\n"
"Run 'xbs <command> -h' for command-specific help.\n",
XBS_VERSION);
}

/* Apple-style source version: path suffix if present, else git describe. */
static int
cmd_vers(int argc, char **argv)
{
    const char *path = ".";
    char buf[256];

    if (argc > 2) {
        fprintf(stderr, "usage: xbs vers [path]\n");
        return 1;
    }
    if (argc == 2 && argv[1][0] != '-')
        path = argv[1];

    version_from_path(path, buf, sizeof buf);
    if (!strcmp(buf, "1"))
        git_source_version(path, buf, sizeof buf);
    printf("%s\n", buf);
    return 0;
}

int
main(int argc, char **argv)
{
    g_argv0 = argv[0];

    if (argc < 2) {
        usage_main(stderr);
        return 1;
    }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-help") ||
        !strcmp(argv[1], "--help")) {
        usage_main(stdout);
        return 0;
    }

    if (!strcmp(argv[1], "buildit"))
        return cmd_buildit(argc - 1, argv + 1);
    if (!strcmp(argv[1], "submitproject"))
        return cmd_submitproject(argc - 1, argv + 1);
    if (!strcmp(argv[1], "vers"))
        return cmd_vers(argc - 1, argv + 1);
    if (!strcmp(argv[1], "version")) {
        printf("xbs %s\n", XBS_VERSION);
        return 0;
    }

    fprintf(stderr, "xbs: unknown command '%s'\n", argv[1]);
    usage_main(stderr);
    return 1;
}