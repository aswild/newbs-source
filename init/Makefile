# newbs init Makefile

COMMON_FLAGS = -Wall -Wextra -Werror
CFLAGS   ?= -g -O2
CFLAGS   += -std=gnu99 $(COMMON_FLAGS)
CXXFLAGS ?= -g -O2
CXXFLAGS += -std=gnu++11 $(COMMON_FLAGS)

CC    ?= gcc
CXX   ?= g++
STRIP ?= strip
ifneq ($(CROSS_COMPILE),)
CC    = $(CROSS_COMPILE)gcc
CXX   = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip
endif

HOSTCC     ?= gcc
HOSTCFLAGS ?= -g -O2
HOSTCFLAGS += $(COMMON_FLAGS)

TARGET_CPIO = init.cpio.gz
INITRAMFS_LIST = initramfs_list.txt

TARGET_INIT = init
TARGET_OBJ  = init.o switch_root.o fsmagic.o PError.o
TARGET_INIT_S = .init.s

all: $(TARGET_CPIO)

$(TARGET_INIT): $(TARGET_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -static -o $@ $^

$(TARGET_INIT_S): $(TARGET_INIT)
	$(STRIP) -o $@ $<

gen_init_cpio: gen_init_cpio.c
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTLDFLAGS) -o $@ $^

$(TARGET_CPIO): $(TARGET_INIT_S) $(INITRAMFS_LIST) gen_init_cpio
	set -eo pipefail; ./gen_init_cpio $(INITRAMFS_LIST) | gzip -c -9 >$@

clean:
	rm -f $(TARGET_INIT) $(TARGET_INIT_S) $(TARGET_CPIO) gen_init_cpio $(TARGET_OBJ)

.PHONY: all clean
