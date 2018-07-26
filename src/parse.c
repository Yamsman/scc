#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include "ast.h"
#include "lex.h"
#include "parse.h"

//TODO: remove intermediate decl/expr/stmt variables if only used a few times
//TODO: list length less than zero initial loop check
//TODO: if/else -> switch for branch and iteration stmt functions
//TODO: condense lex_adv(lx) calls for switch statements (possibly lex_consume?)

ast_n *parse_decl(lexer *lx);
s_type *parse_decl_spec(lexer *lx);
s_type *parse_decltr(lexer *lx, s_type *type, char **vname);
ast_n *parse_decl_body(lexer *lx, s_type *type);

s_param *parse_fparam(lexer *lx);
s_param *parse_fparam_list(lexer *lx);
ast_n *parse_fdef(lexer *lx);

s_param *parse_smemb(lexer *lx);
s_type *parse_sdef(lexer *lx);

ast_n *parse_expr(lexer *lx);
ast_n *parse_expr_assign(lexer *lx);
ast_n *parse_expr_cond(lexer *lx);
ast_n *parse_expr_logic_or(lexer *lx);
ast_n *parse_expr_logic_and(lexer *lx);
ast_n *parse_expr_or(lexer *lx);
ast_n *parse_expr_xor(lexer *lx);
ast_n *parse_expr_and(lexer *lx);
ast_n *parse_expr_equal(lexer *lx);
ast_n *parse_expr_relation(lexer *lx);
ast_n *parse_expr_shift(lexer *lx);
ast_n *parse_expr_addt(lexer *lx);
ast_n *parse_expr_mult(lexer *lx);
ast_n *parse_expr_cast(lexer *lx);
ast_n *parse_expr_unary(lexer *lx);
ast_n *parse_expr_postfix(lexer *lx);
ast_n *parse_expr_primary(lexer *lx);

ast_n *parse_stmt(lexer *lx);
ast_n *parse_stmt_cmpd(lexer *lx);
ast_n *parse_stmt_expr(lexer *lx);
ast_n *parse_stmt_selection(lexer *lx);
ast_n *parse_stmt_iteration(lexer *lx);
ast_n *parse_stmt_label(lexer *lx);
ast_n *parse_stmt_jump(lexer *lx);

ast_n *parse(lexer *lx) {
	//TODO: rewrite
	ast_n *list = NULL; // = parse_stmt();
	ast_n *cur = NULL;
	while (lex_peek(lx).type != TOK_END) {
		//Look ahead to see if next is decl or function def
		int par = 0;
		int fdef = -1;
		token_n *toklist = NULL;
		for (;;) {
			token t = lex_peek(lx);
			switch (t.type) {
				case TOK_RPR: par = 1;    break;
				case TOK_EQ:  fdef = 0;   break;
				case TOK_SEM: fdef = 0;   break; 
				case TOK_LBR: fdef = par; break;
				case TOK_END:
					puts("premature end of input");
					return NULL;
			}
			token_n *tn = make_tok_node(t);
			tn->next = toklist;
			toklist = tn;

			if (fdef >= 0) break;
			lex_adv(lx);
		}

		//Unget tokens
		while (toklist != NULL) {
			token_n *next = toklist->next;
			lex_unget(lx, toklist);
			toklist = next;
		}

		//Parse accordingly
		ast_n *node;
		switch (fdef) {
			case 0: node = parse_decl(lx); break;
			case 1: node = parse_fdef(lx); break;
		}
		node->next = NULL;

		//Add declaration/function definition to list
		if (list == NULL) {
			list = node;
		} else {
			cur->next = node;
		}
		cur = node;
	}

	return list;
}

/*
 * parse_decl()
 * 	no allocation done yet
 * 	-> parse_decl_spec()
 * 	-> parse_decl_body(spec)
 * 		allocates the ast_decl
 * 		loops for each comma delimited declaration
 */

