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

map kw_table;
void init_kwtable() {
	map_init(&kw_table, 40);
	#define kw(id, str) map_insert(&kw_table, str, (void*)id);
	#include "kw.inc"
	#undef kw
	return;
}

void close_kwtable() {
	map_close(&kw_table);
	return;
}

token BLANK_TOKEN = {-1, -1, 0, 0};
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

	//Copy location for macro
	if (type == TGT_MACRO) {
		n_tgt->loc = lx->tgt->loc;
	} else {
		n_tgt->loc.line = 1;
		n_tgt->loc.col = 1;
		n_tgt->loc.fname = name;
	}

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

int lex_ident(lexer *lx, token *t) {
	char *cur = lx->tgt->pos;
	if (!isalpha(*cur) && *cur != '_') return 0;
	while (isalnum(*cur) || *cur == '_') 
		cur++;
	int len = cur - lx->tgt->pos;
	t->dat.sval = malloc(len+1);
	memcpy(t->dat.sval, lx->tgt->pos, len);
	t->dat.sval[len] = '\0';
	t->type = TOK_IDENT;
	return len;
}

//Used for holding state of decimal and exponent
enum NUM_STATUS {
	S_NONE,	//No decimal/exponent
	S_INIT,	//Decimal/exponent exists, but has no trailing digits
	S_DONE	//Decimal/exponent exists and has trailing digits
};

