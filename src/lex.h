#ifndef LEX_H
#define LEX_H

#include "sym.h"
#include "err.h"
#include "util/vector.h"

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
	TOK_DEFINED,
	TOK_CONST,
	TOK_STR,
	TOK_KW,
	TOK_IDENT
};

enum TGT_TYPE {
	TGT_FILE,
	TGT_MACRO
};

typedef struct TOKEN {
	int type;			//Kind of token
	int nline;			//Preceeded by newline
	struct SRC_POS loc;		//Location in file

	union { 			//Data for literals
		long long ival;		//Integer constant
		unsigned long long uval;//Unsigned integer constant
		double fval;		//Float constant
		char *sval;		//String literal
	} dat;
	struct TYPE *dtype;		//Type of literal
} token;

//Used for ungetting tokens
typedef struct TOKEN_NODE {
	struct TOKEN t;			//Token
	struct TOKEN_NODE *next;	//Pointer to next token
} token_n;

typedef struct LEX_TARGET {
	char *name;			//File/macro name
	int type;			//File or macro
	char *buf;			//Beginning of buffer
	char *pos;			//Current position in buffer
	char cch;			//The current character, after preprocessing
	struct SRC_POS loc;		//Current location in file
	struct LEX_TARGET *prev;	//Pointer to previous target
} lex_target;

typedef struct LEX_CONDITION {
	int is_true;			//Conditional is currently true
	int was_true;			//Conditional has already been true once
	int has_else;			//Else has been encountered
} lex_cond;

typedef struct LEXER {
	struct LEX_TARGET *tgt;		//Input stack
	struct TOKEN_NODE *pre;		//Pre-lexed tokens
	struct TOKEN ahead;		//Current token
	struct VECTOR flist;		//List of processed files
	struct VECTOR conds;		//Preprocessor conditions
	struct SYMTABLE stb;		//Symbol table
} lexer;

extern struct TOKEN BLANK_TOKEN;

void init_kwtable();
void close_kwtable();

struct LEXER *lexer_init(char *fname);
int lex_open_file(struct LEXER *lx, char *fname);
void lexer_close(struct LEXER *lx);
void lexer_tgt_open(struct LEXER *lx, char *name, int type, char *buf);
void lexer_tgt_close(struct LEXER *lx);
void lexer_add_cond(struct LEXER *lx, int pass);
void lexer_del_cond(struct LEXER *lx);

//Used internally to process individual characters
char lex_cur(struct LEXER *lx);
char lex_nchar(struct LEXER *lx, int *len, struct SRC_POS *loc);
char lex_adv_char(struct LEXER *lx);

//Used internally for processing tokens
void lex_next(struct LEXER *lx, int m_exp);
void lex_ident(struct LEXER *lx, struct TOKEN *t);
void lex_num(struct LEXER *lx, struct TOKEN *t);
int lex_wspace(struct LEXER *lx);
int lex_expand_macro(struct LEXER *lx, struct TOKEN t);

//Used externally to control the lexer
struct TOKEN lex_peek(struct LEXER *lx);
void lex_adv(struct LEXER *lx);
void lex_unget(struct LEXER *lx, struct TOKEN_NODE *t);
struct SRC_POS lex_loc(struct LEXER *lx);

int is_type_spec(struct TOKEN t);
struct TOKEN_NODE *make_tok_node(struct TOKEN t);

#endif
