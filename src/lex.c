#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "sym.h"
#include "err.h"
#include "lex.h"
#include "ppd.h"
#include "util/map.h"

//goto num_case temporarily disabled for positive and negative signs preceeding a digit

map kw_table;
void init_kwtable() {
	map_init(&kw_table, 40);
	#define tok(id, estr, str)
	#define kw(id, estr, str) map_insert(&kw_table, str, (void*)id);
	#include "tok.inc"
	#undef kw
	#undef tok
	return;
}

void close_kwtable() {
	map_close(&kw_table);
	return;
}

token BLANK_TOKEN = {-1, -1, 0, 0, 0};
lexer *lexer_init(char *fname) {
	lexer *lx = malloc(sizeof(struct LEXER));
	lx->tgt = NULL;
	lx->pre = NULL;
	lx->ahead = BLANK_TOKEN;
	vector_init(&lx->flist, VECTOR_EMPTY);
	vector_init(&lx->conds, VECTOR_EMPTY);

	if (!lex_open_file(lx, fname)) {
		c_error(NULL, "Unable to open file '%s': ", fname);
		fflush(stderr);
		perror(NULL);
		free(lx);
		return NULL;
	}
	symtable_init(&lx->stb);
	lx->m_exp = 1;
	lx->m_cexpr = 0;

	return lx;
}

int lex_open_file(lexer *lx, char *fname) {
	//Attempt to open file
	FILE *src_f = fopen(fname, "r");
	if (!src_f) return 0;

	//Read data from file
	//TODO: use sys/stat.h to get file size
	fseek(src_f, 0, SEEK_END);
	int len = ftell(src_f);
	char *buf = malloc(len+1);
	fseek(src_f, 0, SEEK_SET);
	fread(buf, len, 1, src_f);
	fclose(src_f);
	buf[len] = '\0';

	vector_add(&lx->flist, fname);
	lexer_tgt_open(lx, fname, TGT_FILE, buf);
	return 1;
}

void lexer_close(lexer *lx) {
	//Pop all targets if any remain
	while (lx->tgt != NULL)
		lexer_tgt_close(lx);

	//Free filenames
	for (int i=0; i<lx->flist.len; i++)
		free(lx->flist.table[i]);

	vector_close(&lx->flist);
	vector_close(&lx->conds);
	symtable_close(&lx->stb);
	free(lx);
	return;
}

void lexer_tgt_open(lexer *lx, char *name, int type, char *buf) {
	lex_target *n_tgt = malloc(sizeof(struct LEX_TARGET));
	n_tgt->name = name;
	n_tgt->type = type;
	n_tgt->buf = buf;
	n_tgt->pos = buf;
	n_tgt->cch = *buf;

	//Copy location for macro
	if (type == TGT_MACRO) {
		n_tgt->loc = lx->tgt->loc;
	} else {
		n_tgt->loc.line = 1;
		n_tgt->loc.col = 0;
		n_tgt->loc.fname = name;
	}

	vector_init(&n_tgt->mp_exp, VECTOR_EMPTY);
	n_tgt->prev = lx->tgt;
	lx->tgt = n_tgt;
	return;
}

void lexer_tgt_close(lexer *lx) {
	lex_target *tgt = lx->tgt;
	if (tgt->type == TGT_FILE) {
		//Name is freed when lexer is closed
		free(tgt->buf);
	}
	lx->tgt = tgt->prev;
	vector_close(&tgt->mp_exp);

	free(tgt);
	return;
}

void lexer_add_cond(lexer *lx, int pass) {
	lex_cond *cond = malloc(sizeof(struct LEX_CONDITION));
	cond->is_true = pass;
	cond->was_true = 0;
	cond->has_else = 0;
	vector_push(&lx->conds, cond);
	return;
}

void lexer_del_cond(lexer *lx) {
	lex_cond *cond = vector_pop(&lx->conds);
	free(cond);
	return;
}