//ast_decl *parse_decl(spec *type);
ast_n *parse_decl(lexer *lx) {
	//Get specifiers
	s_type *type = parse_decl_spec(lx);
	if (!type)
		return NULL;
	
	//Used for user defined types
	if (lex_peek(lx).type == TOK_SEM) {
		lex_adv(lx);
		//TODO: make a symbol for the user type
		return astn_new(DECL, DECL_STD);
	}

	//Parse declaration body (declarator [+ initializer])
	char *name = NULL;
	ast_n *node = parse_decl_body(lx, type);

	switch (lex_peek(lx).type) {
		//Parse multiple declarations
		case TOK_CMM: {
			ast_n *node_ex = node;
			while (lex_peek(lx).type == TOK_CMM) {
				lex_adv(lx);
				node_ex->dat.decl.next = parse_decl_body(lx, type);
				node_ex = node_ex->dat.decl.next;
			}

			//Expect ';'
			lex_adv(lx);
			}
			break;

		case TOK_SEM:
			lex_adv(lx);
			break;

		case TOK_LBR:
			break;
	}

	return node;
}

//TODO: type qualifiers
s_type *parse_decl_spec(lexer *lx) {
	s_type *type = type_new(TYPE_UNDEF);

	for (;;) {
		token t = lex_peek(lx);
		switch (t.type) {
			case TOK_KW_VOID:
			case TOK_KW_CHAR:
			case TOK_KW_SHORT:
			case TOK_KW_INT:
			case TOK_KW_LONG:
			case TOK_KW_FLOAT:
			case TOK_KW_DOUBLE:
				if (type->kind != TYPE_UNDEF) {
					printf("ERROR: Multiple data types in declaration");
				}
				type->kind = (t.type - TOK_KW_VOID) + TYPE_VOID;
				break;
			case TOK_KW_EXTERN:
			case TOK_KW_STATIC:
			case TOK_KW_AUTO:
			case TOK_KW_REGISTER:
				if (type->s_class != CLASS_UNDEF) {
					printf("ERROR: Multiple storage classes in declaration");
				}
				type->s_class = (t.type - TOK_KW_EXTERN) + CLASS_EXTERN;
				break;
			case TOK_KW_STRUCT:
			case TOK_KW_UNION:
				return parse_sdef(lx);
			default:
				return type;
		}
		lex_adv(lx);
	}

	return type;
}

//TODO: standardize names of "type" variables across declarator functions
s_type *parse_decltr_ptr(lexer *lx, s_type *base_type) {
	if (lex_peek(lx).type != TOK_ASR)
		return base_type;
	lex_adv(lx);

	//Clone the base type
	s_type *ptype = type_new(TYPE_PTR);

	//Parse qualifier list
	token t = lex_peek(lx);
	while (t.type == TOK_KW_CONST || t.type == TOK_KW_VOLATILE) {
		switch (t.type) {
			case TOK_KW_CONST: ptype->is_const = 1;	break;
			case TOK_KW_VOLATILE: ptype->is_volatile = 1; break;
		}
		lex_adv(lx);
	}

	ptype->ref = parse_decltr_ptr(lx, base_type);
	return ptype;
}

s_type *parse_decltr_back(lexer *lx, s_type *base_type) {
	s_type *type = base_type;
	if (lex_peek(lx).type == TOK_LBK) {
		lex_adv(lx);

		//TODO: const_expr
		type = type_new(TYPE_PTR);
		type->ref_len = atoi(lex_peek(lx).str);
		lex_adv(lx);

		if (lex_peek(lx).type != TOK_RBK) {
			//Missing ']'
		}
		lex_adv(lx);

		//Currently parsing in wrong direction
		type->ref = parse_decltr_back(lx, base_type);
	} else if (lex_peek(lx).type == TOK_LPR) {
		lex_adv(lx);

		type = type_new(TYPE_FUNC);
		type->ret = base_type;

		if (lex_peek(lx).type != TOK_RPR)
			type->param = parse_fparam_list(lx);

		lex_adv(lx);
	}

	return type;
}

