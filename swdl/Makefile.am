# mknImage Makefile

AM_CPPFLAGS = -D_GNU_SOURCE -I$(srcdir)/lib
AM_CFLAGS   = -std=gnu11 -Wall -Wextra -Werror -Wno-unknown-warning-option
AM_CXXFLAGS = -std=gnu++14 -Wall -Wextra -Werror -Wno-unknown-warning-option
if ENABLE_LTO
AM_CFLAGS += -flto
AM_CXXFLAGS += -flto
endif
if SWDL_TEST
AM_CPPFLAGS += -DSWDL_TEST
endif

LIBSOURCES = lib/nImage.h \
             lib/common.c \
             lib/crc32.c \
             lib/log.c

bin_PROGRAMS = bin/mknImage
bin_mknImage_SOURCES = $(LIBSOURCES) \
                       mknImage/mknImage.h \
                       mknImage/mknImage.c \
                       mknImage/crc32_cmd.c \
                       mknImage/create.c \
                       mknImage/check.c

if ENABLE_SWDL
bin_PROGRAMS += bin/newbs-swdl
bin_newbs_swdl_SOURCES = $(LIBSOURCES) \
                         swdl/main.cpp \
                         swdl/flashbanks.cpp \
                         swdl/lib.cpp \
                         swdl/program.cpp \
                         swdl/PError.h swdl/PError.cpp

install-exec-hook:
	ln -sfT newbs-swdl $(DESTDIR)$(bindir)/swdl

uninstall-hook:
	rm -f $(DESTDIR)$(bindir)/swdl
endif
