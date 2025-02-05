#!/usr/bin/env sh

printf "Choose an option\n"
printf "  * debug build      : d\n"
printf "  * production build : p\n"
read ans
printf "\n"

[ "$ans" == "d" ] && gcc -Og -std=gnu99 -pedantic -I glad/include/ -lc -lm -lharfbuzz -lX11 -lGLX -lGL CuTest.c min-terminal.c ringbuf.c termbuf.c rendering.c util.c glad/src/gl.c glad/src/glx.c -o min-terminal && exit $?

[ "$ans" == "p" ] && gcc -O3 -std=gnu99 -pedantic -I glad/include/ -lc -lm -lharfbuzz -lX11 -lGLX -lGL CuTest.c min-terminal.c ringbuf.c termbuf.c rendering.c util.c glad/src/gl.c glad/src/glx.c -o min-terminal && exit $?


printf "Expected you to type d or p.\n"
exit 1

