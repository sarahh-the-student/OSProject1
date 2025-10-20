#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_BG_PROCESSES 100

extern char **environ;

// Global variables
pid_t bg_processes[MAX_BG_PROCESSES];
int bg_count = 0;
pid_t foreground_pid = -1;
int timer_active = 0;

// Function declarations
void print_prompt();
char **tokenize_input(char *input);
void execute_command(char **args);
int is_builtin(char **args);
void execute_builtin(char **args);
void handle_sigint(int sig);
void handle_sigchld(int sig);
void setup_signal_handlers();
void add_bg_process(pid_t pid);
void remove_bg_process(pid_t pid);
void check_bg_processes();
void setup_timer();
void cancel_timer();
int handle_redirection(char **args);
int handle_piping(char **args);

// Built-in command functions
void quash_cd(char **args);
void quash_pwd(char **args);
void quash_echo(char **args);
void quash_env(char **args);
void quash_setenv(char **args);
void quash_exit(char **args);

int main() {
    char input[MAX_INPUT];
    char **args;
    
    setup_signal_handlers();
    
    while (1) {
        check_bg_processes();
        print_prompt();
        
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) {
            continue;
        }
        
        args = tokenize_input(input);
        if (args[0] != NULL) {
            execute_command(args);
        }
        
        // Free allocated memory for args
        for (int i = 0; args[i] != NULL; i++) {
            free(args[i]);
        }
        free(args);
    }
    
    return 0;
}

void print_prompt() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s> ", cwd);
    } else {
        printf("quash> ");
    }
    fflush(stdout);
}

