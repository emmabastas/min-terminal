#ifndef INCLUDED_ARGUMENTS_H
#define INCLUDED_ARGUMENTS_H

/*
  This module parses `argv` into a more abstract `arguments`, the idea is that
  the code in this module is the only pice of code concearning itself with
  arguments parsing.

  For parsning the arguments Argp is used
  https://www.gnu.org/software/libc/manual/html_node/Argp.html

  We also use `wordexp` (without command substitution)
  (https://www.gnu.org/software/libc/manual/html_node/Word-Expansion.html)
  to parse the value of the --execute="..." option.
 */

struct arguments {
    char **argv;         // Argument array passed as-is to `execv*` functions.
    char *program_path;  // Path to the program to run as the shell process.
    char *program_name;  // Name of the program.
};

void arguments_parse(int argc, char **argv, struct arguments *args_ret);

#endif /* INCLUDED_ARGUMENTS_H */
