#include <stdio.h>
#include <string.h>

#define INPUT_BUFFER 64
#define MAX_ARGS 16

typedef enum {
    PARSE_OK,
    PARSE_EXIT,
    PARSE_FAIL
} ParseResult;

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
        // Implement ls command
        // TODO
    } else if (strcmp(args[0], "pwd") == 0) {
        // Implement pwd command
        // TODO
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