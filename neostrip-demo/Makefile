# Makefile for neostrip demo program

ifeq ($(findstring arm,$(shell uname -p)),arm)
ifeq ($(origin CXX),default)
CXX = g++
endif
else
ifeq ($(origin CXX),default)
CXX = arm-linux-gnueabihf-g++ -mcpu=cortex-a7 -mfpu=neon-vfpv4
endif
KINCLUDE ?= /workspace/linux-raspberrypi/HEADERS_INSTALL/include
endif

CXXFLAGS		?= -O2 -ffast-math
EXTRA_CXXFLAGS	= -Wall -Wextra
USE_CXXFLAGS	= $(EXTRA_CXXFLAGS) $(CXXFLAGS)

ifneq ($(KINCLUDE),)
USE_CXXFLAGS += -I$(KINCLUDE)
endif

bindir		?= /usr/bin

TARGET 		= neostrip-demo
SOURCES		= neostrip-demo.cpp \
			  Neostrip.cpp \
			  utils.cpp

DEPLOY_IP	?= 10.11.0.2
DEPLOY_USER	?= root
DEPLOY_PATH	?= /home/root/$(TARGET)

OBJECTS		= $(SOURCES:.cpp=.o)
DEPS		= $(SOURCES:.cpp=.d)

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ -lm

%.o: %.cpp
	$(CXX) -c $(USE_CXXFLAGS) -MD -MP -o $@ $<

install: $(TARGET)
	install -m 0755 -D $(TARGET) $(DESTDIR)$(bindir)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(bindir)/$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(DEPS)

deploy: $(TARGET)
	scp $(TARGET) $(DEPLOY_USER)@$(DEPLOY_IP):$(DEPLOY_PATH)

.PHONY: all install uninstall clean deploy

-include $(DEPS)
