# Common paths
SRC = src
OUT = out
LIB = lib

# Common compilation flags
ARCH ?= $(shell gcc -dumpmachine | cut -d - -f 1)
CC = gcc
CFLAGS += -std=c99 -Wall -pedantic -D_POSIX_C_SOURCE=200809L
INCLUDES = -I "$(SRC)" -I "$(LIB)/libtickit/include"
LDFLAGS += -L "$(LIB)/libtickit/.libs" -l:libtickit.a -ltermkey -lunibilium -lm
DEBUGFLAGS = -g -DDEBUG
OPTFLAGS = -O2 -DNDEBUG

# V8 version and arch
V8_VERSION = 6.4
ifeq ($(ARCH),x86_64)
V8_ARCH ?= x64
else
ifeq ($(ARCH),i386)
V8_ARCH ?= ia32
else
V8_ARCH ?= $(ARCH)
endif
endif

OBJECTS = $(OUT)/util/log.o \
          $(OUT)/util/map.o \
		  $(OUT)/util/event.o \
		  $(OUT)/util/buffer.o \
		  $(OUT)/options.o \
		  $(OUT)/statusbar.o \
		  $(OUT)/actions.o \
		  $(OUT)/commands.o \
		  $(OUT)/file.o \
		  $(OUT)/views/splash.o \
		  $(OUT)/views/edit.o \
          $(OUT)/core.o \
          $(OUT)/main.o


# Default task
.PHONY: all
all: _OPT=$(OPTFLAGS)
all: hedit


# External libs

.PHONY: libtickit
libtickit:
	$(MAKE) -C $(LIB)/libtickit

.PHONY: v8
v8:
	(cd $(LIB) && ./prepare-v8.sh $(V8_VERSION))
	$(MAKE) -C $(LIB)/v8 $(V8_ARCH).release GYPFLAGS="-Dclang=0 -Dwerror=''"



# HEdit

$(OUT)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(_DEBUG) $(_OPT) $(CFLAGS) $(INCLUDES) -c -o $@ $<

.PHONY: hedit
hedit: libtickit $(OBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(_DEBUG) $(_OPT) $(CFLAGS) $(INCLUDES) -o $(OUT)/hedit $(OBJECTS) $(LDFLAGS)

.PHONY: debug
debug: _DEBUG=$(DEBUGFLAGS)
debug: hedit

.PHONY: clean
clean:
	rm -rf $(OUT)

.PHONY: install
install:
	cp $(OUT)/hedit /usr/bin/hedit
	chown root:root /usr/bin/hedit
	chmod 755 /usr/bin/hedit
