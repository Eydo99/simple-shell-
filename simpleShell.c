/**
 * @file shell_documented.c
 * @brief A minimal Unix shell implementation in C.
 *
 * This program implements a simple interactive shell that supports:
 *
 *   Built-in commands:
 *     - cd [path]          Change the current working directory.
 *     - echo [args...]     Print arguments to stdout. Supports quoted strings.
 *     - export KEY=VALUE   Store a shell variable in an internal key-value store.
 *     - exit               Terminate the shell.
 *
 *   External commands:
 *     Any other input is treated as an external program and executed via
 *     execvp(). The shell forks a child process to run it.
 *
 *   Background execution:
 *     Appending '&' to any external command runs it in the background.
 *     A SIGCHLD handler prints a notification when a background child exits.
 *
 *   Variable expansion:
 *     Arguments beginning with '$' are substituted with the value stored
 *     by a previous 'export' command before the command is executed.
 *
 * Compile:
 *   gcc -o shell shell_documented.c
 *
 * @note The environment variable store uses a linear-search array capped at
 *       MAX_ENV_VARS entries. A hash table would improve performance for
 *       large numbers of variables (noted in the source).
 *
 * @note Several debugging printf() calls are present throughout the code.
 *       They should be removed or conditionally compiled for production use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>


/* -------------------------------------------------------------------------
 * Constants and Macros
 * -------------------------------------------------------------------------*/

/** @brief Maximum number of shell variables that can be stored via export. */
#define MAX_ENV_VARS 100

/** @brief Convenience alias so 'string' can be used instead of 'char*'. */
#define string char*

/** @brief Command classification: built-in shell command (cd, echo, export, exit). */
#define BUILTIN 1

/** @brief Command classification: external program to be run via execvp(). */
#define EXTERNAL 2

/** @brief Execution mode: run the child process in the foreground (wait for it). */
#define foreground 1

/** @brief Execution mode: run the child process in the background (do not wait). */
#define background 0

/** @brief Built-in command identifier for 'cd'. */
#define Cd     1

/** @brief Built-in command identifier for 'echo'. */
#define Echo   2

/** @brief Built-in command identifier for 'export'. */
#define Export 3

/** @brief Built-in command identifier for 'exit'. */
#define Exit   4


/* -------------------------------------------------------------------------
 * Data Structures
 * -------------------------------------------------------------------------*/

/**
 * @brief Represents a single shell variable as a key-value pair.
 *
 * Used by the 'export' built-in to store user-defined variables that can
 * later be referenced with the '$' prefix in subsequent commands.
 */
typedef struct my_struct {
    string key;   /**< Variable name  (heap-allocated via strdup). */
    string value; /**< Variable value (heap-allocated via strdup). */
} Expression;


/* -------------------------------------------------------------------------
 * Global State
 * -------------------------------------------------------------------------*/

/**
 * @brief Array of all shell variables set by 'export'.
 *
 * Indexed 0 .. expression_count-1. Access is O(n) linear search;
 * replace with a hash table for better scalability.
 */
static Expression expression[MAX_ENV_VARS];

/**
 * @brief Number of variables currently stored in the expression array.
 */
int expression_count = 0;

/**
 * @brief Shell loop termination flag.
 *
 * Set to 1 by the 'exit' built-in to break the main REPL loop.
 */
int exit_status = 0;


/* -------------------------------------------------------------------------
 * Function Prototypes
 * -------------------------------------------------------------------------*/

/** @brief Set the shell's initial working directory to '/'. */
void setup_environment();

/** @brief Main Read-Eval-Print Loop (REPL) — runs until exit_status is set. */
void shell();

/**
 * @brief Read one line from stdin and tokenise it into an argument array.
 *
 * Handles special parsing for 'echo' (preserves quoted string content) and
 * 'export' (keeps the full KEY=VALUE token together).
 *
 * @return Pointer to a static NULL-terminated array of string tokens.
 */
char **parse_input();