s_type *parse_decltr(lexer *lx, s_type *type, char **vname) {
	//Parse pointer types if applicable
	//s_type *ptr_type = parse_decltr_ptr(type);
	//if (ptr_type != type)
	//	return parse_decltr(ptr_type, vname);
	type = parse_decltr_ptr(lx, type); //should work the same?

	//If the asterisk is inside the parenthesis, it indicates that
	//the pointer is bound to what comes after it rather than the
	//preceeding base type
	if (lex_peek(lx).type == TOK_LPR) {
		lex_adv(lx);
		s_type *n_type = parse_decltr(lx, NULL, vname);
		
		if (lex_peek(lx).type != TOK_RPR) {
			//Missing '('
		}
		lex_adv(lx);

		//Attach trailing array/func types to n_type
		s_type *b_type = parse_decltr_back(lx, type);
		if (n_type == NULL) {
			n_type = b_type;
		} else {
			for (s_type *i = n_type;; i = i->ref) {
				if (i->ref == NULL) {
					i->ref = b_type;
					break;
				}
			}
		}
		return n_type;
	}

	//Do not recurse to parse_decltr after this point
	//Read vname
	token t = lex_peek(lx);
	if (t.type == TOK_IDENT) {
		lex_adv(lx);
		*vname = t.str;
	}
	return parse_decltr_back(lx, type);
}

//TODO: split into parse_decl_decltr and parse_decl_init
ast_n *parse_decl_body(lexer *lx, s_type *base_type) {
	//Parse declarator
	char *vname = NULL;
	s_type *vtype = parse_decltr(lx, type_clone(base_type), &vname);

	//Create a new symbol
	//parse_decltr may not set vname in order to implement parameters
	if (vname == NULL) {
		printf("ERROR: Expected variable name\n");
		return NULL;
	}

	symbol *s = symtable_def(&lx->stb, vname, vtype);
	if (!s) { //If duplicate exists
		//error handling?
	}

	ast_n *node = astn_new(DECL, DECL_STD);
	ast_decl *decl_n = &node->dat.decl;
	decl_n->sym = s;

	//Check for and parse initialization
	if (lex_peek(lx).type == TOK_ASSIGN) {
		lex_adv(lx);
		decl_n->init = parse_expr_assign(lx);
	}

	return node;
}

s_param *parse_fparam(lexer *lx) {
	//Parse type
	char *vname = NULL;
	s_type *base_type = parse_decl_spec(lx);

	//Parse declarator
	s_type *type = parse_decltr(lx, type_clone(base_type), &vname);

	//Create parameter and return
	return param_new(type, vname);
}

s_param *parse_fparam_list(lexer *lx) {
	//Return NULL for a blank list
	if (lex_peek(lx).type == TOK_RPR)
		return NULL;

	//Parse initial item
	s_param *param = parse_fparam(lx);

	//Parse multiple declarations
	s_param *param_ex = param;
	while (lex_peek(lx).type == TOK_CMM) {
		lex_adv(lx);
		param_ex->next = parse_fparam(lx);
		param_ex = param_ex->next;
	}

	return param;
}

//Parse a function definition
ast_n *parse_fdef(lexer *lx) {
	//Get return type
	s_type *r_type = parse_decl_spec(lx);

	//Get declarator
	char *f_name;
	s_type *f_type = parse_decltr(lx, r_type, &f_name);
	f_type->ret = r_type;

	//Create symbol
	symbol *s = symtable_def(&lx->stb, f_name, f_type);
	if (s->fbody != NULL) {
		printf("ERROR: Redefinition of '%s'\n", f_name);
	}

	//Enter function scope
	symtable_scope_enter(&lx->stb);
	for (s_param *p = f_type->param; p != NULL; p = p->next)
		symtable_def(&lx->stb, p->name, p->type);
	lx->stb.func = s;

	//Create node and parse body
	ast_n *node = astn_new(DECL, DECL_FUNC);
	node->dat.decl.sym = s;
	node->dat.decl.block = parse_stmt_cmpd(lx);

	//Leave function scope
	symtable_scope_leave(&lx->stb);
	lx->stb.func = NULL;

	return node;
}

