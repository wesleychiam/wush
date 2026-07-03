#include <stdio.h>
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
// Processes ls type instructions using external commands
static int ls(char **args) {
    pid_t pid = fork();
    int status; // Status of child process
    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Child process
        execvp(args[0], args);
        perror("execvp failed");
        return 1;
    } else {
        // Parent process
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return 0;
        return 1;
    }
}

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors 
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
    if (strcmp(args[0], "ls") == 0) {
        ls(args);
        return PARSE_OK;
    } else if (strcmp(args[0], "pwd") == 0) {
        // Implement pwd command
        // TODO
        return PARSE_OK;
    } else if (strcmp(args[0], "exit") == 0) {
        return PARSE_EXIT;
    } else {
        printf("%s: command not found\n", args[0]);
        return PARSE_FAIL;
    }
}

int main(void) {
    ParseResult status = PARSE_OK;
    while (status != PARSE_EXIT) {
        char buffer[INPUT_BUFFER];
        printf("wush> ");
        fgets(buffer, sizeof(buffer), stdin);

        status = parse(buffer);
        
    }
}