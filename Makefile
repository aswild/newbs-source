# Makefile for neostrip demo program

ifeq ($(findstring arm,$(shell uname -p)),arm)
ifeq ($(origin CC),default)
CC 			    = gcc
endif
else
ifeq ($(origin CC),default)
CC 			    = arm-linux-gnueabihf-gcc -mcpu=cortex-a7 -mfpu=neon-vfpv4
endif
KINCLUDE	   ?= /workspace/linux-raspberrypi/HEADERS_INSTALL/include
endif

CFLAGS 		   ?= -O2
EXTRA_CFLAGS 	= -Wall -Wextra
USE_CFLAGS	 	= $(EXTRA_CFLAGS) $(CFLAGS)

ifneq ($(KINCLUDE),)
USE_CFLAGS	   += -I$(KINCLUDE)
endif

bindir		   ?= /usr/bin

TARGET 			= neostrip-demo
SOURCES			= neostrip-demo.c

DEPLOY_IP	   ?= 10.11.0.2
DEPLOY_USER	   ?= root
DEPLOY_PATH    ?= /home/root/$(TARGET)

OBJECTS			= $(SOURCES:.c=.o)
DEPS			= $(SOURCES:.c=.d)

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) -c $(USE_CFLAGS) -MD -MP -o $@ $<

install: $(TARGET)
	install -m 0755 -D $(TARGET) $(DESTDIR)$(bindir)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(TARGET)

clean:
	rm -f $(OBJECTS) $(DEPS)

deploy: $(TARGET)
	scp -v $(TARGET) $(DEPLOY_USER)@$(DEPLOY_IP):$(DEPLOY_PATH)

.PHONY: all install uninstall clean deploy

-include $(DEPS)
