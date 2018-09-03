# mknImage Makefile

AM_CPPFLAGS = -D_GNU_SOURCE -I$(srcdir)/lib
AM_CFLAGS   = -std=gnu11 -Wall -Wextra -Werror
AM_CXXFLAGS = -Wall -Wextra -Werror
if ENABLE_LTO
AM_CFLAGS += -flto
AM_CXXFLAGS += -flto
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
                         newbs-swdl/newbs-swdl.cpp \
                         newbs-swdl/PError.h newbs-swdl/PError.cpp
endif