/**
 * @brief Determine whether a command is built-in or external.
 *
 * @param cmd  The command name (first token from user input).
 * @return BUILTIN (1) or EXTERNAL (2).
 */
int input_type(string cmd);

/**
 * @brief Execute an external command by forking a child process.
 *
 * Strips any trailing '&' before calling execvp(). If the command was
 * foreground, waits for the child to exit and reports non-zero exit codes.
 *
 * @param args  NULL-terminated argument array. args[0] is the program name.
 */
void execute_external(char **args);

/**
 * @brief Dispatch a built-in command to its handler function.
 *
 * @param args  NULL-terminated argument array. args[0] is the command name.
 */
void execute_builtin(char **args);

/**
 * @brief Identify which built-in command is being invoked.
 *
 * @param args  The command name string.
 * @return One of: Cd (1), Echo (2), Export (3), Exit (4).
 */
int builtin_commmand_type(string args);

/**
 * @brief Determine whether a command should run in the foreground or background.
 *
 * Scans the argument list for a bare '&' token.
 *
 * @param cmd  NULL-terminated argument array.
 * @return foreground (1) if '&' is absent, background (0) if present.
 */
int check_forground(char **cmd);

/**
 * @brief Install the SIGCHLD signal handler via signal().
 *
 * Called once at startup so background child exits are reported without
 * the shell needing to explicitly wait for them.
 */
void register_child_signal();

/**
 * @brief SIGCHLD handler — reaps zombie background children.
 *
 * Uses WNOHANG so it never blocks the parent shell process.
 *
 * @param sig  Signal number (always SIGCHLD; unused in the body).
 */
void on_child_exit(int sig);

/**
 * @brief Remove the '&' token from an argument array in-place.
 *
 * Replaces the first '&' element with NULL, effectively truncating the
 * array at that position so execvp() never sees the '&'.
 *
 * @param args  NULL-terminated argument array to modify.
 */
void remove_and(char **args);

/**
 * @brief Check whether a token begins with the variable-expansion prefix '$'.
 *
 * @param arg  The token string to test.
 * @return 1 if arg[0] == '$', 0 otherwise.
 */
int check_$(string arg);

/**
 * @brief Expand '$VAR' tokens in an argument array using the expression store.
 *
 * Iterates over args[1..] and replaces any token starting with '$' with
 * its stored value. Tokens whose variable is not found are replaced with "".
 *
 * @param args  NULL-terminated argument array to expand in-place.
 */
void evaluate_expression(char **args);

/**
 * @brief Execute the 'export KEY=VALUE' built-in.
 *
 * Parses 'KEY=VALUE' (with optional surrounding quotes on value) and stores
 * the pair in the expression array via add_expression().
 *
 * @param args  Argument array: args[1] is the raw 'KEY=VALUE' string.
 */
void execute_export(char **args);

/**
 * @brief Execute the 'cd [path]' built-in.
 *
 * Changes the shell's working directory. '~' is treated as $HOME.
 * A NULL argument also navigates to $HOME.
 *
 * @param args  The target path string, or NULL / "~" for home directory.
 */
void execute_cd(string args);

/**
 * @brief Execute the 'echo [args...]' built-in.
 *
 * Prints args[1] onward separated by spaces, followed by a newline.
 *
 * @param args  NULL-terminated argument array; args[0] is "echo".
 */
void execute_echo(char **args);

/**
 * @brief Store or update a key-value pair in the expression array.
 *
 * If the key already exists its value is updated. Both key and value are
 * copied with strdup() so the originals can be safely freed or reused.
 *
 * @param key    Variable name string.
 * @param value  Variable value string.
 */
void add_expression(string key, string value);

/**
 * @brief Look up a variable by name in the expression store.
 *
 * @param key  Variable name to search for.
 * @return The stored value string, or NULL if not found.
 */
string find_expression(string key);

