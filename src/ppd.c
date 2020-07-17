#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sym.h"
#include "err.h"
#include "lex.h"
#include "eval.h"
#include "ppd.h"
#include "util/vector.h"
#include "util/map.h"

//All ppd_* functions have macro expansion disabled by default (starting from lex_adv)

void ppd_defparams(lexer *lx, s_type *mtype) {
	token *t = &lx->ahead;
	s_pos *loc = &lx->tgt->loc;
	lex_advc(lx);
	lex_next(lx);
	if (t->nline) {
		c_error(loc, "Expected ')' before newline\n");
		return;
	}

	//Read parameters
	while (t->type != TOK_RPR) {
		if (t->nline) {
			c_error(loc, "Unexpected end of line in macro parameter list\n");
			return;
		}

		//Read identifier
		if (t->type != TOK_IDENT) {
			c_error(loc, "Missing identifier in macro parameter list\n");
			break;
		}
		vector_add(&mtype->param, param_new(type_new(TYPE_MACRO), t->dat.sval));
		lex_next(lx);

		//Read comma
		if (t->type != TOK_CMM && t->type != TOK_RPR) {
			c_error(loc, "Missing comma in macro parameter list\n");
			break;
		} else if (t->type == TOK_CMM) {
			lex_next(lx);
		}
	}
}

void ppd_define(lexer *lx) {
	s_pos *loc = &lx->tgt->loc;

	//Read macro name
	token mname;
	lex_wspace(lx);
	lex_ident(lx, &mname);
	int mlen = strlen(mname.dat.sval);
	if (mlen == 0)
		c_error(loc, "Invalid macro name\n");

	//Check for and read macro parameter list
	s_type *mtype = type_new(TYPE_MACRO);
	if (lex_cur(lx) == '(')
		ppd_defparams(lx, mtype);

	//Check for whitespace
	int wspace = 0;
	char cur = lex_cur(lx);
	while (isspace(cur) && cur != '\n') {
		wspace = 1;
		cur = lex_advc(lx);
	}
	if (!wspace && cur != '\n')
		c_error(loc, "No whitespace after macro name\n");

	//Copy macro body to buffer
	int len = 0;
	int bsize = 256;
	char *buf = malloc(bsize);
	while (cur != '\n') {
		//Resize if needed
		if (len+2 == bsize) {
			bsize *= 2;
			buf = realloc(buf, bsize);
		}

		//Copy character to buffer
		buf[len++] = cur;
		cur = lex_advc(lx);
	}
	buf[len] = '\0';

	//Check for ## on edges of macro
	if (len >= 2) {
		if (buf[0] == '#' && buf[1] == '#' ||
		    buf[len-2] == '#' && buf[len-1] == '#') {
			c_error(loc, "'##' at beginning or end of macro\n");
			free(mname.dat.sval);
			free(buf);
			return;
		}
	}

	//Add to symtable
	symbol *sym = symtable_def(&lx->stb, mname.dat.sval, mtype, &mname.loc);
	sym->mac_exp = buf;
	return;
}

void ppd_error(lexer *lx, int is_warn) {
	s_pos loc = lx->tgt->loc;

	//Read message
	int len = 0;
	int bsize = 256;
	char cur = lex_cur(lx);
	char *buf = malloc(bsize);
	while (isspace(cur)) cur = lex_advc(lx);
       	while (cur != '\n') {
		//Resize if needed
		if (len+2 == bsize) {
			bsize *= 2;
			buf = realloc(buf, bsize);
		}

		//Copy character to buffer
		buf[len++] = cur;
		cur = lex_advc(lx);
	}
	buf[len] = '\0';

	//Print error/warning
	if (is_warn) c_warn(&loc, "%s\n", buf);
	else c_error(&loc, "%s\n", buf);

	free(buf);
	return;
}

void ppd_include(lexer *lx) {
	char cur = lex_cur(lx);
	s_pos *loc = &lx->tgt->loc;
	while (cur == ' ') cur = lex_advc(lx);

	//Get include type
	char type = cur;
	if (type != '<' && type != '"')
		c_error(loc, "#include expects <fname> or \"fname\"\n");
	cur = lex_advc(lx);

	//Get filename
	int fnlen = 0;
	int bsize = 256;
	char *fname = malloc(bsize);
	char endc = (type == '"') ? '"' : '>';
	while (cur != endc) {
		if (cur == '\n') {
			c_error(loc, "Expected '%c' before end of line\n", endc);
			return;
		} else if (cur == '\0') {
			c_error(loc, "Expected '%c' before end of file\n", endc);
			return;
		}
		fname[fnlen++] = cur;
		cur = lex_advc(lx);
	}
	fname[fnlen] = '\0';
	lex_advc(lx);

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
		c_error(loc, "Unable to open file '%s': ", fname);
		fflush(stdout);
		perror(NULL);
		free(fdir);
	}
