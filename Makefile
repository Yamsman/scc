CC=gcc
CFLAGS=-g -std=c99 -pedantic-errors
SRC=$(wildcard src/*.c) src/util/map.c src/util/vector.c
EXEC=scc

all:
	$(CC) $(SRC) $(CFLAGS) -o $(EXEC)
clean:
	rm $(EXEC) *.o
