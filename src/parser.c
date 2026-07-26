#include "parser.h"
#include "execution.h"

#include <assert.h>
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

typedef enum { READ_DOUBLE_QUOTE, READ_SINGLE_QUOTE, READ_NORMAL } ReadState;

typedef enum {
  TOKEN_OK,
  TOKEN_UNMATCHED_QUOTATION,
  TOKEN_HANGING_ESCAPE
} TokenState;

// Parses tokens according to the redirection symbol if it is an operator
// Returns the corresponding enum type, otherwise REDIR_NONE
static Redirection get_redirection(const char *token, bool is_operator) {
  if (strcmp("<", token) == 0 && is_operator)
    return REDIR_INPUT;
  if (strcmp("<<", token) == 0 && is_operator)
    return REDIR_HERE_DOC;
  if (strcmp(">", token) == 0 && is_operator)
    return REDIR_OUTPUT;
  if (strcmp(">>", token) == 0 && is_operator)
    return REDIR_OUTPUT_APPEND;
  return REDIR_NONE;
}

// Tokenises a string based on delimiters, quotations, and escapes.
// Takes a pointer to string to be tokenised, a string of all delimiters, a
// pointer for repeat calls to parse tokenised string, a pointer for indicating
// shell operators, and a token state pointer.
// Uses a read state to separate tokens involving escapes and quotation marks
// Returns pointer to first token in string, or NULL if no tokens remain.
// Modifies original string by '\0' insertion and escape/quotation overwrites.
// Constraint: saveptr != NULL
// Constraint: delim != NULL
static char *strtok_erq(char *str, const char *delim, char **saveptr,
                        bool *is_operator, TokenState *token_state) {
  assert(saveptr != NULL);
  assert(delim != NULL);
  assert(is_operator != NULL);
  assert(token_state != NULL);

  // End case
  if (str == NULL && *saveptr == NULL) {
    *token_state = TOKEN_OK;
    return NULL;
  }
  // Set initial state and pointers
  char *read_ptr;
  char *write_ptr;
  char *output_ptr;
  *token_state = TOKEN_OK;
  ReadState read_state = READ_NORMAL;
  if (str == NULL) {
    read_ptr = *saveptr;
  } else {
    read_ptr = str;
  }
  *is_operator = true;
  // Skip leading delimiters
  while (*read_ptr != '\0' && strchr(delim, *read_ptr) != NULL) {
    read_ptr++;
  }
  // Update pointers and add escape state
  if (*read_ptr == '\0') {
    *saveptr = NULL;
    return NULL;
  }
  write_ptr = read_ptr;
  output_ptr = read_ptr;
  bool escaped = false;
  // Scan until end/next delimiter
  while (*read_ptr != '\0' &&
         (strchr(delim, *read_ptr) == NULL || read_state != READ_NORMAL ||
          (escaped && *read_ptr != '\n'))) {
    assert(read_ptr >= write_ptr);
    switch (read_state) {
    case READ_DOUBLE_QUOTE:
      switch (*read_ptr) {
      case '\\':
        if (escaped) {
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          escaped = false;
        } else {
          escaped = true;
          read_ptr++;
        }
        break;
      case '"':
        if (escaped) {
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          escaped = false;
        } else {
          read_state = READ_NORMAL;
          read_ptr++;
        }
        break;
      default:
        if (escaped) {
          *write_ptr = '\\';
          write_ptr++;
          escaped = false;
        }
        *write_ptr = *read_ptr;
        write_ptr++;
        read_ptr++;
        break;
      }
      break;
    case READ_SINGLE_QUOTE:
      switch (*read_ptr) {
      case '\'':
        read_state = READ_NORMAL;
        read_ptr++;
        break;
      default:
        *write_ptr = *read_ptr;
        write_ptr++;
        read_ptr++;
        break;
      }
      break;
    case READ_NORMAL:
      switch (*read_ptr) {
      case '"':
        *is_operator = false;
        if (escaped) {
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          escaped = false;
        } else {
          read_state = READ_DOUBLE_QUOTE;
          read_ptr++;
        }
        break;
      case '\'':
        *is_operator = false;
        if (escaped) {
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          escaped = false;
        } else {
          read_state = READ_SINGLE_QUOTE;
          read_ptr++;
        }
        break;
      case '\\':
        *is_operator = false;
        if (escaped) {
          *write_ptr = *read_ptr;
          write_ptr++;
          read_ptr++;
          escaped = false;
        } else {
          escaped = true;
          read_ptr++;
        }
        break;
      default:
        escaped = false;
        *write_ptr = *read_ptr;
        write_ptr++;
        read_ptr++;
        break;
      }
      break;
    default:
      printf("strtok_erq: invalid state reached\n");
      abort();
    }
  }
  // Insert null-terminator
  if (*read_ptr == '\0') {
    *write_ptr = '\0';
    *saveptr = NULL;
  } else {
    *write_ptr = '\0';
    read_ptr++;
    *saveptr = read_ptr;
  }
  // Check there are no unmatched brackets or hanging escapes
  if (escaped) {
    printf("strtok_erq: hanging escape character\n");
    *token_state = TOKEN_HANGING_ESCAPE;
    return NULL;
  }
  if (read_state != READ_NORMAL) {
    printf("strtok_erq: unmatched quotation mark\n");
    *token_state = TOKEN_UNMATCHED_QUOTATION;
    return NULL;
  }

  return output_ptr;
}

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors
// Returns PARSE_FAIL if user enters a bad/unknown command
ParseResult parse(char *inp) {
  // Redirection variables
  ParseState parse_state = ARGUMENT;
  Redirection arg = REDIR_NONE;
  Redirection output_redir = REDIR_NONE;
  Redirection input_redir = REDIR_NONE;

  // Pipe variables
  int nstages = 0;
  int stage_start[MAX_ARGS];
  stage_start[nstages++] = 0;

  // Here-document & filename buffer variables
  char here_doc_delim[FILENAME_BUFFER];
  char input_filename[FILENAME_BUFFER];
  char output_filename[FILENAME_BUFFER];

  // First pass: split input string into tokens
  char *delims = " \t\n";
  char *ptr;
  bool is_operator;
  TokenState token_state;
  char *token = strtok_erq(inp, delims, &ptr, &is_operator, &token_state);
  switch (token_state) {
  case TOKEN_OK:
    break;
  case TOKEN_UNMATCHED_QUOTATION:
    return PARSE_FAIL;
  case TOKEN_HANGING_ESCAPE:
    return PARSE_FAIL;
  default:
    printf("strtok_erq: invalid state reached\n");
    abort();
  }
  char *args[MAX_ARGS];
  int nargs = 0;

  // Read tokens
  while (token != NULL && nargs < MAX_ARGS - 1) {
    arg = get_redirection(token, is_operator);
    bool is_pipe = strcmp(token, "|") == 0 && is_operator;
    if (parse_state == ARGUMENT) {
      // Compare argument to redirection (or ordinary argument/pipe)
      switch (arg) {
      case REDIR_INPUT:
        if (input_redir != REDIR_NONE) {
          printf("parse: multiple input streams detected\n");
          return PARSE_FAIL;
        }
        parse_state = EXPECT_INPUT;
        input_redir = REDIR_INPUT;
        break;
      case REDIR_HERE_DOC:
        if (input_redir != REDIR_NONE) {
          printf("parse: multiple input streams detected\n");
          return PARSE_FAIL;
        }
        parse_state = EXPECT_HERE_DOC;
        input_redir = REDIR_HERE_DOC;
        break;
      case REDIR_OUTPUT:
        if (output_redir != REDIR_NONE) {
          printf("parse: multiple output streams detected\n");
          return PARSE_FAIL;
        }
        parse_state = EXPECT_OUTPUT;
        output_redir = REDIR_OUTPUT;
        break;
      case REDIR_OUTPUT_APPEND:
        if (output_redir != REDIR_NONE) {
          printf("parse: multiple output streams detected\n");
          return PARSE_FAIL;
        }
        parse_state = EXPECT_OUTPUT_APPEND;
        output_redir = REDIR_OUTPUT_APPEND;
        break;
      case REDIR_NONE:
        // Parse pipe logic
        if (is_pipe && (nargs == 0 || args[nargs - 1] == NULL)) {
          // Consecutive pipes seen: <command> | |
          printf("Usage: <command> | <command>\n");
          return PARSE_FAIL;
        } else if (is_pipe) {
          args[nargs++] = NULL;
          stage_start[nstages++] = nargs;
        } else {
          args[nargs++] = token;
        }
        break;
      default:
        printf("parse: invalid state reached\n");
        abort();
      }

    } else {
      // Prepare to copy filename/delimiter
      if (strlen(token) >= FILENAME_BUFFER) {
        printf("parse: filename exceeds buffer capacity: %d\n",
               FILENAME_BUFFER);
        return PARSE_FAIL;
      }

      // Do not accept filenames named as a redirection operator or a pipe
      if (arg != REDIR_NONE || is_pipe) {
        switch (parse_state) {
        case EXPECT_INPUT:
          printf("parse: expected filename after '<'\n");
          return PARSE_FAIL;
        case EXPECT_HERE_DOC:
          printf("parse: expected delimiter after '<<'\n");
          return PARSE_FAIL;
        case EXPECT_OUTPUT:
          printf("parse: expected filename after '>'\n");
          return PARSE_FAIL;
        case EXPECT_OUTPUT_APPEND:
          printf("parse: expected filename after '>>'\n");
          return PARSE_FAIL;
        default:
          printf("parse: invalid state reached\n");
          abort();
        }
      }

      switch (parse_state) {
      case EXPECT_INPUT:
        strcpy(input_filename, token);
        break;
      case EXPECT_HERE_DOC:
        strcpy(here_doc_delim, token);
        break;
      case EXPECT_OUTPUT:
      case EXPECT_OUTPUT_APPEND:
        strcpy(output_filename, token);
        break;
      default:
        printf("parse: invalid state reached\n");
        abort();
      }
      parse_state = ARGUMENT;
    }

    token = strtok_erq(NULL, delims, &ptr, &is_operator, &token_state);
    switch (token_state) {
    case TOKEN_OK:
      break;
    case TOKEN_UNMATCHED_QUOTATION:
      return PARSE_FAIL;
    case TOKEN_HANGING_ESCAPE:
      return PARSE_FAIL;
    default:
      printf("strtok_erq: invalid state reached\n");
      abort();
    }
  }

  // Sentinel-terminated arrays
  args[nargs] = NULL;
  stage_start[nstages] = -1;

  // Check for incomplete redirection command
  if (parse_state != ARGUMENT) {
    printf("parse: expected filename\n");
    return PARSE_FAIL;
  }

  // Check for incomplete pipe command
  bool pipe_found = nstages > 1;
  if (pipe_found &&
      (stage_start[1] == 1 || stage_start[nstages - 1] == nargs)) {
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
    if (external_command(args, input_filename, here_doc_delim, input_redir,
                         output_filename, output_redir, pipe_found, stage_start,
                         nstages)) {
      return PARSE_FAIL;
    }

    return PARSE_OK;
  }
}
