#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100
#define MAX_ENV_VARS 100

struct EnvVar {
    char name[256];
    char value[256];
} env_vars[MAX_ENV_VARS];

int env_var_count = 0;

void replace_env_vars(char *command) {
    for (int i = 0; i < env_var_count; i++) {
        char placeholder[258];
        snprintf(placeholder, sizeof(placeholder), "$%s", env_vars[i].name);

        char *pos = strstr(command, placeholder);
        while (pos) {
            char new_command[MAX_INPUT];
            snprintf(new_command, pos - command + 1, "%s", command);
            strcat(new_command, env_vars[i].value);
            strcat(new_command, pos + strlen(placeholder));
            strcpy(command, new_command);

            pos = strstr(command, placeholder);
        }
    }
}

void execute_pipeline(char *commands[], int count) {
    int pipe_fd[2], input_fd = 0;
    pid_t pid;

    for (int i = 0; i < count; i++) {
        pipe(pipe_fd);
        
        if ((pid = fork()) == 0) {
            dup2(input_fd, STDIN_FILENO);
            if (i < count - 1) {
                dup2(pipe_fd[1], STDOUT_FILENO);
            }
            close(pipe_fd[0]);

            char *args[MAX_ARGS];
            int argc = 0;
            char *token = strtok(commands[i], " ");
            while (token && argc < MAX_ARGS - 1) {
                args[argc++] = token;
                token = strtok(NULL, " ");
            }
            args[argc] = NULL;

            execvp(args[0], args);
            perror("execvp");
            exit(1);
        } else {
            wait(NULL);
            close(pipe_fd[1]);
            input_fd = pipe_fd[0];
        }
    }
}

void execute_command(char *command) {
    replace_env_vars(command);
    

    if (strchr(command, '|')) {
        char *commands[MAX_ARGS];
        int count = 0;
        char *token = strtok(command, "|");
        while (token && count < MAX_ARGS - 1) {
            commands[count++] = token;
            token = strtok(NULL, "|");
        }
        commands[count] = NULL;
        execute_pipeline(commands, count);
        return;
    }

    char *redir_in = strchr(command, '<');
    char *redir_out = strchr(command, '>');
    char *background = strchr(command, '&');

    int input_fd = -1, output_fd = -1;
    if (redir_in) {
        *redir_in = 0;
        char *file = strtok(redir_in + 1, " ");
        input_fd = open(file, O_RDONLY);
        if (input_fd < 0) {
            perror("open");
            return;
        }
    }

    if (redir_out) {
        *redir_out = 0;
        char *file = strtok(redir_out + 1, " ");
        output_fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            perror("open");
            return;
        }
    }

    if (background) {
        *background = 0;
    }

    char *args[MAX_ARGS];
    int argc = 0;
    char *token = strtok(command, " ");

    while (token && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL;

    if (argc == 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        if (input_fd != -1) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        if (output_fd != -1) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        if (background == NULL) {
            waitpid(pid, NULL, 0);
        }
        if (input_fd != -1) close(input_fd);
        if (output_fd != -1) close(output_fd);
    } else {
        perror("fork");
    }
    if (redir_in) {
    *redir_in = 0;
    char *file = strtok(redir_in + 1, " ");
    printf("Debug: Opening file for input: %s\n", file); 
    input_fd = open(file, O_RDONLY);
    if (input_fd < 0) {
        perror("open");
        return;
    }
}
}
int main() {
    char input[MAX_INPUT];

    while (1) {
        printf("xsh# ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = 0;  // Remove newline

        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            break;
        }

        execute_command(input);
    }

    return 0;
}
