#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include "lex.h"
#include "ast.h"

ast_n *astn_new(int kind, int s_kind, token t) {
	ast_n *node = calloc(1, sizeof(struct AST_NODE));
	node->kind = kind;

	switch (kind) {
		case DECL:
			node->dat.decl.kind = s_kind;
			break;
		case EXPR:
			node->dat.expr.kind = s_kind;
			break;
		case STMT:
			node->dat.stmt.kind = s_kind;
			break;
	}
	node->tok = t;

	return node;
}

void astn_del(ast_n *node) {
	if (node == NULL)
		return;

	//note: don't delete symbols here
	switch (node->kind) {
		case DECL:
			astn_del(node->dat.decl.init);
			astn_del(node->dat.decl.block);
			astn_del(node->dat.decl.next);
			break;
		case EXPR:
			if (node->dat.expr.type != NULL)
				type_del(node->dat.expr.type);
			astn_del(node->dat.expr.left);
			astn_del(node->dat.expr.right);
			break;
		case STMT:
			if (node->dat.stmt.lbl != NULL)
				free(node->dat.stmt.lbl);
			astn_del(node->dat.stmt.expr);
			astn_del(node->dat.stmt.body);
			astn_del(node->dat.stmt.else_body);
			break;
	}
	astn_del(node->next);

	//Free node data type tags
	if (node->tok.dtype != NULL)
		type_del(node->tok.dtype);

	free(node);
	return;
}

//Retrieves the resultant type of evaluating an expression
//Currently, the returned s_type will have to be freed by the caller in all cases
s_type *astn_type(ast_n *node) {
	if (node == NULL) return NULL;
	if (node->kind != EXPR) return NULL;
	switch (node->dat.expr.kind) {
		//Expressions that always result in an integer
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
		case EXPR_MOD:	
		case EXPR_NOT:	
		case EXPR_LOGIC_NOT:
			return type_new(TYPE_INT);

		//Binary operations
		case EXPR_ADD:	
		case EXPR_SUB:	
		case EXPR_MUL:	
		case EXPR_DIV: {
			//Get types of LHS and RHS
			s_type *lhs = astn_type(node->dat.expr.left);
			s_type *rhs = astn_type(node->dat.expr.right);

			/*
			 * Valid cases:
			 * int/int (int)
			 * int/ptr ptr/int (ptr)
			 * int/float float/int float/float (float)
			 */
			s_type *type = NULL;
			if (lhs->kind == TYPE_FLOAT || rhs->kind == TYPE_FLOAT) {
				if (lhs->kind != TYPE_PTR && rhs->kind != TYPE_PTR)
					type = type_new(TYPE_FLOAT);
			}
			if (lhs->kind == TYPE_INT || rhs->kind == TYPE_INT) {
				if (lhs->kind == TYPE_PTR) {
					type = type_clone(lhs);
				} else if (rhs->kind == TYPE_PTR) {
					type = type_clone(rhs);
				} else {
					type = type_new(TYPE_INT);
				}
			}

			type_del(lhs);
			type_del(rhs);
			return type;
			}

		//Unary operations
		case EXPR_POS:	
		case EXPR_NEG:
		case EXPR_INC_PRE:
		case EXPR_DEC_PRE:
		case EXPR_INC_POST:
		case EXPR_DEC_POST:
			return astn_type(node->dat.expr.left);

		//Cast operation resulting in the casted type
		case EXPR_CAST:
			return type_clone(node->dat.expr.type);

		//Conditional
		case EXPR_COND:
			//Move to the result and fall through
			node = node->dat.expr.right;
		case EXPR_COND_RES: {
			s_type *lhs = astn_type(node->dat.expr.left);
			s_type *rhs = astn_type(node->dat.expr.right);
			
			//Ensure the two possible types are compatible
			s_type *type = NULL;
			if (lhs->kind == rhs->kind)
				type = type_clone(lhs);

			type_del(lhs);
			type_del(rhs);
			return type;
			}

		//Referencing
		case EXPR_REF: {
			//Add a new reference to the front of the pointer chain
			s_type *type = astn_type(node->dat.expr.left);
			s_type *r_type = type_new(TYPE_PTR);
			r_type->ref = type;
			return r_type;
		}

		//Dereferencing 
		case EXPR_DEREF:
		case EXPR_ARRAY: {
			//Follow the type chain
			s_type *type = astn_type(node->dat.expr.left);
			s_type *d_type = NULL;
			if (type != NULL) d_type = type->ref;
			type->ref = NULL;
			type_del(type);
			return d_type;
			}

		//Struct/union member access
		case EXPR_PTR_MEMB:
		case EXPR_MEMB:	{
			s_type *st_type = astn_type(node->dat.expr.left);
			s_type *m_type = NULL;
			if (node->dat.expr.kind == EXPR_PTR_MEMB)
				st_type = st_type->ref;

			//Find matching parameter
			for (int i=0; i<st_type->param.len; i++) {
				s_param *param = (s_param*)st_type->param.table[i];
				if (!strcmp(param->name, node->dat.expr.sym->name))
					m_type = type_clone(param->type);
			}

			type_del(st_type);
			return m_type;
			}

		//Return type
		case EXPR_CALL:	
			return type_clone(node->dat.expr.left->dat.expr.sym->type->ret);

		//Variable
		case EXPR_IDENT:
			return type_clone(node->dat.expr.sym->type);

		//Literals
		case EXPR_CONST:
		case EXPR_STR:
			return type_clone(node->tok.dtype);
	}
	return NULL;
}
