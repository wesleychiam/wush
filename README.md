# wush
A Unix shell implemented from scratch in C.

## Description
The program will repeatedly prompt the user until they wish to exit. It features
built-in commands such as exit and cd, and external commands, for example: `ls`,
`pwd`, `whoami`. It also has redirection capabilities using `<`, `>`, `>>`, and
the here-document `<<`, as well as pipes `|` between commands. The shell also
has basic support for signal handling, such as when the user presses `Ctrl+C`:
at the prompt, it clears the current input and shows a fresh prompt; and during
a foreground command or pipeline, it terminates that job without terminating the
shell.

## Startup
To initialise the program, run in terminal:
```sh
    make
    cd build
    ./main
```

## Usage examples
Example usage of external commands, after running the above commands:
```sh
    wush> cd bad_path
    cd: No such file or directory
    wush> ls   
    main  main.c  main.o
    wush> cd ..
    wush> ls
    LICENSE  Makefile  README.md  build  src
    wush> exit
```

Example usage of redirection and simple pipe commands:
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

Example usage of multi-stage pipelines:
```sh
    wush> printf pear\napple\nbanana\n
    pear
    apple
    banana
    wush> printf pear\napple\nbanana\n | sort
    apple
    banana
    pear
    wush> printf pear\napple\nbanana\n | sort | head -n 1
    apple
```

Example usage of signal-handling:

```sh
    wush> echo hello^C
    wush> 
``` 
(Partially typed input)

```sh
    wush> sleep 10   
    ^Cwush> 
```
(Terminated mid-sleep job)

## Limitations
The shell relies on the convention that standard input, standard output, and
standard error occupy descriptors 0-2 for redirection functionality.

There is also a small race condition before terminal handoff, mentioned in the
`design-decisions.md` file.
