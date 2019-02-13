#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "err.h"
#include "sym.h"
#include "lex.h"
#include "eval.h"
#include "util/vector.h"

//Returns the precedence of each operation
int rankof(int op) {
	switch (op) {
		case TOK_QMK:		
		case TOK_COL:		return 12;
		case TOK_DEFINED:	return 11;
		case TOK_LOGIC_NOT:
		case TOK_NOT:		return 10;
		case TOK_ASR:
		case TOK_DIV:
		case TOK_MOD:		return 9;
		case TOK_ADD:
		case TOK_SUB:		return 8;
		case TOK_LSHIFT:
		case TOK_RSHIFT:	return 7;
		case TOK_EQ: 
		case TOK_NEQ:		return 6;
		case TOK_AND:		return 5;
		case TOK_XOR:		return 4;
		case TOK_OR:		return 3;
		case TOK_LOGIC_AND:	return 2;
		case TOK_LOGIC_OR:	return 1;
		case TOK_CMM:		return 0;
	}
	return -1;
}

//Returns 1 if the operator is unary or 0 otherwise
int is_unary(int op) {
	switch (op) {
		case TOK_DEFINED:
		case TOK_LOGIC_NOT:
		case TOK_NOT:
			return 1;
	}
	return 0;
}

//Performs operation op on lhs and rhs and returns the result
long long do_op(int op, int lhs, int rhs, int trs) {
	switch (op) {
		case TOK_QMK:		return (lhs) ? rhs : trs;
		case TOK_LOGIC_NOT:	return !lhs;
		case TOK_NOT:		return ~lhs;
		case TOK_ASR:		return lhs * rhs;
		case TOK_DIV:		return lhs / rhs;
		case TOK_MOD:		return lhs % rhs;
		case TOK_ADD:		return lhs + rhs;
		case TOK_SUB:		return lhs - rhs;
		case TOK_LSHIFT:	return lhs << rhs;
		case TOK_RSHIFT:	return lhs >> rhs;
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
 * Reads and evaluates a constant expression from the lexer
 * If mode is nonzero, the expression is treated as part of an #if directive
 * If an error is encountered, the integer pointed to by err is set to nonzero
 * Returns the expression's result
 */
int eval_constexpr(lexer *lx, int *err, int mode) {
	//Lexer positions
	char *cur = lx->tgt->pos;
	s_pos *loc = &lx->tgt->loc;

	//Set up calculator stacks
	vector ops, vals;
	vector_init(&ops, VECTOR_EMPTY);
	vector_init(&vals, VECTOR_EMPTY);

	//Repeat until endline or end of input
	int eflag_prev = c_errflag;
	for (;;) {
		//Skip whitespace, but stop before a newline
		while (isspace(*cur) && (*cur != '\n' && *cur != '\0'))
			cur++;
		lx->tgt->pos = cur;

		//Finish calculations if the condition has ended
		if (*cur == '\n' || *cur == '\0') {
			while (ops.len > 0) {
				if (vals.len < 1) {
					c_error(loc, "Operator in constant expression has no RHS\n");
					break;
				}
				do_calc(&ops, &vals);
			}
			if (vals.len > 1) {
				c_error(loc, "Missing operator in constant expression\n");
			}
			break;
		}
		
		//Get the next token
		lex_next(lx, 1);
		cur = lx->tgt->pos;
		token t = lex_peek(lx);

		//Handle 'defined' operation
		if ((long long)vector_top(&ops) == TOK_DEFINED) {
			vector_pop(&ops);
			if (t.type != TOK_IDENT) {
				c_error(loc, "Expected identifier after 'defined'\n");
				continue;
			}

			//Search the symbol table to check if the identifier is defined
			long long res = (symtable_search(&lx->stb, t.dat.sval) != NULL);
			vector_push(&vals, (void*)res);
			continue;
		}

		//Use token as new input for evaluation
		if (t.type == TOK_CONST) {
			//Give an error if a float is encountered
			if (t.dtype->kind == TYPE_FLOAT) {
				c_error(loc, "Floating point value in constant expression\n");
				continue;
			}
			
			//Otherwise, push the value to the stack
			vector_add(&vals, (void*)t.dat.ival);
		} else if (t.type == TOK_IDENT) {
			//If the constexpr isn't an #if conditional, error
			if (!mode) {
				c_error(&t.loc, "Identifier in constant expression\n");
				vector_add(&vals, (void*)0);
				continue;
			}

			//If the identifier is "defined," push the defined unary operator
			if (!strcmp(t.dat.sval, "defined")) {
				vector_push(&ops, (void*)TOK_DEFINED);
				continue;
			}

			//Otherwise, push zero
			vector_add(&vals, (void*)0);
		} else if (t.type == TOK_LPR) {
			//Push a left parentheses
			vector_push(&ops, (void*)(long long)t.type);
		} else if (t.type == TOK_RPR) {
			//Perform calculations until a right parentheses is reached
			while ((long long)vector_top(&ops) != TOK_LPR) {
				if (ops.len == 0) {
					c_error(loc, "Missing '(' in constant expression\n");
					break;
				}
				do_calc(&ops, &vals);
			}
			vector_pop(&ops);
		} else {
			//Check if the operator is valid
			int prec = rankof(t.type);
			if (prec < 0) {
				c_error(loc, "Invalid operation in constant expression\n");
				continue;
			}

			//Error for no LHS
			if (vals.len == 0 && !is_unary(t.type)) {
				c_error(loc, "Operator in constant expression has no LHS\n");
				continue;
			}

			//Handle special case for ternary operator
			if (t.type == TOK_COL) {
				if ((long long)vector_top(&ops) != TOK_QMK)
					c_error(loc, "Missing '?' before ':' in constant expresion\n");
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
			vector_push(&ops, (void*)(long long)t.type);
		}
	}

	//Get result and error flag
	int res = (long long)vector_pop(&vals);
	*err = !eflag_prev && c_errflag;

	//Free the stacks
	vector_close(&ops);
	vector_close(&vals);

	return res;
}
