# xbs - open-source Apple Build & Integration (B&I) tool
#
# Portable makefile: works with both GNU make (gmake, default on macOS)
# and BSD make (bmake, FreeBSD/NetBSD/DragonFly).  Suffix rules (.c.o)
# are understood by both; there are no GNU-only constructs here.

CC      ?= cc
# NOTE: BSD make (bmake) predefines CFLAGS in <sys.mk> as "-O -pipe", which
# kills a `?=` default here.  `+=` guarantees our flags are present under
# both bmake and GNU make (an explicit make(1)-line CFLAGS= override still
# wins because it replaces this whole line).
CFLAGS  += -O2 -g -Wall -Wextra -Wno-unused-parameter
PREFIX  ?= /usr/local
DESTDIR ?=
MANDIR  ?= $(PREFIX)/share/man

SRCS    := src/main.c src/buildit.c src/submitproject.c src/common.c
OBJS    := $(SRCS:.c=.o)
BIN     := xbs

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Rebuild every object when the shared header changes.
$(OBJS): src/xbs.h

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN) $(OBJS)

.PHONY: all install uninstall clean