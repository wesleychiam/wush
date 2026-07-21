# wush
A Unix shell implemented from scratch in C.

## Description
The program will repeatedly prompt the user until they wish to exit. It features
built-in commands such as exit and cd, and external commands, for example: `ls`,
`pwd`, `whoami`. It also has redirection capabilities using `<`, `>`, `>>`, and
the here-document `<<`, as well as a single pipe `|` between two commands.

## Startup
To initialise the program, run in terminal:
```sh
    make
    cd build
    ./main
```

## Example usage
Example usage of external commands, after running the above commands:
```sh
    wush> cd bad_path
    cd: No such file or directory
    wush> ls   
    main  main.c  main.o
    wush> cd ..
    wush> ls
    LICENSE  MakeFile  README.md  build  src
    wush> exit
```

Example usage of redirection and pipe commands:
```sh
    wush> echo hello world > file1.txt
    wush> echo line 2 >> file1.txt
    wush> cat < file1.txt | wc -l 
    2
    wush> cat << EOF >> file1.txt
    > line 3   
    > another line
    > EOF
    wush> cat < file1.txt | wc -l
    4
```

## Limitations
The shell only supports one pipe `|` between two commands, so commands with
multiple pipes are currently rejected by the parser, returning an error message.

Another limitation of the shell is the parser requiring commands to be separated
by whitespace, so running a command such as `cat<file1.txt` will result in an
`execvp` failure and not execute.

The shell relies on `STDIN`, `STDOUT`, and `STDERR` open and reserving file
descriptors 0-2 for redirection functionality.

## Plans
Main planned feature for v3 is support for pipelines containing more than two
commands.