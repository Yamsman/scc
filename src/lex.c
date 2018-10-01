#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

token BLANK_TOKEN = {-1, -1, NULL};
lexer *lexer_init(char *fname) {
	lexer *lx = malloc(sizeof(struct LEXER));
	lx->tgt = NULL;
	lx->pre = NULL;
	lx->ahead = BLANK_TOKEN;

	if (!lex_open_file(lx, fname)) {
		c_err(NULL, "Unable to open file '%s': ", fname);
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
	//fseek(src_f, 0, SEEK_END);
	//int len = ftell(src_f);
	int len = 500;
	char *buf = malloc(len+1);
	//fseek(src_f, 0, SEEK_SET);
	fread(buf, len, 1, src_f);
	fclose(src_f);

	lexer_tgt_open(lx, fname, TGT_FILE, buf);
	return 1;
}

void lexer_close(lexer *lx) {
	while (lx->tgt != NULL)
		lexer_tgt_close(lx);

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

	n_tgt->prev = lx->tgt;
	lx->tgt = n_tgt;
	return;
}

void lexer_tgt_close(lexer *lx) {
	lex_target *tgt = lx->tgt;
	if (tgt->type == TGT_FILE) {
		free(tgt->name);
		free(tgt->buf);
	}
	lx->tgt = tgt->prev;

	free(tgt);
	return;
}

int lex_ident(lexer *lx, token *t) {
	char *cur = lx->tgt->pos;
	while (isalnum(*cur) || *cur == '_') 
		cur++;
	int len = cur - lx->tgt->pos;
	t->str = malloc(len+1);
	memcpy(t->str, lx->tgt->pos, len);
	t->str[len] = '\0';
	t->type = TOK_IDENT;
	return len;
}

//Used for holding state of decimal and exponent
enum NUM_STATUS {
	S_NONE,	//No decimal/exponent
	S_INIT,	//Decimal/exponent exists, but has no trailing digits
	S_DONE	//Decima/exponent exists and has trailing digits
};

int lex_num(lexer *lx, token *t) {
	char *cur = lx->tgt->pos;

	//Read prefix
	int base = 10;
	if (*cur == '0' && isalnum(*(cur+1))) {
		base = 8;
		if (*(cur+1) == 'x') {
			base = 16;
			cur++;
		} if (*(cur+1) == 'b') {
			base = 2;
			cur++;
		}
		cur++;
	}

	//Read number
	int dec_state = S_NONE; 
	int exp_state = S_NONE;
	long long int_val = 0;
	long long frac_val = 0;
	long long exp_val = 0;
	long long *tgt_val = &int_val;
	for (;;cur++) {
		char digit = tolower(*cur);
		int dval = digit - '0';

		//Handle floating-point characters
		if (digit == '.') {
			if (dec_state != S_NONE) {
				printf("ERROR: Multiple decimal points in constant\n");
				goto n_end;
			}
			dec_state = S_INIT;
			tgt_val = &frac_val;
			continue;
		} else if (digit == 'e' && base != 16) {
			if (exp_state != S_NONE) {
				printf("ERROR: Multiple exponents in constant\n");
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
			if (base == 10) {
				goto n_end;
			}
			dval = 10 + (digit - 'a');
		case '9': case '8':
			if (base == 8) {
				printf("ERROR: Invalid digit in octal constant\n");
				goto n_end;
			}
		case '7': case '6': case '5':
		case '4': case '3': case '2':
			if (base == 2) {
				printf("ERROR: Invalid digit in binary constant\n");
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

n_end:  if (dec_state != S_NONE && base != 10) {
		printf("ERROR: Decimal point in non-base 10 constant\n");
	}
	if (exp_state == S_INIT) {
		printf("ERROR: Missing exponent in constant\n");
	}

	/*
	//Read suffix
	s_type *ty;
	if (dec_state != S_NONE || exp_state == S_DONE) {
		v_type = TYPE_FLOAT;
	} else {
		v_type = TYPE_INT;
	}

	char *sffx_beg = cur;
	while (isalpha(*cur)) {
		switch (tolower(*cur)) {
			case 'l':
				if (t->kind == TYPE_INT)
					t->size = 8; //long
				else if (v_type == TYPE_LONG)
					t->size = TYPE_LONG_LONG;
				break;
			case 'u':
				t->is_signed = 0;
				break;
		}
		cur++;
	}
	*/

	//TODO: verification
	int len = cur - lx->tgt->pos;

	return len;
}

void lex_next(lexer *lx, int m_exp) {
	token t = {-1, -1, NULL};
	lex_target *tgt = lx->tgt;

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
	int nline = (tgt->pos == tgt->buf && tgt->type == TGT_FILE);
reset:	while (isspace(*tgt->pos)) {
		if (*tgt->pos == '\n') nline = 1;
		tgt->pos++;
	}
	t.nline = nline;

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
		case '.':	t.type = TOK_PRD; 	break;
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
			t.str = malloc(len+1);
			memcpy(t.str, (begin+1), len);
			t.str[len] = '\0';
			cur++;
			break;
		case '#':
			t.type = TOK_SNS;
			if (*cur == '#') {
				cur++, t.type = TOK_DNS;
			} else if (!nline) {
				printf("Error: stray '#' in source\n");
			}
			break;
		case '?': 	t.type = TOK_QMK; 	break;
		case '+':
			t.type = TOK_ADD;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_ADD;
			} else if (*cur == '+') {
				cur++, t.type = TOK_INC;
			}
			break;
		case '-':
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
						puts("ERROR: Unterminated comment");
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
			int kw_id = (long long)map_get(&kw_table, t.str); 
			if (kw_id > 0)
				t.type = kw_id;

			//Check for macro expansion
			symbol *s = symtable_search(&lx->stb, t.str);
			if (m_exp && s != NULL && s->mac_exp != NULL) {
				//Check target stack to prevent reexpansion
				int expanded = 0;
				for (lex_target *i = tgt; i != NULL; i = i->prev) {
					if (i->type == TGT_MACRO && !strcmp(i->name, t.str)) {
						expanded = 1;
						break;
					}
				}
				if (expanded) break;

				//Expand the macro
				tgt->pos = cur;
				lexer_tgt_open(lx, t.str, TGT_MACRO, s->mac_exp);
				tgt = lx->tgt;

				free(t.str);
				t.str = NULL;
				goto reset;
			}

			//If the token is an identifier, free the string
			if (kw_id != 0) {
				free(t.str);
				t.str = NULL;
			}

			} break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			//Read number
			cur--;
			int len = lex_num(lx, &t);
			cur += len;

			//printf("%s -> %f\n", t.str, atof(t.str));
			t.type = TOK_CONST;
			break;
		default: 
			//illegal char
			t.type = TOK_END;
			break;
	}
end:	tgt->pos = cur;
	lx->ahead = t;
	
	return;
}

//Perform preprocessing
void lex_ppd(lexer *lx) {
	//Read directive
	lex_next(lx, 0);
	switch (lex_peek(lx).type) {
		case TOK_KW_DEFINE:  ppd_define(lx);			break;
		case TOK_KW_ELIF:    puts("ERROR: #elif before #if");	break;
		case TOK_KW_ELSE:    puts("ERROR: #else before #if");	break;
		case TOK_KW_ENDIF:   puts("ERROR: #endif before #if");	break;
		case TOK_KW_ERROR:   //ppd_error(lx);			break;
		case TOK_KW_IF:      //ppd_if(lx);			break;
		case TOK_KW_IFDEF:   //ppd_ifdef(lx);			break;
		case TOK_KW_IFNDEF:  /*ppd_ifndef(lx);*/		break;
		case TOK_KW_INCLUDE: ppd_include(lx);			break;
		case TOK_KW_LINE:    break;
		case TOK_KW_PRAGMA:  break;
		case TOK_KW_UNDEF:   ppd_undef(lx);			break;
		default:
			puts("ERROR: Invalid preprocessor directive");
			break;
	}

	return;
}

token lex_peek(lexer *lx) {
	if (lx->ahead.type < 0)
		lex_adv(lx);
	return lx->ahead;
}

void lex_adv(lexer *lx) {
reset:	lex_next(lx, 1);
	if (lx->ahead.type == TOK_SNS) {
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
