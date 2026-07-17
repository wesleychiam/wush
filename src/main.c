#include <assert.h>
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
typedef enum {
  EXPECT_INPUT,
  EXPECT_OUTPUT,
  EXPECT_OUTPUT_APPEND,
  ARGUMENT
} ParseState;
typedef enum {
  REDIR_NONE,
  REDIR_INPUT,
  REDIR_OUTPUT,
  REDIR_OUTPUT_APPEND
} Redirection;

// Takes command arguments, input, and output file descriptors
// Constraint: fd > 2 (owned) or fd = -1 (no change i.e. STDIN, STDOUT)
// Constraint: if input_fd, output_fd > 2, input_fd != output_fd
// Automatically closes owned file descriptors and exits child on failure
static void run_child(char **args, int input_fd, int output_fd) {
  assert(input_fd > 2 || input_fd == -1);
  assert(output_fd > 2 || output_fd == -1);
  assert((input_fd == -1 || output_fd == -1) || input_fd != output_fd);

  if (input_fd > 2) {
    if (dup2(input_fd, STDIN_FILENO) < 0) {
      perror("Child input redirection");
      _exit(1);
    }
    close(input_fd);
  }

  if (output_fd > 2) {
    if (dup2(output_fd, STDOUT_FILENO) < 0) {
      perror("Child output redirection");
      _exit(1);
    }
    close(output_fd);
  }

  execvp(args[0], args);
  perror("Child execvp");
  _exit(COMMAND_NOT_FOUND);
}

// Takes parsed tokens
// Processes external command instructions
// Returns non-zero on failure, 0 on success
static int external_command(char **args, const char *input_filename,
                            const char *output_filename,
                            Redirection output_redir) {
  pid_t pid = fork();
  int status; // Status of child process

  if (pid < 0) {
    perror("fork failed");
    return 1;

  } else if (pid == 0) {
    // Child process
    int input_fd;
    if (input_filename != NULL) {
      if ((input_fd = open(input_filename, O_RDONLY, 0644)) < 0) {
        perror(input_filename);
        _exit(1);
      }
    } else {
      input_fd = -1;
    }
    
    int output_fd;
    if (output_filename != NULL) {
      switch (output_redir) {
      case REDIR_OUTPUT_APPEND:
        output_fd = open(output_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        break;
      case REDIR_OUTPUT:
        output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        break;
      default:
        printf("Error: invalid state reached\n");
        abort();
      }
      if (output_fd < 0) {
        perror(output_filename);
        _exit(1);
      }
    } else {
      output_fd = -1;
    }

    run_child(args, input_fd, output_fd);
    // run_child must terminate the child and therefore does not return
    abort();

  } else {
    // Parent process
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
      return WEXITSTATUS(status);
    return 1;
  }
}

// Takes parsed tokens
// Processes any instruction involving pipe
// Returns non-zero on failure, 0 on success
static int external_pipe(char **args, const char *input_filename,
                         const char *output_filename, Redirection output_redir,
                         int pipe_start) {
  // Create pipe
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("pipe failed");
    return 1;
  }

  // Left command parent
  pid_t l_pid = fork();
  int l_status;
  if (l_pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    perror("pipe failed");
    return 1;
  } else if (l_pid == 0) {
    // Left child
    // Close unused read end
    close(pipefd[0]); 
    // Left child logic
    int input_fd;
    if (input_filename != NULL) {
      if ((input_fd = open(input_filename, O_RDONLY, 0644)) < 0) {
        perror(input_filename);
        _exit(1);
      }
    } else {
      input_fd = -1;
    }
    run_child(args, input_fd, pipefd[1]);
    // run_child must terminate the child and therefore does not return
    abort();
  }
  // Right command parent
  pid_t r_pid = fork();
  int r_status;
  if (r_pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    perror("pipe failed");
    waitpid(l_pid, &l_status, 0);
    return 1;
  } else if (r_pid == 0) {
    // Right child
    // Close unused write end
    close(pipefd[1]);
    // Right child logic
    int output_fd;
    if (output_filename != NULL) {
      switch (output_redir) {
        case REDIR_OUTPUT_APPEND:
          output_fd = open(output_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
          break;
        case REDIR_OUTPUT:
          output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
          break;
        default:
          printf("Error: invalid state reached\n");
          abort();
        }
      if (output_fd < 0) {
        perror(output_filename);
        _exit(1);
      }
    } else {
      output_fd = -1;
    }
    run_child(args + pipe_start, pipefd[0], output_fd);
    // run_child must terminate the child and therefore does not return
    abort();
  }
  // Parent
  close(pipefd[0]);
  close(pipefd[1]);
  waitpid(l_pid, &l_status, 0);
  waitpid(r_pid, &r_status, 0);

  if (WIFEXITED(l_status) && WIFEXITED(r_status)) {
    return WEXITSTATUS(r_status);
  }
  return 1;
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
  if (strcmp(">>", token) == 0)
    return REDIR_OUTPUT_APPEND;
  return REDIR_NONE;
}

// Takes input string, representing prompted user input
// Decomposes string into corresponding instruction(s)
// Delegates to respective processes or handles errors
// Returns PARSE_FAIL if user enters a bad/unknown command
static ParseResult parse(char *inp) {
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
      strcpy(state == EXPECT_INPUT ? input : output, token);
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
    char *input_filename = input_redir == REDIR_NONE ? NULL : input;
    char *output_filename = output_redir == REDIR_NONE ? NULL : output;

    int error;
    if (pipe_found) {
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
