#ifndef LEX_H
#define LEX_H

#include "sym.h"
#include "err.h"
#include "util/vector.h"

enum TOK_TYPES {
	//Get token and keyword types from definitions in tok.inc
	#define tok(id, estr, str) id,
	#define kw(id, estr, str) id,
	#include "tok.inc"
	#undef kw
	#undef tok
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
	struct VECTOR mp_exp;		//Parameter expansions; parallel with macro parameter list
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
	int m_exp;			//Flag for enabling/disabling macro expansion
	int m_cexpr;			//Flag for ignoring parameter-less macro errors
} lexer;

//Keyword table initialization functions
extern struct TOKEN BLANK_TOKEN;
void init_kwtable();
void close_kwtable();

//Lexer top-level context control functions
struct LEXER *lexer_init(char *fname);
int lex_open_file(struct LEXER *lx, char *fname);
void lexer_close(struct LEXER *lx);
void lexer_tgt_open(struct LEXER *lx, char *name, int type, char *buf);
void lexer_tgt_close(struct LEXER *lx);
void lexer_add_cond(struct LEXER *lx, int pass);
void lexer_del_cond(struct LEXER *lx);

//Used internally to process individual characters
char lex_prcc(struct LEXER *lx, char **cpos, char *cch, struct SRC_POS *cloc);
char lex_nchar(struct LEXER *lx, int *len, struct SRC_POS *loc, int pflag);
char lex_cur(struct LEXER *lx);
char lex_peekc(struct LEXER *lx);
char lex_advc(struct LEXER *lx);

//Used internally for processing tokens
void lex_next(struct LEXER *lx);
void lex_ident(struct LEXER *lx, struct TOKEN *t);
void lex_num(struct LEXER *lx, struct TOKEN *t);
void lex_str(struct LEXER *lx, struct TOKEN *t);
int lex_wspace(struct LEXER *lx);
int lex_expand_macro(struct LEXER *lx, char *ident, struct SRC_POS *loc);

//Used externally to control the lexer
struct TOKEN lex_peek(struct LEXER *lx);
void lex_adv(struct LEXER *lx);
void lex_unget(struct LEXER *lx, struct TOKEN_NODE *t);

//Helper functions
int is_type_spec(struct LEXER *lx, struct TOKEN t);
struct TOKEN_NODE *make_tok_node(struct TOKEN t);
const char *tok_str(token t, int nflag);
void lexer_debug();

#endif