//Lowest-level lexing function: returns the next state after performing single-character preprocessing
//cpos, cch, and cloc are pointers to a state, and are modified to represent the suceeding state
char lex_prcc(lexer *lx, char **cpos, char *cch, s_pos *cloc) {
	char *pos;
	char nchar;
	s_pos loc;
	if (cpos == NULL || cch == NULL || cloc == NULL) return '\0';

	/*
	 * Move to the next character
	 * If the current column is 0, the lexer is at the beginning of a file context
	 */
	int is_reset = (cloc->col == 0);
	pos = (is_reset) ? *cpos : *(cpos)+1;
	nchar = *pos;
	loc = *cloc;

	//Update location
	if (*cch == '\n') {
		loc.col = (is_reset) ? 1 : 0;
		loc.line++;
	}
	loc.col++;

	/*
	 * Perform trigraph conversion/newline splicing if needed
	 */
	for (;;) {
		//Process trigraphs
		if (*pos == '?' && *(pos+1) == '?') {
			int conv = 1;
			switch (*(pos+2)) {
				case '=': nchar = '#';  break;
				case '(': nchar = '[';  break;
				case '/': nchar = '\\'; break;
				case ')': nchar = ']';  break;
				case '\'': nchar = '^'; break;
				case '<': nchar = '{';  break;
				case '!': nchar = '|';  break;
				case '>': nchar = '}';  break;
				case '-': nchar = '~';  break;
				default: conv = 0; break;
			}

			//A conversion only takes place if the trigram was valid
			if (conv) {
				pos += 2;
				loc.col += 2;
			}
		}

		//Process newline splicing
		if (nchar == '\\' && *(pos+1) == '\n') {
			pos += 2;
			nchar = *pos;
			loc.col = 1;
			loc.line++;
		} else {
			//Stop looping if a line was not spliced
			break;
		}
	}

	*cpos = pos;
	*cch = nchar;
	*cloc = loc;
	return nchar;
}

//Handles preprocessing and gets the next character
//If not NULL, the integer pointed to by len will be set to the number of characters processed
//If not NULL, the struct pointed to by nloc will be set to the resultant source location
//If pflag != 0, the reset flag will be held for context switches during lookahead
char lex_nchar(lexer *lx, int *len, s_pos *nloc, int pflag) {
	char *pos, *prev_pos;
	char cch, prev_cch;
	s_pos loc, prev_loc;
	
	/*
	 * Get the next character
	 */
	static int is_reset = 0;
	int is_start = (lx->tgt->loc.col == 0);
reset:	pos = lx->tgt->pos;
	cch = lx->tgt->cch;
	loc = lx->tgt->loc;
tpaste:	if (!is_reset) lex_prcc(lx, &pos, &cch, &loc);
	if (is_start) lx->tgt->loc = loc;

	//Release the current context if the end has been reached
	if (cch == '\0' && lx->m_exp) {
		//The last context will stay open until the lexer is closed
		if (lx->tgt->prev == NULL)
			return '\0';

		lexer_tgt_close(lx);
		is_reset = 1;
		goto reset;
	}

	/*
	 * Replace comments with whitespace
	 */
	prev_pos = pos; prev_cch = cch; prev_loc = loc;
	if (cch == '/') {
		lex_prcc(lx, &pos, &cch, &loc);
		if (cch == '/') {
			//C++ style comments
			for (;;) {
				if (cch == '\n' || cch == '\0') break;
				lex_prcc(lx, &pos, &cch, &loc);
			}
			if (cch == '\0') {
				//error
			}
		} else if (cch == '*') {
			//C style comments
			lex_prcc(lx, &pos, &cch, &loc);
			char prevc = cch;
			for (;;) {
				if (prevc == '\0') break;
				lex_prcc(lx, &pos, &cch, &loc);
				if (prevc == '*' && cch == '/') break;
				prevc = cch;
			}
			if (cch == '\0') {
				c_error(&loc, "Unterminated comment\n");
			} else {
				cch = ' ';
			}
		} else {
			//Rewind
			pos = prev_pos; cch = prev_cch; loc = prev_loc;
		}
	}

	/*
	 * Macro expansion
	 * Only attempt if currently at beginning of an identifier and expansion is enabled
	*/
	char prev = (is_reset || is_start) ? '\0' : *lx->tgt->pos;
	prev_pos = pos; prev_cch = cch; prev_loc = loc;
	if (lx->m_exp && (!isalpha(prev) && prev != '_') && (isalpha(cch) || cch == '_')) {
		//Read identifier
		int len = 0;
		int bsize = 256;
		char cur = cch;
		char *buf = malloc(bsize);
		s_pos m_loc = lx->tgt->loc;
		while (isalnum(cur) || cur == '_') {
			//Resize if needed
			if (len+2 == bsize) {
				bsize *= 2;
				buf = realloc(buf, bsize);
			}

			//Copy character to buffer
			buf[len++] = cur;
			cur = lex_prcc(lx, &pos, &cch, &loc);
		}
		buf[len] = '\0';

		//Attempt to expand the macro
		if (!lex_expand_macro(lx, buf, &m_loc, &pos, &cch, &loc)) {
			is_reset = 1;
			goto reset;
		} else {
			free(buf);
		}

		//If the macro was not expanded, restore to before the identifier was read
		pos = prev_pos; cch = prev_cch; loc = prev_loc;
	}

	/*
	 * Handle token pasting
	 */
	//Only attempt once per chunk of whitespace
	prev_pos = pos; prev_cch = cch; prev_loc = loc;
	if (isspace(cch) && !isspace(prev))
		while (isspace(cch) && cch != '\n') lex_prcc(lx, &pos, &cch, &loc);

	//Perform token pasting if a double pound sign exists
	int has_dns = 0;
	if (lx->tgt->type == TGT_MACRO && cch == '#') {
		//Check for a '##'
		lex_prcc(lx, &pos, &cch, &loc);
		if (cch == '#') {
			has_dns = 1;
			do lex_prcc(lx, &pos, &cch, &loc);
			while (isspace(cch) && cch != '\n');
		}
	}
	if (has_dns) {
		is_reset = 1;
		goto tpaste;
	} else {
		pos = prev_pos; cch = prev_cch; loc = prev_loc;
	}

	//Update external values if applicable and return the next character
	if (len != NULL) *len = pos - lx->tgt->pos;
	if (nloc != NULL) {
		if (lx->tgt->type != TGT_MACRO)
			*nloc = loc;
		else
			*nloc = lx->tgt->loc;
	}
	if (!pflag)
		is_reset = 0;

	return cch;
}

