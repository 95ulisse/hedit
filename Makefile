# Common paths
SRC = src
OUT = out
DEPS = deps
GEN = $(OUT)/gen

# V8 version and arch
ARCH ?= $(shell gcc -dumpmachine | cut -d - -f 1)
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

# Common compilation flags
CC = gcc
CXX = g++
CFLAGS += -std=c99 -Wall -Werror -pedantic -D_POSIX_C_SOURCE=200809L
CXXFLAGS += -std=c++11 -Wall -Werror -pedantic
INCLUDES = -I "$(SRC)" -I "$(DEPS)/libtickit/include" -I "$(DEPS)/v8/include"
LDFLAGS += -L "$(DEPS)/libtickit/.libs" \
           -lm -pthread \
           -l:libtickit.a -ltermkey -lunibilium \
           -Wl,--start-group "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_libplatform.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_base.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_libbase.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_nosnapshot.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_libsampler.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_initializers.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/src/libv8_init.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/third_party/icu/libicui18n.a" \
                             "$(DEPS)/v8/out/$(V8_ARCH).release/obj.target/third_party/icu/libicuuc.a" \
           -Wl,--end-group

DEBUGFLAGS += -g -DDEBUG
OPTFLAGS += -O2 -DNDEBUG


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
		  $(OUT)/views/log.o \
          $(OUT)/core.o \
          $(OUT)/js.o \
          $(GEN)/js-builtin-modules.o \
          $(OUT)/main.o

JS_MODULES = $(shell find $(SRC)/js -type f -name '*.js')


# Default task
.PHONY: all
all: release


# --------------------------------------------------------------------
# External libs
# --------------------------------------------------------------------

.PHONY: libtickit
libtickit:
	cd $(DEPS)/libtickit && (patch -N -p1 < ../libtickit.patch || true)
	$(MAKE) -C $(DEPS)/libtickit

.PHONY: v8
v8:
	./scripts/build-v8.sh "$(DEPS)" "$(V8_VERSION)" "$(V8_ARCH)"


# --------------------------------------------------------------------
# HEdit
# --------------------------------------------------------------------

# C sources
$(OUT)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(_DEBUG) $(_OPT) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# C++ sources
$(OUT)/%.o: $(SRC)/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(_DEBUG) $(_OPT) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

# Embedded JS files
$(GEN)/js-builtin-modules.o: $(JS_MODULES)
	@mkdir -p $(dir $@)
	./scripts/gen-js.sh "$(SRC)/js" "$(GEN)/js-builtin-modules.cc"
	$(CXX) $(_DEBUG) $(_OPT) $(CXXFLAGS) $(INCLUDES) -c -o $@ $(GEN)/js-builtin-modules.cc

# Main target
.PHONY: hedit
hedit: libtickit v8 $(OBJECTS)
	@mkdir -p $(OUT)
	$(CXX) $(_DEBUG) $(_OPT) -o $(OUT)/hedit $(OBJECTS) $(LDFLAGS)

.PHONY: debug
debug: _DEBUG=$(DEBUGFLAGS)
debug: hedit

.PHONY: release
release: _OPT=$(OPTFLAGS)
release: hedit

# JS docs
.PHONY: docs docs-clean docs-publish
docs:
	(cd ./docs && npm i && ./node_modules/.bin/jsdoc -c jsdoc.json)
docs-clean:
	rm -rf ./docs/out ./docs/node_modules
docs-publish: docs
	git checkout gh-pages
	cp -R ./docs/out/* .
	git add .
	(git commit -m "Documentation update." && git push origin gh-pages) || true
	git checkout master


# --------------------------------------------------------------------
# Utilities
# --------------------------------------------------------------------

.PHONY: clean
clean:
	rm -rf $(OUT)

.PHONY: install
install:
	cp $(OUT)/hedit /usr/bin/hedit
	chown root:root /usr/bin/hedit
	chmod 755 /usr/bin/hedit