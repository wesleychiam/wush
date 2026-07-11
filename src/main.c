#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define COMMAND_NOT_FOUND 127
#define FILENAME_BUFFER 32
#define INPUT_BUFFER 64
#define MAX_ARGS 16

typedef enum { PARSE_OK, PARSE_EXIT, PARSE_FAIL } ParseResult;
typedef enum { EXPECT_INPUT, EXPECT_OUTPUT, ARGUMENT } ParseState;
typedef enum { REDIR_NONE, REDIR_INPUT, REDIR_OUTPUT } Redirection;

// Takes parsed tokens
// Processes external command instructions
static int externalCommand(char **args, const char *input_filename,
                           const char *output_filename) {
  pid_t pid = fork();
  int status; // Status of child process

  if (pid < 0) {
    perror("fork failed");
    return 1;

  } else if (pid == 0) {
    // Child process
    int fd;
    if (input_filename != NULL) {
      // Input redirection
      fd = open(input_filename, O_RDONLY, 0644);
      if (fd < 0) {
        perror(input_filename);
        _exit(1);
      }
      if (dup2(fd, STDIN_FILENO) < 0) {
        perror("Input redirecton");
        _exit(1);
      }
      close(fd);
    }

    if (output_filename != NULL) {
      // Output redirection
      fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        perror(output_filename);
        _exit(1);
      }
      if (dup2(fd, STDOUT_FILENO) < 0) {
        perror("Output redirection");
        _exit(1);
      }
      close(fd);
    }
    execvp(args[0], args);
    perror("execvp failed");
    _exit(COMMAND_NOT_FOUND);

  } else {
    // Parent process
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
      return 0;
    return 1;
  }
}

// Takes parsed tokens and number of arguments
// Process change directory type (cd) instructions
// Return 0 on success, else -1
static int builtin_cd(char **args, int nargs) {
  int status;
  if (nargs == 1) {
    status = chdir(getenv("HOME"));
    if (status == -1)
      perror("cd");
  } else if (nargs == 2) {
    status = chdir(args[1]);
    if (status == -1)
      perror("cd");
  } else {
    status = -1;
    printf("Usage: cd <path>\n");
  }
  return status;
}

// Parses tokens according to the redirection operator.
// Returns the corresponding enum type, otherwise REDIR_NONE
static Redirection get_redirection(const char *token) {
  if (strcmp("<", token) == 0)
    return REDIR_INPUT;
  if (strcmp(">", token) == 0)
    return REDIR_OUTPUT;
  return REDIR_NONE;
}

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors
// Returns PARSE_FAIL if user enters a bad/unknown command
static ParseResult parse(char *inp) {
  // First pass: split input string into tokens
  char *delims = " \t\n";
  char *ptr;
  char *token = strtok_r(inp, delims, &ptr);
  char *args[MAX_ARGS];

  ParseState state = ARGUMENT;
  Redirection arg = REDIR_NONE;
  char output[FILENAME_BUFFER];
  char input[FILENAME_BUFFER];
  bool output_redir = false;
  bool input_redir = false;

  int nargs = 0;
  while (token != NULL && nargs < MAX_ARGS - 1) {
    arg = get_redirection(token);
    if (state == ARGUMENT) {
      switch (arg) {
      case REDIR_INPUT:
        if (input_redir) {
          printf("Error: multiple input streams detected\n");
          return PARSE_FAIL;
        }
        state = EXPECT_INPUT;
        input_redir = true;
        break;
      case REDIR_OUTPUT:
        if (output_redir) {
          printf("Error: multiple output streams detected\n");
          return PARSE_FAIL;
        }
        state = EXPECT_OUTPUT;
        output_redir = true;
        break;
      case REDIR_NONE:
        args[nargs++] = token;
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
      if (arg == REDIR_INPUT || arg == REDIR_OUTPUT) {
        printf("Error: expected filename after '%s'\n",
               state == EXPECT_INPUT ? "<" : ">");
        return PARSE_FAIL;
      }
      strcpy(state == EXPECT_INPUT ? input : output, token);
      state = ARGUMENT;
    }

    token = strtok_r(NULL, delims, &ptr);
  }

  if (output_redir && state == EXPECT_OUTPUT) {
    printf("Error: no specified output stream in redirection\n");
    return PARSE_FAIL;
  }
  if (input_redir && state == EXPECT_INPUT) {
    printf("Error: no specified input stream in redirection\n");
    return PARSE_FAIL;
  }
  args[nargs] = NULL;

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
    char *input_filename = input_redir ? input : NULL;
    char *output_filename = output_redir ? output : NULL;
    int error = externalCommand(args, input_filename, output_filename);
    if (error)
      return PARSE_FAIL;
    return PARSE_OK;
  }
}

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
