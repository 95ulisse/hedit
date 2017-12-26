SRC = src
OUT = out
LIB = lib

CC = gcc
CFLAGS += -std=c99 -Wall -pedantic -D_POSIX_C_SOURCE=200809L
INCLUDES = -I "$(SRC)" -I "$(SRC)/util" -I "$(LIB)/libtickit/include"
LDFLAGS += -L "$(LIB)/libtickit/.libs" -l:libtickit.a -ltermkey -lunibilium
DEBUGFLAGS = -g -DDEBUG

OBJECTS = $(OUT)/util/map.o \
          $(OUT)/log.o \
		  $(OUT)/options.o \
		  $(OUT)/statusbar.o \
          $(OUT)/core.o \
          $(OUT)/main.o


# Default task
all: hedit


# External libs

libtickit:
	$(MAKE) -C $(LIB)/libtickit



# HEdit

$(OUT)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(_DEBUG) $(CFLAGS) $(INCLUDES) -c -o $@ $<

hedit: libtickit $(OBJECTS)
	@mkdir -p $(OUT)
	$(CC) $(_DEBUG) $(CFLAGS) $(INCLUDES) -o $(OUT)/hedit $(OBJECTS) $(LDFLAGS)

debug: _DEBUG=$(DEBUGFLAGS)
debug: hedit

clean:
	rm -rf $(OUT)

install: default
	cp $(OUT)/hedit /usr/bin/hedit



.PHONY: all hedit debug libtickit clean install