#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"
#include "sym.h"
#include "lex.h"
#include "ast.h"
#include "eval.h"
#include "util/vector.h"

//Returns the precedence of each operation
int rankof(int op) {
	switch (op) {
		case TOK_DEFINED:	return 13;
		case TOK_NOT:
		case TOK_TLD:		return 12;
		case TOK_ASR:
		case TOK_DIV:
		case TOK_MOD:		return 11;
		case TOK_ADD:
		case TOK_SUB:		return 10;
		case TOK_LSHIFT:
		case TOK_RSHIFT:	return 9;
		case TOK_GTH:
		case TOK_LTH:
		case TOK_GEQ:
		case TOK_LEQ:		return 8;
		case TOK_EQ: 
		case TOK_NEQ:		return 7;
		case TOK_AND:		return 6;
		case TOK_XOR:		return 5;
		case TOK_OR:		return 4;
		case TOK_LOGIC_AND:	return 3;
		case TOK_LOGIC_OR:	return 2;
		case TOK_QMK:		
		case TOK_COL:		return 1;
		case TOK_CMM:		return 0;
	}
	return -1;
}

//Returns 1 if the operator is unary or 0 otherwise
int is_unary(int op) {
	switch (op) {
		case TOK_DEFINED:
		case TOK_NOT:
		case TOK_TLD:
			return 1;
	}
	return 0;
}

//Performs operation op on lhs and rhs and returns the result
long long do_op(int op, int lhs, int rhs, int trs) {
	switch (op) {
		case TOK_QMK:		return (lhs) ? rhs : trs;
		case TOK_NOT:		return !lhs;
		case TOK_TLD:		return ~lhs;
		case TOK_ASR:		return lhs * rhs;
		case TOK_DIV:		return lhs / rhs;
		case TOK_MOD:		return lhs % rhs;
		case TOK_ADD:		return lhs + rhs;
		case TOK_SUB:		return lhs - rhs;
		case TOK_LSHIFT:	return lhs << rhs;
		case TOK_RSHIFT:	return lhs >> rhs;
		case TOK_GTH:		return lhs > rhs;
		case TOK_LTH:		return lhs < rhs;
		case TOK_GEQ:		return lhs >= rhs;
		case TOK_LEQ:		return lhs <= rhs;
		case TOK_EQ: 		return lhs == rhs;
		case TOK_NEQ:		return lhs != rhs;
		case TOK_AND:		return lhs & rhs;
		case TOK_XOR:		return lhs ^ rhs;
		case TOK_OR:		return lhs | rhs;
		case TOK_LOGIC_AND:	return lhs && rhs;
		case TOK_LOGIC_OR:	return lhs || rhs;
		case TOK_CMM:		return lhs, rhs;
	}
	return -1;
}

//Pops one or two numbers from vals and an operator from ops, and pushes the result
void do_calc(vector *ops, vector *vals) {
	int op = (long long)vector_pop(ops);
	int trs = (op == TOK_QMK) ? (long long)vector_pop(vals) : 0;
	int rhs = (!is_unary(op)) ? (long long)vector_pop(vals) : 0;
	int lhs = (long long)vector_pop(vals);
	long long res = do_op(op, lhs, rhs, trs);
	vector_push(vals, (void*)res);
	return;
}

/* 
 * Evaluates a constant expression passed as a vector of tokens in infix order
 * The tokens are deallocated as they are processed
 * If an error is encountered, the integer pointed to by err is set to nonzero
 * Returns the expression's result
 * TODO: pass input as a contiguous array of tokens to reduce memory usage
 */
