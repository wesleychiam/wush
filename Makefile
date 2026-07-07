CC = gcc
CFLAGS = -std=c99 -g -Wall
all: build/main 
build/main: build/main.o
	$(CC) $(CFLAGS) build/main.o -o build/main 
build/main.o: src/main.c
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o
clean:
	rm -f build/*.o build/main
	
