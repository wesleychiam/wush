#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define INPUT_BUFFER 64
#define MAX_ARGS 16

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors 
static void parse(char *inp) {
    // First pass: split input string into tokens
    char *delims = " \t\n";
    char *ptr;
    char *token = strtok_r(inp, delims, &ptr);
    char *args[MAX_ARGS];
    int nargs = 0;
    while(token != NULL && nargs < MAX_ARGS - 1) {
        args[nargs] = token;
        printf("%s ", args[nargs]);
        nargs++;
        token = strtok_r(NULL, delims, &ptr);
    }
    args[nargs] = NULL;

    // Second pass: compare tokens with defined functions
    // TODO
}

int main(void) {
    bool running = true;
    while (running) {
        char buffer[INPUT_BUFFER];
        printf("wush> ");
        fgets(buffer, sizeof(buffer), stdin);
        parse(buffer);
    }
}