//Returns the current character
char lex_cur(lexer *lx) {
	//Ensure the preprocessor gets to run on the first character of a file
	if (lx->tgt->loc.col == 0) lex_advc(lx);
	return lx->tgt->cch;
}

//Looks ahead by one character but does not advance the lexer
//However, context changes are still possible
char lex_peekc(lexer *lx) {
	return lex_nchar(lx, NULL, NULL, 1);
}

//Advances the lexer by one character
char lex_advc(lexer *lx) {
	int len; s_pos loc;
	char nc = lex_nchar(lx, &len, &loc, 0);
	if (nc == '\0') return nc;

	lx->tgt->cch = nc;
	lx->tgt->pos += len;
	lx->tgt->loc = loc;
	return lx->tgt->cch;
}

//Reads an identifier
void lex_ident(lexer *lx, token *t) {
	int len = 0;
	int bsize = 256;
	char cur = lex_cur(lx);
	char *buf = malloc(bsize);
	while (isalnum(cur) || cur == '_') {
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
	t->dat.sval = buf;
	t->type = TOK_IDENT;
	return;
}

//Used for holding state of decimal and exponent
enum NUM_STATUS {
	S_NONE,	//No decimal/exponent
	S_INIT,	//Decimal/exponent exists, but has no trailing digits
	S_DONE	//Decimal/exponent exists and has trailing digits
};

void lex_num(lexer *lx, token *t) {
	char cur = lex_cur(lx);
	s_pos *loc = &lx->tgt->loc;

	//Read sign
	int sign = 1;
	if (cur == '-' || cur == '+') {
		sign = (cur == '-') ? -1 : 1;
		cur = lex_advc(lx);
	}

	//Read base prefix
	int base = 10;
	if (cur == '0' && isalnum(lex_peekc(lx))) {
		base = 8;
		char nextc = tolower(lex_peekc(lx));
		switch (nextc) {
			case 'x': case 'X':
				base = 16;
				lex_advc(lx);
				break;
			case 'b': case 'B':
				base = 2;
				lex_advc(lx);
				break;
			default:
				if (!isdigit(nextc)) {
					//invalid prefix ...
				}
				break;
		}
		lex_advc(lx);
	}

	//Read number
	int dec_state = S_NONE; 
	int exp_state = S_NONE;
	unsigned long long int_val = 0;
	unsigned long long frac_val = 0;
	unsigned long long exp_val = 0;
	unsigned long long *tgt_val = &int_val;
	for (;; cur = lex_advc(lx)) {
		char digit = tolower(cur);
		int dval = digit - '0';

		//Handle floating-point characters
		if (digit == '.') {
			if (dec_state != S_NONE) {
				c_error(loc, "Multiple decimal points in constant\n");
				goto n_end;
			}
			dec_state = S_INIT;
			tgt_val = &frac_val;
			continue;
		} else if (tolower(digit) == 'e' && base != 16) {
			if (exp_state != S_NONE) {
				c_error(loc, "Multiple exponents in constant\n");
				goto n_end;
			}
			exp_state = S_INIT;
			tgt_val = &exp_val;
			continue;
		}

		//Read digits
		//Cases fall through from top to bottom to verify input
		switch (digit) {
		case 'f': case 'e': case 'd':
		case 'c': case 'b': case 'a':
			if (base == 10)
				goto n_end;
			dval = 10 + (digit - 'a');
		case '9': case '8':
			if (base == 8) {
				c_error(loc, "Invalid digit in octal constant\n");
				goto n_end;
			}
		case '7': case '6': case '5':
		case '4': case '3': case '2':
			if (base == 2) {
				c_error(loc, "Invalid digit in binary constant\n");
				goto n_end;
			}
		case '1': case '0':
			if (dec_state == S_INIT) dec_state = S_DONE;
			if (exp_state == S_INIT) exp_state = S_DONE;
			*(tgt_val) *= base;
			*(tgt_val) += dval;
			break;

		default: goto n_end;
		}
	}
n_end:  if (dec_state != S_NONE && base != 10)
		c_error(loc, "Decimal point in non-base 10 constant\n");
	if (exp_state == S_INIT)
		c_error(loc, "Missing exponent in constant\n");

	//Determine kind
	s_type *ty = type_new(TYPE_INT);
	if (dec_state != S_NONE || exp_state == S_DONE)
		ty->kind = TYPE_FLOAT;
	ty->size = SIZE_LONG;
	ty->is_signed = 1;
	t->dtype = ty;

	//Read suffix
	s_pos suffix_loc = *loc;
	int l_count = 0, u_count = 0, f_count = 0;
	while (isalpha(cur)) {
		switch (tolower(cur)) {
			case 'l': //Long flag
				l_count++; 
				break;
			case 'u': //Unsigned flag
				u_count++;
				break;
			case 'f': //Float flag
				ty->kind = TYPE_FLOAT;
				f_count++;
				break;
			default: goto s_end;
		}
		cur = lex_advc(lx);
	}

	//Suffix verification
	if (f_count > 1) goto s_err;
	if (ty->kind == TYPE_INT) {
		if (u_count > 1) goto s_err;
		if (l_count > 2) goto s_err;
		if (l_count == 2) //Promote to long long
			ty->size = SIZE_LONG_LONG;
	} else {
		if (u_count > 0) goto s_err;
		if (l_count > 1) goto s_err;
		if (l_count != 0) //Promote to double
			ty->size = SIZE_LONG_LONG;
	}

	//Set token data
s_end:	if (ty->kind == TYPE_INT) {
		if (!ty->is_signed) {
			t->dat.uval = int_val;
		} else {
			t->dat.ival = int_val * sign;
		}
	} else {
		//Calculate value from integer, fraction, and exponent parts
		double dec = frac_val/pow(10, floor(log10(frac_val)+1));
		t->dat.fval = int_val;
		if (frac_val != 0) t->dat.fval += dec;
		t->dat.fval *= pow(10, exp_val);
	}
	return;

	//Suffix error handling
s_err:	c_error(&suffix_loc, "Invalid suffix on constant\n");
	goto s_end;
}

//Reads an escape character
char lex_eschar(lexer *lx) {
	//Simple escape characters
	s_pos loc = lx->tgt->loc;
	switch (lex_cur(lx)) {
		case '\'': lex_advc(lx); return '\'';
		case '\"': lex_advc(lx); return '\"';
		case '\?': lex_advc(lx); return '\?';
		case '\\': lex_advc(lx); return '\\';
		case 'a':  lex_advc(lx); return '\a';
		case 'b':  lex_advc(lx); return '\b';
		case 'f':  lex_advc(lx); return '\f';
		case 'n':  lex_advc(lx); return '\n';
		case 'r':  lex_advc(lx); return '\r';
		case 't':  lex_advc(lx); return '\t';
		case 'v':  lex_advc(lx); return '\v';
	}

	//Number escape character
	char cur = lex_cur(lx);
	unsigned int val = 0;
	if (cur == 'x') {
		//Hexadecimal
		cur = lex_advc(lx);
		if (!isxdigit(cur)) {
			c_warn(&loc, "\\x escape character without hexadecimal digits\n");
			return 0;
		}

		//Read number and check range
		while (isxdigit(cur)) {
			int dval = (!isdigit(cur)) ? 10 + (tolower(cur) - 'a') : (cur - '0');
			val = (val * 16) + dval;
			cur = lex_advc(lx);
		}
		if (val > 0xFF)
			c_warn(&loc, "hexadecimal escape char out of range\n");
	} else if (isdigit(cur) && cur != '8' && cur != '9') {
		//Octal
		//Read number
		while (isdigit(cur) && cur != '8' && cur != '9') {
			val = (val * 8) + (cur - '0');
			cur = lex_advc(lx);
		}

		//Check range
		if (val > 0377)
			c_warn(&loc, "octal escape char out of range\n");
	}

	return val;
}

//Reads a quotation-enclosed string
void lex_str(lexer *lx, token *t) {
	int len = 0;
	int bsize = 256;
	char cur = lex_advc(lx);
	char *buf = malloc(bsize);
	for (;;) {
		//Resize if needed
		if (len+2 == bsize) {
			bsize *= 2;
			buf = realloc(buf, bsize);
		}

		//Get character
		char c = cur;
		if (cur == '"') {
			lex_advc(lx);
			//Check for whitespace concatenation
			while (isspace(lex_cur(lx))) lex_advc(lx);
			if (lex_cur(lx) != '"') break;
			cur = lex_advc(lx);
			continue;
		} else if (cur == '\\') {
			//Escape sequence
			lex_advc(lx);
			buf[len++] = lex_eschar(lx);
			cur = lex_cur(lx);
			continue;
		}

		//Copy character to buffer
		buf[len++] = c;
		cur = lex_advc(lx);
	}
	buf[len] = '\0';
	t->dat.sval = buf;
	t->type = TOK_STR;
	return;
}

//Processes the next token
//If m_exp is nonzero, macros will be expanded
void lex_next(lexer *lx) {
	token t = {-1, -1, lx->tgt->loc, 0, NULL};
	lex_target *tgt = lx->tgt;
	s_pos loc = tgt->loc;

	//Use ungotten tokens
	token_n *pre = lx->pre;
	if (pre != NULL) {
		lx->ahead = pre->t;
		lx->pre = pre->next;
		free(pre);
		return;
	}

	//Skip whitespace
	//Count the beginning of a file as a newline
	//When the lexer is reset (when a new context is created) it begins from here
	int nline = (tgt->pos == tgt->buf && tgt->type == TGT_FILE);
reset:	nline |= lex_wspace(lx);
	tgt = lx->tgt;
	t.nline = nline;
	loc = tgt->loc;

	//Process the next character
	t.loc = tgt->loc;
	int noadvc = 0;
	switch (lex_cur(lx)) {
		case '\0':
			//End of stream
			t.type = TOK_END;
			lx->ahead = t;
			return;
		case ';': 	t.type = TOK_SEM; 	break;
		case ':':	t.type = TOK_COL;	break;
		case ',':	t.type = TOK_CMM; 	break;
		case '.':
			if (isdigit(lex_peekc(lx))) goto num_case;
			t.type = TOK_PRD; 
			break;
		case '{':	t.type = TOK_LBR; 	break;
		case '}':	t.type = TOK_RBR; 	break;
		case '(':	t.type = TOK_LPR; 	break;
		case ')':	t.type = TOK_RPR; 	break;
		case '[':	t.type = TOK_LBK;	break;
		case ']':	t.type = TOK_RBK;	break;
		case '"':
			lex_str(lx, &t);
			noadvc = 1;
			break;
		case '\'':
			//Character literal
			lex_advc(lx);

			//Process escape character
			char val = lex_cur(lx);
			if (val == '\\') {
				lex_advc(lx);
				val = lex_eschar(lx);
			}

			//Check for closing '
			if (lex_cur(lx) == '\'') {
				lex_advc(lx);
			} else {
				c_error(&loc, "Multiple characters in character literal\n");
			}

			t.dat.ival = val;
			t.type = TOK_CONST;
			noadvc = 1;
			break;
		case '#':
			t.type = TOK_SNS;
			lx->m_exp = 0;
			if (lex_peekc(lx) == '#') {
				lex_advc(lx);
				t.type = TOK_DNS;
				c_error(&loc, "Stray '##' in source\n");
			} else if (!nline && loc.col != 1) {
				c_error(&loc, "Stray '#' in source\n");
			}
			lex_advc(lx); //Advance manually; do not attempt to expand directive
			noadvc = 1;
			lx->m_exp = 1;
			break;
		case '?': 	t.type = TOK_QMK; 	break;
		case '+':
			//if (isdigit(*cur)) goto num_case;
			t.type = TOK_ADD;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_ADD;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '+') {
				t.type = TOK_INC;
				lex_advc(lx);
			}
			break;
		case '-':
			//if (isdigit(*cur)) goto num_case;
			t.type = TOK_SUB;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_SUB;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '-') {
				t.type = TOK_DEC;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '>') {
				t.type = TOK_PTR;
				lex_advc(lx);
			}
			break;
		case '*':
			t.type = TOK_ASR;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_MUL;
				lex_advc(lx);
			}
			break;
		case '/': 
			t.type = TOK_DIV;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_DIV;
				lex_advc(lx);
			}
			break;
		case '%':
			t.type = TOK_MOD;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_MOD;
				lex_advc(lx);
			}
			break;
		case '&':
			t.type = TOK_AND;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_AND;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '&') {
				t.type = TOK_LOGIC_AND;
				lex_advc(lx);
			}
			break;
		case '|':
			t.type = TOK_OR;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_OR;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '|') {
				t.type = TOK_LOGIC_OR;
				lex_advc(lx);
			}
			break;
		case '^':
			t.type = TOK_XOR;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_ASSIGN_XOR;
				lex_advc(lx);
			}
			break;
		case '~':
			t.type = TOK_TLD;
			break;
		case '=':
			t.type = TOK_ASSIGN;
			if (lex_peekc(lx) == '=')
				t.type = TOK_EQ;
			lex_advc(lx);
			break;
		case '!':
			t.type = TOK_NOT;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_NEQ;
				lex_advc(lx);
			}
			break;
		case '<':
			t.type = TOK_LTH;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_LEQ;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '<') {
				t.type = TOK_LSHIFT;
				lex_advc(lx);
				if (lex_peekc(lx) == '=') {
					t.type = TOK_ASSIGN_LSHIFT;
					lex_advc(lx);
				}
			}
			break;
		case '>':
			t.type = TOK_GTH;
			if (lex_peekc(lx) == '=') {
				t.type = TOK_GEQ;
				lex_advc(lx);
			} else if (lex_peekc(lx) == '<') {
				t.type = TOK_RSHIFT;
				lex_advc(lx);
				if (lex_peekc(lx) == '=') {
					t.type = TOK_ASSIGN_RSHIFT;
					lex_advc(lx);
				}
			}
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e':
		case 'f': case 'g': case 'h': case 'i': case 'j':
		case 'k': case 'l': case 'm': case 'n': case 'o':
		case 'p': case 'q': case 'r': case 's': case 't':
		case 'u': case 'v': case 'w': case 'x': case 'y':
		case 'z':
		case 'A': case 'B': case 'C': case 'D': case 'E':
		case 'F': case 'G': case 'H': case 'I': case 'J':
		case 'K': case 'L': case 'M': case 'N': case 'O':
		case 'P': case 'Q': case 'R': case 'S': case 'T':
		case 'U': case 'V': case 'W': case 'X': case 'Y':
		case 'Z': case '_': {
			//Read identifier
			lex_ident(lx, &t);

			//Check if matches keyword
			//String is not freed yet; could be used as macro name
			int kw_id = (long long)map_get(&kw_table, t.dat.sval); 
			if (kw_id > 0)
				t.type = kw_id;

			//If the token is an identifier, free the string
			if (kw_id != 0) {
				free(t.dat.sval);
				t.dat.sval = NULL;
			}
			noadvc = 1;
			} break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			//Read number
