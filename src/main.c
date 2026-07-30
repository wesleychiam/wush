#include "parser.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  // Check stdin is a terminal and configure parent signal handling
  if (isatty(STDIN_FILENO)) {
    struct sigaction psa = {0};
    psa.sa_handler = SIG_IGN;
    sigemptyset(&psa.sa_mask);
    if (sigaction(SIGINT, &psa, NULL) == -1) {
      perror("sigaction");
      return 1;
    }
    if (sigaction(SIGTTOU, &psa, NULL) == -1) {
      perror("sigaction");
      return 1;
    }
  }
  // Begin shell
  ParseResult status = PARSE_OK;
  while (status != PARSE_EXIT) {
    char buffer[INPUT_BUFFER];
    printf("wush> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
      break;
    status = parse(buffer);
  }
}