char **tokenize_input(char *input) {
    char **tokens = malloc(MAX_ARGS * sizeof(char*));
    char *token;
    int position = 0;
    
    // Handle variable expansion first
    char expanded_input[MAX_INPUT] = {0};
    char *src = input;
    char *dest = expanded_input;
    
    while (*src) {
        if (*src == '$' && (src == input || *(src-1) != '\\')) {
            // Found a variable
            char var_name[256] = {0};
            char *var_start = ++src;
            
            while ((*src >= 'a' && *src <= 'z') ||
                   (*src >= 'A' && *src <= 'Z') ||
                   (*src >= '0' && *src <= '9') ||
                   *src == '_') {
                src++;
            }
            
            strncpy(var_name, var_start, src - var_start);
            char *var_value = getenv(var_name);
            
            if (var_value) {
                strcat(dest, var_value);
                dest += strlen(var_value);
            }
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
    
    // Tokenize the expanded input
    token = strtok(expanded_input, " \t");
    while (token != NULL && position < MAX_ARGS - 1) {
        tokens[position] = malloc(strlen(token) + 1);
        strcpy(tokens[position], token);
        position++;
        token = strtok(NULL, " \t");
    }
    tokens[position] = NULL;
    
    return tokens;
}

void execute_command(char **args) {
    // Check for background process
    int background = 0;
    int arg_count = 0;
    while (args[arg_count] != NULL) {
        arg_count++;
    }
    
    if (arg_count > 0 && strcmp(args[arg_count-1], "&") == 0) {
        background = 1;
        free(args[arg_count-1]);
        args[arg_count-1] = NULL;
    }
    
    if (is_builtin(args)) {
        execute_builtin(args);
    } else {
        // Check for redirection FIRST
        if (handle_redirection(args)) {
            return;
        }
    
        // Check for piping
        if (handle_piping(args)) {
            return;
        }
        
        // Regular command execution
        pid_t pid = fork();
        
        if (pid == 0) {
            if (execvp(args[0], args) == -1) {
                perror("execvp() failed");
                exit(EXIT_FAILURE);
            }
        } else if (pid > 0) {
            if (!background) {
                foreground_pid = pid;
                setup_timer();
                
                int status;
                waitpid(pid, &status, 0);
                
                cancel_timer();
                foreground_pid = -1;
                
                if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    printf("An error occurred.\n");
                }
            } else {
                add_bg_process(pid);
                printf("[%d] %d\n", bg_count, pid);
            }
        } else {
            perror("fork() failed");
        }
    }
}

int is_builtin(char **args) {
    if (args[0] == NULL) return 0;
    
    char *builtins[] = {"cd", "pwd", "echo", "env", "setenv", "exit", NULL};
    
    for (int i = 0; builtins[i] != NULL; i++) {
        if (strcmp(args[0], builtins[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void execute_builtin(char **args) {
    if (strcmp(args[0], "cd") == 0) {
        quash_cd(args);
    } else if (strcmp(args[0], "pwd") == 0) {
        quash_pwd(args);
    } else if (strcmp(args[0], "echo") == 0) {
        quash_echo(args);
    } else if (strcmp(args[0], "env") == 0) {
        quash_env(args);
    } else if (strcmp(args[0], "setenv") == 0) {
        quash_setenv(args);
    } else if (strcmp(args[0], "exit") == 0) {
        quash_exit(args);
    }
}

// Built-in command implementations
void quash_cd(char **args) {
    if (args[1] == NULL) {
        char *home = getenv("HOME");
        if (home) {
            if (chdir(home) != 0) {
                perror("cd");
            }
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
}

void quash_pwd(char **args) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

void quash_echo(char **args) {
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i+1] != NULL) {
            printf(" ");
        }
    }
    printf("\n");
}

void quash_env(char **args) {
    if (args[1] == NULL) {
        // Print all environment variables
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
    } else {
        // Print specific environment variable
        char *value = getenv(args[1]);
        if (value) {
            printf("%s\n", value);
        }
    }
}

void quash_setenv(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "setenv: missing variable name\n");
        return;
    }
    
    char *name = args[1];
    char *value = args[2] ? args[2] : "";
    
    if (setenv(name, value, 1) != 0) {
        perror("setenv");
    }
}

void quash_exit(char **args) {
    exit(EXIT_SUCCESS);
}

// Signal handlers
void handle_sigint(int sig) {
    if (foreground_pid > 0) {
        kill(foreground_pid, SIGINT);
        printf("\n");
    } else {
        printf("\n");
        print_prompt();
    }
}

void handle_sigchld(int sig) {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_bg_process(pid);
    }
}

void setup_signal_handlers() {
    signal(SIGINT, handle_sigint);
    signal(SIGCHLD, handle_sigchld);
}

// Background process management
void add_bg_process(pid_t pid) {
    if (bg_count < MAX_BG_PROCESSES) {
        bg_processes[bg_count++] = pid;
    }
}

void remove_bg_process(pid_t pid) {
    for (int i = 0; i < bg_count; i++) {
        if (bg_processes[i] == pid) {
            for (int j = i; j < bg_count - 1; j++) {
                bg_processes[j] = bg_processes[j+1];
            }
            bg_count--;
            break;
        }
    }
}

void check_bg_processes() {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_bg_process(pid);
        printf("[%d] Done\n", pid);
    }
}

// Timer functions for task 5
void timer_handler(int sig) {
    if (foreground_pid > 0) {
        printf("\nProcess timed out after 10 seconds. Terminating...\n");
        kill(foreground_pid, SIGTERM);
        foreground_pid = -1;
        timer_active = 0;
        print_prompt();
    }
}

void setup_timer() {
    timer_active = 1;
    signal(SIGALRM, timer_handler);
    alarm(10);
}

void cancel_timer() {
    if (timer_active) {
        alarm(0);
        timer_active = 0;
    }
}

// I/O Redirection implementation
int handle_redirection(char **args) {
    int input_redirect = -1, output_redirect = -1;
    char *input_file = NULL, *output_file = NULL;
    
    // Find redirection operators
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            input_redirect = i;
            if (args[i+1] != NULL) {
                input_file = args[i+1];
            }
        } else if (strcmp(args[i], ">") == 0) {
            output_redirect = i;
            if (args[i+1] != NULL) {
                output_file = args[i+1];
            }
        }
    }
    
    if (input_redirect == -1 && output_redirect == -1) {
        return 0; // No redirection
    }
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        
        // Input redirection
        if (input_redirect != -1 && input_file) {
            int fd = open(input_file, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        // Output redirection
        if (output_redirect != -1 && output_file) {
            int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        // Create new args array without redirection tokens
        char **new_args = malloc(MAX_ARGS * sizeof(char*));
        int new_pos = 0;
        
        for (int i = 0; args[i] != NULL; i++) {
            // Skip the redirection operators and their file arguments
            if (i == input_redirect || i == input_redirect + 1 ||
                i == output_redirect || i == output_redirect + 1) {
                continue;
            }
            new_args[new_pos++] = args[i];
        }
        new_args[new_pos] = NULL;
        
        // Execute the command with new arguments
        if (execvp(new_args[0], new_args) == -1) {
            perror("execvp failed");
            free(new_args);
            exit(EXIT_FAILURE);
        }
        
        free(new_args);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return 1;
    } else {
        perror("fork failed");
        return 1;
    }
    
    return 1;
}

// Piping implementation
int handle_piping(char **args) {
    int pipe_index = -1;
    
    // Find pipe operator
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_index = i;
            break;
        }
    }
    
    if (pipe_index == -1) {
        return 0; // No piping
    }
    
    // Split commands
    char **cmd1 = malloc((pipe_index + 1) * sizeof(char*));
    char **cmd2 = malloc((MAX_ARGS - pipe_index) * sizeof(char*));
    
    for (int i = 0; i < pipe_index; i++) {
        cmd1[i] = args[i];
    }
    cmd1[pipe_index] = NULL;
    
    int j = 0;
    for (int i = pipe_index + 1; args[i] != NULL; i++) {
        cmd2[j] = args[i];
        j++;
    }
    cmd2[j] = NULL;
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    
    pid_t pid1 = fork();
    
    if (pid1 == 0) {
        // First child - writes to pipe
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        if (execvp(cmd1[0], cmd1) == -1) {
            perror("execvp cmd1");
            exit(EXIT_FAILURE);
        }
    }
    
    pid_t pid2 = fork();
    
    if (pid2 == 0) {
        // Second child - reads from pipe
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        
        if (execvp(cmd2[0], cmd2) == -1) {
            perror("execvp cmd2");
            exit(EXIT_FAILURE);
        }
    }
    
    // Parent
    close(pipefd[0]);
    close(pipefd[1]);
    
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    
    free(cmd1);
    free(cmd2);
    
    return 1;
}
