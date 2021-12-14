/*******************************************************************************
* Program:      Assignment 3 smallsh   
* Author:       Adriane Hairston
* Date:         July 22, 2021
*               smallsh is a shell implemenation with custom exit, cd, and
*               status commands. All others are redirected with exec().
*
* Citations:
* Donotalo, How to play with strings in C, CodnGame.com, 2018,
* https://www.codingame.com/playgrounds/14213/how-to-play-with-strings-in-c/string-split. 
* So, Foo, Writing a Simple Shell, YouTube.com, 2017,
* https://www.youtube.com/watch?v=z4LEuxMGGs8.
* getcwd(3p) - Linux Man Pages, SysTutorials,
* https://www.systutorials.com/docs/linux/man/3p-getcwd/.
* Oregon State University CS344 Explorations and Ed board. 
* ****************************************************************************/
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <signal.h>
#include <fcntl.h>
#include <assert.h>

#define MAX_ARGS 512
volatile int allow_bg = 1;
int last_foreground_status = 0;
struct sigaction sigint = {{0}}; // Crt C
struct sigaction sigtstp = {{0}}; // Crt Z terminal stop
int process_arr[256];
int process_count = 0;

// Define a struct to hold the command line input from the user
struct command {
    char *command;
    char *args[MAX_ARGS];
    char *input_file;
    char *output_file;
    bool is_bkg;
};

void substitute_string(char* strng, char* sub, int pos_start, int pos_end);
int process_input(struct command* cmmd, int pid);
void change_directory(struct command* cmmd, int num_args);
void print_status(int status);
void execute_command(struct command* cmmd);
void check_background();
void exit_command();
void print_command(struct command *cmd);
const char* safe(const char* s); 


void set_foreground_handlers();
void init_handlers(); 
void sigtstp_handler(int sig);

int main(){

    init_handlers();
  
    int pid = getpid();

    // Run shell until "exit" command or handler triggered
    while (1) {
        struct command cmd;
        memset(&cmd, 0, sizeof(struct command));
        cmd.is_bkg = false;

        // Process input
        int num_params = process_input(&cmd, pid);
        // Note : rework how command is being checked to be valid
        // If process_input returns there should be a valid command

        if (strcmp(cmd.command, "exit") == 0 ) {
            exit_command();
        }
        else if (strcmp(cmd.command, "cd") == 0 ) {
            change_directory(&cmd, num_params);
        }
        else if (strcmp(cmd.command, "status") == 0 ) {
            print_status(last_foreground_status);
        } else {
             // Handle all other commands
            // print_command(&cmd);
            execute_command(&cmd);
        }
    }
    return 0;
}

const char* safe(const char* s) {
    return s ? s : "NULL";
}

void print_command(struct command *cmd) {
    printf("command: %s\n", safe(cmd->command));
    printf("input: %s\n", safe(cmd->input_file));
    printf("output: %s\n", safe(cmd->output_file));
    printf("background: %d\n", cmd->is_bkg);
    for (int i = 0; i < MAX_ARGS; i++)
    {
        if (!cmd->args[i])
        {
            break;
        }

        printf("%d | %s\n", i, safe(cmd->args[i]));
    }
}

int is_valid_input(char* s){
    if (!s) return 0;
    if (s[0] == '\0') return 0;
    if (s[0] == '\n') return 0;
    if (s[0] == '#') return 0;

    return 1;
}

