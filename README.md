# wush
A Unix shell implemented from scratch in C.

## Description
The program will repeatedly prompt the user until they wish to exit. It features
built-in commands such as exit and cd, and external commands, for example: `ls`,
`pwd`, `whoami`. It also has redirection capabilities using `<`, `>`, `>>`, and
the here-document `<<`, as well as pipes `|` between two commands.

## Startup
To initialise the program, run in terminal:
```sh
    make
    cd build
    ./main
```

## Example usages
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

## Limitations
A limitation of the shell is the parser requiring commands to be separated by
whitespace, so running a command such as `cat<file1.txt` will result in an
`execvp` failure and not execute.

The shell relies on standard input, standard output, and standard error occupy
descriptors 0-2 for redirection functionality.

## Plans
- Signal handling, such as `^C`.