int lex_num(lexer *lx, token *t) {
	char *cur = lx->tgt->pos;
	s_pos *loc = &lx->tgt->loc;

	//Read sign
	int sign = 1;
	if (*cur == '-' || *cur == '+') {
		sign = (*cur == '-') ? -1 : 1;
		cur++;
		loc->col++;
	}

	//Read base prefix
	int base = 10;
	if (*cur == '0' && isalnum(*(cur+1))) {
		base = 8;
		char nextc = tolower(*(cur+1));
		switch (nextc) {
			case 'x': case 'X':
				base = 16;
				cur++;
				loc->col++;
				break;
			case 'b': case 'B':
				base = 2;
				cur++;
				loc->col++;
				break;
			default:
				if (!isdigit(nextc)) {
					//invalid prefix ...
				}
				break;
		}
		cur++;
		loc->col++;
	}

	//Read number
	int dec_state = S_NONE; 
	int exp_state = S_NONE;
	unsigned long long int_val = 0;
	unsigned long long frac_val = 0;
	unsigned long long exp_val = 0;
	unsigned long long *tgt_val = &int_val;
	for (;;cur++) {
		char digit = tolower(*cur);
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
		loc->col++;
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
	while (isalpha(*cur)) {
		switch (tolower(*cur)) {
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
		cur++;
		loc->col++;
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
	int len = cur - lx->tgt->pos;
	return len;

	//Suffix error handling
s_err:	c_error(&suffix_loc, "Invalid suffix on constant\n");
	goto s_end;
}

//Processes the next token
//If m_exp is nonzero, macros will be expanded
void lex_next(lexer *lx, int m_exp) {
	token t = {-1, -1, lx->tgt->loc, 0, NULL};
	lex_target *tgt = lx->tgt;
	s_pos *loc = &tgt->loc;

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
reset:	tgt = lx->tgt;
	loc = &tgt->loc;
	nline |= lex_wspace(lx);
	t.nline = nline;
	t.loc = *loc;

	//Get next char
	int len;
	char *begin = tgt->pos;
	char *cur = begin;
	switch (*(cur++)) {
		case '\0':
			//End lexing if end has been reached
			//Otherwise, go to previous target
			lexer_tgt_close(lx);
			tgt = lx->tgt;
			if (tgt == NULL) {
				t.type = TOK_END;
				lx->ahead = t;
				return;
			}
			goto reset;
		case ';': 	t.type = TOK_SEM; 	break;
		case ':':	t.type = TOK_COL;	break;
		case ',':	t.type = TOK_CMM; 	break;
		case '.':	
			if (isdigit(*cur)) goto num_case;
			t.type = TOK_PRD; 
			break;
		case '{':	t.type = TOK_LBR; 	break;
		case '}':	t.type = TOK_RBR; 	break;
		case '(':	t.type = TOK_LPR; 	break;
		case ')':	t.type = TOK_RPR; 	break;
		case '[':	t.type = TOK_LBK;	break;
		case ']':	t.type = TOK_RBK;	break;
		case '\'': 	t.type = TOK_SQT; 	break;
		case '"':
			//TODO: whitespace concatenation
			t.type = TOK_STR;
			for (;;cur++) {
				if (*cur == '"') {
					break;
				} else if (*cur == '\\') {
					//TODO: escape sequences
					cur++;
					continue;
				}
			}
			
			len = cur - (begin+1);
			t.dat.sval = malloc(len+1);
			memcpy(t.dat.sval, (begin+1), len);
			t.dat.sval[len] = '\0';
			cur++;
			break;
		case '#':
			t.type = TOK_SNS;
			if (*cur == '#') {
				cur++, t.type = TOK_DNS;
			} else if (!nline && loc->col != 1) {
				c_error(loc, "Stray '#' in source\n");
			}
			break;
		case '?': 	t.type = TOK_QMK; 	break;
		case '+':
			if (isdigit(*cur)) goto num_case;
			t.type = TOK_ADD;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_ADD;
			} else if (*cur == '+') {
				cur++, t.type = TOK_INC;
			}
			break;
		case '-':
			if (isdigit(*cur)) goto num_case;
			t.type = TOK_SUB;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_SUB;
			} else if (*cur == '-') {
				cur++, t.type = TOK_DEC;
			} else if (*cur == '>') {
				cur++, t.type = TOK_PTR;
			}

			break;
		case '*':
			t.type = TOK_ASR;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_MUL;
			}
			break;
		case '/': 
			t.type = TOK_DIV;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_DIV;
			} else if (*cur == '/') {
				//C++ style comments
				for (;;) {
					if (*cur == '\n') {
						tgt->pos = cur;
						goto reset;
					} else if (*cur == '\0') {
						t.type = TOK_END;
						goto end;
					}
					cur++;
				}
			} else if (*cur == '*') {
				//C style comments
				for (;;) {
					if (*cur == '*' && *(cur+1) == '/') {
						cur += 2;
						tgt->pos = cur;
						goto reset;
					} else if (*cur == '\0') {
						c_error(loc, "Unterminated comment\n");
						t.type = TOK_END;
						goto end;
					}
					cur++;
				}
			}
			break;
		case '%':
			t.type = TOK_MOD;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_MOD;
			}
			break;
		case '&':
			t.type = TOK_AND;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_AND;
			} else if (*cur == '&') {
				cur++, t.type = TOK_LOGIC_AND;
			}
			break;
		case '|':
			t.type = TOK_OR;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_OR;
			} else if (*cur == '|') {
				cur++, t.type = TOK_LOGIC_OR;
			}
			break;
		case '^':
			t.type = TOK_XOR;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_XOR;
			}
			break;
		case '~':
			t.type = TOK_NOT;
			break;
		case '=':
			t.type = TOK_ASSIGN;
			if (*cur == '=')
				cur++, t.type = TOK_EQ;
			break;
		case '!':
			t.type = TOK_LOGIC_NOT;
			if (*cur == '=') {
				cur++, t.type = TOK_NEQ;
			}
			break;
		case '<':
			t.type = TOK_LTH;
			if (*cur == '=') {
				cur++, t.type = TOK_LEQ;
			} else if (*cur == '<') {
				cur++, t.type = TOK_LSHIFT;
				if (*cur == '=') {
					cur++, t.type = TOK_ASSIGN_LSHIFT;
				}
			}
			break;
		case '>':
			t.type = TOK_GTH;
			if (*cur == '=') {
				cur++, t.type = TOK_GEQ;
			} else if (*cur == '<') {
				cur++, t.type = TOK_RSHIFT;
				if (*cur == '=') {
					cur++, t.type = TOK_ASSIGN_RSHIFT;
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
			cur--;
			int len = lex_ident(lx, &t);
			cur += len;

			//Check if matches keyword
			//String is not freed yet; could be used as macro name
			int kw_id = (long long)map_get(&kw_table, t.dat.sval); 
			if (kw_id > 0)
				t.type = kw_id;

			//Check for macro expansion
			if (m_exp && !lex_expand_macro(lx, t))
				goto reset;

			//If the token is an identifier, free the string
			if (kw_id != 0) {
				free(t.dat.sval);
				t.dat.sval = NULL;
			}

			} break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			//Read number
num_case:		cur--;
			int len = lex_num(lx, &t);
			cur += len;

			/*
			if (t.dtype->kind == TYPE_INT)
				printf("%lld\n", t.dat.ival);
			else if (t.dtype->kind == TYPE_FLOAT)
				printf("%lf\n", t.dat.fval);
			*/
			//free(t.dtype); //temporary
			t.type = TOK_CONST;
			break;
		default: 
			//illegal char
			t.type = TOK_END;
			break;
	}
