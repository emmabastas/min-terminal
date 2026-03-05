############
# PREAMBLE #
############
# https://tech.davis-hansson.com/p/make/

SHELL := bash
.ONESHELL:
.SHELLFLAGS := -eu -o pipefail -c
.DELETE_ON_ERROR:
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

ifeq ($(origin .RECIPEPREFIX), undefined)
  $(error This Make does not support .RECIPEPREFIX. Please use GNU Make 4.0 or later)
endif
.RECIPEPREFIX = >




SOURCE_FILES = \
min-terminal.c \
ringbuf.c \
termbuf.c \
rendering.c \
keymap.c \
arguments.c \
diagnostics.c \
util.c \
dist/CuTest.c \
dist/glad/src/gl.c \
dist/glad/src/glx.c \
tests/unit-tests.c

COMMON_FLAGS = -std=c99 -D _GNU_SOURCE -Wall -Wextra -Wpedantic -Werror \
    -lc -lm -lharfbuzz -lX11 -lGLX -lGL \
    -I dist/ -I dist/glad/include/
# -fsanitize=address causes glXChooseFBConfig to return NULL for whatever reason..
DEBUG_FLAGS = -g -Og -fsanitize=undefined
PRODUCTION_FLAGS = -O3
UNITTEST_FLAGS = -D UNITTEST -Wno-unused-variable -fsanitize=address

.PHONY: all
all: debug

###############
# DEBUG BUILD #
###############

.PHONY: debug
debug: build/debug/min-terminal

build/debug/min-terminal: $(SOURCE_FILES:%.c=build/debug/%.o)
> gcc $(COMMON_FLAGS) $(DEBUG_FLAGS) -o $@ $^

build/debug/%.o: %.c
> mkdir -p ${dir $@}
> gcc $(COMMON_FLAGS) $(DEBUG_FLAGS) -c ./$< -o ./$@

####################
# PRODUCTION BUILD #
####################

.PHONY: release
release: build/release/min-terminal

build/release/min-terminal: $(SOURCE_FILES:%.c=build/release/%.o)
> gcc $(COMMON_FLAGS) $(PRODUCTION_FLAGS) -o $@ $^

build/release/%.o: %.c
> mkdir -p ${dir $@}
> gcc $(COMMON_FLAGS) $(PRODUCTION_FLAGS) -c ./$< -o ./$@


##################
# UNITTEST BUILD #
##################

.PHONY: unittest
unittest: build/unittest/unit-test

build/unittest/unit-test: $(SOURCE_FILES:%.c=build/unittest/%.o)
> gcc -D UNITTEST $(COMMON_FLAGS) $(DEBUG_FLAGS) -fsanitize=address -o $@ $^

build/unittest/%.o: %.c
> mkdir -p ${dir $@}
> gcc -D UNITTEST $(COMMON_FLAGS) $(DEBUG_FLAGS) -Wno-unused-variable -fsanitize=address  -c ./$< -o ./$@


########
# MISC #
########

.PHONY: clean
clean:
> rm -rf ./build