s_param *parse_sdef_body(lexer *lx) {
	//Advance past '{'
	lex_adv(lx);

	//Parse list of declarations
	s_param *m_list = NULL;
	s_param *m_prev = NULL;
	while (lex_peek(lx).type != TOK_RBR) {
		if (lex_peek(lx).type == TOK_END) {
			printf("ERROR: Expected '}' before end of file\n");
		}

		//Get type
		//TODO: no type classes when parsing for specifier
		char *mname = NULL;
		s_type *base_type = parse_decl_spec(lx);

		//Parse declarator
		//TODO: bitfields
		s_type *mtype = parse_decltr(lx, type_clone(base_type), &mname);
		if (mname != NULL) {
			//Create new member
			s_param *memb_ex = param_new(mtype, mname);
			if (m_list == NULL) {
				m_list = memb_ex;
				m_prev = m_list;
			} else {
				m_prev->next = memb_ex;
				m_prev = memb_ex;
			}
		} else {
			printf("ERROR: Declaration declares nothing\n");
		}

		//Expect ';'
		if (lex_peek(lx).type != TOK_SEM) {
			printf("ERROR: Expected ';' before ...\n");
		}
		lex_adv(lx);
	}

	if (lex_peek(lx).type != TOK_RBR) {
		printf("ERROR: Expected '}' before ...\n");
	}
	lex_adv(lx);

	return m_list;
}

//Parse a struct type
//TODO: s_param -> s_memb
s_type *parse_sdef(lexer *lx) {
	s_type *type = NULL;
	switch (lex_peek(lx).type) {
		case TOK_KW_STRUCT:
			type = type_new(TYPE_STRUCT);
			break;
		case TOK_KW_UNION:
			type = type_new(TYPE_UNION);
			break;
		default:
			return NULL;
	}
	lex_adv(lx);

	//Get identifier and member list
	char *tname = NULL;
	s_param *m_list = NULL;
	if (lex_peek(lx).type == TOK_IDENT) {
		tname = lex_peek(lx).str;
		lex_adv(lx);
	}
	if (lex_peek(lx).type == TOK_LBR)
		m_list = parse_sdef_body(lx);
	type->memb = m_list;

	/* 
	 * Four cases:
	 * struct is anonymous (list but no name)
	 * struct is already defined (name but no list)
	 * struct needs to be defined (name and list)
	 * invalid (no name and no list)
	 */
	if (tname == NULL) {
		if (m_list == NULL) {
			printf("ERROR: Expected '{' or identifier before ...\n");
			return NULL;
		} 
	} else {
		symbol *s = NULL;
		if (m_list == NULL) {
			s = symtable_search(&lx->stb, tname);
			if (s == NULL) {
				printf("ERROR: Undefined struct '%s'\n", tname);
				return NULL;
			}
		}
		s = symtable_def(&lx->stb, tname, type);
	}

	return type;
}

/*
 * Expressions
 */
//TODO: clean switches in multiple-outcome expression functions
//TODO: remove redundant code

ast_n *parse_expr(lexer *lx) {
	ast_n *node = parse_expr_assign(lx);

	ast_n *n_node = node;
	while (lex_peek(lx).type == TOK_CMM) {
		lex_adv(lx);
		n_node->next = parse_expr_assign(lx);
		n_node = n_node->next;
	}

	return node;
}