int eval_constexpr(lexer *lx, vector *input, int *err) {
	//Initialize stacks
	vector ops, vals;
	vector_init(&ops, VECTOR_DEFAULT);
	vector_init(&vals, VECTOR_DEFAULT);

	//Process input token by token
	int i = 0;
	int eflag_prev = c_errflag;
	s_pos *last_loc = NULL;
	for (; i<input->len; i++) {
		token *t = input->table[i];
		last_loc = &t->loc;
		if (t->type == TOK_CONST) {
			//Ensure the constant is an integer
			if (t->dtype->kind == TYPE_FLOAT) {
				c_error(&t->loc, "Floating point value in constant expression\n");
				continue;
			}

			//Push the operand
			vector_push(&vals, (void*)t->dat.ival);
		} else if (t->type == TOK_DEFINED) {
			//Move to the next token and check for an identifier
			token *ident = input->table[++i];
			if (i >= input->len || ident->type != TOK_IDENT) {
				c_error(&t->loc, "Expected identifier after 'defined'\n");
				continue;
			}

			//Search the symbol table to check if the identifier is defined
			long long res = (symtable_search(&lx->stb, t->dat.sval) != NULL);
			vector_push(&vals, (void*)res);
		} else if (t->type == TOK_IDENT) {
			vector_add(&vals, (void*)0);
		} else if (t->type == TOK_LPR) {
			//Push a left parentheses
			vector_push(&ops, (void*)(long long)t->type);
		} else if (t->type == TOK_RPR) {
			//Perform calculations until a right parentheses is reached
			while ((long long)vector_top(&ops) != TOK_LPR) {
				if (ops.len == 0) {
					c_error(&t->loc, "Missing '(' in constant expression\n");
					break;
				}
				do_calc(&ops, &vals);
			}
			vector_pop(&ops);
		} else {
			//Check if the operator is valid
			int op = t->type;
			int prec = rankof(t->type);
			if (prec < 0) {
				c_error(&t->loc, "Invalid operation in constant expression\n");
				continue;
			}

			//Error for no LHS
			if (vals.len == 0 && !is_unary(t->type)) {
				c_error(&t->loc, "Operator in constant expression has no LHS\n");
				continue;
			}

			//Handle special case for ternary operator
			if (t->type == TOK_COL) {
				int has_qmk = 0;
				for (int j=0; j<ops.len; j++)
					if ((long long)(ops.table[j]) == TOK_QMK) has_qmk = 1;
				if (!has_qmk)
					c_error(&t->loc, "Missing '?' before ':' in "
							"constant expresion\n");
				continue;
			}

			//Repeat until the current operation's precedence is less than
			//the precedence of the operator at the top of the stack
			for (;;) {
				if (ops.len < 1) break;
				int top_prec = rankof((long long)vector_top(&ops));
				if (prec >= top_prec) break;
				do_calc(&ops, &vals);
			}
			vector_push(&ops, (void*)(long long)t->type);
		}
	}

	//Finish calculations if the condition has ended
	while (ops.len > 0) {
		if (vals.len < 1) {
			c_error(last_loc, "Operator in constant expression has no RHS\n");
			break;
		}
		do_calc(&ops, &vals);
	}
	if (vals.len > 1) {
		c_error(last_loc, "Missing operator in constant expression\n");
	}

	//Get result
	int res = (long long)vector_top(&vals);
	*err = !eflag_prev && c_errflag;
	vector_close(&ops);
	vector_close(&vals);

	return res;
}

/* 
 * Evaluates a constant expression passed as a syntax tree
 * If an error is encountered, the integer pointed to by err is set to nonzero
 * Returns the expression's result
 */
int _eval_constexpr_ast(ast_n *node, int *err) {
	if (node == NULL) return 0;

	//Perform action
	s_pos *loc = &node->tok.loc;
	int l_res = _eval_constexpr_ast(node->dat.expr.left, err);
	int r_res = _eval_constexpr_ast(node->dat.expr.right, err);
	int t_res = 0;
	switch (node->dat.expr.kind) {
		case EXPR_COND: {
			ast_n *res = node->dat.expr.right;
			r_res = _eval_constexpr_ast(res->dat.expr.left, err);
			t_res = _eval_constexpr_ast(res->dat.expr.right, err);
			}
			return do_op(node->tok.type, l_res, r_res, t_res); 
		case EXPR_COND_RES:
			return 0;
		case EXPR_LOGIC_OR:
		case EXPR_LOGIC_AND:
		case EXPR_OR:
		case EXPR_XOR:
		case EXPR_AND:
		case EXPR_GTH:
		case EXPR_LTH:
		case EXPR_GEQ:
		case EXPR_LEQ:
		case EXPR_LSHIFT:
		case EXPR_RSHIFT:
		case EXPR_EQ:
		case EXPR_NEQ:
		case EXPR_ADD:
		case EXPR_SUB:
		case EXPR_MUL:
		case EXPR_DIV:
			return do_op(node->tok.type, l_res, r_res, 0);
		case EXPR_MOD:
		case EXPR_POS:
		case EXPR_NEG:
		case EXPR_NOT:
		case EXPR_LOGIC_NOT:
			return do_op(node->tok.type, l_res, 0, 0);
			break;
		case EXPR_CAST:
		case EXPR_REF:
		case EXPR_DEREF:
		case EXPR_INC_PRE:
		case EXPR_DEC_PRE:
		case EXPR_ARRAY:
		case EXPR_CALL:
		case EXPR_MEMB:
		case EXPR_PTR_MEMB:
		case EXPR_INC_POST:
		case EXPR_DEC_POST:
			c_error(loc, "Invalid operation in constant expression\n");
			return 0;
		case EXPR_IDENT:
			c_error(loc, "Identifier in constant expression\n");
			return 0;
		case EXPR_CONST:
			if (node->tok.dtype->kind == TYPE_FLOAT)
				c_error(loc, "Floating point value in constant expression\n");
			return node->tok.dat.ival;
		default:
			c_error(loc, "Invalid constant expression\n");
			break;
	}
	return -1;
}

int eval_constexpr_ast(ast_n *root, int *err) {
	int eflag_prev = c_errflag;
	int res = _eval_constexpr_ast(root, err);
	*err = !eflag_prev && c_errflag;
	return res;
}
