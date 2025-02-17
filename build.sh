#!/usr/bin/env sh

printf "Choose an option\n"
printf "  * Debug build                            : d\n"
printf "  * Production build                       : p\n"
printf "  * Build and run unit tests               : u\n"
printf "  * Build and run unit tests with memcheck : um\n"
printf "  * Run memcheck on ./min-terminal         : memcheck\n"
printf "  * Download esctest test suite            : get esctest\n"
printf "  * Run esctest                            : esctest\n"
read ans
printf "\n"

linker_flags="-lc -lm -lharfbuzz -lX11 -lGLX -lGL"
input_files="CuTest.c min-terminal.c ringbuf.c termbuf.c rendering.c keymap.c arguments.c diagnostics.c util.c glad/src/gl.c glad/src/glx.c"
unit_test_input_files="CuTest.c ./tests/unit-tests.c ringbuf.c termbuf.c rendering.c keymap.c diagnostics.c util.c glad/src/gl.c glad/src/glx.c"
debug_flags="-g -Og -std=gnu99 -pedantic"
production_flags="-O3 -std=gnu99 -pedantic"

memcheck_common_flags="--tool=memcheck \
--leak-check=full \
--show-leak-kinds=definite,indirect,possible \
--track-origins=yes"

cmd=""

[ "$ans" == "d" ] && cmd="gcc $debug_flags $linker_flags -I glad/include/ $input_files -o min-terminal"

[ "$ans" == "p" ] && cmd="gcc $production_flags $linker_flags -I glad/include/ $input_files -o min-terminal"

[ "$ans" == "u" ] && cmd="gcc $production_flags $linker_flags -I glad/include/ $unit_test_input_files -o unit-test && ./unit-test"

[ "$ans" == "um" ] && cmd="gcc $debug_flags $linker_flags -I glad/include/ $unit_test_input_files -o unit-test \
&& valgrind $memcheck_common_flags --suppressions=./tests/unit-tests-supressions.txt ./unit-test"

[ "$ans" == "memcheck" ] && cmd="valgrind $memcheck_common_flag ./min-terminal"

[ "$ans" == "get esctest" ] && cmd="cd ./tests && git clone git@github.com:ThomasDickey/esctest2.git --branch master --single-branch && cd esctest2 && git checkout fb8be26032ce4d5b8e05b2302d0492296aceec70 && cd ../../"

[ "$ans" == "esctest" ] && cmd="./min-terminal ./tests/esctest2/esctest/esctest.py --max-vt-level=1 --expected-terminal=xterm"

[ "$cmd" == "" ] && printf "Invalid input.\n" && exit 1

printf "$cmd\n"
eval $cmd
exit $?

