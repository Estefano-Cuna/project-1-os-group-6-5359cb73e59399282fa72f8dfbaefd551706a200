#include "lexer.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//PART1
#include <unistd.h>
#include <limits.h>
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif
//

int main()
{

	//PART1
    char cwd[1024];
    char hostname[HOST_NAME_MAX];
    char *user = getenv("USER");

    if (user == NULL) {
        user = "unknown";
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", HOST_NAME_MAX);
    }

	while (1) {
		
			check_background_jobs();

	//PART1
			if (getcwd(cwd, sizeof(cwd)) == NULL) {
				perror("getcwd() error");
				return 1;
			}

			printf("%s@%s:%s> ", user, hostname, cwd);

			// Read user input
			char *input = get_input();
			printf("whole input: %s\n", input);

			// Parse and process the input
			tokenlist *tokens = get_tokens(input);
			expand_environment_variables(tokens);
			expand_tilde(tokens);

			bool run_in_background = false;
        	if (tokens->size > 0 && strcmp(tokens->items[tokens->size - 1], "&") == 0) {
            	run_in_background = true;
            	free(tokens->items[tokens->size - 1]);  // Free the '&' string
            	tokens->items[tokens->size - 1] = NULL; // Remove '&' from the token list
            	tokens->size--;                          // Decrease the token list size
        	}

			// Check for piping
			if (strstr(input, "|") != NULL) {
				// Piping detected, count the number of pipes
				int pipe_count = 0;
				for (int i = 0; i < strlen(input); i++) {
					if (input[i] == '|') {
						pipe_count++;
					}
				}

				// Execute the command with piping
				execute_command_piping(tokens, pipe_count, run_in_background);
			} else {
				// No piping detected, execute the command without piping
				execute_command(tokens, run_in_background);
			}

			// Free allocated memory
			free(input);
			free_tokens(tokens);
	}
	return 0;

}

char *get_input(void) {
	char *buffer = NULL;
	int bufsize = 0;
	char line[5];
	while (fgets(line, 5, stdin) != NULL)
	{
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;
		buffer = (char *)realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;
		if (newln != NULL)
			break;
	}
	buffer = (char *)realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;
	return buffer;
}

tokenlist *new_tokenlist(void) {
	tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **)malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

void add_token(tokenlist *tokens, char *item) {
	int i = tokens->size;

	tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *)malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);
	tokenlist *tokens = new_tokenlist();
	char *tok = strtok(buf, " ");
	while (tok != NULL)
	{
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}
	free(buf);
	return tokens;
}

void free_tokens(tokenlist *tokens) {
	for (int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}
