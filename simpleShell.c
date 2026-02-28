#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<string.h>
#include<signal.h>

#define string char*
#define BUILTIN 1
#define EXTERNAL 2
#define foreground 1
#define background 0
#define Cd 1
#define Echo 2
#define Export 3
#define Exit 4




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
int exit_status=0;


int main() {
    register_child_signal();
    setup_environment();
    shell();
    return 0;
}



//finished
void setup_environment()
{
    chdir(getenv("HOME"));
}



void shell()
{

    do
    {
        ///////////////////////////////////////////////////////////parse input done/////////////////////////////////////////////////////////////
        char** args=parse_input();
        //exit_status=(strcmp(args[0],"exit") == 0) ? 1 : 0;
        int i=0;
        while(args[i]!=NULL)//debugging loop
        {       
             printf("arg %d: %s\n", i, args[i]); 
            i++;
        }
         ///////////////////////////////////////////////////////////parse input done/////////////////////////////////////////////////////////////

         ///////////////////////////////////////////////////////////////evaluate expression(to be implemented)/////////////////////////////////////////////////////////////
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
    printf("you entered: %s\n", input); // debugging line

    static string args[100];
    string token = strtok(input, " ");
    int i=0;
    while (token !=NULL)
    {
        args[i] = token;
        i++;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
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
        waitpid(child_pid, NULL, 0);

    }
}

void execute_builtin(char** args)
{
    switch(builtin_commmand_type(args[0]))
    {
        case Cd:
            if(args[1] != NULL && strcmp(args[1],"~") != 0)  chdir(args[1]);
            else  chdir(getenv("HOME"));
            break;

        case Echo:
            //execute_echo(args);
            break;

        case Export:
            //execute_export(args);
            break;

        case Exit:
            exit_status=1;
            break;
    }
    {

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



//finished

void on_child_exit(int sig)
{
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG); // WNOHANG = don't block
    if (pid > 0 && WIFEXITED(status))
    {
        printf("Child terminated\n");
    }
}


//finished
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







