# NEWBS Util Makefile
#
# Copyright 2017 Allen Wild <allenwild93@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

TARGET  = newbs-util

SOURCES = newbs-util.c \
		  newbs-action.c \
		  newbs-parse.c \
		  newbs-reboot.c \

OBJECTS = $(SOURCES:.c=.o)
DEPS	= $(SOURCES:.c=.d)

CC_FLAGS = -std=gnu11 -Wall -Werror -g -O2 -MD -MP $(CFLAGS)

build: $(TARGET)
all: build

$(TARGET): $(OBJECTS)
	$(CC) $(CC_FLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS) $(DEPS)

.PHONY: all build clean
-include $(DEPS)
