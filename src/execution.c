#include "execution.h"
#include "parser.h"

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

// Takes an input filename and opens the file
// On success return the corresponding file descriptor
// On failure return -1
// Constraint: filename is defined
static int open_input_file(const char *input_filename) {
  assert(input_filename != NULL);

  int input_fd;
  if ((input_fd = open(input_filename, O_RDONLY)) < 0) {
    perror(input_filename);
    return -1;
  }

  return input_fd;
}

// Takes an output filename and a redirection
// Opens the file with access modes depending on the redirection
// On success return the corresponding file descriptor
// On failure return -1
// Constraint: filename is defined
static int open_output_file(const char *output_filename,
                            Redirection output_redir) {
  assert(output_filename != NULL);

  int output_fd;
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
    return -1;
  }
  return output_fd;
}

// Takes parsed tokens
// Processes external command instructions
// Returns non-zero on failure, 0 on success
int external_command(char **args, const char *input_filename,
                     const char *output_filename, Redirection output_redir) {
  pid_t pid = fork();
  int status; // Status of child process

  if (pid < 0) {
    perror("fork failed");
    return 1;

  } else if (pid == 0) {
    // Child process
    int input_fd = -1;
    if (input_filename != NULL) {
      if ((input_fd = open_input_file(input_filename)) < 0) {
        // Invalid file descriptor
        _exit(1);
      }
    }

    int output_fd = -1;
    if (output_filename != NULL) {
      if ((output_fd = open_output_file(output_filename, output_redir)) < 0) {
        // Invalid file descriptor
        _exit(1);
      }
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
int external_pipe(char **args, const char *input_filename,
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
    int input_fd = -1;
    if (input_filename != NULL) {
      if ((input_fd = open_input_file(input_filename)) < 0) {
        // Invalid file descriptor
        _exit(1);
      }
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
    int output_fd = -1;
    if (output_filename != NULL) {
      if ((output_fd = open_output_file(output_filename, output_redir)) < 0) {
        // Invalid file descriptor
        _exit(1);
      }
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
int builtin_cd(char **args, int nargs) {
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

static int exec_cmd(char **args, int input_fd, int output_fd) {
  pid_t pid = fork();
  int status;
  if (pid < 0) {
    perror("exec_cmd");
    _exit(1);
  } else if (pid == 0) {
    // Child logic
    run_child(args, input_fd, output_fd);
    // run_child must terminate and there does not return
    printf("Error: failed to terminate child\n");
    abort();
  } else {
    close(input_fd);
    close(output_fd);
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
    return 1;
  }
}

int builtin_here_doc(char **args, const char *here_doc_delim,
                     const char *output_filename, Redirection output_redir) {
  char path[] = "/tmp/heredocbuffer.XXXXXX";

  int input_fd = mkstemp(path);
  if (input_fd < 3) {
    // The program relies on STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
    // reserving file descriptors 0-2, and -1 is returned for failed mkstemp
    perror("mkstemp");
    return -1;
  }

  int output_fd = -1;
  if (output_filename != NULL) {
    if ((output_fd = open_output_file(output_filename, output_redir)) < 3) {
      // The program relies on STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO
      // reserving file descriptors 0-2, and -1 is returned for a failed
      // open_output_file call
      close(input_fd);
      return -1;
    }
  }

  char buffer[INPUT_BUFFER];
  int n = strlen(here_doc_delim);
  while (true) {
    printf("> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
      break;
    if (strncmp(buffer, here_doc_delim, n) == 0) {
      break;
    } else {
      write(input_fd, buffer, strlen(buffer));
    }
  }

  unlink(path);
  if (lseek(input_fd, 0, SEEK_SET) == -1) {
    perror("lseek");
    if (input_fd > 2)
      close(input_fd);
    if (output_fd > 2)
      close(output_fd);
    return -1;
  }

  return exec_cmd(args, input_fd, output_fd);
}
