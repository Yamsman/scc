#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include "ast.h"
#include "lex.h"
#include "parse.h"
#include "inst.h"
#include "gen.h"

int main(int argc, char **argv) {
	if (argc < 2) return -1;
	char *fname = malloc(strlen(argv[1])+1);
	strncpy(fname, argv[1], strlen(argv[1]));

	//Initialize lexer
	init_kwtable();
	lexer *lx = lexer_init(fname);
	if (!lx) return -1;

	//Parse the input
	ast_n *tree = parse(lx);

	//Generate assembly
	asm_f f;
	asmf_init(&f);
	gen_text(&f, tree);

	//Print instructions
	inst_n *n = f.text;
	while (n != NULL) {
		inst_str(n);
		n = n->next;
	}

	//Clean up
	asmf_close(&f);
	astn_del(tree);
	lexer_close(lx);
	close_kwtable();

	return 0;
}
