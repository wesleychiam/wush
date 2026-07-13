CC = gcc
CFLAGS = -std=c99 -g -Wall -D_POSIX_C_SOURCE=200809L
all: build/main 
build/main: build/main.o
	$(CC) $(CFLAGS) build/main.o -o build/main 
build/main.o: src/main.c
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o
clean:
	rm -f build/* build/main
format:
	clang-format -i src/*.c