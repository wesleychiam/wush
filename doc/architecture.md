# Overview
wush is divided into the following related files:
`main.c` - shell initialisation and user prompt
`parser.c` - syntax validation command interpretation, and tokenisation.
`execution.c` - file descriptor handling and command execution.
`Makefile` - linking, compiling, formatting, and cleaning commands.

# Command flow
The flow below reads like a decision tree:

- `main`
- user enters command
- `parser`
    1: `external_command`
        - `prepare_input_fd` (`open_input_file`)
        - `prepare_output_fd` (`open_output_file`)
            1: `exec_pipe`
                - `run_child` for each stage
            2: `exec_cmd`
                - `run_child` once
    2: built-in handler
        1: `exit`
        2: `cd`
    3: parse fail

All paths eventually return control to the main shell loop.

# Ownership
`main` takes ownership of shell initialisation and prompt-level signal handling.
`parser` takes ownership of command representation.
`external_command` prepares the input and output descriptors, then transfers
ownership to `exec_cmd` or `exec_pipe`.
`run_child` takes ownership of the descriptors passed to it in the child
process.

