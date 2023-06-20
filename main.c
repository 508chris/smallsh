#define _POSIX_C_SOURCE200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stddef.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdint.h>


// Globals
char *input_file = NULL;
char *output_file = NULL;
int exit_status = 0;
pid_t bg_pid = -1;


/*
This function checks if any background processes have completed and
prints out the process id and status.
*/
void check_background_processes(){
    pid_t child_pid;
    int status;

    while((child_pid = waitpid(0, &status, WUNTRACED | WNOHANG)) > 0){
        if (WIFEXITED(status)){
            fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) child_pid, WEXITSTATUS(status));
        }
        else if(WIFSIGNALED(status)){
            fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) child_pid, WTERMSIG(status));
        }
        else if(WIFSTOPPED(status)){
            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) child_pid);
            kill(child_pid, SIGCONT);
        }
    }
}

/*
This function handles SIGTSTP signal
*/
void handle_SIGTSTP(int signo){

}

/*
This function handles SIGINT signal
*/
void handle_SIGINT(int signo){

}

/*
This function forks a new process to execute the given command.
Will redirect input/output if input/output file is provided.
*/
int fork_program(char **args, int background){

	int childStatus;

    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = handle_SIGINT;
    sigaction(SIGINT, &SIGINT_action, NULL);

    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    int reset_stdin = dup(STDIN_FILENO);
    int reset_stdout = dup(STDOUT_FILENO);

	// Fork a new process
	pid_t spawnPid = fork();

	switch(spawnPid){
	case -1:
		perror("fork()\n");
		exit(1);
		break;
	case 0:
		// In the child process

        // Redirect input to input file if !null
        if (input_file != NULL){
            int infile = open(input_file, O_RDONLY);
            if (infile == -1){
                perror("error");
                exit(2);
            }
            dup2(infile, STDIN_FILENO);
            close(infile);

        }

        // Redirect output to output file if !null
        if (output_file != NULL){
            int outfile = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (outfile == -1){
                perror("error");
                exit(2);
            }
            dup2(outfile, STDOUT_FILENO);
            close(outfile);

        } 

		execvp(args[0], args);
		perror("execvp");
		exit(2);
		break;
	default:
		// In the parent process
        if (background == -1){
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            if (spawnPid == -1){
                perror("waitpid");
                exit_status = 1;
            }
            else {
                if(WIFEXITED(childStatus)){
                    exit_status = WEXITSTATUS(childStatus);
                }
                else if (WIFSIGNALED(childStatus)){
                    exit_status = 128 + WTERMSIG(childStatus);
                }
                else if (WIFSTOPPED(childStatus)){
                    kill(spawnPid, SIGCONT);
                }
            }

            // Mark i/o files null, reset stdin/stdout
            input_file = NULL;
            output_file = NULL;
            dup2(reset_stdin, STDIN_FILENO);
            dup2(reset_stdout, STDOUT_FILENO);
        }
        else{
            bg_pid = spawnPid;
            spawnPid = waitpid(spawnPid, &childStatus, WNOHANG);
        } 

        signal(SIGTSTP, handle_SIGTSTP);
        signal(SIGINT, handle_SIGINT);
		break;
	} 

    return exit_status;
}


/*
This function is directly from the smallsh assignment page under Additional Hints and Guiddance
Source: Ryan Gambord - https://www.youtube.com/watch?v=-3ty5W_6-IQ
*/
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub){

    char *str = *haystack;
    size_t haystack_len = strlen(str);
    size_t const needle_len = strlen(needle), sub_len = strlen(sub);

    for (;;){
        str = strstr(str, needle);
        ptrdiff_t off = str - *haystack;

        if (!str) break;
        if (sub_len > needle_len){
            str = realloc(*haystack, sizeof **haystack * (haystack_len * sub_len - needle_len + 1));
            if (!str) goto exit;
            *haystack = str;
            str = *haystack + off;
        }
        memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
        memcpy(str, sub, sub_len);
        haystack_len = haystack_len + sub_len - needle_len;
        str += sub_len;
    }
    str = *haystack;
    if (sub_len < needle_len){
        str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
        if (!str) goto exit;
        *haystack = str;
    }

exit:
    return str;
}


/*
This function is an implementation of shell command cd.
It will change the current working directory based on user input.
*/
void change_directories(int num_tokens, char **tokens){
    if (num_tokens == 0){
        char *path = getenv("HOME");
        if (chdir(path) == -1){
            perror("chdir");
        }
    }
    else if (num_tokens == 1){
        char *path = tokens[0];
        if (chdir(path) == -1){
            perror("chdir");
        }
    }
    else{
        fprintf(stderr, "Too many arugments provided.\n");
    }
    return;
}


