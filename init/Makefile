# newbs init Makefile

COMMON_FLAGS = -Wall -Wextra -Werror
CFLAGS   ?= -g -O2
CFLAGS   += -std=gnu11 $(COMMON_FLAGS)
CXXFLAGS ?= -g -O2
CXXFLAGS += -std=gnu++11 -fno-exceptions $(COMMON_FLAGS)

CC    ?= gcc
CXX   ?= g++
STRIP ?= strip
ifneq ($(CROSS_COMPILE),)
CC    = $(CROSS_COMPILE)gcc
CXX   = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip
endif

# enable the unit test code, values are 0/1/auto
ENABLE_TESTS ?= auto
ifeq ($(ENABLE_TESTS),auto)
ifeq ($(shell $(CC) -dumpmachine 2>/dev/null | grep -o x86 2>/dev/null),x86)
_ENABLE_TESTS = 1
else
_ENABLE_TESTS = 0
endif
else
_ENABLE_TESTS = $(ENABLE_TESTS)
endif

ifeq ($(_ENABLE_TESTS),1)
CPPFLAGS += -DENABLE_TESTS
endif

HOSTCC     ?= gcc
HOSTCFLAGS ?= -g -O2
HOSTCFLAGS += $(COMMON_FLAGS)

TARGET_CPIO = init.cpio.zst
TARGET_CPIO_UNCOMPRESSED = init.cpio
INITRAMFS_LIST = initramfs_list.txt

TARGET_INIT = init
TARGET_OBJ  = init.o switch_root.o fsmagic.o log.o
TARGET_INIT_S = .init.s
HEADERS = newbs_init.h

all: $(TARGET_CPIO)

$(TARGET_OBJ) : $(HEADERS)
$(HEADERS):

$(TARGET_INIT): $(TARGET_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -static -o $@ $^

$(TARGET_INIT_S): $(TARGET_INIT)
	$(STRIP) -o $@ $<

gen_init_cpio: gen_init_cpio.c
	$(HOSTCC) $(HOSTCFLAGS) $(HOSTLDFLAGS) -o $@ $^

$(TARGET_CPIO_UNCOMPRESSED): $(TARGET_INIT_S) $(INITRAMFS_LIST) gen_init_cpio
	./gen_init_cpio $(INITRAMFS_LIST) >$@

$(TARGET_CPIO): $(TARGET_CPIO_UNCOMPRESSED)
	zstd -19 -c $^ >$@

clean:
	rm -f $(TARGET_INIT) $(TARGET_INIT_S) $(TARGET_CPIO) gen_init_cpio $(TARGET_OBJ)

.PHONY: all clean
