SRC = src
OUT = out
LIB = lib

CC = gcc
CFLAGS += -std=c99 -Wall -pedantic -D_POSIX_C_SOURCE=200809L
INCLUDES = -I "$(SRC)" -I "$(SRC)/util" -I "$(LIB)/libtickit/include"
LDFLAGS += -L "$(LIB)/libtickit/.libs" -l:libtickit.a -ltermkey -lunibilium
DEBUGFLAGS = -g -DDEBUG
OPTFLAGS = -O2 -DNDEBUG

OBJECTS = $(OUT)/util/log.o \
          $(OUT)/util/map.o \
		  $(OUT)/util/event.o \
		  $(OUT)/util/buffer.o \
		  $(OUT)/options.o \
		  $(OUT)/statusbar.o \
		  $(OUT)/actions.o \
          $(OUT)/core.o \
          $(OUT)/main.o


# Default task
all: _OPT=$(OPTFLAGS)
all: hedit


# External libs

libtickit:
	$(MAKE) -C $(LIB)/libtickit



# HEdit

$(OUT)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(_DEBUG) $(_OPT) $(CFLAGS) $(INCLUDES) -c -o $@ $<

hedit: libtickit $(OBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(_DEBUG) $(_OPT) $(CFLAGS) $(INCLUDES) -o $(OUT)/hedit $(OBJECTS) $(LDFLAGS)

debug: _DEBUG=$(DEBUGFLAGS)
debug: hedit

clean:
	rm -rf $(OUT)

install: default
	cp $(OUT)/hedit /usr/bin/hedit



.PHONY: all hedit debug libtickit clean install