done:	return;
}

void ppd_undef(lexer *lx) {
	//Read macro name
	lex_next(lx);
	token mname = lex_peek(lx);
	if (mname.type != TOK_IDENT)
		c_error(&lx->tgt->loc, "Invalid macro name '%s'\n", mname.dat.sval);

	//Check the global scope
	symbol *s = map_get(&lx->stb.s_global->table, mname.dat.sval);
	if (s == NULL || s->type->kind != TYPE_MACRO)
		return;
	map_remove(&lx->stb.s_global->table, mname.dat.sval);

	return;
}

/*
 * Conditional directives
 */

//Reads a constant expression from the lexer and evaluate it
int ppd_constexpr(lexer *lx, int *err) {
	vector toks;
	vector_init(&toks, VECTOR_DEFAULT);

	//Get tokens until the end of the line
	//Expand macros by default, unless the previous token was "defined"
	//Function-like macros without parameters do not produce an error
	lx->m_exp = 1; lx->m_cexpr = 1;
	int nline = lex_wspace(lx);
	token t = BLANK_TOKEN;
	while (!(nline = lex_wspace(lx))) {
		lex_next(lx);
		t = lex_peek(lx);

		//Add the token to the vector
		token *tn = malloc(sizeof(struct TOKEN));
		if (t.type == TOK_IDENT && !strcmp(t.dat.sval, "defined"))
			t.type = TOK_DEFINED;
		*tn = t;
		vector_push(&toks, tn);
		lx->m_exp = !(t.type == TOK_DEFINED);
	}
	lx->m_exp = 0; lx->m_cexpr = 0;

	for (int i=0; i<toks.len; i++) {
		printf("%s ", tok_str(*(token*)toks.table[i], 0));
		s_pos x = ((token*)toks.table[i])->loc;
		printf("(%i:%i)\n", x.line, x.col);
	}

	//Call the evaluator
	int res = eval_constexpr(lx, &toks, err);
	
	//Clean up
	for (int i = 0; i<toks.len; i++) {
		token *t = toks.table[i];
		if (t->dtype != NULL) {
			type_del(t->dtype);
		} else if (t->type == TOK_IDENT) {
			free(t->dat.sval);
		}
		free(t);
	}
	vector_close(&toks);
	return res;
}

void ppd_if(lexer *lx) {
	int err = 0;
	int res = (ppd_constexpr(lx, &err)) ? 1 : 0;
	lexer_add_cond(lx, res);
	return;
}

void ppd_ifdef(lexer *lx) {
	lex_next(lx);
	token t = lex_peek(lx);
	if (t.type != TOK_IDENT)
		c_error(&t.loc, "Expected identifier after #ifdef\n");

	int res = (symtable_search(&lx->stb, t.dat.sval) != NULL) ? 1 : 0;
	lexer_add_cond(lx, res);
	return;
}

void ppd_ifndef(lexer *lx) {
	lex_next(lx);
	token t = lex_peek(lx);
	if (t.type != TOK_IDENT)
		c_error(&t.loc, "Expected identifier after #ifndef\n");

	int res = (symtable_search(&lx->stb, t.dat.sval) != NULL) ? 0 : 1;
	lexer_add_cond(lx, res);
	return;
}

void ppd_elif(lexer *lx) {
	lex_cond *cond = vector_top(&lx->conds);
	if (cond == NULL) {
		c_error(&lx->tgt->loc, "#elif before #if\n");
		return;
	}

	int err = 0;
	cond->was_true |= cond->is_true;
	cond->is_true = (ppd_constexpr(lx, &err)) ? 1 : 0;
	cond->is_true = 0;
	return;
}

void ppd_else(lexer *lx) {
	lex_cond *cond = vector_top(&lx->conds);
	if (cond == NULL) {
		c_error(&lx->tgt->loc, "#else before #if\n");
		return;
	}
	if (cond->has_else) {
		c_error(&lx->tgt->loc, "#else after #else\n");
		return;
	}

	cond->was_true |= cond->is_true;
	cond->is_true = 1;
	cond->has_else = 1;
	return;
}

void ppd_endif(lexer *lx) {
	lex_cond *cond = vector_top(&lx->conds);
	if (cond == NULL) {
		c_error(&lx->tgt->loc, "#endif before #if\n");
		return;
	}

	lexer_del_cond(lx);
	return;
}
