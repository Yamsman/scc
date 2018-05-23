#include <stdio.h>
#include <stdlib.h>
#include "lex.h"
#include "sym.h"
#include "parse.h"

int main(int argc, char **argv) {
	if (argc < 2) return -1;
	lexer *lx = lexer_open(argv[1]);

	symtable_init();
	parse(lx);

	//symtable_add("test");
	//printf("ss1: %p\n", symtable_search("test", SCOPE_CURRENT));
	/*
	for (int i=0; i<10; i++) {
		token t = lex_peek();
		printf("%i\n", t.type);
		lex_adv();
	}
	*/

	symtable_close();
	return 0;
}