num_case:		lex_num(lx, &t);
			//free(t.dtype); //temporary
			t.type = TOK_CONST;
			noadvc = 1;
			break;
		default: 
			//illegal char
			t.type = TOK_END;
			break;
	}
	//Unless the specific case handled it manually, advance to the next char
end:	if (!noadvc)
		lex_advc(lx);
	lx->ahead = t;
	return;
}

//Skips whitespace
//Returns 1 if a newline is skipped
int lex_wspace(lexer *lx) {
	int nline = 0;
	char cur = lex_cur(lx);
	for (;;) {
		//Handle whitespace checks at end of lexer context
		if (cur == '\0' && lx->m_exp) {
			if (lx->tgt->prev != NULL) {
				//lexer_tgt_close(lx);
				lex_advc(lx);
				cur = lex_cur(lx);
			} else {
				break;
			}
			continue;
		}
		
		//Check for newline and skip whitespace
		if (!isspace(cur)) break;
		if (cur == '\n') nline = 1;
		cur = lex_advc(lx);
	}
	return nline;
}

/* 
 * Attempts to expand a macro with name ident at location m_loc
 * Returns zero if a macro was identified and expanded
 * If successful, cpos, cch, and cloc will be updated with the next lexer state
 * If expansion fails, they wil be left unmodified
 */