end:	loc->col += (cur - begin);
	tgt->pos = cur;
	lx->ahead = t;
	
	return;
}

//Skips whitespace
//Returns 1 if a newline is skipped
int lex_wspace(lexer *lx) {
	int nline = 0;
	lex_target *tgt = lx->tgt;
	s_pos *loc = &tgt->loc;
	while (isspace(*tgt->pos)) {
		if (*tgt->pos == '\n') { 
			nline = 1;
			loc->col = 0;
			loc->line++;
		}
		tgt->pos++;
		loc->col++;
	}
	return nline;
}

//Sets the lexer to expand a macro
//Returns zero if a macro was identified and expanded
int lex_expand_macro(lexer *lx, token t) {
	lex_target *tgt = lx->tgt;
	char *cur = tgt->pos;

	//Check for macro expansion
	symbol *s = symtable_search(&lx->stb, t.dat.sval);
	if (s != NULL && s->mac_exp != NULL) {
		//Check target stack to prevent reexpansion
		int expanded = 0;
		for (lex_target *i = tgt; i != NULL; i = i->prev) {
			if (i->type == TGT_MACRO && !strcmp(i->name, t.dat.sval)) {
				expanded = 1;
				break;
			}
		}
		if (expanded) return -1;
		tgt->pos = cur;

		//Check for function macro arguments
		//TODO: skip whitespace, error handling, loc updating
		/*
		lex_next(lx, 0);
		if (lex_peek(lx).kind == TOK_LPR) {
			cur = tgt->pos;
			char *begin = cur;
			while (*cur != ')' && *cur != ',')
				cur++;

		}
		tgt->pos = cur;
		*/

		//Expand the macro
		lexer_tgt_open(lx, t.dat.sval, TGT_MACRO, s->mac_exp);
		tgt = lx->tgt;

		//Free the identifier
		free(t.dat.sval);
		t.dat.sval = NULL;
		return 0;
	}
	return -1;
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
		case TOK_KW_ERROR:	ppd_error(lx);			break;
		case TOK_KW_IF:		ppd_if(lx);			break;
		case TOK_KW_IFDEF:	ppd_ifdef(lx);			break;
		case TOK_KW_IFNDEF:	ppd_ifndef(lx);			break;
		case TOK_KW_INCLUDE:	ppd_include(lx);		break;
		case TOK_KW_LINE:					break;
		case TOK_KW_PRAGMA:					break;
		case TOK_KW_UNDEF:	ppd_undef(lx);			break;
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
reset:	cond = vector_top(&lx->conds);
	if (cond != NULL && (!cond->is_true || cond->was_true)) {
		//Repeat until a conditional directive changes the state
		while (*lx->tgt->pos != '\0') {
			int nline = lex_wspace(lx);
			if (nline && *lx->tgt->pos == '#') {
				lx->tgt->pos++;
				lx->tgt->loc.col++;
				lex_next(lx, 0);

				//Only perform conditional directives
				token t = lex_peek(lx);
				if (is_condppd(t.type)) {
					lex_ppd(lx);
					goto reset;
				}
			}
			lx->tgt->pos++;
			lx->tgt->loc.col++;
		}

		//Premature end of input
		if (*lx->tgt->pos == '\0')
			c_error(&lx->tgt->loc, "Expected #endif before end of input\n");
	}
}

void lex_adv(lexer *lx) {
reset:	lex_condskip(lx);
	lex_next(lx, 1);
	if (lx->ahead.type == TOK_SNS) {
		lex_next(lx, 0);
		lex_ppd(lx);
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

int is_type_spec(token t) {
	if (t.type - TOK_KW_VOID >= 0 && 
	    t.type - TOK_KW_UNION <= TOK_KW_UNION - TOK_KW_VOID)
		return 1;
	return 0;
}

token_n *make_tok_node(token t) {
	token_n *node = malloc(sizeof(struct TOKEN_NODE));
	node->t = t;
	node->next = NULL;
	return node;
}
