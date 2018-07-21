#ifndef LEX_H
#define LEX_H

#include "sym.h"

//lex_adv -> lex_text or lex_macro
//	if '#' -> lex_preprocess

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
	TOK_KW_IF,
	TOK_KW_ELSE,
	TOK_KW_SWITCH,
	TOK_KW_WHILE,
	TOK_KW_FOR,
	TOK_KW_DO,
	TOK_KW_CASE,
	TOK_KW_DEFAULT,
	TOK_KW_GOTO,
	TOK_KW_CONTINUE,
	TOK_KW_BREAK,
	TOK_KW_RETURN,

	//Type specifiers
	TOK_KW_VOID,
	TOK_KW_CHAR,
	TOK_KW_SHORT,
	TOK_KW_INT,
	TOK_KW_LONG,
	TOK_KW_FLOAT,
	TOK_KW_DOUBLE,

	TOK_KW_STRUCT,
	TOK_KW_UNION,
	
	//Type storage classes
	TOK_KW_TYPEDEF,
	TOK_KW_EXTERN,
	TOK_KW_STATIC,
	TOK_KW_AUTO,
	TOK_KW_REGISTER,

	//Type qualifiers
	TOK_KW_CONST,
	TOK_KW_VOLATILE,

	//Preprocessor directives
	TOK_PP_DEFINE,
	TOK_PP_ELIF,
	TOK_PP_ELSE,
	TOK_PP_ENDIF,
	TOK_PP_ERROR,
	TOK_PP_IF,
	TOK_PP_IFDEF,
	TOK_PP_IFNDEF,
	TOK_PP_INCLUDE,
	TOK_PP_LINE,
	TOK_PP_PRAGMA,
	TOK_PP_UNDEF,

	//Special tokens
	TOK_CONST,
	TOK_STR,
	TOK_KW,
	TOK_IDENT
};

typedef struct TOKEN {
	int type;
	int kw;
	char *str;
} token;

typedef struct LEX_TARGET {
	char *buf;
	char *pos;
	int dealloc; //Deallocate when target is closed
	struct MAP exp;	//List of expanded macros
	struct LEX_TARGET *prev;
} lex_target;

typedef struct LEXER {
	struct LEX_TARGET *tgt;
	struct TOKEN ahead;
	struct SYMTABLE stb;
} lexer;

struct LEXER *lexer_init(char *fname);
void lexer_close(struct LEXER *lx);
void lexer_tgt_open(struct LEXER *lx, char *buf, int dealloc);
void lexer_tgt_close(struct LEXER *lx);

token lex_peek(struct LEXER *lx);
void lex_adv(struct LEXER *lx);
int is_type_spec(struct TOKEN t);

#endif
