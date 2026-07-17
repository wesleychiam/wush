# Design decisions

# v1
`cd` changes the shell's working directory. It is implemented internally, as the
child process, if implemented externally via execvp, only the child process
changes directory before terminating.

`execvp` searches the user's PATH automatically, allowing user commands such as
`ls` and `pwd` to be executed without the full path.

`fork` creates a child process, which is used to handle the externally
implemented commands, because the shell must not terminate when running execvp.

# v2
`-D_POSIX_C_SOURCE=200809L` defines the POSIX interface, with `200809L`
corresponding to the 2008 POSIX.1-2008 operating system interface. It exposes
POSIX functions such as `execvp`, `fork`, and `strtok_r`. This version balances
the availability of newer interfaces and source-code portability of the program,
so no compiler or platform-specific extensions are relied on.

Output redirection parsed by the shell before external command execution, using
an `output` string buffer, containing the parsed filename. This is to prevent
passing `> <filename>` tokens to execvp.
On input such as `doesnotexist > bad.txt`, creating an empty file and printing
an error message is intentional; validating after `execvp` is difficult as
`execvp` terminates on success, and duplicating `execvp` may risk TOCTOU race
conditions.  

Enum types `ParseState` and `Redirection` were created to assist parsing clearly
and support future redirection tokens. `ParseState` helps with tracking whether
a filename is expected, and to filter out potential syntax errors.

For pipe instructions, `pipe_start` was chosen to indicate the index of the next
instruction, which will simplify the start of `external_pipe`. Checking that the
`pipe_start` in parser is not equal to 1 is due to `pipe_start = nargs` called
after `nargs++`, so the index will never be 0 unless there is no pipe token.
For a single pipeline, the parser replaces the pipe token with `NULL`, producing
two adjacent `argv` sequences in the same array. `pipe_start` stores the index
where the second command begins. This avoids copying tokens into separate arrays
and may also support extending the parser to multiple pipeline stages later.

`run_child` configures and executes an already-forked child process for
`external_command` and `external_pipe`. The helper reduces duplicate `dup2`,
`close`, and `execvp` logic.
Its contract requires each I/O file descriptor to be either `-1` or `>2`. Values
greater than 2 are treated as owned by the helper: they are duplicated onto
`STDIN_FILENO` or `STDOUT_FILENO` and closed. If both descriptors are owned,
they must be unique. A value of `-1` means that the corresponding stream should
not be changed.
It is not responsible for opening the file, creating child processes, closing
unrelated pipe ends, and waiting for children. It was designed to be unaware of
whether its descriptors are from pipes or files for simplicity - abstracting
pipe details improves its portability; especially where it is not needed for
`external_command`.
The function does not return during normal operation. On success, `execvp`
replaces the child's program image. On failure, the child reports an error and
terminates via `_exit`, reducing cleanup for the parent.