#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<string.h>
#include<signal.h>

/* Maximum number of environment variables that can be stored */
#define MAX_ENV_VARS 100
/* Shorthand for char* to simplify string declarations */
#define string char*
/* Command type constants */
#define BUILTIN 1
#define EXTERNAL 2
/* Process execution mode constants */
#define foreground 1
#define background 0
/* Builtin command type constants */
#define Cd 1
#define Echo 2
#define Export 3
#define Exit 4

/* Struct to store a key-value pair for environment variables */
typedef struct my_struct {
    string key;   /* variable name */
    string value; /* variable value */
} Expression;

/* Array to store all exported environment variables */
static Expression expression[MAX_ENV_VARS];
/* Counter for number of stored environment variables */
int expression_count = 0; 

/* Function declarations */
void setup_environment();
void shell();
char** parse_input();
int command_type(string cmd);
void execute_external(char** args);
void execute_builtin(char** args);
int check_forground(char** cmd);
void register_child_signal();
int builtin_commmand_type (string args);
void on_child_exit(int sig);
int input_type(string cmd);
void remove_and(char** args);
void evaluate_expression(char** args);
void execute_export(char** args);
void execute_cd(string args);
void execute_echo(char** args);
void add_expression(string key, string value);
string find_expression(string key);
char** input_echo(char** args, string dummy);
char** input_export(char** args, string dummy);

/* Global flag to control shell loop - set to 1 when user types exit */
int exit_status=0;

/*
 * main - Entry point of the shell program
 * Registers signal handler, sets up environment, then starts the shell loop
 */
int main() {
    register_child_signal();
    setup_environment();
    shell();
    return 0;
}

/*
 * setup_environment - Initializes the shell environment
 * Sets the current working directory to root
 */
void setup_environment()
{
   chdir("/");
}

/*
 * shell - Main shell loop
 * Continuously reads input, evaluates expressions, and executes commands
 * until the user types "exit"
 */
void shell()
{
    do
    {
        /* Parse user input into command and arguments */
        char** args=parse_input();
        int i=0;
        while(args[i]!=NULL) /* debugging loop */
        {       
             printf("arg %d: %s\n", i, args[i]); 
            i++;
        }

        /* Replace $variable references with their stored values */
        evaluate_expression(args);
        int j=0;
        while(args[j]!=NULL) /* debugging loop */
        {       
             printf("evaluated arg %d: %s\n", j, args[j]); 
            j++;
        }

        /* Determine if command is builtin or external and execute accordingly */
        switch(input_type(args[0]))
        {
            case BUILTIN:
                printf("builtin command detected\n"); /* debugging line */
                execute_builtin(args);
                break;

            case EXTERNAL:
                printf("external command detected\n"); /* debugging line */
                execute_external(args);
                break;
                
            default:
                printf("Unknown command type\n");
        }
    }
    while(!exit_status);
}

/*
 * parse_input - Reads and tokenizes user input
 * Handles special parsing for echo (quoted strings) and export (key=value pairs)
 * Returns: array of strings (args) where args[0] is the command
 */
char** parse_input(){

    static char input[100];
    /* Read a line of input from stdin */
    fgets(input, 100, stdin);
    /* Remove trailing newline character */
    input[strcspn(input, "\n")] = '\0';
    /* Keep a copy of original input for special parsing */
    string dummy=strdup(input);
    static string args[100];
    /* Get the first token (command name) */
    string token = strtok(input, " ");
    args[0] = token;

    /* Delegate to special parsers for echo and export commands */
    if(strcmp(args[0], "export") == 0)
    {
        return input_export(args, dummy);
    }
    else if(strcmp(args[0], "echo") == 0)
    {
        return input_echo(args, dummy);
    }
    else
    {
        /* For all other commands, split by spaces normally */
        int i = 1;
        while(token != NULL)
        {
            token = strtok(NULL, " ");
            args[i] = token;
            i++;
        }
        args[i] = NULL;
        i=0;
        while(args[i]!=NULL)
        {
            printf("%s\n", args[i]); /* debugging line */
            i++;    
        }
        return args;
    }
}

/*
 * input_echo - Special parser for echo command
 * Handles quoted strings by extracting content between double quotes
 * If no quotes, splits arguments by space normally
 * Parameters:
 *   args  - argument array to fill
 *   dummy - original unmodified input string
 * Returns: filled args array
 */
