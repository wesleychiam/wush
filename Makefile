CC = gcc
CFLAGS = -std=c99 -g -Wall -D_POSIX_C_SOURCE=200809L

all: build/main

build/main: build/main.o build/parser.o build/execution.o
	$(CC) $(CFLAGS) build/main.o build/parser.o build/execution.o -o build/main

build/parser.o: src/parser.c src/parser.h src/execution.h
	$(CC) $(CFLAGS) -c src/parser.c -o build/parser.o
build/execution.o: src/execution.c src/execution.h src/parser.h
	$(CC) $(CFLAGS) -c src/execution.c -o build/execution.o
build/main.o: src/main.c src/parser.h
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

clean:
	rm -f build/*
format:
	clang-format -i src/*.c src/*.h
