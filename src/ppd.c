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

void ppd_defparams(lexer *lx, s_type *mtype) {
	token *t = &lx->ahead;
	s_pos *loc = &lx->tgt->loc;
	lx->tgt->pos++;
	lex_next(lx, 0);
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
		lex_next(lx, 0);

		//Read comma
		if (t->type != TOK_CMM && t->type != TOK_RPR) {
			c_error(loc, "Missing comma in macro parameter list\n");
			break;
		} else if (t->type == TOK_CMM) {
			lex_next(lx, 0);
		}
	}
}

void ppd_define(lexer *lx) {
	s_pos *loc = &lx->tgt->loc;

	//Read macro name manually
	token mname;
	lex_wspace(lx);
	int mlen = lex_ident(lx, &mname);
	if (mlen == 0)
		c_error(loc, "Invalid macro name\n");
	lx->tgt->pos += mlen;

	//Check for and read macro parameter list
	s_type *mtype = type_new(TYPE_MACRO);
	if (*lx->tgt->pos == '(')
		ppd_defparams(lx, mtype);

	//Get bounds of macro body
	char *cur = lx->tgt->pos;
	char *start, *end;
	while (isspace(*cur) && *cur != '\n') cur++;
	if (cur == lx->tgt->pos && *cur != '\n')
		c_error(loc, "No whitespace after macro name\n");

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

		//Check for ## on edges of macro
		if (len >= 2) {
			if (buf[0] == '#' && buf[1] == '#' ||
			    buf[len-2] == '#' && buf[len-1] == '#') {
				c_error(loc, "'##' at beginning or end of macro\n");
				lx->tgt->pos = end;
				free(mname.dat.sval);
				free(buf);
				return;
			}
		}
	}

	//Add to symtable
	symbol *sym = symtable_def(&lx->stb, mname.dat.sval, mtype, &mname.loc);
	sym->mac_exp = buf;
	lx->tgt->pos = end;
	return;
}

void ppd_error(lexer *lx) {
	s_pos *loc = &lx->tgt->loc;

	//Read message
	char *cur = lx->tgt->pos;
	while (isspace(*cur) && *cur != '\n') cur++;
	char *start = cur;
	while (*cur != '\n') cur++;

	int len = cur - start;
	char *buf = malloc(len+1);
	memcpy(buf, start, len);
	buf[len] = '\0';

	//Print error
	c_error(loc, "%s\n", buf);
	free(buf);

	lx->tgt->pos = cur;
	return;
}

void ppd_include(lexer *lx) {
	char *cur = lx->tgt->pos;
	s_pos *loc = &lx->tgt->loc;
	while (*cur == ' ') cur++;

	//Get include type
	char type = *cur++;
	if (type != '<' && type != '"')
		c_error(loc, "#include expects <fname> or \"fname\"\n");

	//Get filename
	char *begin = cur;
	char endc = (type == '"') ? '"' : '>';
	while (*cur != endc) {
		if (*cur == '\n') {
			c_error(loc, "Expected '%c' before end of line\n", endc);
			return;
		} else if (*cur == '\0') {
			c_error(loc, "Expected '%c' before end of file\n", endc);
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
		c_error(loc, "Unable to open file '%s': ", fname);
		fflush(stdout);
		perror(NULL);
		free(fdir);
	}
done:	return;
}

void ppd_undef(lexer *lx) {
	//Read macro name
	lex_next(lx, 0);
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
	vector_init(&toks, VECTOR_EMPTY);

	//Get tokens until the end of the line
	int nline = lex_wspace(lx);
	while (!(nline = lex_wspace(lx))) {
		lex_next(lx, 0);
		token t = lex_peek(lx);

		//Add the token to the vector
		token *tn = malloc(sizeof(struct TOKEN));
		*tn = t;
		vector_push(&toks, tn);
	}

	//Call the evaluator
	int res = eval_constexpr(&toks, err);

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
	lex_next(lx, 0);
	token t = lex_peek(lx);
	if (t.type != TOK_IDENT)
		c_error(&t.loc, "Expected identifier after #ifdef\n");

	int res = (symtable_search(&lx->stb, t.dat.sval) != NULL) ? 1 : 0;
	lexer_add_cond(lx, res);
	return;
}

void ppd_ifndef(lexer *lx) {
	lex_next(lx, 0);
	token t = lex_peek(lx);
	if (t.type != TOK_IDENT)
		c_error(&t.loc, "Expected identifier after #ifndef\n");
	lex_next(lx, 0);

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
