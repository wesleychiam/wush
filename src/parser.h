#ifndef PARSER_H
#define PARSER_H

typedef enum { PARSE_OK, PARSE_EXIT, PARSE_FAIL } ParseResult;

typedef enum {
  REDIR_NONE,
  REDIR_INPUT,
  REDIR_OUTPUT,
  REDIR_OUTPUT_APPEND
} Redirection;

ParseResult parse(char *inp);

#endif