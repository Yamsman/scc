#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lex.h"

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

lexer *lexer_open(char *fname) {
	lexer *lx = malloc(sizeof(struct LEXER));
	lx->tgt = malloc(sizeof(struct LEX_TARGET));
	lx->ahead = BLANK_TOKEN;

	//Read data from file into target
	FILE *src_f = fopen(fname, "r");
	fseek(src_f, 0, SEEK_END);
	int len = ftell(src_f);
	lx->tgt->buf = malloc(len+1);
	fseek(src_f, 0, SEEK_SET);
	fread(lx->tgt->buf, len, 1, src_f);

	lx->tgt->pos = lx->tgt->buf;
	lx->tgt->prev = NULL;
	return lx;
}

void lexer_close(lexer *lx) {

}

void lex_next(lexer *lx) {
	token t = {-1, -1, NULL};

	//Skip whitespace
	int newline = 0;
	while (isspace(*lx->tgt->pos)) {
		if (*lx->tgt->pos = '\n') newline = 1;
		lx->tgt->pos++;
	}

	//Get next char
	int len;
	char *begin = lx->tgt->pos;
	char *cur = begin;
	switch (*(cur++)) {
		case '\0': 	t.type = TOK_END; 	break;
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
			cur--;
			while (isalnum(*cur) || *cur == '_') 
				cur++;
			len = cur - begin;
			t.str = malloc(len+1);
			memcpy(t.str, begin, len);
			t.str[len] = '\0';
			t.type = TOK_IDENT;

			//Check if matches keyword
			for (int i=0; i<KW_NUM; i++) {
				if (!strcmp(t.str, kw[i])) {
					t.type = TOK_KW_IF+i;
					break;
				}
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
	lx->tgt->pos = cur;
	lx->ahead = t;
	
	return;
}

token lex_peek(lexer *lx) {
	if (lx->ahead.type < 0)
		lex_next(lx);	
	return lx->ahead;
}

int is_type_spec(token t) {
	if (t.type - TOK_KW_VOID >= 0 && 
	    t.type - TOK_KW_UNION <= TOK_KW_UNION - TOK_KW_VOID)
		return 1;
	return 0;
}

void lex_adv(lexer *lx) {
	lex_next(lx);

	//Perform preprocessing
	//if (lx->ahead.type == TOK_SNS)
		//lex_ppd(lx);

	return;
}
