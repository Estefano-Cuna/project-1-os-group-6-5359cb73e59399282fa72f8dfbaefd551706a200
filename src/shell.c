#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>  
#include <pwd.h> //verify this can be used
#include <ctype.h> //verify this can be used
#include <sys/stat.h> //verify this can be used
#include <sys/wait.h>
#include <sys/types.h>


static int next_job_number = 1;


char *my_strdup(const char *s) {
    char *new_str = malloc(strlen(s) + 1);
    if (new_str) {
        strcpy(new_str, s);
    }
    return new_str;
}

void expand_environment_variables(tokenlist *tokens) {
    for (int i = 0; i < tokens->size; i++) {
        if (tokens->items[i][0] == '$') {
            const char *env_val = getenv(tokens->items[i] + 1);
            if (env_val != NULL) {
                free(tokens->items[i]);
                tokens->items[i] = my_strdup(env_val);
            }
        }
    }
}

void expand_tilde(tokenlist *tokens) {
    const char *homeDir = getenv("HOME");
    if (homeDir != NULL) {

    for (int i = 0; i < tokens->size; i++) {
        if (tokens->items[i][0] == '~') {
            char *expandedPath = NULL;

            if (tokens->items[i][1] == '\0' || tokens->items[i][1] == '/') {
                // tilde at the beginning or followed by /
                expandedPath = my_strdup(homeDir);
                if (tokens->items[i][1] == '/' && tokens->items[i][2] != '\0' && tokens->items[i][2] != '/') {
                    expandedPath = realloc(expandedPath, strlen(expandedPath) + strlen(tokens->items[i] + 1) + 1);
                    strcat(expandedPath, tokens->items[i] + 1);
                }
            } else {
                // is a tilde by itself
                expandedPath = my_strdup(homeDir);
                expandedPath = realloc(expandedPath, strlen(expandedPath) + strlen(tokens->items[i]) + 1);
                strcat(expandedPath, tokens->items[i]);
            }

            // replace  token with the expanded path
            if (expandedPath != NULL) {
                free(tokens->items[i]);
                tokens->items[i] = expandedPath;
            }
        }
    }
}
}



void execute_command(tokenlist *tokens, bool run_in_background) {
    if (tokens->size == 0) {
        // No command entered.
        return;
    }

    pid_t pid;
    int status;
    const char *cmd = tokens->items[0];
    struct stat statbuf;
    char fullPath[1024];

    // Check if the command contains a path
    if (strchr(cmd, '/') != NULL) {
        if (stat(cmd, &statbuf) == 0 && (statbuf.st_mode & S_IXUSR)) {
            strcpy(fullPath, cmd); // Use the command as the full path
        } else {
            fprintf(stderr, "%s: command not found or not executable\n", cmd);
            return;
        }
    } else {
        // Perform a PATH search
        char *path = getenv("PATH");
        char *pathCopy = strdup(path);
        char *dir = strtok(pathCopy, ":");

        int found = 0;
        while (dir != NULL && !found) {
            snprintf(fullPath, sizeof(fullPath), "%s/%s", dir, cmd);
            if (stat(fullPath, &statbuf) == 0 && (statbuf.st_mode & S_IXUSR)) {
                found = 1;
            }
            dir = strtok(NULL, ":");
        }
        free(pathCopy);
        
        if (!found) {
            fprintf(stderr, "%s: command not found\n", cmd);
            return;
        }
    }

    // Prepare for I/O redirection
    FILE *inputFile = NULL, *outputFile = NULL;
    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], ">") == 0) {
            outputFile = fopen(tokens->items[i + 1], "w");
            if (outputFile == NULL) {
                perror("fopen");
                return;
            }
            i++;
        } else if (strcmp(tokens->items[i], "<") == 0) {
            inputFile = fopen(tokens->items[i + 1], "r");
            if (inputFile == NULL) {
                perror("fopen");
                return;
            }
            i++;
        }
    }

    // Prepare arguments for execv, excluding redirection tokens
    char *args[tokens->size + 1];
    int arg_index = 0;
    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], ">") == 0 || strcmp(tokens->items[i], "<") == 0) {
            i++; // Skip next token (filename)
        } else {
            args[arg_index++] = tokens->items[i];
        }
    }
    args[arg_index] = NULL;

    // Fork a child process
    pid = fork();
    if (pid == 0) {
        // Child process
        if (inputFile != NULL) {
            dup2(fileno(inputFile), STDIN_FILENO);
            fclose(inputFile);
        }
        if (outputFile != NULL) {
            dup2(fileno(outputFile), STDOUT_FILENO);
            fclose(outputFile);
        }
        if (execv(fullPath, args) == -1) {
            perror("execv");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("fork");
    } else {
        if (run_in_background) {
            register_background_job(pid, tokens, next_job_number++);
        } else {
            if (inputFile != NULL) fclose(inputFile);
            if (outputFile != NULL) fclose(outputFile);
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
}
//Part 7
void execute_command_piping(tokenlist *tokens, int pipe_count, bool run_in_background) {
    if (tokens->size == 0 || pipe_count == 0) {
        // No command entered or no pipes to handle.
        return;
    }

    pid_t pid;
    int status;
    int fd[2 * pipe_count];
    int start = 0;
    int cmd_index = 0;

    for (int i = 0; i < pipe_count; i++) {
        if (pipe(fd + i * 2) < 0) {
            perror("pipe");
            return;
        }
    }

    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "|") == 0 || i == tokens->size - 1) {
            tokenlist *cmd_tokens = new_tokenlist();
            for (int j = start; j < (i == tokens->size - 1 ? tokens->size : i); j++) {
                add_token(cmd_tokens, tokens->items[j]);
            }

            pid = fork();
            if (pid == 0) {
                // Child process
                if (cmd_index < pipe_count) {
                    if (dup2(fd[cmd_index * 2 + 1], STDOUT_FILENO) < 0) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                if (cmd_index > 0) {
                    if (dup2(fd[(cmd_index - 1) * 2], STDIN_FILENO) < 0) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }
                }
                for (int j = 0; j < 2 * pipe_count; j++) {
                    close(fd[j]);
                }

                //new code
                char fullPath[1024];
                char **cmd_args = cmd_tokens->items;
                cmd_args[cmd_tokens->size] = NULL;

                if (!resolve_command_path(cmd_args[0], fullPath)) {
                    fprintf(stderr, "%s: command not found\n", cmd_args[0]);
                    exit(EXIT_FAILURE);
                }

                execv(fullPath, cmd_args);
                perror("execv");
                exit(EXIT_FAILURE);
                //end new code


            } else if (pid < 0) {
                perror("fork");
                return;
            }

            free_tokens(cmd_tokens);
            start = i + 1;
            cmd_index++;
        }
    }

    for (int i = 0; i < 2 * pipe_count; i++) {
        close(fd[i]);
    }


    if (!run_in_background)
    {
        for (int i = 0; i < pipe_count + 1; i++) {
            wait(NULL);
        }
    } else {
        register_background_job(pid, tokens, next_job_number++);
    }
}

