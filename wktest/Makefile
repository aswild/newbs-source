#
# Test module Please Ignore
# Copyright 2019 Allen Wild
# SPDX-License-Identifier: GPLv2
#

export KERNEL_SRC ?= /workspace/newbs/build-raspberrypi4-64/tmp/work-shared/raspberrypi4-64/kernel-build-artifacts
export CROSS_COMPILE ?= aarch64-linux-gnu-
export ARCH ?= arm64

ifneq ($(KERNELRELEASE),)
obj-m := wktest.o
else

all: modules
modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

.PHONY: all modules modules_install clean

endif