/**
 * @brief Parse 'echo' input, handling optional surrounding double-quotes.
 *
 * Called by parse_input() when the command is 'echo'. Strips enclosing
 * quotes if present, then tokenises the remainder by spaces.
 *
 * @param args   The partially-filled args array (args[0] already set to "echo").
 * @param dummy  A strdup'd copy of the original raw input line.
 * @return The completed NULL-terminated args array.
 */
char **input_echo(char **args, string dummy);

/**
 * @brief Parse 'export' input, keeping the KEY=VALUE token intact.
 *
 * Called by parse_input() when the command is 'export'. Extracts everything
 * after "export " as a single token so the '=' is not used as a delimiter.
 *
 * @param args   The partially-filled args array (args[0] already set to "export").
 * @param dummy  A strdup'd copy of the original raw input line.
 * @return The completed NULL-terminated args array.
 */
char **input_export(char **args, string dummy);


/* =========================================================================
 * main
 * ========================================================================*/

/**
 * @brief Program entry point.
 *
 * Sets up signal handling, initialises the environment, then enters the
 * interactive shell loop.
 *
 * @return 0 on normal exit (after the user types 'exit').
 */
int main()
{
    register_child_signal(); /* Install SIGCHLD handler for background jobs */
    setup_environment();     /* Set starting working directory              */
    shell();                 /* Enter the REPL                              */
    return 0;
}


/* =========================================================================
 * setup_environment
 * ========================================================================*/

/**
 * @brief Initialise the shell's working directory to the filesystem root '/'.
 *
 * Called once at startup. A real shell would typically start in the user's
 * home directory ($HOME), but this implementation starts at '/'.
 */
void setup_environment()
{
    chdir("/");
}


/* =========================================================================
 * shell  (REPL)
 * ========================================================================*/

/**
 * @brief The main Read-Eval-Print Loop.
 *
 * Each iteration:
 *  1. Reads and tokenises one line of user input (parse_input).
 *  2. Expands any '$VAR' references in the argument list (evaluate_expression).
 *  3. Classifies the command as built-in or external and dispatches it.
 *
 * The loop continues until the global exit_status flag is set to 1 by the
 * 'exit' built-in command.
 */
void shell()
{
    do
    {
        /* --- Step 1: Parse user input into a NULL-terminated arg array --- */
        char **args = parse_input();

        /* Debugging: print each parsed token */
        int i = 0;
        while (args[i] != NULL)
        {
            printf("arg %d: %s\n", i, args[i]);
            i++;
        }

        /* --- Step 2: Expand '$VAR' tokens using the expression store --- */
        evaluate_expression(args);

        /* Debugging: print each token after variable expansion */
        int j = 0;
        while (args[j] != NULL)
        {
            printf("evaluated arg %d: %s\n", j, args[j]);
            j++;
        }

        /* --- Step 3: Classify and execute the command --- */
        switch (input_type(args[0]))
        {
            case BUILTIN:
                printf("builtin command detected\n"); /* debug */
                execute_builtin(args);
                break;

            case EXTERNAL:
                printf("external command detected\n"); /* debug */
                execute_external(args);
                break;

            default:
                printf("Unknown command type\n");
        }

    } while (!exit_status);
}


/* =========================================================================
 * parse_input
 * ========================================================================*/

/**
 * @brief Read one input line from stdin and split it into tokens.
 *
 * The function reads up to 99 characters, strips the trailing newline, then
 * dispatches to a command-specific parser for 'echo' and 'export' to handle
 * their special quoting/delimiter rules. All other commands are split simply
 * by spaces using strtok().
 *
 * @return Pointer to a static NULL-terminated array of token strings.
 *
 * @note Both 'input' and 'args' are declared static, so the returned pointer
 *       is valid until the next call to parse_input().
 */
