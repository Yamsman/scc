#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "sym.h"
#include "ast.h"
#include "err.h"
#include "lex.h"
#include "parse.h"
#include "inst.h"
#include "gen.h"

void usage(int detail) {
	printf("USAGE: scc [OPTION]... [FILE]...\n");
	if (!detail) return;
	printf("\nOptions:\n");
	printf(" -h\t\tprint this help message\n");
	printf(" -o [outfile]\tspecifies the name of the output file\n");
	printf(" -e\t\tstop after preprocessing phase (-E for detailed output)\n"); 
	printf(" -l\t\tstop after lexical analysis phase (-L for detailed output)\n");
	printf(" -p\t\tstop after parsing phase\n\n");
	return;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(0);
		return -1;
	}

	/*
	 * Handle option parsing
	 */
	int opt, hflag = 0, eflag = 0, lflag = 0, pflag = 0;
	char *ofname = "out.s";
	while ((opt = getopt(argc, argv, "ho:eElLp")) != -1) {
		switch (opt) {
			case 'h': hflag = 1;		break;
			case 'o': ofname = optarg;	break;
			case 'e': eflag = 1;		break;
			case 'E': eflag = 2;		break;
			case 'l': lflag = 1;		break;
			case 'L': lflag = 2;		break;
			case 'p': pflag = 1;		break;
			default:
				c_error(NULL, "Unknown option '%c'\n", opt);
				break;
		}
	}
	if (hflag) {
		usage(1);
		return 0;
	}

	//Compile each file
	init_kwtable();
	for (int i=optind; i<argc; i++) {
		//Get filename
		char *fname = malloc(strlen(argv[i])+1);
		strncpy(fname, argv[i], strlen(argv[i]));
		fname[strlen(argv[i])] = '\0';

		/*
		 * Initialize lexer
		 */
		lexer *lx = lexer_init(fname);
		if (!lx) continue;

		//Stop early and print information for eflag/lflag
		if (eflag) {
			lexer_debug_pprc(lx, eflag-1);
			goto lclean;
		} else if (lflag) {
			lexer_debug(lx, lflag-1);
			goto lclean;
		}

		/*
		 * Perform parsing
		 */
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
lclean:		lexer_close(lx);
	}
	if (c_errflag)
		puts("Compilation terminated.");
	close_kwtable();

	return 0;
}
