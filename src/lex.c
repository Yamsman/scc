#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lex.h"
#include "sym.h"
#include "util/map.h"

//TODO: struct for holding source file

//keywords
#define KW_NUM 28
char *kw[KW_NUM] = {
	"if",
	"else",
	"switch",
	"while",
	"for",
	"do",
	"case",
	"default",
	"goto",
	"continue",
	"break",
	"return",

	"void",
	"char",
	"short",
	"int",
	"long",
	"float",
	"double",

	"struct",
	"union",
	
	"typedef",
	"extern",
	"static",
	"auto",
	"register",
	
	"const",
	"volatile"
};

token BLANK_TOKEN = {-1, -1, NULL};

lexer *lexer_init(char *fname) {
	lexer *lx = malloc(sizeof(struct LEXER));
	lx->ahead = BLANK_TOKEN;
	symtable_init(&lx->stb);

	//Read data from file into target
	FILE *src_f = fopen(fname, "r");
	fseek(src_f, 0, SEEK_END);
	int len = ftell(src_f);
	char *buf = malloc(len+1);
	fseek(src_f, 0, SEEK_SET);
	fread(buf, len, 1, src_f);

	lexer_tgt_open(lx, buf, 0);
	return lx;
}

void lexer_close(lexer *lx) {

}

void lexer_tgt_open(lexer *lx, char *buf, int dealloc) {
	lex_target *n_tgt = malloc(sizeof(struct LEX_TARGET));
	n_tgt->dealloc = dealloc;
	n_tgt->buf = buf;
	n_tgt->pos = buf;
	map_init(&n_tgt->exp, 16);

	if (lx->tgt != NULL)
		n_tgt->prev = lx->tgt;
	lx->tgt = n_tgt;
	return;
}

void lexer_tgt_close(lexer *lx) {
	lex_target *tgt = lx->tgt;
	if (tgt->dealloc)
		free(tgt->buf);
	map_close(&tgt->exp);
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

void lex_next(lexer *lx) {
	token t = {-1, -1, NULL};
	lex_target *tgt = lx->tgt;

	//Skip whitespace
	int newline = 0;
reset:	while (isspace(*tgt->pos)) {
		if (*tgt->pos == '\n') newline = 1;
		tgt->pos++;
	}

	//Get next char
	int len;
	char *begin = tgt->pos;
	char *cur = begin;
	switch (*(cur++)) {
		case '\0':
			//End lexing if end has been reached
			//Otherwise, go to previous target
			if (tgt->prev == NULL)  {
				t.type = TOK_END;
				break;
			}
			lexer_tgt_close(lx);
			tgt = lx->tgt;
			goto reset;
		case ';': 	t.type = TOK_SEM; 	break;
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
			} else if (!newline) {
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
		case '/': //TODO: comments
			t.type = TOK_DIV;
			if (*cur == '=') {
				cur++, t.type = TOK_ASSIGN_DIV;
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
		case 'Z': case '_':
			//Read identifier
			cur--;
			int len = lex_ident(lx, &t);
			cur += len;

			//Check if matches keyword
			//TODO: put identifiers inside map
			for (int i=0; i<KW_NUM; i++) {
				if (!strcmp(t.str, kw[i])) {
					t.type = TOK_KW_IF+i;
					break;
				}
			}

			//Check for macro expansion
			symbol *s = symtable_search(&lx->stb, t.str);
			if (s != NULL && s->type->kind == TYPE_MACRO) {
				if (map_get(&tgt->exp, t.str) != NULL) break;
				tgt->pos = cur;
				lexer_tgt_open(lx, s->exp, 1);
				map_insert(&tgt->exp, t.str, s);
				tgt = lx->tgt;
				free(t.str);
				goto reset;
			}
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			//Read number
			cur--;
			int has_pt = 0, has_ex = 0;
			for (;;cur++) {
				if (isdigit(*cur)) {
					if (has_ex == 1 || has_ex == 2)
						has_ex = 3;
				} else if (*cur == '.') {
					if (has_ex) {
							
					}
					has_pt++;	
				} else if (*cur == 'e' || *cur == 'E') {
					if (has_ex != 0)
						continue;
					has_ex++;
				} else if (*cur == '-' || *cur == '+') {
					if (has_ex != 1)
						continue;
					has_ex++;
				} else {
					break;
				}
			}

			//TODO: verification
			int llen = cur - begin;
			t.str = malloc(llen+1);
			memcpy(t.str, begin, llen);
			t.str[llen] = '\0';

			//printf("%s -> %f\n", t.str, atof(t.str));
			t.type = TOK_CONST;
			break;
		default: 
			//illegal char
			t.type = TOK_END;
			break;
	}
	tgt->pos = cur;
	lx->ahead = t;
	
	return;
}

//Perform preprocessing
void lex_ppd(lexer *lx) {
	//Read directive
	lex_next(lx);
	token direc = lex_peek(lx);
	if (direc.type != TOK_IDENT)
		puts("invalid preprocessor directive");

	if (!strcmp(direc.str, "define")) {
		//Read macro name
		lex_next(lx);
		token mname = lex_peek(lx);
		if (mname.type != TOK_IDENT)
			puts("invalid macro name");

		//Get bounds of macro body
		char *cur = lx->tgt->pos;
		char *start, *end;
		while (isspace(*cur)) cur++;
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
		char *buf = malloc(len+1);
		memcpy(buf, start, len);
		buf[len] = '\0';
		for (int i=0; i<len; i++) {
			if (buf[0] == '#' && buf[1] == '#' ||
			    buf[len-2] == '#' && buf[len-1] == '#')
				puts("'##' at beginning or end of macro");
		}

		//Add to symtable
		symbol *sym = symtable_add(&lx->stb, mname.str, type_new(TYPE_MACRO));
		sym->exp = buf;
		lx->tgt->pos = end;
	} else {
		puts("invalid preprocessor directive");

	}
}

token lex_peek(lexer *lx) {
	if (lx->ahead.type < 0)
		lex_adv(lx);
	return lx->ahead;
}

int is_type_spec(token t) {
	if (t.type - TOK_KW_VOID >= 0 && 
	    t.type - TOK_KW_UNION <= TOK_KW_UNION - TOK_KW_VOID)
		return 1;
	return 0;
}

void lex_adv(lexer *lx) {
reset:	lex_next(lx);

	//Perform preprocessing
	if (lx->ahead.type == TOK_SNS) {
		lex_ppd(lx);
		goto reset;
	}

	return;
}
