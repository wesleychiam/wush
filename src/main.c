#include "parser.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t sigint_received = 0;

static void handle_sigint(int sig) {
  (void)sig;
  write(STDOUT_FILENO, "\n", 1);
  sigint_received = 1;
}

int main(void) {
  // Check stdin is a terminal and configure parent signal handling
  if (isatty(STDIN_FILENO)) {
    struct sigaction psa = {0};
    sigemptyset(&psa.sa_mask);
    // Record the signal disposition for SIGINT
    psa.sa_handler = handle_sigint;
    if (sigaction(SIGINT, &psa, NULL) == -1) {
      perror("sigaction");
      return 1;
    }
    // Record the signal disposition for SIGTTOU
    psa.sa_handler = SIG_IGN;
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
    fflush(stdout);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
      if (sigint_received) {
        sigint_received = 0;
        clearerr(stdin);
        continue;
      }
      // Non-SIGINT signal or input error
      break;
    }

    status = parse(buffer);
  }

  return status == PARSE_EXIT ? 0 : 1;
}
