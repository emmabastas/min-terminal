#!/usr/bin/env sh

printf "Choose an option\n"
printf "  * Debug build                            : d\n"
printf "  * Production build                       : p\n"
printf "  * Build and run unit tests               : u\n"
printf "  * Build and run unit tests with memcheck : um\n"
read ans
printf "\n"

linker_flags="-lc -lm -lharfbuzz -lX11 -lGLX -lGL"
input_files="CuTest.c min-terminal.c ringbuf.c termbuf.c rendering.c keymap.c diagnostics.c util.c glad/src/gl.c glad/src/glx.c"
unit_test_input_files="CuTest.c ./tests/unit-tests.c ringbuf.c termbuf.c rendering.c keymap.c diagnostics.c util.c glad/src/gl.c glad/src/glx.c"
debug_flags="-Og -std=gnu99 -pedantic"
production_flags="-O3 -std=gnu99 -pedantic"

cmd=""

[ "$ans" == "d" ] && cmd="gcc $debug_flags $linker_flags -I glad/include/ $input_files -o min-terminal"

[ "$ans" == "p" ] && cmd="gcc $production_flags $linker_flags -I glad/include/ $input_files -o min-terminal"

[ "$ans" == "u" ] && cmd="gcc $production_flags $linker_flags -I glad/include/ $unit_test_input_files -o unit-test && ./unit-test"

[ "$ans" == "um" ] && cmd="gcc $production_flags $linker_flags -I glad/include/ $unit_test_input_files -o unit-test && printf 'Y\n' | valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt --suppressions=./tests/unit-tests-supressions.txt --gen-suppressions=yes ./unit-test && printf 'results written to valgrind-out.txt\n'"

[ "$cmd" == "" ] && printf "Invalid input.\n" && exit 1

printf "$cmd\n"
eval $cmd
exit $?