ast_n *parse_expr_assign(lexer *lx) {
	ast_n *node = parse_expr_cond(lx);

	//lhs must be unary_expr
	ast_n *n_node;
	int atype = 0;
	switch (lex_peek(lx).type) {
		case TOK_ASSIGN_ADD: atype = EXPR_ADD; goto base;
		case TOK_ASSIGN_SUB: atype = EXPR_SUB; goto base;
		case TOK_ASSIGN_MUL: atype = EXPR_MUL; goto base;
		case TOK_ASSIGN_DIV: atype = EXPR_DIV; goto base;
		case TOK_ASSIGN_MOD: atype = EXPR_MOD; goto base;
		case TOK_ASSIGN_AND: atype = EXPR_AND; goto base;
		case TOK_ASSIGN_OR: atype = EXPR_OR; goto base;
		case TOK_ASSIGN_XOR: atype = EXPR_XOR; goto base;
		case TOK_ASSIGN_LSHIFT: atype = EXPR_LSHIFT; goto base;
		case TOK_ASSIGN_RSHIFT: atype = EXPR_RSHIFT; goto base;

		case TOK_ASSIGN: {
base:			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_ASSIGN);
			ast_expr *n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_assign(lx);
			break;
			}

		default:
			return node;
	}

	//Expand assignment
	if (atype) {
		ast_n *opr = astn_new(EXPR, atype);
		opr->dat.expr.left = node;
		opr->dat.expr.right = n_node->dat.expr.right;
		n_node->dat.expr.right = opr;
	}

	return n_node;
}

ast_n *parse_expr_cond(lexer *lx) {
	ast_n *node = parse_expr_logic_or(lx);

	//ternary

	return node;
}

ast_n *parse_expr_logic_or(lexer *lx) {
	ast_n *node = parse_expr_logic_and(lx);

	if (lex_peek(lx).type == TOK_LOGIC_OR) {
		lex_adv(lx);
		ast_n *n_node = astn_new(EXPR, EXPR_LOGIC_OR);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_logic_and(lx);
		return n_node;
	}
	return node;
}

ast_n *parse_expr_logic_and(lexer *lx) {
	ast_n *node = parse_expr_or(lx);

	if (lex_peek(lx).type == TOK_LOGIC_AND) {
		lex_adv(lx);
		ast_n *n_node = astn_new(EXPR, EXPR_LOGIC_AND);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_or(lx);
		return n_node;
	}
	return node;
}

ast_n *parse_expr_or(lexer *lx) {
	ast_n *node = parse_expr_xor(lx);

	if (lex_peek(lx).type == TOK_OR) {
		lex_adv(lx);
		ast_n *n_node = astn_new(EXPR, EXPR_OR);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_xor(lx);
		return n_node;
	}
	return node;
}

ast_n *parse_expr_xor(lexer *lx) {
	ast_n *node = parse_expr_and(lx);

	if (lex_peek(lx).type == TOK_XOR) {
		lex_adv(lx);
		ast_n *n_node = astn_new(EXPR, EXPR_XOR);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_and(lx);
		return n_node;
	}
	return node;
}

ast_n *parse_expr_and(lexer *lx) {
	ast_n *node = parse_expr_equal(lx);

	if (lex_peek(lx).type == TOK_AND) {
		lex_adv(lx);
		ast_n *n_node = astn_new(EXPR, EXPR_AND);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_equal(lx);
		return n_node;
	}
	return node;
}

ast_n *parse_expr_equal(lexer *lx) {
	ast_n *node = parse_expr_relation(lx);

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_EQ:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_EQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_relation(lx);
			return n_node;
		case TOK_NEQ:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_NEQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_relation(lx);
			return n_node;
	}

	return node;
}

ast_n *parse_expr_relation(lexer *lx) {
	ast_n *node = parse_expr_shift(lx);

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_LTH:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_LTH);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
		case TOK_GTH:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_GTH);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
		case TOK_LEQ:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_LEQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
		case TOK_GEQ:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_GEQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
	}

	return node;
}

ast_n *parse_expr_shift(lexer *lx) {
	ast_n *node = parse_expr_addt(lx);

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_LSHIFT:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_LSHIFT);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt(lx);
			return n_node;
		case TOK_RSHIFT:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_RSHIFT);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt(lx);
			return n_node;
	}

	return node;
}

