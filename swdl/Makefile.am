# mknImage Makefile

AM_CPPFLAGS = -D_GNU_SOURCE
AM_CFLAGS   = -std=gnu11 -Wall -Wextra -Werror
if ENABLE_LTO
AM_CFLAGS += -flto
endif

bin_PROGRAMS = mknImage
mknImage_SOURCES = mknImage.h \
                   mknImage.c \
                   common.c \
                   create.c \
                   crc32.c \
                   log.c

if INSTALL_SWDL
bin_SCRIPTS = newbs-swdl
pkgdata_DATA = newbs-swdl-functions.sh
endif
