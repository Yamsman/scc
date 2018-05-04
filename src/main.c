#include <stdio.h>
#include <stdlib.h>
#include "lex.h"
#include "sym.h"
#include "parse.h"

int main(int argc, char **argv) {
	if (argc < 2) return -1;
	FILE *src_f = fopen(argv[1], "r");
	fseek(src_f, 0, SEEK_END);
	int len = ftell(src_f);
	lx = malloc(len+1);
	fseek(src_f, 0, SEEK_SET);
	fread(lx, len, 1, src_f);

	symtable_init();
	parse();

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
