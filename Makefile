CC=gcc
CFLAGS=-g -std=c99 -pedantic-errors
SRC=ast.c sym.c lex.c parse.c main.c
EXEC=scc

all:
	$(CC) $(SRC) $(CFLAGS) -o $(EXEC)

clean:
	rm $(EXEC) *o
