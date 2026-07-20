#ifndef EXECUTION_H
#define EXECUTION_H

#include "parser.h"

#include <stdbool.h>

int external_command(char **args, const char *input_filename,
                     const char *here_doc_delim, Redirection input_redir,
                     const char *output_filename, Redirection output_redir,
                     bool pipe_found, int pipe_index);
int builtin_cd(char **args, int nargs);
int builtin_here_doc(char **args, const char *here_doc_delim,
                     const char *output_filename, Redirection output_redir);
#endif