#
# Test module Please Ignore
# Copyright 2019 Allen Wild
# SPDX-License-Identifier: GPLv2
#
KERNEL_SRC ?= /lib/modules/$(uname -r)/build

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
