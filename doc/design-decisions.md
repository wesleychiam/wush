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

Output redirection is parsed by the shell before external command execution, 
using an `output` string buffer, containing the parsed filename. This is to 
prevent passing `> <filename>` tokens to execvp.
On input such as `doesnotexist > bad.txt`, creating an empty file and printing
an error message is intentional; validating after `execvp` is difficult as
`execvp` terminates on success, and duplicating `execvp` may risk TOCTOU race
conditions.  

Enum types `ParseState` and `Redirection` were created to assist parsing clearly
and support future redirection tokens. `ParseState` helps with tracking whether
a filename is expected, and to filter out syntax errors.

For pipe instructions, `pipe_start` was chosen to indicate the index of the next
instruction, which will simplify the start of `exec_pipe`. Checking that the
`pipe_start` in parser is not equal to 1 is due to `pipe_start = nargs` called
after `nargs++`, so the index will never be 0 unless there is no pipe token.
For a single pipeline, the parser replaces the pipe token with `NULL`, producing
two adjacent `argv` sequences in the same array. `pipe_start` stores the index
where the second command begins. This avoids copying tokens of each pipe command 
into separate arrays and may also support extending the parser to multiple 
pipeline stages later.

`run_child` configures and executes an already-forked child process for
`exec_pipe` and `exec_cmd`. The helper reduces duplicate `dup2`, `close`, and
`execvp` logic.
Its contract requires each I/O file descriptor to be either `-1` or `>2`. Values
greater than 2 are treated as owned by the helper: they are duplicated onto
`STDIN_FILENO` or `STDOUT_FILENO` and closed. If both descriptors are owned,
they must be unique. A value of `-1` means that the corresponding stream should
not be changed.
It is not responsible for opening the file, creating child processes, closing
unrelated pipe ends, and waiting for children. It was designed to be unaware of
whether its descriptors are from pipes or files for simplicity - abstracting
pipe details allows it to be used by commands that do not need pipes.
The function does not return during normal operation. On success, `execvp`
replaces the child's program image. On failure, the child reports an error and
terminates via `_exit`, reducing cleanup for the parent.

`open_input_file` and `open_output_file` are a pair of functions created to open
file descriptors corresponding to a filename and redirection. The purpose of 
these functions is to separate the responsibility of retrieving file descriptors
from `exec_pipe` and `exec_cmd`. Their main purpose is to fork and manage child
processes.

Their contract requires the filename to be defined, because it is otherwise
difficult to return a suitable error status or a file descriptor to show
no redirection (`-1`). Therefore it should be only called when a file genuinely
needs opening. 
This simplifies the contract of the helper because it only returns -1 
on failure or a descriptor on success. The failure is propagated through the
preparation helper to `external_command`, allowing the shell to report the error
and reutrn to its prompt without forking a child.

`external_command` was refactored to streamline the duplicate file descriptor
preparation logic in external and pipe commands. It is called by parser, and
calls the corresponding function after setting the appropriate file descriptor
values using the filename and descriptor arguments.
It is done this way to represent the shared responsibility of an external
command, and to let the parser call a singular function instead of adding
complexity to the parser for execution logic that it is not responsible for.

`exec_cmd` and `exec_pipe` are the functions that are delegated to, from the
`external_command` control. Their responsibility is to execute the instruction
by forks and `run_child` before closing relevant file descriptors. This
decomposition ensures that their logic is joined by `external_command` up until
the fork process.

Once `external_command` passes the prepared descriptors to `exec_cmd` or
`exec_pipe`, the selected execution function takes ownership of them. It closes
the parent copies on both normal and failure paths, while each child closes any
inherited descriptors that it does not use.

Here-document contents are collected by the parent process and stored in a
temporary file using `mkstemp`. The file is eventually unlinked and rewound with
`lseek` before its execution, allowing its descriptor to be handled the same way
as ordinary input redirection. The implementation of here-documents through
`mkstemp` instead of pipes to avoid extra closing rules and responsibilities on
top of the current implementation. A secondary reason was to learn and apply a
new tool - `mkstemp`.

`prepare_input_fd` and `prepare_output_fd` are another pair of helper functions
introduced to reduce file duplication in pipe handling and external commands.
Their contract requires the filenames (or delimiters for a here-document) to be
non-null. Null filenames/no redirection are handled by their caller function 
`external_command`. They open the file and return `-1` indicating failure or a
descriptor `>2` indicating success.

# v3
`exec_pipe` now accepts an array of integers `stage_start` instead of a singular
integer `pipe_start` to represent the index at the start of each stage in the
pipeline in the shared `args` array.
This is compatible with the `args` array, as it allows `run_child` calls to 
accept `args + stage_start[i]` instead of other approaches; for example,
individual argument arrays for each pipeline, which would overlap with the
values in `args` and makes the pipe chaining difficult.
Furthermore, to keep pipe chaining simple, a "rolling-pipe" approach was used,
where a new pipe is created for every connection each stage, but the same array
`pipefd` is reused. This is favoured over creating or storing new pipes for each
stage as the closing logic can get messy for a command with many pipes.
The parent process forks all stages before waiting for any child process. It
stores child processes in a `pids` array, and the parent calls `waitpid` to wait
for every child process after the primary pipeline `for` loop. This prevents a
deadlock from occuring where the parent blocks the current stage, due to some
corresponding file descriptor only being closed in a future stage. It scans each
child in the same order that they are forked, which enables a `status` value to
be read and overwritten to by the parent.
Since it only executes pipe instructions, its contract asserts that there is at
least two stages in the command. This keeps the function to its intended usage.
It retains the responsibility of closing I/O file descriptors and handling
execution errors. It is not responsible for exiting, unless aborting or in the
child, in order to propagate the error to `external_command`.
Once an I/O file descriptor is closed, it is set to `-1`. This prevents the
child in the `for` loop closing a file descriptor that is already closed, which
causes an error in `run_child`, leading to a crash in a valid pipe command.
There is also a possibility of the child reading the closed I/O descriptor
number, and calling `close` closes a different process that has the same
descriptor number, where a later `pipe` call reuses the same descriptor number.
As per convention, the child status of the final stage is returned as the
overall status code if the entire operation succeeds. Otherwise, it will return 
`1` if any `waitpid` call fails, as the status codes cannot be reliably read.

# v4
`strtok_erq` is a reentrant version of `strtok` with support for escapes and
quotes. Replacing the `strtok_r` function with a custom one keeps most of the
current parser logic unchanged. It uses a `read_ptr` and `write_ptr` to parse
the tokens because the output size will be less than the input if there are
quotes or escapes used. The invariant `read_ptr >= write_ptr` is derived by
this. As it sets `saveptr` similarly to `strtok_r` and only modifies within
bounds of the input string, the function is safe. 