ast_n *parse_expr_addt(lexer *lx) {
	ast_n *node = parse_expr_mult(lx);

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_ADD:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_ADD);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt(lx);
			return n_node;
		case TOK_SUB:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_SUB);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt(lx);
			return n_node;
	}

	return node;
}

ast_n *parse_expr_mult(lexer *lx) {
	ast_n *node = parse_expr_cast(lx);

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_ASR:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_MUL);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast(lx);
			return n_node;
		case TOK_DIV:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_DIV);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast(lx);
			return n_node;
		case TOK_MOD:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_MOD);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast(lx);
			return n_node;
	}

	return node;
}

//TODO:
ast_n *parse_expr_cast(lexer *lx) {
	ast_n *node = parse_expr_unary(lx);

	if (lex_peek(lx).type == TOK_LPR) {

	}

	return node; 
}

ast_n *parse_expr_unary(lexer *lx) {
	ast_n *n_node = NULL;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_AND: lex_adv(lx); n_node = astn_new(EXPR, EXPR_REF); break;
		case TOK_ASR: lex_adv(lx); n_node = astn_new(EXPR, EXPR_DEREF); break;
		case TOK_ADD: lex_adv(lx); n_node = astn_new(EXPR, EXPR_POS); break;
		case TOK_SUB: lex_adv(lx); n_node = astn_new(EXPR, EXPR_NEG); break;
		case TOK_NOT: lex_adv(lx); n_node = astn_new(EXPR, EXPR_LOGIC_NOT); break;
		case TOK_TLD: lex_adv(lx); n_node = astn_new(EXPR, EXPR_NOT); break;
		case TOK_INC: lex_adv(lx); n_node = astn_new(EXPR, EXPR_INC_PRE); break;
		case TOK_DEC: lex_adv(lx); n_node = astn_new(EXPR, EXPR_DEC_PRE); break;
		//TODO case TOK_KW_SIZEOF:
	}

	//TODO: parse expr_cast instead if & * + - ! ~
	ast_n *node = parse_expr_postfix(lx);
	if (n_node != NULL) {
		n_node->dat.expr.left = node;
		return n_node;
	}

	return node;
}

//TODO: also include function calls/array accessors
ast_n *parse_expr_postfix(lexer *lx) {
	ast_n *node = parse_expr_primary(lx);

	ast_n *n_node;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_LBK: //Array accessor
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_ARRAY);
			n_node->dat.expr.left = node;

			//Get offset
			//n_node->dat.expr.right = parse_expr();
			n_node->dat.expr.right = parse_expr_primary(lx);
			
			if (lex_peek(lx).type != TOK_RBK) {
				//Missing ']'
			}
			lex_adv(lx);

			return n_node;
		case TOK_LPR: //Function calls
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_CALL);
			n_node->dat.expr.left = node;

			//Empty parameter list
			if (lex_peek(lx).type == TOK_RPR) {
				lex_adv(lx);
				return n_node;
			}

			//Get parameter list
			n_node->dat.expr.right = parse_expr(lx);
			
			if (lex_peek(lx).type != TOK_RPR) {
				//Missing ')'
			}
			lex_adv(lx);

			return n_node;
		case TOK_PRD:
			break;
		case TOK_PTR:
			break;
		case TOK_INC:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_INC_POST);
			n_node->dat.expr.left = node;
			return n_node;
		case TOK_DEC:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_INC_POST);
			n_node->dat.expr.left = node;
			return n_node;
	}

	return node;
}

ast_n *parse_expr_primary(lexer *lx) {
	ast_n *node;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_IDENT: //TODO: improve handling of undefined variables
			lex_adv(lx);
			symbol *s = symtable_search(&lx->stb, t.str);
			node = astn_new(EXPR, EXPR_IDENT);
			if (s != NULL) {
				node->dat.expr.sym = s;
			} else {
				printf("Undefined variable \"%s\"\n", t.str);
				//undefined variable
			}

			return node;
		case TOK_CONST:
			lex_adv(lx);
			node = astn_new(EXPR, EXPR_CONST);
			return node;
		case TOK_STR:
			lex_adv(lx);
			node = astn_new(EXPR, EXPR_STR);
			return node;
		case TOK_LPR:
			lex_adv(lx);
			node = parse_expr(lx);
			lex_adv(lx); //expect ')'
			return node;
	}
}

