#ifndef AST_H
#define AST_H

#include "err.h"
#include "lex.h"

/*
 * Node enums
 */
enum NODE_KIND {
	DECL,
	EXPR,
	STMT
};

enum DECL_KIND {
	DECL_STD,
	DECL_FUNC
};

enum EXPR_KIND {
	EXPR_ASSIGN,
	EXPR_LOGIC_OR,
	EXPR_LOGIC_AND,
	EXPR_COND,
	EXPR_COND_RES,
	EXPR_OR,
	EXPR_XOR,
	EXPR_AND,
	EXPR_GTH,
	EXPR_LTH,
	EXPR_GEQ,
	EXPR_LEQ,
	EXPR_LSHIFT,
	EXPR_RSHIFT,
	EXPR_EQ,
	EXPR_NEQ,
	EXPR_ADD,
	EXPR_SUB,
	EXPR_MUL,
	EXPR_DIV,
	EXPR_MOD,
	EXPR_CAST,
	EXPR_REF,
	EXPR_DEREF,
	EXPR_POS,
	EXPR_NEG,
	EXPR_NOT,
	EXPR_LOGIC_NOT,
	EXPR_INC_PRE,
	EXPR_DEC_PRE,
	EXPR_ARRAY,
	EXPR_CALL,
	EXPR_MEMB,
	EXPR_PTR_MEMB,
	EXPR_INC_POST,
	EXPR_DEC_POST,
	EXPR_IDENT,
	EXPR_CONST,
	EXPR_STR
};

enum STMT_KIND {
	STMT_EXPR,
	STMT_CMPD,
	STMT_IF,
	STMT_SWITCH,
	STMT_WHILE,
	STMT_FOR,
	STMT_DO_WHILE,
	STMT_LABEL,
	STMT_CASE,
	STMT_DEFAULT,
	STMT_GOTO,
	STMT_CONTINUE,
	STMT_BREAK,
	STMT_RETURN
};

/*
 * Main node structure
 */
typedef struct AST_NODE {
	int kind;
	union {
		struct AST_DECL {
			int kind;
			struct SYMBOL *sym;
			struct AST_NODE *init; //expr
			struct AST_NODE *block; //stmt
			struct AST_NODE *next; //decl
		} decl;

		struct AST_EXPR {
			int kind;
			struct SYMBOL *sym;
			struct AST_NODE *left; //expr
			struct AST_NODE *right; //expr
		} expr;

		struct AST_STMT {
			int kind;
			char *lbl;
			struct AST_NODE *expr; //expr
			struct AST_NODE *body; //stmt
			struct AST_NODE *else_body; //stmt
		} stmt;
	} dat;
	struct TOKEN tok;
	struct AST_NODE *next;
} ast_n;

typedef struct AST_DECL ast_decl;
typedef struct AST_EXPR ast_expr;
typedef struct AST_STMT ast_stmt;

struct AST_NODE *astn_new(int kind, int s_kind, token t);
void astn_del(struct AST_NODE *node);

#endif
