#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  ParseResult status = PARSE_OK;
  while (status != PARSE_EXIT) {
    char buffer[INPUT_BUFFER];
    printf("wush> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
      break;
    status = parse(buffer);
  }
}
