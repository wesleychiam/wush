#ifndef EXECUTION_H
#define EXECUTION_H

#include "parser.h"

int external_command(char **args, const char *input_filename,
                     const char *output_filename, Redirection output_redir);
int external_pipe(char **args, const char *input_filename,
                  const char *output_filename, Redirection output_redir,
                  int pipe_start);
int builtin_cd(char **args, int nargs);
int builtin_here_doc(char **args, const char *here_doc_delim,
                     const char *output_filename, Redirection output_redir);
#endif