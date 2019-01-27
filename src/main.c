#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include "ast.h"
#include "err.h"
#include "lex.h"
#include "parse.h"
#include "inst.h"
#include "gen.h"

int main(int argc, char **argv) {
	if (argc < 2) return -1;
	init_kwtable();
	for (int i=1; i<argc; i++) {
		//Get filename
		char *fname = malloc(strlen(argv[i])+1);
		strncpy(fname, argv[i], strlen(argv[i]));
		fname[strlen(argv[i])] = '\0';

		//Initialize lexer
		lexer *lx = lexer_init(fname);
		if (!lx)
			continue;

		//Parse the input
		ast_n *tree = parse(lx);
		if (c_errflag)
			goto pclean;

		//Generate assembly
		asm_f f;
		asmf_init(&f);
		gen_text(&f, tree);
		if (c_errflag)
			goto aclean;

		//Print instructions
		inst_n *n = f.text;
		while (n != NULL) {
			inst_str(n);
			n = n->next;
		}

		//Clean up
aclean:		asmf_close(&f);
pclean:		astn_del(tree);
		lexer_close(lx);
	}
	if (c_errflag)
		puts("Compilation terminated.");
	close_kwtable();

	return 0;
}
