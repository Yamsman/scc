#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sym.h"
#include "lex.h"
#include "ppd.h"
#include "util/map.h"

void ppd_defparams(lexer *lx) {
	lx->tgt->pos++;
	token *t = &lx->ahead;
	lex_next(lx, 0);
	if (t->nline) {
		printf("ERROR: Expected ')' before newline\n");
		return;
	}

	//Read parameters
	//TODO: Proper error handling
	while (t->type != TOK_RPR) {
		if (t->nline) {
			printf("ERROR: Unexpected end of line in macro parameter list\n");
			return;
		}

		//Read identifier
		lex_next(lx, 0);
		if (t->type == TOK_IDENT) {
		} else {
			printf("ERROR: Expected identifier before ...\n");
			break;
		}
	}
}

void ppd_define(lexer *lx) {
	//Read macro name
	lex_next(lx, 0);
	token mname = lex_peek(lx);
	if (mname.type != TOK_IDENT)
		printf("ERROR: Invalid macro name\n");

	//Check for and read macro parameter list
	if (*lx->tgt->pos == '(')
		ppd_defparams(lx);

	//Get bounds of macro body
	char *cur = lx->tgt->pos;
	char *start, *end;
	while (isspace(*cur) && *cur != '\n') cur++;
	if (cur == lx->tgt->pos && *cur != '\n')
		printf("ERROR: No whitespace after macro name\n");

	start = cur;
	while (*cur != '\n') {
		if (*cur == '\\' && *(cur+1) == '\n')
			cur++;
		cur++;
	}
	end = cur;
	while (isspace(*(cur-1))) cur--;

	//Copy body to buffer
	int len = cur - start;
	char *buf = NULL;
	if (len > 0) {
		buf = malloc(len+1);
		memcpy(buf, start, len);
		buf[len] = '\0';
		for (int i=0; i<len; i++) {
			if (buf[0] == '#' && buf[1] == '#' ||
			    buf[len-2] == '#' && buf[len-1] == '#')
				puts("'##' at beginning or end of macro");
		}
	}

	//Add to symtable
	symbol *sym = symtable_def(&lx->stb, mname.str, type_new(TYPE_MACRO));
	sym->mac_exp = buf;
	lx->tgt->pos = end;
	return;
}

void ppd_error(lexer *lx) {
	
}

void ppd_if(lexer *lx) {

}

void ppd_ifdef(lexer *lx) {
	
}

void ppd_ifndef(lexer *lx) {

}

void ppd_include(lexer *lx) {
	char *cur = lx->tgt->pos;
	while (*cur == ' ') cur++;

	//Get include type
	char type = *cur++;
	if (type != '<' && type != '"')
		printf("ERROR: #include expects <fname> or \"fname\"\n");

	//Get filename
	char *begin = cur;
	char endc = (type == '"') ? '"' : '>';
	while (*cur != endc) {
		if (*cur == '\n') {
			printf("ERROR: Expected '%c' before end of line\n", endc);
			return;
		} else if (*cur == '\0') {
			printf("ERROR: Expected '%c' before end of file\n", endc);
			return;
		}
		cur++;
	}
	int fnlen = (cur++) - begin;
	char *fname = malloc(fnlen+1);
	memcpy(fname, begin, fnlen);

	//Attempt to locate and open the included file
	FILE *input_f = NULL;
	char *fdir = malloc(fnlen + sizeof("/usr/include/") + 1);
	fdir[0] = '\0';
	if (type == '"') {
		char *slash = strrchr(fname, '/');
		if (!slash) {
			strcpy(fdir, "./");
			strcat(fdir, fname);
		} else {
			memcpy(fdir, fname, slash - fname);
			fdir[slash-fname] = '\0';
			strcat(fdir, slash);
		}
		if (lex_open_file(lx, fdir)) goto done;
	}
	fdir[0] = '\0';

	//Attempt system include directory
	strcpy(fdir, "/usr/include/");
	strcat(fdir, fname);
	if (!lex_open_file(lx, fdir)) {
		printf("ERROR: Unable to open file '%s': ", fname);
		fflush(stdout);
		perror(NULL);
		free(fdir);
	}
done:	free(fname);
	return;
}

void ppd_undef(lexer *lx) {
	//Read macro name
	lex_next(lx, 0);
	token mname = lex_peek(lx);
	if (mname.type != TOK_IDENT)
		printf("ERROR: Invalid macro name '%s'\n", mname.str);

	//Check the global scope
	symbol *s = map_get(&lx->stb.s_global->table, mname.str);
	if (s == NULL || s->type->kind != TYPE_MACRO)
		return;
	map_remove(&lx->stb.s_global->table, mname.str);

	return;
}