void register_background_job(pid_t pid, tokenlist *tokens, int job_number) {
    if (job_number > MAX_BG_JOBS) {
        fprintf(stderr, "Exceeded maximum number of background jobs\n");
        return;
    }

    bg_jobs[job_number - 1].job_number = job_number;
    bg_jobs[job_number - 1].pid = pid;
    bg_jobs[job_number - 1].status = 0; // running
    // Concatenate tokens into a single command string
    bg_jobs[job_number - 1].command = concatenate_tokens(tokens);

    printf("[%d] %d\n", job_number, pid);
}

void check_background_jobs() {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (bg_jobs[i].status == 0 && bg_jobs[i].pid != 0) {
            int status;
            pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
            if (result > 0) { // Process has completed
                bg_jobs[i].status = 1;
                printf("[%d] + done %s\n", bg_jobs[i].job_number, bg_jobs[i].command);
                // Free the command string if dynamically allocated
                free(bg_jobs[i].command);
                bg_jobs[i].command = NULL;
                bg_jobs[i].pid = 0;
            } else if (result < 0) {
                // Handle errors
            }
        }
    }
}

char *concatenate_tokens(tokenlist *tokens) {
    if (tokens == NULL || tokens->size == 0) {
        return NULL;
    }

    // Calculate total length needed for the concatenated string
    int total_length = 0;
    for (int i = 0; i < tokens->size; i++) {
        total_length += strlen(tokens->items[i]) + 1; // +1 for space or null terminator
    }

    // Allocate memory for the concatenated string
    char *concatenated = malloc(total_length);
    if (concatenated == NULL) {
        perror("malloc");
        return NULL;
    }

    concatenated[0] = '\0'; // Start with an empty string

    // Concatenate all tokens
    for (int i = 0; i < tokens->size; i++) {
        strcat(concatenated, tokens->items[i]);
        if (i < tokens->size - 1) {
            strcat(concatenated, " "); // Add a space between tokens
        }
    }

    return concatenated;
}

int resolve_command_path(const char *cmd, char *fullPath) {
    struct stat statbuf;
    if (strchr(cmd, '/') != NULL) {
        if (stat(cmd, &statbuf) == 0 && (statbuf.st_mode & S_IXUSR)) {
            strcpy(fullPath, cmd);
            return 1;
        }
    } else {
        char *path = getenv("PATH");
        char *pathCopy = strdup(path);
        char *dir;
        for (dir = strtok(pathCopy, ":"); dir != NULL; dir = strtok(NULL, ":")) {
            snprintf(fullPath, 1024, "%s/%s", dir, cmd);
            if (stat(fullPath, &statbuf) == 0 && (statbuf.st_mode & S_IXUSR)) {
                free(pathCopy);
                return 1;
            }
        }
        free(pathCopy);
    }
    return 0;
}
