#include <stdio.h>
#include <stdlib.h>
#include "sym.h"
#include "ast.h"
#include "lex.h"
#include "parse.h"
#include "inst.h"
#include "gen.h"

int main(int argc, char **argv) {
	if (argc < 2) return -1;
	init_kwtable();

	lexer *lx = lexer_init(argv[1]);
	if (!lx) return 0;
	ast_n *tree = parse(lx);

	asm_f f;
	asmf_init(&f);
	gen_text(&f, tree);

	inst_n *n = f.text;
	while (n != NULL) {
		inst_str(n);
		n = n->next;
	}

	close_kwtable();
	return 0;
}