/*******************************************************************************
* Name:             processInput
* Preconditions:    none
* Receives:         struct command* cmmd, int pid
* Returns:          int number of parameters
*                   A line of input is read from the user. The command is the 
*                   first supplied word and all other words are stored as 
*                   parameters (space deliminated).  					
* ****************************************************************************/
int process_input(struct command* cmmd, int pid){
    char line[2048];
    bool input_found = false;
    bool output_found = false;

    int count = 0;
    GET_INPUT:
    // Refresh buffer
    count = 0;
    memset(line, 0, 2048);
    // Check for terminated background process
    check_background();
    printf(": ");
    fflush(stdout);

    // Read a line until newline character is reached
    while(1){
        int c = fgetc(stdin);
         if (c == '\n'){
            break;
        }
        line[count++] = (char) c;
    }
    // Check if stream had error
    if (ferror(stdin)){
        clearerr(stdin);
        goto GET_INPUT;
    }
    if (!is_valid_input(line)){
        goto GET_INPUT;
    }
    // if (count == 1) return 0;

    char* string_read = strdup(line);

	char delimiter[] = " ";
	char *ptr = strtok(string_read, delimiter);
    int i = 0;
	while (ptr != NULL)
	{
        // Replace $$ with pid
        int j = 0;
        bool is_gtr = false;
        bool is_less = false;
        bool is_amp = false;
        char* bigbuf = malloc(strlen(ptr) * 2);
        while (ptr[j] != '\0'){
            if ( j > 0 && ptr[j] == '$' && ptr[j-1] == '$'){
                // Convert the pid to a string before substituting it in
                int pid_length = snprintf( NULL, 0, "%d", pid );
                char* pid_str = malloc( pid_length + 1 );
                snprintf( pid_str, pid_length + 1, "%d", pid );
                // Copy the token into a bigger buffer so when the pid is added
                // it can accomadte the new size
                strcpy(bigbuf, ptr);
                substitute_string(bigbuf, pid_str, j-1, j+1); 
                ptr = bigbuf;
    
                fflush(stdout);
                j += pid_length - 2;
            }
            j++;
        }

        // Check for input and output 
        if (strcmp(ptr, "<") == 0) {
			ptr = strtok(NULL, delimiter);
            cmmd->input_file = calloc(strlen(ptr) + 1, sizeof(char));
			strcpy(cmmd->input_file, ptr);
            input_found = true;
            is_less = true;
		} else if (!input_found){
            cmmd->input_file = NULL;
        }

        if (strcmp(ptr, ">") == 0) {
			ptr = strtok(NULL, delimiter);
            cmmd->output_file = calloc(strlen(ptr) + 1, sizeof(char));
			strcpy(cmmd->output_file, ptr);
            output_found = true;
            is_gtr = true;
		} else if (!output_found){
            cmmd->output_file = NULL;
        }


        if (strcmp(ptr, "&") == 0){
            // Remove from the list of parameters, toggle the boolean, decrement i
            cmmd->is_bkg = true;
            is_amp = true;
        }

        // Save other input as parameters
        if (is_gtr || is_less || is_amp){

        } else{
            cmmd->args[i] = calloc(strlen(ptr) + 1, sizeof(char));
            strcpy(cmmd->args[i], ptr);
            i++;
        }
    
        
		ptr = strtok(NULL, delimiter);
	}
    
    // Save first input word as command
    cmmd->command = strdup(cmmd->args[0]);

    return i;
}

/*******************************************************************************
* Name:             substitute_string 
* Preconditions:    all params have values, no null pointers
* Receives:         char* strng, the original string
*                   char* sub, string that is to be substituted into strng
*                   int pos_start, the start position of where the sub goes
*                   int pos_end, the position directly after the last char
*                       of where the sub string will go in the original
* Returns:          void					
* ****************************************************************************/
void substitute_string(char* strng, char* sub, int pos_start, int pos_end){
    // Get the length of the string and the sub
    int old_length = strlen(strng);
    int new_length = old_length - (pos_end - pos_start) + strlen(sub);
    int sub_length = strlen(sub);

    // Create a new buffer that is long enough to accommodate the new length
    char buff[new_length+ 1];
    memset(buff, '\0', new_length+1);

    int sub_itr = 0;
    // Iterate through the original string copying over the characters to buff
    for (int k = 0; k < new_length; k++){       
        // Copy characters from the substite string
        if (k >= pos_start && k < (pos_start + sub_length)){
            buff[k] = sub[sub_itr];
            sub_itr++;
        }
        // Copy characters from original string below the start position
        else if (k < pos_start) {
            buff[k] = strng[k];
        }
        // Copy characters from original string past the end position
        else{
            int difference = k - sub_length + (pos_end - pos_start);
            buff[k] = strng[difference];
        }
    }
    fflush(stdout);

    memmove(strng, buff, new_length * sizeof(char));
}

/*******************************************************************************
* Name:             change_directory (cd)
* Preconditions:    none
* Receives:         struct command* cmmd, int num_args
* Returns:          void
*                   When num_args is 0, the directory is changed to the HOME
*                   from the environment, otherwise it can be set to a relative
*                   or absolute path supplied in parameters. 					
* ****************************************************************************/
void change_directory(struct command* cmmd, int num_args){
    long size;
    char *buf;
    char *ptr;

    // If no parameters are supplied cd changes to HOME directory
    if (num_args <= 1) {
        // Set size to max path size
        size = pathconf(".", _PC_PATH_MAX);
        // Allocate space for path buffer and store HOME path in it
        if ((buf = (char *)malloc((size_t)size)) != NULL){
            ptr = getenv("HOME");
            // Change the directory to the home directory
            chdir(ptr);
        }
    }
    // Otherwise, take the parameter path to change directories
    else {
    // If the first character is a slash treat it as a absolute path
        char first_char = cmmd->args[1][0];
        if (first_char == '/'){
            int check_err = chdir(cmmd->args[1]);
            if (check_err == -1){
                printf("cd: %s: No such file or directory \n", cmmd->args[0]);
                fflush(stdout);
            }
        }
        // Else ammend it to the current working directory
         else {
            size = pathconf(".", _PC_PATH_MAX);
            if ((buf = (char *)malloc((size_t)size)) != NULL){
                ptr = getcwd(buf, (size_t)size);
            
                strcat(buf,"/");
                strcat(buf,cmmd->args[1]);
                int check_err = chdir(ptr);
                if (check_err == -1){
                    printf("cd: %s: No such file or directory \n", ptr);
                    fflush(stdout);
                }
            }
        }
    }
}