char** input_echo(char** args, string dummy)
{
    /* Default: skip "echo " and point to the rest */
    string begining=strchr(dummy,' ')+1;
    /* If input has quotes, extract content between them */
    if (strchr(dummy,'"') !=NULL)
    {
        begining=strchr(dummy,'"')+1;
        /* Remove closing quote by replacing with null terminator */
        begining[strlen(begining)-1]='\0';
    }
    /* Split the echo content by spaces into separate args */
    string token=strtok(begining," ");
    int j=1;
    while(token!=NULL)
    {
        args[j]=token;
        token=strtok(NULL," ");
        j++;
    }
    args[j]=NULL;
    j=0;
    while(args[j]!=NULL)
    {
        printf("%s\n", args[j]); /* debugging line */
        j++;
    }
    return args;
}

/*
 * input_export - Special parser for export command
 * Extracts everything after "export " as a single argument
 * The actual key=value splitting is handled in execute_export
 * Parameters:
 *   args  - argument array to fill
 *   dummy - original unmodified input string
 * Returns: filled args array with args[1] = full "key=value" string
 */
char** input_export(char** args, string dummy)
{
    /* Skip "export " and point to the key=value part */
    string key=strchr(dummy,' ')+1;
    args[1]=key;
    args[2]=NULL;
    printf("%s\n", key); /* debugging line */
    return args;
}

/*
 * input_type - Determines if a command is builtin or external
 * Parameters:
 *   cmd - the command name string
 * Returns: BUILTIN or EXTERNAL constant
 */
int input_type(string cmd)
{
    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "echo") == 0 || strcmp(cmd, "export") == 0)
        return BUILTIN;
    else
        return EXTERNAL;
}

/*
 * check_forground - Checks if a command should run in foreground or background
 * A command runs in background if it ends with "&"
 * Parameters:
 *   cmd - array of command arguments
 * Returns: foreground or background constant
 */
int check_forground(char** cmd)
{
    int i=0;
    while(cmd[i]!=NULL)
    {
        if (strcmp(cmd[i],"&") == 0)
            return background;
        i++;
    }
    return foreground;
}

/*
 * execute_external - Executes an external (non-builtin) command
 * Forks a child process, re-splits args by spaces (to handle variable expansion),
 * then executes the command using execvp
 * For foreground commands, waits for the child to complete
 * For background commands, returns immediately
 * Parameters:
 *   args - array of command and its arguments
 */
void execute_external(char** args)
{
    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        /* Child process */
        /* Remove & from args before executing */
        remove_and(args);
        /* Re-split args by spaces to handle variable expansion like $x="-a -l -h" */
        char* new_args[100];
        int k = 0;
        for(int j = 0; args[j] != NULL; j++)
        {
            char* token = strtok(args[j], " ");
            while(token != NULL)
            {
                new_args[k++] = token;
                token = strtok(NULL, " ");
            }
        }
        new_args[k] = NULL;
        /* Replace child process with the requested command */
        execvp(new_args[0], new_args);
        /* If execvp returns, an error occurred */
        perror("Error\n");
        exit(1);
    }
    else if (child_pid > 0 && check_forground(args) == foreground)
    {
        /* Parent process - wait for foreground child to finish */
        int status;
        waitpid(child_pid, &status, 0);
        /* Print error message if command exited with non-zero status */
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
            printf("Error: command exited with status %d\n", WEXITSTATUS(status));
    }
    /* For background commands, parent returns immediately without waiting */
}

/*
 * execute_builtin - Dispatches builtin commands to their handlers
 * Parameters:
 *   args - array of command and its arguments
 */
void execute_builtin(char** args)
{
    switch(builtin_commmand_type(args[0]))
    {
        case Cd:
            execute_cd(args[1]);
            break;

        case Echo:
            execute_echo(args);
            break;

        case Export:
            execute_export(args);
            break;

        case Exit:
            /* Set flag to terminate the shell loop */
            exit_status=1;
            break;
    }
}

/*
 * builtin_commmand_type - Identifies which builtin command was entered
 * Parameters:
 *   args - the command name string
 * Returns: Cd, Echo, Export, or Exit constant
 */
int builtin_commmand_type (string args)
{
    if (strcmp(args,"cd") == 0) return Cd;
    else if (strcmp(args,"echo") == 0) return Echo;
    else if (strcmp(args,"export") == 0) return Export;
    else if (strcmp(args,"exit") == 0) return Exit;
}

/*
 * register_child_signal - Registers the SIGCHLD signal handler
 * Called once at startup so the shell is notified when child processes terminate
 */
void register_child_signal()
{
    signal(SIGCHLD,on_child_exit);
}

