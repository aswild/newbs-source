# mknImage Makefile

CC      ?= gcc
INSTALL ?= install
prefix  ?= /usr/local
bindir  ?= $(prefix)/bin

AM_CPPFLAGS = -D_GNU_SOURCE
AM_CFLAGS   = -std=gnu11 -g -O2 -flto -Wall -Werror

CC_PROGRAMS = mknImage
mknImage_SOURCES = mknImage.c common.c create.c crc32.c

INSTALL_BIN = $(CC_PROGRAMS)

all: ez-build

install: all
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m755 $(INSTALL_BIN) $(DESTDIR)$(bindir)

uninstall:
	rm -f $(addprefix $(DESTDIR)$(bindir)/,$(INSTALL_BIN))

.PHONY: all install uninstall

include ezmake.mk
