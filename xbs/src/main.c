/*
 * main.c - "xbs": open-source Apple Build & Integration command-line tool.
 *
 *  xbs buildit      build a Darwin project (install/clean/verify/sum/...)
 *  xbs submitproject  package sources for a B&I-style build
 *  xbs --help       this help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xbs.h"

#define XBS_VERSION "0.2.0"

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
"  version                 print the tool version\n"
"\n"
"Run 'xbs <command> -h' for command-specific help.\n",
XBS_VERSION);
}

int
main(int argc, char **argv)
{
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
    if (!strcmp(argv[1], "version")) {
        printf("xbs %s\n", XBS_VERSION);
        return 0;
    }

    fprintf(stderr, "xbs: unknown command '%s'\n", argv[1]);
    usage_main(stderr);
    return 1;
}