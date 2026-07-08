# Design decisions

# v1
`cd` changes the shell's working directory. It is implemented interally, as the
child process, if implemented externally via execvp, only the child process
changes directory before terminating.

`execvp` searches the user's PATH automatically, allowing user commands such as
`ls` and `pwd` to be executed without the full path.

`fork` creates a child process, which is used to handle the externally
implemented commands, because the shell must not terminate when running execvp.

# v2
`-D_POSIX_C_SOURCE=200809L` defines the POSIX interface, with 200809L
corresponding to the 2008 POSIX.1-2008 operating system interface. It exposes
POSIX functions such as `execvp`, `fork`, and `strtok_r`. This version balances
the availability of newer interfaces and source-code portability of the program,
so no compiler or platform-specific extensions are relied on.

Output redirection parsed by the shell before external command execution, using
an `output` string buffer, containing the parsed filename. This is to prevent
passing `> <filename>` tokens to execvp.