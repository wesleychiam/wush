# Design decisions

`cd` changes the shell's working directory. It is implemented interally, as the
child process, if implemented externally via execvp, only the child process
changes directory before terminating.

`execvp` searches the user's PATH automatically, allowing user commands such as
`ls` and `pwd` to be executed without the full path.

`fork` creates a child process, which is used to handle the externally
implemented commands, because the shell must not terminate when running execvp.