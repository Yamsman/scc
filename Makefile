CC=gcc
CFLAGS=-g -std=c99 -pedantic-errors
SRC=$(wildcard src/*.c)
EXEC=scc

all:
	$(CC) $(SRC) $(CFLAGS) -o $(EXEC)
clean:
	rm $(EXEC) *.o