/*
 * Statements
 */

ast_n *parse_stmt(lexer *lx) {
	ast_n *node;
	switch (lex_peek(lx).type) {
		case TOK_KW_IF:
		case TOK_KW_SWITCH:
			node = parse_stmt_selection(lx);
			break;
		case TOK_KW_WHILE:
		case TOK_KW_FOR:
		case TOK_KW_DO:
			node = parse_stmt_iteration(lx);
			break;
		case TOK_KW_CASE:
		case TOK_KW_DEFAULT:
			node = parse_stmt_label(lx);
			break;
		case TOK_KW_GOTO:
		case TOK_KW_BREAK:
		case TOK_KW_CONTINUE:
		case TOK_KW_RETURN:
			node = parse_stmt_jump(lx);
			break;
		case TOK_LBR:
			node = parse_stmt_cmpd(lx);
			break;
		default:
			node = parse_stmt_expr(lx);
			break;
	}
	return node;
}

ast_n *parse_stmt_cmpd(lexer *lx) {
	//Expect '{'
	if (lex_peek(lx).type == TOK_LBR)
		lex_adv(lx);

	//Parse list of declarations/statements here
	ast_n *base = NULL;
	ast_n *last = NULL;
	while (lex_peek(lx).type != TOK_RBR) {
		//Premature end of file
		token t = lex_peek(lx);
		if (t.type == TOK_END) {
			printf("Error: Expected '}' before end of input!\n");
			break;
		}

		//Parse a statement or a declaration
		ast_n *node;
		if (is_type_spec(lex_peek(lx))) {
			node = parse_decl(lx);
		} else {
			node = parse_stmt(lx);
		}

		//Add the node to the list
		if (base == NULL) {
			base = node;
		} else {
			last->next = node;
		}
		last = node;
	}

	//Expect '}'
	if (lex_peek(lx).type == TOK_RBR)
		lex_adv(lx);

	return base;
}

ast_n *parse_stmt_expr(lexer *lx) {
	ast_n *node;
	if (lex_peek(lx).type == TOK_SEM) {
		lex_adv(lx);
		return NULL;
	}

	node = astn_new(STMT, STMT_EXPR);
	node->dat.stmt.expr = parse_expr(lx);

	if (lex_peek(lx).type == TOK_SEM) {
		lex_adv(lx);
	} else {
		//missing ';'
	}

	return node;
}

