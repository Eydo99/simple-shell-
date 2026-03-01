#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<string.h>
#include<signal.h>

#define MAX_ENV_VARS 100
#define string char*
#define BUILTIN 1
#define EXTERNAL 2
#define foreground 1
#define background 0
#define Cd 1
#define Echo 2
#define Export 3
#define Exit 4



typedef struct my_struct {
    string key;
    string value;
} Expression;

static Expression expression[MAX_ENV_VARS];
int expression_count = 0; 


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
int exit_status=0;



//finished
int main() {
    register_child_signal();
    setup_environment();
    shell();
    return 0;
}



//finished
void setup_environment()
{
   chdir("/");
}



void shell()
{

    do
    {
        ///////////////////////////////////////////////////////////parse input done/////////////////////////////////////////////////////////////
        char** args=parse_input();
        int i=0;
        while(args[i]!=NULL)//debugging loop
        {       
             printf("arg %d: %s\n", i, args[i]); 
            i++;
        }
         ///////////////////////////////////////////////////////////parse input done/////////////////////////////////////////////////////////////

         ///////////////////////////////////////////////////////////////evaluate expression(to be implemented)/////////////////////////////////////////////////////////////
         evaluate_expression(args);
         int j=0;
        while(args[j]!=NULL)//debugging loop
        {       
             printf("evaluated arg %d: %s\n", j, args[j]); 
            j++;
        }
         ///////////////////////////////////////////////////////////////evaluate expression(to be implemented)/////////////////////////////////////////////////////////////

         ////////////////////////////////////////////////////////////////execute command/////////////////////////////////////////////////////////////
        switch(input_type(args[0]))
        {
            case BUILTIN:
                printf("builtin command detected\n"); //debugging line
                execute_builtin(args);
                break;

            case EXTERNAL:
                printf("external command detected\n"); //debugging line
                execute_external(args);
                break;
                
            default:
                printf("Unknown command type\n");
        }
         ////////////////////////////////////////////////////////////////execute command/////////////////////////////////////////////////////////////

    }
    while(!exit_status);
}



//finished
char** parse_input()
{
    static char input[100];
    fgets(input, 100, stdin);
    input[strcspn(input, "\n")] = '\0';

    static string args[100];
    int i = 0;
    char* ptr = input;

    while (*ptr != '\0')
    {
        // skip leading spaces
        while (*ptr == ' ') ptr++;
        if (*ptr == '\0') break;

        args[i++] = ptr;
        
        while (*ptr != '\0')
        {
            if (*ptr == '"') // found a quote
            {
                // remove the opening quote by shifting
                memmove(ptr, ptr + 1, strlen(ptr));
                // now scan until closing quote
                while (*ptr != '"' && *ptr != '\0') ptr++;
                // remove closing quote
                if (*ptr == '"') memmove(ptr, ptr + 1, strlen(ptr));
            }
            else if (*ptr == ' ') // end of token
            {
                *ptr++ = '\0';
                break;
            }
            else
            {
                ptr++;
            }
        }
    }
    args[i] = NULL;
     // if command is echo, split quoted args by space
    if (args[0] != NULL && (strcmp(args[0], "echo") == 0||strcmp(args[0], "ls") == 0))
    {
        static string echo_args[100];
        echo_args[0] = args[0]; // keep "echo"
        int k = 1;
        int j = 1;
        while (args[j] != NULL)
        {
            // split args[j] by spaces
            char* token = strtok(args[j], " ");
            while (token != NULL)
            {
                echo_args[k++] = token;
                token = strtok(NULL, " ");
            }
            j++;
        }
        echo_args[k] = NULL;
        return echo_args;
    }
    
    return args;
}






//finished
int input_type(string cmd)
{
    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "echo") == 0 || strcmp(cmd, "export") == 0)
        return BUILTIN;
    else
        return EXTERNAL;
}






//finished
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






//finished
void execute_external(char** args)
{
    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        remove_and(args);
        execvp(args[0],args);
        perror("Error\n");
        exit(1);
    }
    else if (child_pid > 0 && check_forground(args) == foreground)
    {
        int status;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        printf("Error: command exited with status %d\n", WEXITSTATUS(status));

    }
}




//finished
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

            exit_status=1;
            break;
    }
}





//finished
int builtin_commmand_type (string args)
{
    if (strcmp(args,"cd") == 0) return Cd;
    else if (strcmp(args,"echo") == 0) return Echo;
    else if (strcmp(args,"export") == 0) return Export;
    else if (strcmp(args,"exit") == 0) return Exit;
}



//finished
void register_child_signal()
{
    signal(SIGCHLD,on_child_exit);
}



//finished ---> looked it up
void on_child_exit(int sig)
{
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG); // WNOHANG = don't block
    if (pid > 0 && WIFEXITED(status))
    {
        printf("Child terminated\n");
    }
}



//finished --> & is not passed to execvp, it is only used to determine if the process should run in the background or foreground
void remove_and(char** args)
{
    int i=0;
    while(args[i]!=NULL)
    {
        if (strcmp(args[i],"&") == 0)
        {
            args[i]=NULL;
            break;
        }
        i++;
    }
}



//finished
int check_$(string arg)
{
    return (arg[0] == '$');
}




//change to hash table later
void evaluate_expression(char** args)
{
    int i=1;
    while(args[i]!=NULL)
    {
        if (check_$(args[i]))
        {
           string value = find_expression(args[i]+1); // skip the '$' character
            if (value != NULL)
                args[i] = value;
            else
                args[i] = "";
        }
        i++;
    }

}



//change to hash table later
void execute_export(char** args)
{
    int j=1;
    while(args[j]!=NULL)
    {
        string token = strtok(args[j], "=");
        string key = token;
        token = strtok(NULL, "=");
        string value=token;
        printf("exported %s with value %s\n", key, value); //debugging line
        add_expression(key, value);
        j++;
    }
}



//finished
void execute_cd(string args)
{
    if(args!= NULL && strcmp(args,"~") != 0)  chdir(args);
    else  chdir(getenv("HOME"));
}



//finished
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




//finished
void add_expression(string key, string value) {
    if (expression_count < 100) {
        //check duplicates
        for (int i = 0; i < expression_count; i++) {
            if (strcmp(expression[i].key, key) == 0) {
               expression[i].value = strdup(value); // clear old value
                return;
            }
        }
        expression[expression_count].key = strdup(key);
        expression[expression_count].value = strdup(value);
        expression_count++;
    }
}

//finished
string find_expression(string key) {
    for (int i = 0; i < expression_count; i++) {
        if (strcmp(expression[i].key, key) == 0) {
            return expression[i].value;
        }
    }
    return NULL;
}