int lex_expand_macro(lexer *lx, char *ident, s_pos *m_loc, char **cpos, char *cch, s_pos *cloc) {
	lex_target *tgt = lx->tgt;
	symbol *s = symtable_search(&lx->stb, ident);
	char *pos = *cpos;
	char cur = *cch;
	s_pos loc = *cloc;

	//Check to see if there is an applicable macro for the given name
	int expanded = 0;
	int m_param = 0;
	char *m_name = (s != NULL) ? s->name : NULL;
	char *m_exp = (s != NULL) ? s->mac_exp : NULL;
	for (lex_target *i = tgt; i != NULL; i = i->prev) {
		//Only check macro contexts
		if (i->type != TGT_MACRO) continue;

		//If the macro was found in the symbol table, check if it is a macro parameter
		if (m_exp == NULL) {
			symbol *m = symtable_search(&lx->stb, i->name);
			if (m == NULL) continue;

			//Check the macro's parameters
			vector *ptable = &m->type->param;
			for (int j=0; j<ptable->len; j++) {
				s_param *p = ptable->table[j];

				//If a match is found, s becomes the "parent" macro
				if (!strcmp(ident, p->name)) {
					s = m;
					m_param = 1;
					m_name = p->name;
					m_exp = i->mp_exp.table[j];
					goto pfound;
				}
			}
		}

		//Check to see if the macro has been expanded
		if (!strcmp(i->name, ident)) {
			expanded = 1;
		}
	}
	if (m_exp == NULL || expanded) {
		return -1;
	}
	
	//Check for and read function macro arguments
	vector macro_params;
pfound:	vector_init(&macro_params, VECTOR_EMPTY);
	int m_error = 0;
	char *prev_pos = pos;
	char prev_cch = cur;
	s_pos prev_loc = loc;
	while (isspace(cur)) cur = lex_prcc(lx, &pos, &cur, &loc);
	if (cur == '(') {
		//Read macro arguments
		int paren_c = 0;
		cur = lex_prcc(lx, &pos, &cur, &loc);
		while (cur != ')') {
			//Skip whitespace
			while (isspace(cur)) cur = lex_prcc(lx, &pos, &cur, &loc);

			//Read a single argument
			int len = 0;
			int bsize = 256;
			char *buf = malloc(bsize);
			for (;;) {
				//Only stop if a ')' or ',' is encountered
				//outside of nested parentheses
				if (cur == '(') paren_c++;
				if (cur == ',' && paren_c == 0) break;
				if (cur == ')') {
					if (paren_c == 0)
						break;
					paren_c--;
				}
				if (cur == '\0') break;

				//Resize if needed
				if (len+2 == bsize) {
					bsize *= 2;
					buf = realloc(buf, bsize);
				}

				//Copy character to buffer
				buf[len++] = cur;
				lex_prcc(lx, &pos, &cur, &loc);
			}
			buf[len] = '\0';
			vector_add(&macro_params, buf);

			//Post-read checks
			if (cur == ',')
				lex_prcc(lx, &pos, &cur, &loc);
			if (cur == '\0') {
				m_error = -1;
				c_error(&lx->tgt->loc,
					"Unexpected end of input in macro parameter list\n");
				break;
			}
		}
		lex_prcc(lx, &pos, &cur, &loc);
	} else {
		//Rewind and restore whitespace if no argument list was found
		pos = prev_pos;
		cur = prev_cch;
		loc = prev_loc;
	}
	
	//Ensure the number of arguments matches the amount specified in the macro definition
	if (!m_param && macro_params.len != s->type->param.len) {
		m_error = -1;
		if (!lx->m_cexpr) { //Not an error in preprocessor constant expressions
			c_error(m_loc, "Incorrect number of arguments to function-like macro '%s' "
				"(expected %i, read %i)\n",
				ident, s->type->param.len, macro_params.len);
		}
	}

	//If no issues have been encountered, expand the macro
	if (!m_error) {
		//Commit to the state of the current context
		lx->tgt->pos = pos;
		lx->tgt->cch = cur;
		lx->tgt->loc = loc;

		//Open a context for the macro
		lexer_tgt_open(lx, m_name, TGT_MACRO, m_exp);
		tgt = lx->tgt;
		tgt->loc = *m_loc;

		//Update external values
		*cpos = tgt->pos;
		*cch = tgt->cch;
		*cloc = tgt->loc;

		//Set definitions for each parameter
		for (int i=0; i<macro_params.len; i++)
			vector_add(&tgt->mp_exp, macro_params.table[i]);
	} else {
		//Otherwise, free any arguments
		for (int i=0; i<macro_params.len; i++)
			free(macro_params.table[i]);
	}

	//Free the macro name and parameters
	vector_close(&macro_params);
	free(ident);
	return m_error;
}

