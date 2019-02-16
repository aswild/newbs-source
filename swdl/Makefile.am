# mknImage Makefile

AM_CPPFLAGS = -D_GNU_SOURCE -I$(srcdir)/lib
AM_CFLAGS = -std=gnu11 $(COMMON_FLAGS)
AM_CXXFLAGS = -std=gnu++14 $(COMMON_FLAGS)

COMMON_FLAGS = -Wall -Wextra -Werror
if ENABLE_SANITIZE
COMMON_FLAGS += -fsanitize=address -fsanitize=undefined
endif
if ENABLE_LTO
COMMON_FLAGS += -flto
endif
if SWDL_TEST
COMMON_FLAGS += -DSWDL_TEST
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
sbin_PROGRAMS = bin/newbs-swdl
bin_newbs_swdl_SOURCES = $(LIBSOURCES) \
                         swdl/main.cpp \
                         swdl/flashbanks.cpp \
                         swdl/lib.cpp \
                         swdl/program.cpp \
                         swdl/PError.h swdl/PError.cpp

install-exec-hook:
	ln -sfT newbs-swdl $(DESTDIR)$(sbindir)/swdl

uninstall-hook:
	rm -f $(DESTDIR)$(sbindir)/swdl
endif
