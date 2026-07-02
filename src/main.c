#include <stdbool.h>
#include <stdio.h>

#define INPUT_BUFFER 64

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors 
static void parse(char *inp) {
    
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