//Perform preprocessing
void lex_ppd(lexer *lx) {
	//Read directive
	token ppd = lex_peek(lx);
	switch (ppd.type) {
		case TOK_KW_DEFINE:	ppd_define(lx);			break;
		case TOK_KW_ELIF:	ppd_elif(lx);			break;
		case TOK_KW_ELSE:	ppd_else(lx);			break;
		case TOK_KW_ENDIF:   	ppd_endif(lx);			break;
		case TOK_KW_ERROR:	ppd_error(lx, 0);		break;
		case TOK_KW_IF:		ppd_if(lx);			break;
		case TOK_KW_IFDEF:	ppd_ifdef(lx);			break;
		case TOK_KW_IFNDEF:	ppd_ifndef(lx);			break;
		case TOK_KW_INCLUDE:	ppd_include(lx);		break;
		case TOK_KW_LINE:					break;
		case TOK_KW_PRAGMA:					break;
		case TOK_KW_UNDEF:	ppd_undef(lx);			break;
		case TOK_KW_WARNING:	ppd_error(lx, 1);		break;
		default:
			c_error(&ppd.loc, "Invalid preprocessor directive\n");
			break;
	}

	return;
}

token lex_peek(lexer *lx) {
	if (lx->ahead.type < 0)
		lex_adv(lx);
	return lx->ahead;
}