/*******************************************************************************
* Name:             print_status
* Receives:         int status
* Returns:          void
*                   The command prints the exit status or termination signal 
*                   determined by macros.  				
* ****************************************************************************/
void print_status(int status){
    if (WIFEXITED(status)){
        printf("exit value: %d\n", WEXITSTATUS(status));
        fflush(stdout);
    } else {
        printf("terminated by signal %d\n", WTERMSIG(status));
        fflush(stdout);
    }
}

/*******************************************************************************
* Name:             execute_command 
* Preconditions:    command struct has as least the command attr. 
* Receives:         struct command* cmmd, int num_args
* Returns:          void
*                   The command executed in the foreground and placed into 
*                   the background when necessary. File redirection implemented
*                   when specified by the cmmd struct. 				
* ****************************************************************************/
void execute_command(struct command* cmmd) {
    pid_t spawn_pid;

    spawn_pid = fork();
    switch (spawn_pid){
        case -1:
            perror("fork() failed!");
            exit(1);
            break;
        case 0:
        // Handle file redirection, if applicable
        // Open source file
            if (cmmd->input_file){
                int sourceFD = open(cmmd->input_file, O_RDONLY);
                if (sourceFD == -1) { 
                    perror("source open()"); 
                    exit(1); 
                }  
                // Written to terminal
                fflush(stdout); 
                // Redirect stdin to source file
                int result_in = dup2(sourceFD, 0);
                if (result_in == -1) { 
                    perror("source dup2()"); 
                    exit(2); 
                }
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            }
            if (cmmd->output_file){
            // Open target file
                int targetFD = open(cmmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (targetFD == -1) { 
                    perror(" target open failed"); 
                    exit(1); 
                }
            
                // Redirect stdout to target file
                int result_out = dup2(targetFD, 1);
                if (result_out == -1) { 
                    perror("dup target failed"); 
                    exit(2); 
                }
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
            }
        
            // Execute the command and pass along any paramaters
            if (execvp(cmmd->command, cmmd->args)){
                perror("command not found");
                exit(1);
            }
            break;
        default:
            // Execute command in background
            // Not a background command nor in foreground only mode
            if (!cmmd->is_bkg || !allow_bg) {
                waitpid(spawn_pid, &last_foreground_status, 0);
       
            } else {
                assert(cmmd->is_bkg);
                // Add process id to list  
                process_arr[process_count] = spawn_pid;
                printf("background pid is %d\n", process_arr[process_count]);
                fflush(stdout);  
                process_count++; 
                
            }
            break;
    }

}
void check_background(){
    // Find exited processes
    int spawn_pid;
    int status;

    while ((spawn_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("background pid %d is done\n", spawn_pid);
        fflush(stdout);
        print_status(status);  
    }
}

// exit all running status
void exit_command(){
    kill(0, SIGINT);
    exit(0);
}

// Toggles the global boolean, allow_bg, to only allow foreground processes to run.
void sigtstp_handler(int sig) {
    if (allow_bg == 1) {
        char* message = "\nEntering foreground-only mode (& is now ignored)\n";
        // Must be reentrant which is why printf cannot be used
        // 1 is stdout
        write(1, message, 49);    
        allow_bg = 0;
	} else {
        char* message = "\nExiting foreground-only mode\n";
        write(1, message, 29);        
        allow_bg = 1;
	}
}

// Assign pointer to a funciton to handle signal invocation and flags of sigaction struct
void init_handlers() {
   
    sigint.sa_handler = SIG_IGN;
    sigint.sa_flags = 0;
    sigaction(SIGINT, &sigint, NULL);

    sigtstp.sa_handler = &sigtstp_handler; // function to set the flag
    sigtstp.sa_flags = 0;
    sigaction(SIGTSTP, &sigtstp, NULL);
}

// Customize signal functionality 
void set_foreground_handlers(){
    // Declare a set of signals called mask
    sigset_t mask;
    // Initialize or reset the signal set mask to have all signal types
    sigfillset(&mask);
    // Remove the following signal types from the set mask
    sigdelset(&mask,SIGBUS);
    sigdelset(&mask,SIGFPE);
    sigdelset(&mask,SIGILL);
    sigdelset(&mask,SIGSEGV);

    // add the mask to our child's ctrl-z handler
    sigtstp.sa_mask = mask;
    sigtstp.sa_handler = SIG_IGN;
    sigtstp.sa_flags = 0;
    sigaction(SIGTSTP, &sigtstp, NULL);
}

