# xbs — open-source Apple Build & Integration (B&I) tool

`xbs` is a BSD-3-Clause reimplementation of Apple's *build & integrate* tools
(`buildit`, `submitproject`) and the *release control* conventions that
surround them.  It builds Darwin projects the Apple B&I way: it establishes
the `RC_*` build environment, shadow-builds a project from a pristine source
copy, records a reproducible "sum" of what was installed, verifies rebuilds
against it, and packages the built root.  There is no build server client
here — the open-source Darwin projects have no server to submit to — so
`submitproject` produces the same local artifacts the server would have
created.

Everything project-specific lives in a project *database* (`conf/`), not in
code: Xcode-project aliases, release-trains, and build dependencies.

## Commands

| command                               | purpose                                              |
|---------------------------------------|------------------------------------------------------|
| `xbs buildit [opts] <srcroot>`        | build a Darwin project (make-based or Xcode alias)   |
| `xbs submitproject [opts] <srcroot>`  | stage a pristine source drop + `.sum` manifest       |
| `xbs vers [path]`                     | derive the Apple-style source version of a path      |
| `xbs version`                         | print the tool version                               |

Every build command accepts `-h` and `-n` (dry-run: prints exactly what
would run, changes nothing).

## The B&I pipeline

`buildit` runs this pipeline, mirroring Apple's server build:

```
installsrc -> clean -> install -> verify -> sum [-> merge] [-> archive]
```

1. **installsrc** — copy the project's sources to a shadow tree
   (`<proj>.roots/~src/`), pruning junk (`.git`, `.DS_Store`, `__MACOSX`,
   `CVS`, `.svn`, `.hg`, editor droppings).  `-noinstallsrc` builds in place.
2. **clean** — remove the object root; for make projects also `make clean`.
   `-noclean` skips it.
3. **install** — run `make install` for make projects, or
   `xcodebuild install` for projects matched by an Xcode alias.
4. **verify** — compare the freshly installed tree against the stored `.sum`
   manifest; anything that does not match is declared a reproducibility
   break and printed.  `-noverify` skips, `-nosum` suppresses storage.
5. **sum** — write the sorted `.sum` manifest of the built root
   (relative paths; sorted so rebuilds compare identically).
6. **merge** — with `-merge DST`, copy the built root on top of `DST`.
7. **archive** — with `-archive`, `tar` the built root to
   `<proj>-<ver>~dst.tgz`.

Before any of this, **dependency gates** (below) are checked.

## The RC_* build contract

`buildit` sets these variables for the underlying build, mirroring Apple's
`RC_*` contract.  `-env KEY=VALUE` passthroughs are applied first and win.

| variable | value | set when |
|----------|-------|----------|
| `RC_XBS` | `YES` | always |
| `RC_ProjectName` | project name | always |
| `RC_ProjectSourceVersion` | source version | always |
| `RC_ProjectNameAndSourceVersion` | `<name>-<version>` | always |
| `RC_ProjectSourceSubversion` | `0` | always |
| `RC_BUILDING_INTERNAL` | `0` | always |
| `RC_OS` | `-os` value, else existing env, else `MacOSX` | always |
| `RC_RELEASE` | `-release TRAIN` | only with `-release` |
| `RC_TARGET_CONFIG` | `-configuration NAME`, else existing env, else `release` | always |
| `RC_CFLAGS` | `-pipe <flags>` | always |
| `RC_NONARCH_CFLAGS` | `-othercflags FLAGS`, else `$XBS_CFLAGS`, else empty | always |
| `SRCROOT` | shadow source tree (or source tree with `-noinstallsrc`) | always |
| `OBJROOT` | `<proj>.roots/~obj` | always |
| `SYMROOT` | `<proj>.roots/~sym` | always |
| `DSTROOT` | `<proj>.roots/~dst` | always |
| `SDKROOT` | `-sdk` value | only with `-sdk` |
| `MAKEFLAGS` | `-jN` (`-parallel`, else CPU count) | always |
| `INSTALL_DIR` | `/` | always |

## Roots layout

Roots are written under `<rootsbase>/<proj>-<ver>.roots/`, where
`<rootsbase>` comes from `-rootsDirectory`, else `$XBS_ROOTS_DIR`, else
`$TMPDIR`, else `/tmp`:

```
<proj>-<ver>.roots/
    ~src/<proj>-<ver>/      pristine source shadow (after junk pruning)
    ~obj/                   build objects
    ~sym/                   symbols / debug output
    ~dst/                   installed root
    <proj>-<ver>.sum        sorted manifest of ~dst (for verify)
    <proj>-<ver>~dst.tgz    archive, only with -archive
```

## Project database (`conf/projects.conf`)

`buildit` and `submitproject` look up project metadata in the database.
Lookup precedence: `-conf FILE`, then `$XBS_CONF`, then
`<bindir>/conf/projects.conf` (source trees), then
`<bindir>/../share/xbs/conf/projects.conf` (installed).  If none is found,
`buildit` falls back to make-only builds with a warning.

```
alias <name> <project-subdir> [configuration <name>] [target <name>]
```

An alias routes the named project through `xcodebuild install`, building the
project in `<project-subdir>` (relative to `~src`), using the given
configuration and target when present.  Lookup is longest-prefix match, so
`libsyscall_headers_Sim` resolves to its own entry while `libsyscall` uses
the plainer one.  Projects with no matching alias are built with
`make <target>` (default `install`).

## Dependency gates

```
dependency <project> <dep> [version <v>]
```

Before building, `buildit` resolves the project's dependencies (longest
prefix, like aliases) and requires that each dependency has been built into
the same roots directory: a pinned `version` requires
`<roots>/<dep>-<v>.roots/~dst`; unpinned accepts any `<dep>-*.roots/~dst`.
A missing dependency aborts with an actionable message.  `-ignoreDependencies`
skips the gate.  Dependencies never gate on themselves.

## Source versions

Source versions are derived the Apple *vers* way:

1. A trailing `-<digits>.<digits>…` suffix in the source path (so
   `Libsystem-1234.5.6` has version `1234.5.6`).
2. Otherwise `git -C <dir> describe --tags --long --always`, with the
   leading non-numeric tag prefix stripped and the commit count appended as a
   `.N` segment (`xnu-7195.141.6` + 42 commits ⇒ `7195.141.6.42`).  A
   repository with no tags falls back to the short commit SHA.
3. Otherwise `1`.

`xbs vers [path]` reports just the version.

## Building and installing

The makefile works with both GNU make (default on macOS) and BSD make
(bmake); a plain `make` on macOS suffices.

```sh
make                    # build ./xbs
make install            # install binary, conf, man page (DESTDIR-aware)
```

`Xcode project:` `xbs.xcodeproj` builds the same tool
(`xcodebuild -scheme xbs -configuration Release build`).

## Older line: the original build command

The 0.x-series `xbs build <project> [owners] [build-clean|continuous|snapshot]`
$1-style interface was replaced by the explicit `buildit`/`submitproject`
subcommand model.  `xbs buildit` remains flag-compatible with legacy B&I
invocations (`-buildAlias`, `-buildProject`, `-update`, `-dsymsInDstroot`,
`-dry-run` etc.) so existing scripts keep working.

## License

BSD 3-Clause.  Copyright (c) 2026 LibreDarwin.  See `LICENSE`.