char **parse_input()
{
    static char input[100];
    fgets(input, 100, stdin);
    input[strcspn(input, "\n")] = '\0'; /* Strip trailing newline */

    /* Keep a copy of the raw input for commands that need the full string */
    string dummy = strdup(input);

    static string args[100];
    string token = strtok(input, " ");
    args[0] = token; /* First token is always the command name */

    /* 'export' needs the full KEY=VALUE as one token — use special parser */
    if (strcmp(args[0], "export") == 0)
    {
        return input_export(args, dummy);
    }
    /* 'echo' needs to handle quoted strings — use special parser */
    else if (strcmp(args[0], "echo") == 0)
    {
        return input_echo(args, dummy);
    }
    else
    {
        /* General case: split remaining tokens by space */
        int i = 1;
        while (token != NULL)
        {
            token = strtok(NULL, " ");
            args[i] = token;
            i++;
        }
        args[i] = NULL;

        /* Debugging: print each token */
        i = 0;
        while (args[i] != NULL)
        {
            printf("%s\n", args[i]);
            i++;
        }
        return args;
    }
}


/* =========================================================================
 * input_echo
 * ========================================================================*/

/**
 * @brief Tokenise the arguments of an 'echo' command.
 *
 * If the input contains double-quotes the content between the first '"' and
 * the last character (which should be the closing '"') is extracted and split
 * by spaces. Without quotes, everything after "echo " is split by spaces.
 *
 * Example:
 *   Input: echo "hello world"   → args = {"echo", "hello", "world", NULL}
 *   Input: echo hello world     → args = {"echo", "hello", "world", NULL}
 *
 * @param args   Partially-filled argument array (args[0] = "echo").
 * @param dummy  strdup'd copy of the raw input line.
 * @return The completed NULL-terminated args array.
 */
char **input_echo(char **args, string dummy)
{
    /* Point 'beginning' to the content after "echo " */
    string beginning = strchr(dummy, ' ') + 1;

    if (strchr(dummy, '"') != NULL)
    {
        /* Quoted string: start after the opening '"' */
        beginning = strchr(dummy, '"') + 1;
        /* Remove the closing '"' by overwriting it with '\0' */
        beginning[strlen(beginning) - 1] = '\0';
    }

    /* Tokenise the (possibly unquoted) argument string by spaces */
    string token = strtok(beginning, " ");
    int j = 1;
    while (token != NULL)
    {
        args[j] = token;
        token = strtok(NULL, " ");
        j++;
    }
    args[j] = NULL;

    /* Debugging: print each token */
    j = 0;
    while (args[j] != NULL)
    {
        printf("%s\n", args[j]);
        j++;
    }
    return args;
}


/* =========================================================================
 * input_export
 * ========================================================================*/

/**
 * @brief Tokenise the argument of an 'export' command.
 *
 * Keeps the entire 'KEY=VALUE' string as a single token in args[1] so that
 * the '=' character is not used as a delimiter at the parsing stage.
 * The actual split on '=' is done later inside execute_export().
 *
 * Example:
 *   Input: export FOO=bar   → args = {"export", "FOO=bar", NULL}
 *
 * @param args   Partially-filled argument array (args[0] = "export").
 * @param dummy  strdup'd copy of the raw input line.
 * @return The completed NULL-terminated args array.
 */
char **input_export(char **args, string dummy)
{
    /* Everything after "export " is a single KEY=VALUE token */
    string key = strchr(dummy, ' ') + 1;
    args[1] = key;
    args[2] = NULL;
    printf("%s\n", key); /* Debugging: show the raw KEY=VALUE token */
    return args;
}


/* =========================================================================
 * input_type
 * ========================================================================*/

/**
 * @brief Classify a command as either built-in or external.
 *
 * Built-in commands are handled directly by the shell process without
 * forking. External commands require a child process.
 *
 * @param cmd  The command name string to classify.
 * @return BUILTIN if cmd is one of: cd, exit, echo, export.
 *         EXTERNAL for all other commands.
 */