//TODO: switch statements
ast_n *parse_stmt_selection(lexer *lx) {
	ast_n *node;
	if (lex_peek(lx).type == TOK_KW_IF) {
		node = astn_new(STMT, STMT_IF);
		lex_adv(lx);

		if (lex_peek(lx).type == TOK_LPR) {
			lex_adv(lx);
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr(lx);

		if (lex_peek(lx).type == TOK_RPR) {
			lex_adv(lx);
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt(lx);
		if (lex_peek(lx).type == TOK_KW_ELSE) {
			lex_adv(lx);
			node->dat.stmt.else_body = parse_stmt(lx);
		}
	} else if (lex_peek(lx).type == TOK_KW_SWITCH) {
		node = astn_new(STMT, STMT_SWITCH);
		lex_adv(lx);

		if (lex_peek(lx).type == TOK_LPR) {
			lex_adv(lx);
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr(lx);

		if (lex_peek(lx).type == TOK_RPR) {
			lex_adv(lx);
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt(lx);
	}

	return node;
}

ast_n *parse_stmt_iteration(lexer *lx) {
	ast_n *node;
	if (lex_peek(lx).type == TOK_KW_WHILE) {
		node = astn_new(STMT, STMT_WHILE);
		lex_adv(lx);

		if (lex_peek(lx).type == TOK_LPR) {
			lex_adv(lx);
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr(lx);

		if (lex_peek(lx).type == TOK_RPR) {
			lex_adv(lx);
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt(lx);
	} else if (lex_peek(lx).type == TOK_KW_FOR) {
		node = astn_new(STMT, STMT_FOR);
		lex_adv(lx);

		if (lex_peek(lx).type == TOK_LPR) {
			lex_adv(lx);
		} else {
			//missing ')'
		}

		//Parse either a declaration or expression statement
		ast_n *cond[3] = {0};
		if (is_type_spec(lex_peek(lx))) {
			cond[0] = parse_decl(lx);
		} else { 
			cond[0] = parse_stmt_expr(lx);
		}
		cond[1] = parse_stmt_expr(lx);

		//Parse the optional third expression
		if (lex_peek(lx).type != TOK_RPR)
			cond[2] = parse_expr(lx);

		if (lex_peek(lx).type == TOK_RPR) {
			lex_adv(lx);
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt(lx);
	} else if (lex_peek(lx).type == TOK_KW_DO) {
		node = astn_new(STMT, STMT_DO_WHILE);
		lex_adv(lx);

		node->dat.stmt.body = parse_stmt(lx);

		//Parse condition
		if (lex_peek(lx).type != TOK_KW_WHILE) {
			//expected 'while' ..
		}
		lex_adv(lx);

		if (lex_peek(lx).type == TOK_LPR) {
			lex_adv(lx);
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr(lx);

		if (lex_peek(lx).type == TOK_RPR) {
			lex_adv(lx);
		} else {
			//missing '('
		}

		if (lex_peek(lx).type == TOK_SEM) {
			lex_adv(lx);
		} else {
			//missing '('
		}

	}
	return node;
}

ast_n *parse_stmt_label(lexer *lx) {
	ast_n *node = NULL;

	switch (lex_peek(lx).type) {
		case TOK_IDENT:
			node = astn_new(STMT, STMT_LABEL);
			node->dat.stmt.expr = parse_expr_primary(lx); //TODO: replace with a plain identifier (expr_primary can pull parentheses chain)
			break;
		case TOK_KW_CASE:
			lex_adv(lx);
			node = astn_new(STMT, STMT_CASE);
			node->dat.stmt.expr = parse_expr_cond(lx); //TODO: parse_constexpr();
			break;
		case TOK_KW_DEFAULT:
			lex_adv(lx);
			node = astn_new(STMT, STMT_DEFAULT);
			break;
		default:
			return NULL;
	}

	if (lex_peek(lx).type != TOK_COL) {
		//missing ':'
	}
	lex_adv(lx);
	node->dat.stmt.body = parse_stmt(lx);

	return node;
}

ast_n *parse_stmt_jump(lexer *lx) {
	ast_n *node = NULL;

	switch (lex_peek(lx).type) {
		case TOK_KW_GOTO:
			lex_adv(lx);
			node = astn_new(STMT, STMT_GOTO);

			//Get an identifier
			ast_n *label = parse_expr_primary(lx);
			if (label->dat.expr.kind != EXPR_IDENT) {
				//Expected identifier before...
				astn_del(label);
			}
			node->dat.stmt.expr = label;
			break;
		case TOK_KW_CONTINUE:
			lex_adv(lx);
			node = astn_new(STMT, STMT_CONTINUE);
			break;
		case TOK_KW_BREAK:
			lex_adv(lx);
			node = astn_new(STMT, STMT_BREAK);
			break;
		case TOK_KW_RETURN:
			lex_adv(lx);
			node = astn_new(STMT, STMT_BREAK);
			
			if (lex_peek(lx).type != TOK_SEM)
				node->dat.stmt.expr = parse_expr(lx);
			break;
	}

	if (lex_peek(lx).type != TOK_SEM) {
		//Expected ';' before...
	}
	lex_adv(lx);

	return node;
}
