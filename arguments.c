#include "./arguments.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <argp.h>
#include <wordexp.h>

#include "./diagnostics.h"

const char *argp_program_version = "min-terminal";
const char *argp_program_bug_address = "<emma.bastas@protonmail.com>";
static const char DOC[] = "DOC";
static const char ARGS_DOC[] = "ARGS_DOC";

static const struct argp_option ARGP_OPTIONS[] = {
    { .name = "execute",
      .key = 'e',
      .arg = "\"command args ...\"",
      .flags = 0,
      .doc = "Specify a command for the terminal to execute",
      .group = 0,
    },
    { 0 },
};

static error_t parse_opt(int key, char *arg, struct argp_state *state);

struct arguments_internal {
    wordexp_t execute;
};

static struct argp argp = {
    .options = ARGP_OPTIONS,
    .parser = parse_opt,
    .args_doc = ARGS_DOC,
    .doc = DOC
};

void arguments_parse(int argc, char **argv, struct arguments *args_ret) {
    struct arguments_internal iargs = {
        .execute = (wordexp_t) {
            .we_offs = 0,
            .we_wordc = 0,  // These two will be overwritten while parsing cli
            .we_wordv = 0,  // arguments.
        },
    };

    argp_parse(&argp, argc, argv, 0, 0, &iargs);

    if (iargs.execute.we_wordc == 0) {
        char *shell_command = secure_getenv("SHELL");

        // SHELL environment variable wasn't set (or "secure execution" is
        // required but I won't handle that case).
        if (shell_command == NULL) {
            fprintf(stderr,
                    "Environment variable SHELL wasn't set, either give it a "
                    "value or run `min-terminal -e \"command args ...\"`\n");
            exit(EXIT_FAILURE);
        }

        args_ret->argv = malloc(2 * sizeof(char *));
        args_ret->argv[0] = shell_command;
        args_ret->argv[1] = NULL;

        wordfree(&iargs.execute);  // Frees .execute.we_wordv and the strings
                                   // it points to.
    } else {
        args_ret->argv = iargs.execute.we_wordv;
        assert(iargs.execute.we_wordv[iargs.execute.we_wordc] == NULL);

        // We don't want to free here.
        // wordfree(&iargs.execute);
    }

    args_ret->program_name = args_ret->argv[0];
    args_ret->program_path = args_ret->argv[0];

    return;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments_internal *iargs = state->input;

    int ret;
    switch(key) {
    case 'e':
        ret = wordexp(arg,
                      &iargs->execute,
                      WRDE_DOOFFS  | WRDE_NOCMD | WRDE_SHOWERR | WRDE_UNDEF);
        switch(ret) {
        case 0:  // Sucess.
            break;
        case WRDE_BADCHAR:
        case WRDE_BADVAL:
        case WRDE_CMDSUB:
        case WRDE_NOSPACE:
        case WRDE_SYNTAX:
            assert(false);  // TODO: Handle these.
        default:
            assert(false);
        }

        return 0;
    case ARGP_KEY_ARGS:     // Don't really know what this is.
        assert(false);
    case ARGP_KEY_ARG:      // This is called for positional arguments, we don't
        fprintf(stderr,     // expect any positionals so print usage.
                "I saw a positional argument `%s` but I don't expect any"
                "positionals.\n\n",
                arg);
        argp_usage(state);
        return ARGP_ERR_UNKNOWN;
    case ARGP_KEY_NO_ARGS:  // These cases we simply ignore.
    case ARGP_KEY_END:
    case ARGP_KEY_INIT:
    case ARGP_KEY_SUCCESS:
    case ARGP_KEY_ERROR:
    case ARGP_KEY_FINI:
        return 0;
    default:
        diagnostics_type(DIAGNOSTICS_MISC, __FILE__, __LINE__);
        diagnostics_write_string("Unhandled option \'", -1);
        diagnostics_write_string((char *) &key, 1);
        diagnostics_write_string("\'\n", -1);
        assert(false);
    }
}