int input_type(string cmd)
{
    if (strcmp(cmd, "cd")     == 0 ||
        strcmp(cmd, "exit")   == 0 ||
        strcmp(cmd, "echo")   == 0 ||
        strcmp(cmd, "export") == 0)
        return BUILTIN;
    else
        return EXTERNAL;
}


/* =========================================================================
 * check_forground
 * ========================================================================*/

/**
 * @brief Determine whether a command should run in the foreground or background.
 *
 * A trailing '&' token indicates the command should run in the background,
 * meaning the shell will not wait for it to finish.
 *
 * @param cmd  NULL-terminated argument array to scan.
 * @return background (0) if '&' is found anywhere in cmd.
 *         foreground (1) if '&' is absent.
 */
int check_forground(char **cmd)
{
    int i = 0;
    while (cmd[i] != NULL)
    {
        if (strcmp(cmd[i], "&") == 0)
            return background;
        i++;
    }
    return foreground;
}


/* =========================================================================
 * execute_external
 * ========================================================================*/

/**
 * @brief Fork a child process and exec an external command.
 *
 * In the child:
 *   - Strips the '&' token if present (it must not be passed to execvp).
 *   - Re-splits each argument by spaces to handle any compound tokens.
 *   - Calls execvp() to replace the child image with the target program.
 *
 * In the parent:
 *   - If the command is foreground, blocks with waitpid() until the child exits
 *     and reports any non-zero exit code.
 *   - If the command is background, returns immediately; SIGCHLD fires later.
 *
 * @param args  NULL-terminated argument array. args[0] is the program name.
 */
void execute_external(char **args)
{
    pid_t child_pid = fork();

    if (child_pid == 0)
    {
        /* ---- Child process ---- */
        remove_and(args); /* Strip '&' so it is not passed to execvp */

        /*
         * Re-tokenise each argument by spaces. This handles cases where a
         * single args[j] string might still contain embedded spaces.
         */
        char *new_args[100];
        int k = 0;
        for (int j = 0; args[j] != NULL; j++)
        {
            char *token = strtok(args[j], " ");
            while (token != NULL)
            {
                new_args[k++] = token;
                token = strtok(NULL, " ");
            }
        }
        new_args[k] = NULL;

        execvp(new_args[0], new_args); /* Replace child image with the program */
        perror("Error\n");             /* Reached only if execvp fails          */
        exit(1);
    }
    else if (child_pid > 0 && check_forground(args) == foreground)
    {
        /* ---- Parent process (foreground) ---- */
        int status;
        waitpid(child_pid, &status, 0); /* Block until child finishes */

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            printf("Error: command exited with status %d\n", WEXITSTATUS(status));
    }
    /* If background, the parent returns immediately. SIGCHLD handles cleanup. */
}


/* =========================================================================
 * execute_builtin
 * ========================================================================*/

/**
 * @brief Dispatch a built-in command to the appropriate handler.
 *
 * Built-in commands run directly in the shell process (no fork). This allows
 * them to affect the shell's own state, e.g. 'cd' changes the shell's cwd.
 *
 * @param args  NULL-terminated argument array. args[0] is the command name.
 */
void execute_builtin(char **args)
{
    switch (builtin_commmand_type(args[0]))
    {
        case Cd:
            execute_cd(args[1]);  /* args[1] is the target path (may be NULL) */
            break;

        case Echo:
            execute_echo(args);
            break;

        case Export:
            execute_export(args);
            break;

        case Exit:
            exit_status = 1; /* Signal the REPL to terminate after this iteration */
            break;
    }
}


/* =========================================================================
 * builtin_commmand_type
 * ========================================================================*/

/**
 * @brief Map a built-in command name to its integer identifier.
 *
 * @param args  The command name string.
 * @return Cd (1), Echo (2), Export (3), or Exit (4).
 *
 * @note Returns an undefined value if 'args' is not a recognised built-in.
 *       This function is only called after input_type() has already confirmed
 *       the command is built-in, so this should never occur in practice.
 */
