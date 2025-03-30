#!/usr/bin/env python3

from typing import Literal

import subprocess
import sys

shell: Literal["bash"] | Literal["nu"] = "bash"
memcheck: bool = False

linker_flags = [
    "-lc",
    "-lm",
    "-lharfbuzz",
    "-lX11",
    "-lGLX",
    "-lGL"
]
input_files = [
    "dist/CuTest.c",
    "min-terminal.c",
    "ringbuf.c",
    "termbuf.c",
    "rendering.c",
    "keymap.c",
    "arguments.c",
    "diagnostics.c",
    "util.c",
    "dist/glad/src/gl.c",
    "dist/glad/src/glx.c"
]
unit_test_input_files = [
    "dist/CuTest.c",
    "min-terminal.c",
    "./tests/unit-tests.c",
    "ringbuf.c",
    "termbuf.c",
    "rendering.c",
    "keymap.c",
    "arguments.c",
    "diagnostics.c",
    "util.c",
    "dist/glad/src/gl.c",
    "dist/glad/src/glx.c"
]
debug_flags = ["-g", "-Og", "-std=gnu99", "-pedantic", "-D", "_GNU_SOURCE"]
production_flags = ["-O3", "-std=gnu99", "-pedantic", "-D", "_GNU_SOURCE"]
unittest_flags = ["-D UNITTEST"]
includes = ["-I", "dist/", "-I", "dist/glad/include/"]

memcheck_common_flags = [
    "--tool=memcheck",
    "--leak-check=full",
    "--show-leak-kinds=definite,indirect,possible",
    "--track-origins=yes",
    "--suppressions=./tests/unit-tests-supressions.txt",
]

def main():
    map = {
        "r": ("Run min-terminal", runit),
        "bash": ("Run min-terminal with bash", run_with_bash),
        "nu": ("Run min-terminal with nu", run_with_nu),
        "bd": ("Build min-terminal with debug settings", build_debug),
        "bp": ("Build min-terminal with production settings", build_prod),
        "m": ("Toggle running with memcheck", toggle_memcheck),
        "ud": ("Run unit tests with debug settings", unit_debug),
        "up": ("Run unit tests with production settings", unit_prod),
        "get esctest": ("Download esctest suite", get_esctest),
        "esctest": ("Run esctest suite", run_esctest),
        "q": ("Quit", lambda: quit(0))
    }

    while True:
        print_status()
        print_options(map)
        key = sys.stdin.readline().strip()

        try:
            (_, function) = map[key]
            function()
        except KeyError:
            print(f"Unknown option \"{key}\"")

def print_options(map):
    print("----------------")
    for (key, (description, _)) in map.items():
        print(f"{key:>12} {description}")
    print("----------------")

def print_status():

    shell_s = shell
    memcheck_s = "memcheck" if memcheck else ""

    print(f"=== shell: {shell_s} {memcheck_s} ===")

def runit():
    args = memcheck_wrap(["./min-terminal"])

    if shell == "bash":
        args += ["-e", "/run/current-system/sw/bin/bash"]
    if shell == "nu":
        args += ["-e", "nu"]

    run_shell(args)

def run_with_bash():
    global shell
    shell = "bash"

def run_with_nu():
    global shell
    shell = "nu"

def build_debug():
    run_shell(["gcc", *debug_flags, *linker_flags, *includes, *input_files,
               "-o", "min-terminal"])

def build_prod():
    run_shell(["gcc", *production_flags, *linker_flags, *includes, *input_files,
         "-o", "min-terminal"])

def unit_debug():
    run_shell(["gcc", *debug_flags, *linker_flags, *unittest_flags,
               *includes, *unit_test_input_files, "-o", "./tests/unit-test"])

    run_shell(memcheck_wrap(["./tests/unit-test"]))

def unit_prod():
    run_shell(["gcc", *production_flags, *linker_flags, *unittest_flags,
               *includes, *unit_test_input_files, "-o", "tests/unit-test"])

    run_shell(memcheck_wrap(["./tests/unit-test"]))

def toggle_memcheck():
    global memcheck
    memcheck = not memcheck

def get_esctest():
    run_shell(["git", "clone", "git@github.com:ThomasDickey/esctest2.git",
               "--branch", "master", "--single-branch"], cwd="./tests/")
    run_shell(["git", "checkout", "fb8be26032ce4d5b8e05b2302d0492296aceec70"],
              cwd="./tests/esctest2/")

def run_esctest():
    run_shell(["./min-terminal", "-e",
               "./tests/esctest2/esctest/esctest.py --max-vt-level=1 --expected-terminal=xterm --options allowC1Printable disableWideChars"])

def memcheck_wrap(args):
    if memcheck:
        return ["valgrind", *memcheck_common_flags, *args]
    else:
        return args

def run_shell(args, **kwargs):
    print()
    print(">  " + " ".join(args))
    print()
    subprocess.run(args, **kwargs)

if __name__ == "__main__":
    main()
