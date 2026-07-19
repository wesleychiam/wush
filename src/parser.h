#ifndef PARSER_H
#define PARSER_H

#define INPUT_BUFFER 64

typedef enum { PARSE_OK, PARSE_EXIT, PARSE_FAIL } ParseResult;

typedef enum {
  REDIR_NONE,
  REDIR_INPUT,
  REDIR_HERE_DOC,
  REDIR_OUTPUT,
  REDIR_OUTPUT_APPEND
} Redirection;

ParseResult parse(char *inp);

#endif