int builtin_commmand_type(string args)
{
    if      (strcmp(args, "cd")     == 0) return Cd;
    else if (strcmp(args, "echo")   == 0) return Echo;
    else if (strcmp(args, "export") == 0) return Export;
    else if (strcmp(args, "exit")   == 0) return Exit;
}


/* =========================================================================
 * register_child_signal
 * ========================================================================*/

/**
 * @brief Register the SIGCHLD signal handler.
 *
 * SIGCHLD is sent to the parent shell whenever a child process changes state
 * (exits, stops, etc.). The handler reaps zombie processes so they do not
 * accumulate in the process table, and prints a notification for the user.
 */
void register_child_signal()
{
    signal(SIGCHLD, on_child_exit);
}


/* =========================================================================
 * on_child_exit  (SIGCHLD handler)
 * ========================================================================*/

/**
 * @brief SIGCHLD handler: reap background child processes without blocking.
 *
 * Uses waitpid(-1, ..., WNOHANG) which returns immediately if no child has
 * exited yet, preventing the signal handler from ever stalling the shell.
 * Prints "Child terminated" when a background child exits normally.
 *
 * @param sig  The signal number delivered (always SIGCHLD; not used in body).
 */
void on_child_exit(int sig)
{
    int status;
    /* WNOHANG: return immediately if no child has changed state */
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0 && WIFEXITED(status))
    {
        printf("Child terminated\n");
    }
}


/* =========================================================================
 * remove_and
 * ========================================================================*/

/**
 * @brief Remove the '&' background-execution token from an argument array.
 *
 * Searches for the first '&' element and replaces it with NULL, which
 * effectively terminates the array at that position. This ensures the '&'
 * is never passed as an argument to execvp().
 *
 * @param args  NULL-terminated argument array to modify in-place.
 */
void remove_and(char **args)
{
    int i = 0;
    while (args[i] != NULL)
    {
        if (strcmp(args[i], "&") == 0)
        {
            args[i] = NULL; /* Truncate the array here */
            break;
        }
        i++;
    }
}


/* =========================================================================
 * check_$
 * ========================================================================*/

/**
 * @brief Test whether a token begins with the variable-expansion sigil '$'.
 *
 * Used by evaluate_expression() to identify tokens that should be
 * substituted with a stored shell variable's value.
 *
 * @param arg  The token string to test.
 * @return 1 if arg[0] == '$', 0 otherwise.
 */
int check_$(string arg)
{
    return (arg[0] == '$');
}


/* =========================================================================
 * evaluate_expression
 * ========================================================================*/

/**
 * @brief Perform shell variable expansion on an argument array.
 *
 * Iterates over args[1..] (skipping the command name at args[0]). Any token
 * that starts with '$' is looked up in the expression store and replaced with
 * its stored value. If no matching variable is found the token is replaced
 * with an empty string "", mirroring standard shell behaviour.
 *
 * Example:
 *   After 'export GREETING=hello', running 'echo $GREETING' causes
 *   args[1] to change from "$GREETING" to "hello" before execute_echo runs.
 *
 * @param args  NULL-terminated argument array to expand in-place.
 *
 * @note The current implementation uses a linear-search array for variable
 *       lookup (O(n) per lookup). A hash table would reduce this to O(1).
 */
void evaluate_expression(char **args)
{
    int i = 1; /* Start at 1 to skip the command name in args[0] */
    while (args[i] != NULL)
    {
        if (check_$(args[i]))
        {
            /* Skip the '$' prefix when looking up the variable name */
            string value = find_expression(args[i] + 1);
            args[i] = (value != NULL) ? value : "";
        }
        i++;
    }
}


/* =========================================================================
 * execute_export
 * ========================================================================*/

/**
 * @brief Execute the 'export KEY=VALUE' built-in command.
 *
 * Parses the raw 'KEY=VALUE' token from args[1] onward. The value may be
 * optionally surrounded by spaces (e.g. from a quoted input), which are
 * stripped before storing. Calls add_expression() to persist the variable.
 *
 * Supported formats:
 *   export FOO=bar          → key="FOO", value="bar"
 *   export FOO= bar         → key="FOO", value="bar"  (leading space stripped)
 *
 * @param args  NULL-terminated argument array. args[1] is the 'KEY=VALUE' string.
 */
