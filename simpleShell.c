#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include<string.h>

#define string char*
#define BUILTIN 1
#define EXTERNAL 2
#define foreground 1
#define background 0




void setup_environment();
void shell();
char** parse_input();
int command_type(string cmd);
void execute_external(char** args);
int check_forground(char** cmd);



int main() {
    setup_environment();
    shell();
    return 0;
}



void setup_environment()
{
    chdir("/");
}



void shell()
{
    int exit_status;
    do
    {
        ///////////////////////////////////////////////////////////parse input done/////////////////////////////////////////////////////////////
        char** args=parse_input();
        exit_status=(strcmp(args[0],"exit") == 0) ? 1 : 0;
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
        switch(command_type(args[0]))
        {
            case BUILTIN:
                //execute_builtin(args);
                printf("builtin command detected, execution not implemented yet\n");
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




char** parse_input()
{
    char input[100];
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





int command_type(string cmd)
{
    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "echo") == 0 || strcmp(cmd, "export") == 0)
        return BUILTIN;
    else
        return EXTERNAL;
}




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



void execute_external(char** args)
{
    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        execvp(args[0],args);
        perror("Error\n");
        exit(1);
    }
    else if (child_pid > 0 && check_forground(args) == foreground)
    {
        waitpid(child_pid, NULL, 0);

    }
}







