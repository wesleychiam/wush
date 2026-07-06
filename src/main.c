#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_BUFFER 64
#define MAX_ARGS 16

typedef enum {
    PARSE_OK,
    PARSE_EXIT,
    PARSE_FAIL
} ParseResult;

// Takes parsed tokens
// Processes external command instructions 
static int externalCommand(char **args) {
    pid_t pid = fork();
    int status; // Status of child process
    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        execvp(args[0], args);
        perror("execvp failed");
        _exit(127); // Command not found
    } else {
        // Parent process
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return 0;
        return 1;
    }
}

// Takes parsed tokens and number of arguments
// Process change directory type (cd) instructions
// Return 0 on success, else -1
static int builtin_cd(char **args, int nargs) {
    int status;
    if (nargs == 1) {
        status = chdir(getenv("HOME"));
        if (status == -1) perror("cd");
    } else if (nargs == 2) {
        status = chdir(args[1]);
        if (status == -1) perror("cd");
    } else {
        status = -1;
        printf("Usage: cd <path>\n");
    }
    return status;
}

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors
// Returns PARSE_FAIL if user enters a bad/unknown command 
static ParseResult parse(char *inp) {
    // First pass: split input string into tokens
    char *delims = " \t\n";
    char *ptr;
    char *token = strtok_r(inp, delims, &ptr);
    char *args[MAX_ARGS];
    int nargs = 0;
    while(token != NULL && nargs < MAX_ARGS - 1) {
        args[nargs] = token;
        nargs++;
        token = strtok_r(NULL, delims, &ptr);
    }
    args[nargs] = NULL;

    // Second pass: compare tokens with defined functions
    if (nargs == 0) {
        return PARSE_OK;
    } else if (strcmp(args[0], "exit") == 0) {
        return PARSE_EXIT;
    } else if (strcmp(args[0], "cd") == 0) {
        int error = builtin_cd(args, nargs);
        if (error == -1) {
            return PARSE_FAIL;
        }
        return PARSE_OK;
    } else {
        int error = externalCommand(args);
        if (error) return PARSE_FAIL;
        return PARSE_OK;
    }
}

int main(void) {
    ParseResult status = PARSE_OK;
    while (status != PARSE_EXIT) {
        char buffer[INPUT_BUFFER];
        printf("wush> ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break;
        status = parse(buffer);
        
    }
}