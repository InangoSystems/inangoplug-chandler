################################################################################
#
#  Copyright 2020-2021 Inango Systems Ltd.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
################################################################################

CC ?= gcc

CFLAGS  += -std=c99 -g -Wall -Werror -Wextra -pedantic -pthread
LDFLAGS +=
LDLIBS  +=

INCLUDES := -Isrc -I3rd-party/jsmn
SOURCES  := \
    src/chandler.c \
    src/chandler_conf.c \
    src/chandler_jrpc.c \
    src/chandler_json.c \
    src/chandler_log.c \
    src/chandler_ovs.c \
    src/chandler_ovs_db.c \
    src/chandler_stat.c \
    src/chandler_system.c

PREFIX  ?= _bin
TARGET  ?= chandler

all: $(PREFIX)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -fPIC $(SOURCES) $(LDLIBS) -o $(PREFIX)/$(TARGET)

$(PREFIX):
	mkdir $(PREFIX)

clean:
	rm -vf $(PREFIX)/$(TARGET)
