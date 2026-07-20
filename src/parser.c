#include "parser.h"
#include "execution.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME_BUFFER 32
#define MAX_ARGS 16

typedef enum {
  EXPECT_INPUT,
  EXPECT_HERE_DOC,
  EXPECT_OUTPUT,
  EXPECT_OUTPUT_APPEND,
  ARGUMENT
} ParseState;

// Parses tokens according to the redirection operator
// Returns the corresponding enum type, otherwise REDIR_NONE
static Redirection get_redirection(const char *token) {
  if (strcmp("<", token) == 0)
    return REDIR_INPUT;
  if (strcmp("<<", token) == 0)
    return REDIR_HERE_DOC;
  if (strcmp(">", token) == 0)
    return REDIR_OUTPUT;
  if (strcmp(">>", token) == 0)
    return REDIR_OUTPUT_APPEND;
  return REDIR_NONE;
}

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors
// Returns PARSE_FAIL if user enters a bad/unknown command
ParseResult parse(char *inp) {
  // Redirection variables
  char output[FILENAME_BUFFER];
  char input[FILENAME_BUFFER];
  ParseState state = ARGUMENT;
  Redirection arg = REDIR_NONE;
  Redirection output_redir = REDIR_NONE;
  Redirection input_redir = REDIR_NONE;

  // Pipe variables
  int pipe_start = 0;
  bool pipe_found = false;

  // Here-document variables
  char here_doc_delim[FILENAME_BUFFER];
  bool here_doc_found = false;

  // First pass: split input string into tokens
  char *delims = " \t\n";
  char *ptr;
  char *token = strtok_r(inp, delims, &ptr);
  char *args[MAX_ARGS];
  int nargs = 0;

  while (token != NULL && nargs < MAX_ARGS - 1) {
    arg = get_redirection(token);
    bool is_pipe = strcmp(token, "|") == 0;
    if (state == ARGUMENT) {
      switch (arg) {
      case REDIR_INPUT:
        if (input_redir != REDIR_NONE) {
          printf("Error: multiple input streams detected\n");
          return PARSE_FAIL;
        }
        state = EXPECT_INPUT;
        input_redir = REDIR_INPUT;
        break;
      case REDIR_HERE_DOC:
        if (input_redir != REDIR_NONE) {
          printf("Error: multiple input streams detected\n");
          return PARSE_FAIL;
        }
        here_doc_found = true;
        state = EXPECT_HERE_DOC;
        input_redir = REDIR_HERE_DOC;
        break;
      case REDIR_OUTPUT:
        if (output_redir != REDIR_NONE) {
          printf("Error: multiple output streams detected\n");
          return PARSE_FAIL;
        }
        state = EXPECT_OUTPUT;
        output_redir = REDIR_OUTPUT;
        break;
      case REDIR_OUTPUT_APPEND:
        if (output_redir != REDIR_NONE) {
          printf("Error: multiple output streams detected\n");
          return PARSE_FAIL;
        }
        state = EXPECT_OUTPUT_APPEND;
        output_redir = REDIR_OUTPUT_APPEND;
        break;
      case REDIR_NONE:
        // Parse pipe logic
        if (is_pipe && pipe_found) {
          printf("Error: more than one pipe detected\n");
          return PARSE_FAIL;
        } else if (is_pipe) {
          args[nargs++] = NULL;
          pipe_found = true;
          pipe_start = nargs;
        } else {
          args[nargs++] = token;
        }
        break;
      default:
        printf("Error: invalid state reached\n");
        abort();
      }

    } else {
      if (strlen(token) >= FILENAME_BUFFER) {
        printf("Error: filename exceeds buffer capacity: %d\n",
               FILENAME_BUFFER);
        return PARSE_FAIL;
      }

      // Do not accept filenames named as a redirection operator or a pipe
      if (arg != REDIR_NONE || is_pipe) {
        switch (state) {
        case EXPECT_INPUT:
          printf("Error: expected filename after '<'\n");
          return PARSE_FAIL;
        case EXPECT_HERE_DOC:
          printf("Error: expected delimiter after '<<'\n");
          return PARSE_FAIL;
        case EXPECT_OUTPUT:
          printf("Error: expected filename after '>'\n");
          return PARSE_FAIL;
        case EXPECT_OUTPUT_APPEND:
          printf("Error: expected filename after '>>'\n");
          return PARSE_FAIL;
        default:
          printf("Error: invalid state reached\n");
          abort();
        }
      }

      switch (state) {
      case EXPECT_INPUT:
        strcpy(input, token);
        break;
      case EXPECT_HERE_DOC:
        strcpy(here_doc_delim, token);
        break;
      case EXPECT_OUTPUT:
      case EXPECT_OUTPUT_APPEND:
        strcpy(output, token);
        break;
      default:
        printf("Error: invalid state reached\n");
        abort();
      }
      state = ARGUMENT;
    }

    token = strtok_r(NULL, delims, &ptr);
  }

  // Check for incomplete redirection command
  if (state != ARGUMENT) {
    printf("Error: expected filename\n");
    return PARSE_FAIL;
  }
  args[nargs] = NULL;

  // Check for incomplete pipe command
  if (pipe_found && (pipe_start == 1 || pipe_start == nargs)) {
    printf("Usage: <command> | <command>\n");
    return PARSE_FAIL;
  }

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
    char *input_filename = input_redir == REDIR_INPUT ? input : NULL;
    char *output_filename =
        output_redir == (REDIR_OUTPUT || REDIR_OUTPUT_APPEND) ? output : NULL;

    int error;
    if (here_doc_found) {
      error =
          builtin_here_doc(args, here_doc_delim, output_filename, output_redir);
    } else if (pipe_found) {
      error = external_pipe(args, input_filename, output_filename, output_redir,
                            pipe_start);
    } else {
      error =
          external_command(args, input_filename, output_filename, output_redir);
    }
    if (error)
      return PARSE_FAIL;
    return PARSE_OK;
  }
}