/*
 * on_child_exit - Signal handler called when a child process terminates
 * Reaps zombie processes using WNOHANG to avoid blocking the shell
 * Parameters:
 *   sig - the signal number (SIGCHLD)
 */
void on_child_exit(int sig)
{
    int status;
    /* WNOHANG: return immediately if no child has exited */
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid > 0 && WIFEXITED(status))
    {
        printf("Child terminated\n");
    }
}

/*
 * remove_and - Removes the "&" token from the args array
 * "&" is a shell operator and should not be passed to execvp
 * Parameters:
 *   args - array of command arguments
 */
void remove_and(char** args)
{
    int i=0;
    while(args[i]!=NULL)
    {
        if (strcmp(args[i],"&") == 0)
        {
            /* Replace & with NULL to terminate the args array there */
            args[i]=NULL;
            break;
        }
        i++;
    }
}

/*
 * check_$ - Checks if a string starts with the '$' character
 * Used to identify variable references in command arguments
 * Parameters:
 *   arg - the string to check
 * Returns: 1 if starts with '$', 0 otherwise
 */
int check_$(string arg)
{
    return (arg[0] == '$');
}

/*
 * evaluate_expression - Replaces $variable references with their stored values
 * Scans all arguments starting from index 1 (skipping command name)
 * If an arg starts with '$', looks up its value in the expression store
 * Parameters:
 *   args - array of command arguments to evaluate
 */
void evaluate_expression(char** args)
{
    int i=1;
    while(args[i]!=NULL)
    {
        if (check_$(args[i]))
        {
            /* Skip the '$' character and look up the variable name */
            string value = find_expression(args[i]+1);
            if (value != NULL)
                args[i] = value; /* replace $var with its value */
            else
                args[i] = ""; /* variable not found - replace with empty string */
        }
        i++;
    }
}

/*
 * execute_export - Handles the export builtin command
 * Parses key=value pairs and stores them in the expression array
 * Handles both simple values (x=5) and quoted values (x="hello world")
 * Parameters:
 *   args - array where args[1] onwards are key=value pairs
 */
void execute_export(char** args)
{
    int j=1;
    while(args[j]!=NULL)
    {
        /* Split at '=' to get key and value */
        string token = strtok(args[j], "=");
        string key = token;
        token = strtok(NULL, "=");
        string value;
        /* Check if value contains spaces (quoted string) */
        if(strchr(token,' ')==NULL)
            value=token; /* simple value: x=5 */
        else
        {
            /* Quoted value: skip opening quote and remove closing quote */
            value=token+1;
            value[strlen(value)-1]='\0';
        }
        printf("exported %s with value %s\n", key, value); /* debugging line */
        add_expression(key, value);
        j++;
    }
}

/*
 * execute_cd - Handles the cd builtin command
 * Supports: cd (go home), cd ~ (go home), cd <path> (go to path)
 * Parameters:
 *   args - the path to change to, or NULL for home directory
 */
void execute_cd(string args)
{
    if(args!= NULL && strcmp(args,"~") != 0)
        chdir(args); /* change to specified path */
    else
        chdir(getenv("HOME")); /* change to home directory */
}

/*
 * execute_echo - Handles the echo builtin command
 * Prints all arguments separated by spaces followed by a newline
 * Parameters:
 *   args - array where args[1] onwards are the strings to print
 */
void execute_echo(char** args)
{
    int i=1;
    while(args[i]!=NULL)
    {
        printf("%s ", args[i]);
        i++;
    }
    printf("\n");
}

/*
 * add_expression - Stores a key-value pair in the expression array
 * If the key already exists, updates its value
 * If the key is new, adds it to the array (up to MAX_ENV_VARS)
 * Parameters:
 *   key   - the variable name
 *   value - the variable value
 */
void add_expression(string key, string value) {
    if (expression_count < 100) {
        /* Check if key already exists and update it */
        for (int i = 0; i < expression_count; i++) {
            if (strcmp(expression[i].key, key) == 0) {
                expression[i].value = strdup(value);
                return;
            }
        }
        /* Key not found - add new entry */
        expression[expression_count].key = strdup(key);
        expression[expression_count].value = strdup(value);
        expression_count++;
    }
}

/*
 * find_expression - Looks up a variable by name in the expression array
 * Parameters:
 *   key - the variable name to search for
 * Returns: the variable's value string, or NULL if not found
 */
string find_expression(string key) {
    for (int i = 0; i < expression_count; i++) {
        if (strcmp(expression[i].key, key) == 0) {
            return expression[i].value;
        }
    }
    return NULL;
}