//Helper that returns 1 if a directive is conditional-related
int is_condppd(int type) {
	switch (type) {
		case TOK_KW_ELIF:
		case TOK_KW_ELSE:
		case TOK_KW_ENDIF:
		case TOK_KW_IF:	
		case TOK_KW_IFDEF:
		case TOK_KW_IFNDEF:
			return 1;
	}
	return 0;
}

/* 
 * Skip if the current preprocessor condition is false or if the condition
 * has already been true at least once.
 * If another #if directive is encountered while skipping, its condition
 * will always be marked as false, even if it would normally evaluate true.
 */
void lex_condskip(lexer *lx) {
	lex_cond *cond;
	int had_nl = 1;
	lx->m_exp = 0;
reset:	cond = vector_top(&lx->conds);
	if (cond != NULL && (!cond->is_true || cond->was_true)) {
		//Repeat until a conditional directive changes the state
		char cur = lex_cur(lx);
		while (cur != '\0') {
			int nline = lex_wspace(lx);
			if ((nline || had_nl) && lex_cur(lx) == '#') {
				if (cur == '\n') had_nl = 1;
				lex_advc(lx);
				lex_next(lx);

				//Only perform conditional directives
				token t = lex_peek(lx);
				if (is_condppd(t.type)) {
					lex_ppd(lx);
					goto reset;
				}
			}
			cur = lex_advc(lx);
			had_nl = 0;
		}

		//Premature end of input
		if (cur == '\0')
			c_error(&lx->tgt->loc, "Expected #endif before end of input\n");
	}
	lx->m_exp = 1;
	return;
}

