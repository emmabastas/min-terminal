#!/usr/bin/env sh

printf "Choose an option\n"
printf "  * debug build: d\n"
read ans
printf "\n"

[ "$ans" == "d" ] && gcc -Og -std=gnu99 -pedantic -I glad/include/ -lc -lm -lharfbuzz -lX11 -lGLX -lGL CuTest.c min-terminal.c ringbuf.c termbuf.c font.c util.c glad/src/gl.c glad/src/glx.c -o min-terminal && exit $?

printf "Expected you to type d.\n"
exit 1

