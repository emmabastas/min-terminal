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
dist/glad/src/glx.c

COMMON_FLAGS = -std=c99 -D _GNU_SOURCE -Wall -Wextra -Wpedantic -Werror \
	-lc -lm -lharfbuzz -lX11 -lGLX -lGL \
	-I dist/ -I dist/glad/include/
DEBUG_FLAGS = -g -Og -fsanitize=undefined
PRODUCTION_FLAGS = -O3

.PHONY: all
all: debug

###############
# DEBUG BUILD #
###############

.PHONY: debug
debug: build/debug/min-terminal

build/debug/min-terminal: $(SOURCE_FILES:%.c=build/debug/%.o)
	gcc $(COMMON_FLAGS) $(DEBUG_FLAGS) -o $@ $^

build/debug/%.o: %.c
	mkdir -p ${dir $@}
	gcc $(COMMON_FLAGS) $(DEBUG_FLAGS) -c ./$< -o ./$@

####################
# PRODUCTION BUILD #
####################

.PHONY: release
release: build/release/min-terminal

build/release/min-terminal: $(SOURCE_FILES:%.c=build/release/%.o)
	gcc $(COMMON_FLAGS) $(PRODUCTION_FLAGS) -o $@ $^

build/release/%.o: %.c
	mkdir -p ${dir $@}
	gcc $(COMMON_FLAGS) $(PRODUCTION_FLAGS) -c ./$< -o ./$@


##################
# UNITTEST BUILD #
##################

.PHONY: unittest
unittest: SOURCE_FILES += .tests/unit-tests.c
unittest: build/unittest/min-terminal
	./build/unittest/min-terminal

build/unittest/min-terminal: $(SOURCE_FILES:%.c=build/unittest/%.o)
	gcc -D UNITTEST $(COMMON_FLAGS) $(DEBUG_FLAGS) -fsanitize=address -o $@ $^

build/unittest/%.o: %.c
	mkdir -p ${dir $@}
	gcc -D UNITTEST $(COMMON_FLAGS) $(DEBUG_FLAGS) -Wno-unused-variable -fsanitize=address  -c ./$< -o ./$@

########
# MISC #
########

.PHONY: esctest
esctest:
	./build/debug/min-terminal -e "./tests/esctest2/esctest/esctest.py --max-vt-level=1 --expected-terminal=xterm --options allowC1Printable disableWideChars"

.PHONY: clean
clean:
	rm -rf ./build