/*
This function is an implementation of shell command exit.
It will terminate smallsh and exit with the value of exit_status or user input
*/
void exit_smallsh(int num_tokens, char **tokens){
    if (num_tokens == 0){
        fprintf(stderr, "\nexit\n");
        exit(exit_status);
    }
    else if (num_tokens == 1){
        int exit_s = atoi(tokens[0]);
        fprintf(stderr, "\nexit\n");
        exit(exit_s);
    }
    else{
        fprintf(stderr, "Too many arguments provided.\n");
        perror("error");
    }
    exit(0);
}


/*
This function tokenizes user input into separate commands/words and places them into a tokens array.
The function then checks to expand the tokens and sends the tokens array to fork_program()
for processing.
*/
void tokenizeInput(char* line){
    char *tokens[512];
    char *token = NULL;
    char *ifs = getenv("IFS");
    size_t i = 0;
    pid_t pid = getpid();
    char pid_str[10];
    sprintf(pid_str, "%d", pid);

    int background = -1;

    if (ifs == NULL){
        ifs = " \t\n";
    }

    token = strtok(line, ifs);

    // if first word input is cd, process and return
    if (strcmp(token, "cd") == 0){
        while (token != NULL){
            token = strtok(NULL, ifs);

            if (token != NULL){
                char *new_token = strdup(token);
                str_gsub(&new_token, "$$", pid_str);
                tokens[i] = new_token;
            }
            else if(token == NULL) break;
            i++;
        }
        change_directories(i, tokens);
        return;
    }

    // if token is exit
    if (strcmp(token, "exit") == 0){
        while (token != NULL){
            token = strtok(NULL, ifs);
            if (token != NULL){
                char *new_token = strdup(token);
                tokens[i] = new_token;
            }
            else if(token == NULL) break;
            i++;
        }
        exit_smallsh(i, tokens);
    }

    //
    while (token != NULL){
        char *new_token = strdup(token);

        // If token starts with ~/, expand to home dir
        if (token[0] == '~' && token[1] == '/'){
            char *home1 = getenv("HOME");
            if (home1 != NULL){
                str_gsub(&new_token, "~", home1);
            }
        } 

        // if token starts with #, ignore remaining tokens
        if (token[0] == '#'){
            break;
        }

        // Input/output redirection handling
        if (strcmp(token, "<") == 0){
            token = strtok(NULL, ifs);
            new_token = strdup(token);
            if (token[0] == '~' && token[1] == '/'){
                char *home2 = getenv("HOME");
                if (home2 != NULL){
                    str_gsub(&new_token, "~", home2);
                }
            }

            input_file = strdup(new_token);
            token = strtok(NULL, ifs);
            continue;
        }
        if (strcmp(token, ">") == 0){
            token = strtok(NULL, ifs);
            new_token = strdup(token);
            if (token[0] == '~' && token[1] == '/'){
                char *home = getenv("HOME");
                if (home != NULL){
                    str_gsub(&new_token, "~", home);
                }
            }
            output_file = strdup(new_token);
            token = strtok(NULL, ifs);
            continue;
        }
        
        
        else {
            // Check for expansion of $$, $!, $?
            
            str_gsub(&new_token, "$$", pid_str);

            char bg_pid_str[10] = "";
            if (bg_pid != -1){
                sprintf(bg_pid_str, "%d", bg_pid);
            }
            str_gsub(&new_token, "$!", bg_pid_str);

            char exit_status_str[10] = "0";
            if (exit_status != 0){
                sprintf(exit_status_str, "%d", exit_status);
            }
            str_gsub(&new_token, "$?", exit_status_str);

            tokens[i] = strdup(new_token);


        }
        i++;
        token = strtok(NULL, ifs);
    }

    // NULL terminate token array
    tokens[i] = NULL;

    // if last token is &, set background var
    if (strcmp(tokens[i-1], "&") == 0){
        background = 0;
        tokens[i-1] = NULL;
    }

    int status = fork_program(tokens, background);
    
    for (int j = 0; j < i; j++){
        free(tokens[j]);
    }

    return;
}


int main(){
    char *ps1 = "";
    char *line = NULL;
    size_t line_size = 0;
    ssize_t line_length;

    if (getenv("PS1") != NULL){
        ps1 = getenv("PS1");
    }

    while(1){
        signal(SIGTSTP, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        check_background_processes();

        fprintf(stderr, "%s", ps1);
        line_length = getline(&line, &line_size, stdin);
        if (line_length == EOF){
            break;
        }

        if (line[0] == ' ' || line[0] == '\n'){
            continue;
        }

        tokenizeInput(line);
        fflush(stdin);
        fflush(stdout);
        fflush(stderr);
    }

    // Free resources and exit
    free(line);
    exit_smallsh(0, NULL);
    return 0;
}
