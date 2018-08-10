#ifndef LEX_H
#define LEX_H

#include "sym.h"

enum TOK_TYPES {
	TOK_END,
	TOK_SEM,
	TOK_COL,
	TOK_CMM,
	TOK_PRD,
	TOK_LBR,
	TOK_RBR,
	TOK_LPR,
	TOK_RPR,
	TOK_LBK,
	TOK_RBK,
	TOK_SQT,
	TOK_DQT,
	TOK_SNS,
	TOK_DNS,
	TOK_QMK,
	TOK_ADD,
	TOK_SUB,
	TOK_ASR,
	TOK_DIV,
	TOK_MOD,
	TOK_AND,
	TOK_OR,
	TOK_XOR,
	TOK_NOT,
	TOK_TLD,
	TOK_LTH,
	TOK_GTH,
	TOK_LEQ,
	TOK_GEQ,
	TOK_EQ,
	TOK_NEQ,
	TOK_LOGIC_OR,
	TOK_LOGIC_AND,
	TOK_LOGIC_NOT,
	TOK_INC,
	TOK_DEC,
	TOK_PTR,
	TOK_LSHIFT,
	TOK_RSHIFT,
	TOK_ASSIGN,
	TOK_ASSIGN_ADD,
	TOK_ASSIGN_SUB,
	TOK_ASSIGN_MUL,
	TOK_ASSIGN_DIV,
	TOK_ASSIGN_MOD,
	TOK_ASSIGN_AND,
	TOK_ASSIGN_OR,
	TOK_ASSIGN_XOR,
	TOK_ASSIGN_LSHIFT,
	TOK_ASSIGN_RSHIFT,

	//Keywords
	#define kw(id, str) id,
	#include "kw.inc"
	#undef kw

	//Special tokens
	TOK_CONST,
	TOK_STR,
	TOK_KW,
	TOK_IDENT
};

void init_kwtable();
void close_kwtable();

enum TGT_TYPE {
	TGT_FILE,
	TGT_MACRO
};

typedef struct TOKEN {
	int type;
	int nline;	//Preceeded by newline
	char *str;
} token;

//Used for ungetting tokens
typedef struct TOKEN_NODE {
	struct TOKEN t;
	struct TOKEN_NODE *next;
} token_n;

typedef struct LEX_TARGET {
	char *name;			//File/macro name
	int type;			//File or macro
	char *buf;			//Beginning of buffer
	char *pos;			//Current position in buffer
	struct LEX_TARGET *prev;	//Pointer to previous target
} lex_target;

typedef struct LEXER {
	struct LEX_TARGET *tgt;		//Input stack
	struct TOKEN_NODE *pre;		//Pre-lexed tokens
	struct TOKEN ahead;		//Current token
	struct SYMTABLE stb;		//Symbol table
	int lnum;
	int cnum;
} lexer;

struct LEXER *lexer_init(char *fname);
int lex_open_file(lexer *lx, char *fname);
void lexer_close(struct LEXER *lx);
void lexer_tgt_open(lexer *lx, char *name, int type, char *buf);
void lexer_tgt_close(struct LEXER *lx);

//Used internally by lexer/preprocessor
void lex_next(struct LEXER *lx, int m_exp);

struct TOKEN lex_peek(struct LEXER *lx);
void lex_adv(struct LEXER *lx);
void lex_unget(struct LEXER *lx, struct TOKEN_NODE *t);

int is_type_spec(struct TOKEN t);
struct TOKEN_NODE *make_tok_node(struct TOKEN t);

#endif