void lex_adv(lexer *lx) {
reset:	if (lx->ahead.type == TOK_END) return;
	lex_condskip(lx);
	lex_next(lx);
	if (lx->ahead.type == TOK_SNS) {
		lx->m_exp = 0; //Temporarily disable macro expansion
		lex_next(lx);
		lex_ppd(lx);
		lx->m_exp = 1;
		goto reset;
	}
	return;
}

void lex_unget(lexer *lx, token_n *n) {
	n->next = lx->pre;
	lx->pre = n;
	lx->ahead = BLANK_TOKEN; //force lex_peek to call lex_next
	return;
}

/*
 * Helper functions
 */
int is_type_spec(lexer *lx, token t) {
	symbol *td;
	switch (t.type) {
		case TOK_KW_VOID: case TOK_KW_CHAR: case TOK_KW_SHORT:
		case TOK_KW_FLOAT: case TOK_KW_TYPEDEF: case TOK_KW_EXTERN:
		case TOK_KW_STATIC: case TOK_KW_AUTO: case TOK_KW_REGISTER:
		case TOK_KW_SIGNED: case TOK_KW_UNSIGNED: case TOK_KW_CONST:
		case TOK_KW_VOLATILE: case TOK_KW_INT: case TOK_KW_DOUBLE:
		case TOK_KW_LONG: case TOK_KW_STRUCT: case TOK_KW_UNION:
		case TOK_KW_ENUM:
			return 1;
		case TOK_IDENT:
			td = symtable_search(&lx->stb, t.dat.sval);
			return (td->btype->s_class == CLASS_TYPEDEF);
	}
	return 0;
}

token_n *make_tok_node(token t) {
	token_n *node = malloc(sizeof(struct TOKEN_NODE));
	node->t = t;
	node->next = NULL;
	return node;
}

//Returns a string describing the token
//If nflag is nonzero, the "natural" representation will be printed
const char *tok_str(token t, int nflag) {
	//List is built with both enum and natural representations
	//n*2 -> enum, n*2+1 -> natural
	char *tstr_list[] = {
		#define tok(id, estr, str) estr, str,
		#define kw(id, estr, str) estr, str,
		#include "tok.inc"
		#undef kw
		#undef tok
	};

	//Check for special cases
	static char buf[256];
	switch (t.type) {
		case TOK_CONST:
			if (t.dtype->kind == TYPE_FLOAT)
				snprintf(buf, 256, "%lf", t.dat.fval);
			else if (t.dtype->is_signed)
				snprintf(buf, 256, "%lld", t.dat.ival);
			else
				snprintf(buf, 256, "%llu", t.dat.uval);
			return buf;
		case TOK_STR:
		case TOK_IDENT:
			return t.dat.sval;
	}

	return tstr_list[t.type*2 + (nflag != 0)];
}

void lexer_debug() {
	/*
	*/
}
