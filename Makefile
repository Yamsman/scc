CC=gcc
CFLAGS=-g -std=c99 -pedantic-errors
LDFLAGS=-lm
SRC=$(wildcard src/*.c) src/util/map.c src/util/vector.c
EXEC=scc

all:
	$(CC) $(SRC) $(CFLAGS) $(LDFLAGS) -o $(EXEC)
clean:
	rm $(EXEC) *.o