void execute_export(char **args)
{
    int j = 1;
    while (args[j] != NULL)
    {
        /* Split the token on '=' to separate key and value */
        string token = strtok(args[j], "=");
        string key   = token;
        token        = strtok(NULL, "=");

        string value;
        if (strchr(token, ' ') == NULL)
        {
            value = token; /* No surrounding spaces — use as-is */
        }
        else
        {
            /* Value is surrounded by spaces (e.g. from quoting) — strip them */
            value = token + 1;                    /* Skip leading space */
            value[strlen(value) - 1] = '\0';      /* Remove trailing space */
        }

        printf("exported %s with value %s\n", key, value); /* Debugging */
        add_expression(key, value);
        j++;
    }
}


/* =========================================================================
 * execute_cd
 * ========================================================================*/

/**
 * @brief Execute the 'cd [path]' built-in command.
 *
 * Changes the shell process's working directory. Treats NULL and '~' as
 * aliases for the user's home directory ($HOME environment variable).
 *
 * @param args  Target path string. Pass NULL or "~" to go to $HOME.
 */
void execute_cd(string args)
{
    if (args != NULL && strcmp(args, "~") != 0)
        chdir(args);
    else
        chdir(getenv("HOME")); /* Fall back to $HOME for 'cd' or 'cd ~' */
}


/* =========================================================================
 * execute_echo
 * ========================================================================*/

/**
 * @brief Execute the 'echo [args...]' built-in command.
 *
 * Prints args[1] through the last non-NULL token, separated by spaces,
 * followed by a newline character.
 *
 * @param args  NULL-terminated argument array; args[0] is "echo".
 */
void execute_echo(char **args)
{
    int i = 1; /* Skip args[0] which is "echo" itself */
    while (args[i] != NULL)
    {
        printf("%s ", args[i]);
        i++;
    }
    printf("\n");
}


/* =========================================================================
 * add_expression
 * ========================================================================*/

/**
 * @brief Store a new shell variable or update an existing one.
 *
 * If a variable with the same key already exists its value is replaced with
 * a fresh strdup'd copy. If the key is new and capacity allows, a new entry
 * is appended to the expression array.
 *
 * Both key and value are heap-copied with strdup() so the caller's buffers
 * can be safely reused or freed after the call.
 *
 * @param key    The variable name (must be a non-NULL string).
 * @param value  The variable value to store (must be a non-NULL string).
 *
 * @note Silently discards the entry if MAX_ENV_VARS has been reached.
 * @note TODO: Replace the linear-search array with a hash table for O(1) ops.
 */
void add_expression(string key, string value)
{
    if (expression_count < MAX_ENV_VARS)
    {
        /* Check for an existing entry with the same key (update semantics) */
        for (int i = 0; i < expression_count; i++)
        {
            if (strcmp(expression[i].key, key) == 0)
            {
                expression[i].value = strdup(value); /* Replace old value */
                return;
            }
        }

        /* No existing entry found — insert a new one */
        expression[expression_count].key   = strdup(key);
        expression[expression_count].value = strdup(value);
        expression_count++;
    }
}


/* =========================================================================
 * find_expression
 * ========================================================================*/

/**
 * @brief Look up a shell variable by name.
 *
 * Performs a linear search over the expression array.
 *
 * @param key  The variable name to search for.
 * @return The stored value string if found, or NULL if the variable is not set.
 *
 * @note TODO: Replace with a hash table lookup to improve performance.
 */
string find_expression(string key)
{
    for (int i = 0; i < expression_count; i++)
    {
        if (strcmp(expression[i].key, key) == 0)
            return expression[i].value;
    }
    return NULL;
}