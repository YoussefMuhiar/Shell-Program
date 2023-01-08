#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define LINE_MAX_LEN 1024 // the maximum length of an input line
#define ARG_MAX_LEN 96
#define MAX_ARGS 12 // 10 command line parameters + the command + final NULL
#define PID_HISTORY_SIZE 20
#define ARGV_HISTORY_SIZE 15

typedef struct {
    pid_t history[PID_HISTORY_SIZE];
    int current_index;
} pid_history_t;

typedef struct {
    char** history[ARGV_HISTORY_SIZE];
    int current_index;
} argv_history_t;

// free an argument array
void free_argv(char** argv) {
    if(argv) {
        for(int i = 0; argv[i]; ++i) {
            free(argv[i]);
        }
        free(argv);
    }
}

// msh exec
// executes a command
// expects an array of arguments, and pointers to PID and process histories
// returns 1 if the shell should keep running, 0 if it should exit
int msh_exec(char** argv, pid_history_t* pid_history, argv_history_t* argv_history) {
    char buffer[LINE_MAX_LEN];
    char* commandName = argv[0];

    if(commandName && commandName[0] == '!') {
        int n = atoi(commandName + 1);
        free_argv(argv);
        if(n >= 0 && n < ARGV_HISTORY_SIZE) {
            argv = argv_history->history[
                (argv_history->current_index - 1 - n + ARGV_HISTORY_SIZE) % ARGV_HISTORY_SIZE
            ];
        } else {
            puts("Invalid n.");
            return 1;
        }
        if(!argv) {
            puts("Command not in history.");
            return 1;
        } else {
            commandName = argv[0];
        }
    } else {
        free_argv(argv_history->history[argv_history->current_index]);
        argv_history->history[argv_history->current_index++] = argv;
        argv_history->current_index %= ARGV_HISTORY_SIZE;
    }

    if(!commandName) {
        return 1;
    } else if(strcmp(commandName, "exit") == 0 || strcmp(commandName, "quit") == 0) {
        // set keepRunning to 0
        return 0;
    } else if(strcmp(commandName, "cd") == 0) {
        if(argv[1]) {
            chdir(argv[1]);
        }
        return 1;
    } else if(strcmp(commandName, "listpids") == 0) {
        // navigate through a circular buffer
        int count = 0;
        for(int i = 0; i < PID_HISTORY_SIZE; ++i) {
            pid_t pid = pid_history->history[
                (pid_history->current_index + i) % PID_HISTORY_SIZE
            ];
            // only print if initialized (otherwise there have been fewer processes spawned)
            if(pid > 0) {
                printf("%d: %d\n", count++, pid);
            }
        }
        return 1;
    } else if(strcmp(commandName, "history") == 0) {
        // navigate through a circular buffer
        int count = 0;
        for(int i = 0; i < ARGV_HISTORY_SIZE; ++i) {
            char** hargv = argv_history->history[
                (argv_history->current_index + i) % ARGV_HISTORY_SIZE
            ];
            // only print if initialized (otherwise there have been fewer processes spawned)
            if(hargv) {
                printf("%d: ", count++);
                for(int j = 0; hargv[j]; ++j) {
                    printf("%s ", hargv[j]);
                }
                putchar('\n');
            }
        }
        return 1;
    } else {
        // fork the executed process
        pid_t pid = fork();
        if(pid == 0) { // child process
            // workaround for the PATH order from requirement 8
            // because setenv does not exist in standard C99
            // and execve/execvpe is unsuitable
            // (it applies PATH *after* choosing the binary)
            sprintf(buffer, "./%s", commandName);
            if(execvp(buffer, argv) < 0) {
                sprintf(buffer, "/usr/local/bin/%s", commandName);
                if(execvp(buffer, argv) < 0) {
                    sprintf(buffer, "/usr/bin/%s", commandName);
                    if(execvp(buffer, argv) < 0) {
                        sprintf(buffer, "/bin/%s", commandName);
                        if(execvp(buffer, argv) < 0) {
                            printf("%s: Command not found.\n", commandName);
                        }
                    }
                }
            }
            // if execvp failed, quit the forked process
            exit(1);
        } else if (pid < 0) { // fail
            perror("fork");
            return 0;
        } else { // parent process
            wait(NULL);
            // add entries to the circular buffer
            pid_history->history[pid_history->current_index++] = pid;
            pid_history->current_index %= PID_HISTORY_SIZE;
            return 1;
        }
    }
}

// msh get arguments
// extracts arguments, separated by spaces, from a single string (char*)
// returns an NULL-padded array of parameters
char** msh_get_arguments(char* buffer) {
    char** argv = malloc(sizeof(char*) * MAX_ARGS);
    char* word = strtok(buffer, " \n");

    for(int i = 0; i < MAX_ARGS; ++i) {
        argv[i] = NULL;
    }

    // use strdup so the word remains valid later (for history)
    if(word) {
        argv[0] = strdup(word);
    }
    for(int i = 1; i < MAX_ARGS; ++i) {
        word = strtok(NULL, " \n");
        if(word) {
            argv[i] = strdup(word);
        }
    }

    return argv;
}

int main(void) {
    char buffer[LINE_MAX_LEN];
    char** argv;
    int keepRunning;
    pid_history_t pid_history;
    argv_history_t argv_history;
    
    // initialize circular buffers
    pid_history.current_index = 0;
    for(int i = 0; i < PID_HISTORY_SIZE; ++i) {
        pid_history.history[i] = 0;
    }

    argv_history.current_index = 0;
    for(int i = 0; i < ARGV_HISTORY_SIZE; ++i) {
        argv_history.history[i] = NULL;
    }

    do {
        printf("msh> ");
        if(fgets(buffer, LINE_MAX_LEN, stdin)) {
            argv = msh_get_arguments(buffer);
            keepRunning = msh_exec(argv, &pid_history, &argv_history);
        }
    } while(keepRunning);

    for(int i = 0; i < ARGV_HISTORY_SIZE; ++i) {
        free_argv(argv_history.history[i]);
    }

    return 0;
}