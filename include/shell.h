#ifndef SHELL_H
#define SHELL_H
#include "lexer.h"
#include <sys/types.h>

typedef struct {
    int job_number;
    pid_t pid;
    char *command;
    int status; // 0 for running, 1 for completed
} bg_job;

#define MAX_BG_JOBS 10

bg_job bg_jobs[MAX_BG_JOBS];

// Function Declarations
void expand_environment_variables(tokenlist *tokens);
void expand_tilde(tokenlist *tokens);
void execute_command(tokenlist *tokens, bool run_in_background);
void execute_command_piping(tokenlist *tokens, int pipe_count, bool run_in_background);
void register_background_job(pid_t pid, tokenlist *tokens, int job_number);
void check_background_jobs(void);
char *concatenate_tokens(tokenlist *tokens);
int resolve_command_path(const char *cmd, char *fullPath);
#endif // SHELL_H
