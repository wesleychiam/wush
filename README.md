# wush
A Unix shell implemented from scratch in C.

The program will repeatedly prompt the user until they wish to exit. It features
built-in commands such as exit and cd, and external commands, for example: ls,
pwd, whoami. 

To initialise the program, run in terminal:
    make
    cd build
    ./main

Example usage, after running the above commands:
    wush> cd bad_path
    cd: No such file or directory
    wush> ls   
    main  main.c  main.o
    wush> cd ..
    wush> ls
    LICENSE  MakeFile  README.md  build  src
    wush> exit

Planned for v1.0-v2.0 is to implement redirection, pipes, as well as adding
the MakeFile, which will utilise the build directory.
