#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "sym.h"
#include "ast.h"
#include "parse.h"

//TODO: remove intermediate decl/expr/stmt variables if only used a few times
//TODO: list length less than zero initial loop check
//TODO: if/else -> switch for branch and iteration stmt functions
//TODO: condense lex_adv() calls for switch statements (possibly lex_consume?)

ast_n *parse_decl();
s_type *parse_decl_spec();
s_type *parse_decltr(s_type *type, char **vname);
ast_n *parse_decl_body(s_type *type);

s_param *parse_fparam();
s_param *parse_fparam_list();
ast_n *parse_fdef();

s_param *parse_smemb();
s_type *parse_sdef();

ast_n *parse_expr();
ast_n *parse_expr_assign();
ast_n *parse_expr_cond();
ast_n *parse_expr_logic_or();
ast_n *parse_expr_logic_and();
ast_n *parse_expr_or();
ast_n *parse_expr_xor();
ast_n *parse_expr_and();
ast_n *parse_expr_equal();
ast_n *parse_expr_relation();
ast_n *parse_expr_shift();
ast_n *parse_expr_addt();
ast_n *parse_expr_mult();
ast_n *parse_expr_cast();
ast_n *parse_expr_unary();
ast_n *parse_expr_postfix();
ast_n *parse_expr_primary();

ast_n *parse_stmt();
ast_n *parse_stmt_cmpd();
ast_n *parse_stmt_expr();
ast_n *parse_stmt_selection();
ast_n *parse_stmt_iteration();
ast_n *parse_stmt_label();
ast_n *parse_stmt_jump();

ast_n *parse() {
	//TODO: rewrite
	ast_n *node; // = parse_stmt();
	ast_n *cur = NULL;
	while (lex_peek().type != TOK_END) {
		//Check if the next item is a function definition or a declaration
		//TODO: replace with proper unget functionality
		char *save = lx;
		int fdef = 0;
		int pcl = 0;
		for (;;) {
			if (*lx == '{') {
				if (pcl == 1)
					fdef = 1;
				else
					fdef = 0;
				break;
			} else if (*lx == ')') {
				pcl = 1;
			} else if (*lx == ';') {
				fdef = 0;
				break;
			} else if (*lx == '\0') {
				fdef = -1;
				break;
			}
			lx++;
		}
		lx = save;

		ast_n *n;
		switch (fdef) {
			case 1: n = parse_fdef(); break;
			case 0: n = parse_decl(); break;
		}

		if (node == NULL) {
			node = n;
			cur = n;
		}
		cur->next = n;
		cur = n;
	}

	return node;
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
ast_n *parse_decl() {
	//Get specifiers
	s_type *type = parse_decl_spec();
	if (!type)
		return NULL;
	
	//Used for user defined types
	if (lex_peek().type == TOK_SEM) {
		lex_adv();
		//TODO: make a symbol for the user type
		return astn_new(DECL, DECL_STD);
	}

	//Parse declaration body (declarator [+ initializer])
	char *name = NULL;
	ast_n *node = parse_decl_body(type);

	switch (lex_peek().type) {
		//Parse multiple declarations
		case TOK_CMM: {
			ast_n *node_ex = node;
			while (lex_peek().type == TOK_CMM) {
				lex_adv();
				node_ex->dat.decl.next = parse_decl_body(type);
				node_ex = node_ex->dat.decl.next;
			}

			//Expect ';'
			lex_adv();
			}
			break;

		case TOK_SEM:
			lex_adv();
			break;

		case TOK_LBR:
			break;
	}

	return node;
}

//TODO: type qualifiers
s_type *parse_decl_spec() {
	s_type *type = type_new(TYPE_UNDEF);

	for (;;) {
		token t = lex_peek();
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
				return parse_sdef();
			default:
				return type;
		}
		lex_adv();
	}

	return type;
}

//TODO: standardize names of "type" variables across declarator functions
s_type *parse_decltr_ptr(s_type *base_type) {
	if (lex_peek().type != TOK_ASR)
		return base_type;
	lex_adv();

	//Clone the base type
	s_type *ptype = type_new(TYPE_PTR);

	//Parse qualifier list
	token t = lex_peek();
	while (t.type == TOK_KW_CONST || t.type == TOK_KW_VOLATILE) {
		switch (t.type) {
			case TOK_KW_CONST: ptype->is_const = 1;	break;
			case TOK_KW_VOLATILE: ptype->is_volatile = 1; break;
		}
		lex_adv();
	}

	ptype->ref = parse_decltr_ptr(base_type);
	return ptype;
}

s_type *parse_decltr_back(s_type *base_type) {
	s_type *type = base_type;
	if (lex_peek().type == TOK_LBK) {
		lex_adv();

		//TODO: const_expr
		type = type_new(TYPE_PTR);
		type->ref_len = atoi(lex_peek().str);
		lex_adv();

		if (lex_peek().type != TOK_RBK) {
			//Missing ']'
		}
		lex_adv();

		//Currently parsing in wrong direction
		type->ref = parse_decltr_back(base_type);
	} else if (lex_peek().type == TOK_LPR) {
		lex_adv();

		type = type_new(TYPE_FUNC);
		type->ret = base_type;

		if (lex_peek().type != TOK_RPR)
			type->param = parse_fparam_list();

		lex_adv();
	}

	return type;
}

s_type *parse_decltr(s_type *type, char **vname) {
	//Parse pointer types if applicable
	//s_type *ptr_type = parse_decltr_ptr(type);
	//if (ptr_type != type)
	//	return parse_decltr(ptr_type, vname);
	type = parse_decltr_ptr(type); //should work the same?

	//If the asterisk is inside the parenthesis, it indicates that
	//the pointer is bound to what comes after it rather than the
	//preceeding base type
	if (lex_peek().type == TOK_LPR) {
		lex_adv();
		s_type *n_type = parse_decltr(NULL, vname);
		
		if (lex_peek().type != TOK_RPR) {
			//Missing '('
		}
		lex_adv();

		//Attach trailing array/func types to n_type
		s_type *b_type = parse_decltr_back(type);
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
	//Expect identifier
	if (lex_peek().type != TOK_IDENT) {
		//free type
		return NULL;
	}
	token t = lex_peek();
	lex_adv();
	*vname = t.str;

	return parse_decltr_back(type);
}

//TODO: split into parse_decl_decltr and parse_decl_init
ast_n *parse_decl_body(s_type *base_type) {
	//Parse declarator
	char *vname = NULL;
	s_type *t = parse_decltr(type_clone(base_type), &vname);

	//Create a new symbol
	//TODO: check for missing symbol name as a result of allowing parse_decltr to return
	//	NULL in order to implement parse_fparam
	if (vname == NULL) {
		//Missing symbol error
	}
	symbol *s = symtable_add(vname); //TODO: type
	if (!s) { //If duplicate exists
		//error handling?
	}

	ast_n *node = astn_new(DECL, DECL_STD);
	ast_decl *decl_n = &node->dat.decl;
	decl_n->sym = s;

	//Check for and parse initialization
	if (lex_peek().type == TOK_ASSIGN) {
		lex_adv();
		decl_n->init = parse_expr_assign();
	}

	return node;
}

s_param *parse_fparam() {
	//Parse type
	char *vname = NULL;
	s_type *base_type = parse_decl_spec();

	//Parse declarator
	//TODO: symtable integration
	s_type *type = parse_decltr(type_clone(base_type), &vname);
	symtable_add(vname); //temporary

	//Create parameter and return
	return param_new(type, vname);
}

s_param *parse_fparam_list() {
	//Return NULL for a blank list
	if (lex_peek().type == TOK_RPR)
		return NULL;

	//Parse initial item
	s_param *param = parse_fparam();

	//Parse multiple declarations
	s_param *param_ex = param;
	while (lex_peek().type == TOK_CMM) {
		lex_adv();
		param_ex->next = parse_fparam();
		param_ex = param_ex->next;
	}

	return param;
}

//Parse a function definition
ast_n *parse_fdef() {
	//Get return type
	s_type *r_type = parse_decl_spec();

	//Get declarator
	char *f_name;
	s_type *f_type = parse_decltr(r_type, &f_name);
	puts(f_name);
	free(f_name);

	//Create a symbol (TODO)
	symbol *s = NULL;

	//Create node
	ast_n *node = astn_new(DECL, DECL_FUNC);
	node->dat.decl.sym = s;
	node->dat.decl.block = parse_stmt_cmpd();

	return node;
}

//Parse a struct type
//TODO: s_param -> s_memb
s_type *parse_sdef() {
	s_type *type = NULL;
	switch (lex_peek().type) {
		case TOK_KW_STRUCT:
			type = type_new(TYPE_STRUCT);
			break;
		case TOK_KW_UNION:
			type = type_new(TYPE_UNION);
			break;
		default:
			return NULL;
	}
	lex_adv();

	//Get identifier
	char *tname = NULL;
	if (lex_peek().type == TOK_IDENT) {
		tname = lex_peek().str;
		lex_adv();
	}
	
	//perform lookup to see if struct exists in symbol table
	
	//Get list of members
	if (lex_peek().type == TOK_LBR) {
		lex_adv();

		s_param *memb_prev = NULL;
		while (lex_peek().type != TOK_RBR) {
			//Get type
			//TODO: no type classes when parsing for specifier
			char *mname = NULL;
			s_type *base_type = parse_decl_spec();

			//Parse declarator
			//TODO: bitfields
			s_type *mtype = parse_decltr(type_clone(base_type), &mname);

			if (mname == NULL) {
				//Expected identifier before...
			}

			//Create new member
			s_param *memb_ex = param_new(mtype, mname);
			if (type->memb == NULL) {
				type->memb = memb_ex;
				memb_prev = type->memb;
			} else {
				memb_prev->next = memb_ex;
				memb_prev = memb_ex;
			}

			//Expect ';'
			if (lex_peek().type != TOK_SEM) {
				//Expected ';' before...
			}
			lex_adv();
		}

		if (lex_peek().type != TOK_RBR) {
			//Expected '}' before...
		}
		lex_adv();
	}

	return type;
}

/*
 * Expressions
 */
//TODO: clean switches in multiple-outcome expression functions
//TODO: remove redundant code

ast_n *parse_expr() {
	ast_n *node = parse_expr_assign();

	ast_n *n_node = node;
	while (lex_peek().type == TOK_CMM) {
		lex_adv();
		n_node->next = parse_expr_assign();
		n_node = n_node->next;
	}

	return node;
}

ast_n *parse_expr_assign() {
	ast_n *node = parse_expr_cond();

	//lhs must be unary_expr
	ast_n *n_node;
	int atype = 0;
	switch (lex_peek().type) {
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
base:			lex_adv();
			n_node = astn_new(EXPR, EXPR_ASSIGN);
			ast_expr *n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_assign();
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

ast_n *parse_expr_cond() {
	ast_n *node = parse_expr_logic_or();

	//ternary

	return node;
}

ast_n *parse_expr_logic_or() {
	ast_n *node = parse_expr_logic_and();

	if (lex_peek().type == TOK_LOGIC_OR) {
		lex_adv();
		ast_n *n_node = astn_new(EXPR, EXPR_LOGIC_OR);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_logic_and();
		return n_node;
	}
	return node;
}

ast_n *parse_expr_logic_and() {
	ast_n *node = parse_expr_or();

	if (lex_peek().type == TOK_LOGIC_AND) {
		lex_adv();
		ast_n *n_node = astn_new(EXPR, EXPR_LOGIC_AND);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_or();
		return n_node;
	}
	return node;
}

ast_n *parse_expr_or() {
	ast_n *node = parse_expr_xor();

	if (lex_peek().type == TOK_OR) {
		lex_adv();
		ast_n *n_node = astn_new(EXPR, EXPR_OR);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_xor();
		return n_node;
	}
	return node;
}

ast_n *parse_expr_xor() {
	ast_n *node = parse_expr_and();

	if (lex_peek().type == TOK_XOR) {
		lex_adv();
		ast_n *n_node = astn_new(EXPR, EXPR_XOR);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_and();
		return n_node;
	}
	return node;
}

ast_n *parse_expr_and() {
	ast_n *node = parse_expr_equal();

	if (lex_peek().type == TOK_AND) {
		lex_adv();
		ast_n *n_node = astn_new(EXPR, EXPR_AND);
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		n_expr->right = parse_expr_equal();
		return n_node;
	}
	return node;
}

ast_n *parse_expr_equal() {
	ast_n *node = parse_expr_relation();

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek();
	switch (t.type) {
		case TOK_EQ:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_EQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_relation();
			return n_node;
		case TOK_NEQ:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_NEQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_relation();
			return n_node;
	}

	return node;
}

ast_n *parse_expr_relation() {
	ast_n *node = parse_expr_shift();

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek();
	switch (t.type) {
		case TOK_LTH:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_LTH);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift();
			return n_node;
		case TOK_GTH:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_GTH);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift();
			return n_node;
		case TOK_LEQ:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_LEQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift();
			return n_node;
		case TOK_GEQ:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_GEQ);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift();
			return n_node;
	}

	return node;
}

ast_n *parse_expr_shift() {
	ast_n *node = parse_expr_addt();

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek();
	switch (t.type) {
		case TOK_LSHIFT:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_LSHIFT);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt();
			return n_node;
		case TOK_RSHIFT:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_RSHIFT);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt();
			return n_node;
	}

	return node;
}

ast_n *parse_expr_addt() {
	ast_n *node = parse_expr_mult();

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek();
	switch (t.type) {
		case TOK_ADD:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_ADD);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt();
			return n_node;
		case TOK_SUB:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_SUB);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt();
			return n_node;
	}

	return node;
}

ast_n *parse_expr_mult() {
	ast_n *node = parse_expr_cast();

	ast_n *n_node;
	ast_expr *n_expr;
	token t = lex_peek();
	switch (t.type) {
		case TOK_ASR:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_MUL);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast();
			return n_node;
		case TOK_DIV:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_DIV);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast();
			return n_node;
		case TOK_MOD:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_MOD);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast();
			return n_node;
	}

	return node;
}

//TODO:
ast_n *parse_expr_cast() {
	ast_n *node = parse_expr_unary();

	if (lex_peek().type == TOK_LPR) {

	}

	return node; 
}

ast_n *parse_expr_unary() {
	ast_n *n_node = NULL;
	token t = lex_peek();
	switch (t.type) {
		case TOK_AND: lex_adv(); n_node = astn_new(EXPR, EXPR_REF); break;
		case TOK_ASR: lex_adv(); n_node = astn_new(EXPR, EXPR_DEREF); break;
		case TOK_ADD: lex_adv(); n_node = astn_new(EXPR, EXPR_POS); break;
		case TOK_SUB: lex_adv(); n_node = astn_new(EXPR, EXPR_NEG); break;
		case TOK_NOT: lex_adv(); n_node = astn_new(EXPR, EXPR_LOGIC_NOT); break;
		case TOK_TLD: lex_adv(); n_node = astn_new(EXPR, EXPR_NOT); break;
		case TOK_INC: lex_adv(); n_node = astn_new(EXPR, EXPR_INC_PRE); break;
		case TOK_DEC: lex_adv(); n_node = astn_new(EXPR, EXPR_DEC_PRE); break;
		//TODO case TOK_KW_SIZEOF:
	}

	//TODO: parse expr_cast instead if & * + - ! ~
	ast_n *node = parse_expr_postfix();
	if (n_node != NULL) {
		n_node->dat.expr.left = node;
		return n_node;
	}

	return node;
}

//TODO: also include function calls/array accessors
ast_n *parse_expr_postfix() {
	ast_n *node = parse_expr_primary();

	ast_n *n_node;
	token t = lex_peek();
	switch (t.type) {
		case TOK_LBK: //Array accessor
			lex_adv();
			n_node = astn_new(EXPR, EXPR_ARRAY);
			n_node->dat.expr.left = node;

			//Get offset
			//n_node->dat.expr.right = parse_expr();
			n_node->dat.expr.right = parse_expr_primary();
			
			if (lex_peek().type != TOK_RBK) {
				//Missing ']'
			}
			lex_adv();

			return n_node;
		case TOK_LPR: //Function calls
			lex_adv();
			n_node = astn_new(EXPR, EXPR_CALL);
			n_node->dat.expr.left = node;

			//Empty parameter list
			if (lex_peek().type == TOK_RPR) {
				lex_adv();
				return n_node;
			}

			//Get parameter list
			n_node->dat.expr.right = parse_expr();
			
			if (lex_peek().type != TOK_RPR) {
				//Missing ')'
			}
			lex_adv();

			return n_node;
		case TOK_PRD:
			break;
		case TOK_PTR:
			break;
		case TOK_INC:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_INC_POST);
			n_node->dat.expr.left = node;
			return n_node;
		case TOK_DEC:
			lex_adv();
			n_node = astn_new(EXPR, EXPR_INC_POST);
			n_node->dat.expr.left = node;
			return n_node;
	}

	return node;
}

ast_n *parse_expr_primary() {
	ast_n *node;
	token t = lex_peek();
	switch (t.type) {
		case TOK_IDENT: //TODO: improve handling of undefined variables
			lex_adv();
			symbol *s = symtable_search(t.str, SCOPE_VISIBLE);
			node = astn_new(EXPR, EXPR_IDENT);
			if (s != NULL) {
				node->dat.expr.sym = s;
			} else {
				printf("Undefined variable \"%s\"\n", t.str);
				//undefined variable
			}

			return node;
		case TOK_CONST:
			lex_adv();
			node = astn_new(EXPR, EXPR_CONST);
			return node;
		case TOK_STR:
			lex_adv();
			node = astn_new(EXPR, EXPR_STR);
			return node;
		case TOK_LPR:
			lex_adv();
			node = parse_expr();
			lex_adv(); //expect ')'
			return node;
	}
}

/*
 * Statements
 */

ast_n *parse_stmt() {
	ast_n *node;
	switch (lex_peek().type) {
		case TOK_KW_IF:
		case TOK_KW_SWITCH:
			node = parse_stmt_selection();
			break;
		case TOK_KW_WHILE:
		case TOK_KW_FOR:
		case TOK_KW_DO:
			node = parse_stmt_iteration();
			break;
		case TOK_KW_CASE:
		case TOK_KW_DEFAULT:
			node = parse_stmt_label();
			break;
		case TOK_KW_GOTO:
		case TOK_KW_BREAK:
		case TOK_KW_CONTINUE:
		case TOK_KW_RETURN:
			node = parse_stmt_jump();
			break;
		case TOK_LBR:
			node = parse_stmt_cmpd();
			break;
		default:
			node = parse_stmt_expr();
			break;
	}
	return node;
}

ast_n *parse_stmt_cmpd() {
	//Expect '{'
	if (lex_peek().type == TOK_LBR)
		lex_adv();

	//Parse list of declarations/statements here
	ast_n *base = NULL;
	ast_n *last = NULL;
	while (lex_peek().type != TOK_RBR) {
		//Premature end of file
		token t = lex_peek();
		if (t.type == TOK_END) {
			printf("Error: Expected '}' before end of input!\n");
			break;
		}

		//Parse a statement or a declaration
		ast_n *node;
		if (is_type_spec(lex_peek())) {
			node = parse_decl();
		} else {
			node = parse_stmt();
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
	if (lex_peek().type == TOK_RBR)
		lex_adv();

	return base;
}

ast_n *parse_stmt_expr() {
	ast_n *node;
	if (lex_peek().type == TOK_SEM) {
		lex_adv();
		return NULL;
	}

	node = astn_new(STMT, STMT_EXPR);
	node->dat.stmt.expr = parse_expr();

	if (lex_peek().type == TOK_SEM) {
		lex_adv();
	} else {
		//missing ';'
	}

	return node;
}

//TODO: switch statements
ast_n *parse_stmt_selection() {
	ast_n *node;
	if (lex_peek().type == TOK_KW_IF) {
		node = astn_new(STMT, STMT_IF);
		lex_adv();

		if (lex_peek().type == TOK_LPR) {
			lex_adv();
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr();

		if (lex_peek().type == TOK_RPR) {
			lex_adv();
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt();
		if (lex_peek().type == TOK_KW_ELSE) {
			lex_adv();
			node->dat.stmt.else_body = parse_stmt();
		}
	} else if (lex_peek().type == TOK_KW_SWITCH) {
		node = astn_new(STMT, STMT_SWITCH);
		lex_adv();

		if (lex_peek().type == TOK_LPR) {
			lex_adv();
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr();

		if (lex_peek().type == TOK_RPR) {
			lex_adv();
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt();
	}

	return node;
}

ast_n *parse_stmt_iteration() {
	ast_n *node;
	if (lex_peek().type == TOK_KW_WHILE) {
		node = astn_new(STMT, STMT_WHILE);
		lex_adv();

		if (lex_peek().type == TOK_LPR) {
			lex_adv();
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr();

		if (lex_peek().type == TOK_RPR) {
			lex_adv();
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt();
	} else if (lex_peek().type == TOK_KW_FOR) {
		node = astn_new(STMT, STMT_FOR);
		lex_adv();

		if (lex_peek().type == TOK_LPR) {
			lex_adv();
		} else {
			//missing ')'
		}

		//Parse either a declaration or expression statement
		ast_n *cond[3] = {0};
		if (is_type_spec(lex_peek())) {
			cond[0] = parse_decl();
		} else { 
			cond[0] = parse_stmt_expr();
		}
		cond[1] = parse_stmt_expr();

		//Parse the optional third expression
		if (lex_peek().type != TOK_RPR)
			cond[2] = parse_expr();

		if (lex_peek().type == TOK_RPR) {
			lex_adv();
		} else {
			//missing '('
		}

		node->dat.stmt.body = parse_stmt();
	} else if (lex_peek().type == TOK_KW_DO) {
		node = astn_new(STMT, STMT_DO_WHILE);
		lex_adv();

		node->dat.stmt.body = parse_stmt();

		//Parse condition
		if (lex_peek().type != TOK_KW_WHILE) {
			//expected 'while' ..
		}
		lex_adv();

		if (lex_peek().type == TOK_LPR) {
			lex_adv();
		} else {
			//missing ')'
		}

		node->dat.stmt.expr = parse_expr();

		if (lex_peek().type == TOK_RPR) {
			lex_adv();
		} else {
			//missing '('
		}

		if (lex_peek().type == TOK_SEM) {
			lex_adv();
		} else {
			//missing '('
		}

	}
	return node;
}

ast_n *parse_stmt_label() {
	ast_n *node = NULL;

	switch (lex_peek().type) {
		case TOK_IDENT:
			node = astn_new(STMT, STMT_LABEL);
			node->dat.stmt.expr = parse_expr_primary(); //TODO: replace with a plain identifier (expr_primary can pull parentheses chain)
			break;
		case TOK_KW_CASE:
			lex_adv();
			node = astn_new(STMT, STMT_CASE);
			node->dat.stmt.expr = parse_expr_cond(); //TODO: parse_constexpr();
			break;
		case TOK_KW_DEFAULT:
			lex_adv();
			node = astn_new(STMT, STMT_DEFAULT);
			break;
		default:
			return NULL;
	}

	if (lex_peek().type != TOK_COL) {
		//missing ':'
	}
	lex_adv();
	node->dat.stmt.body = parse_stmt();

	return node;
}

ast_n *parse_stmt_jump() {
	ast_n *node = NULL;

	switch (lex_peek().type) {
		case TOK_KW_GOTO:
			lex_adv();
			node = astn_new(STMT, STMT_GOTO);

			//Get an identifier
			ast_n *label = parse_expr_primary();
			if (label->dat.expr.kind != EXPR_IDENT) {
				//Expected identifier before...
				astn_del(label);
			}
			node->dat.stmt.expr = label;
			break;
		case TOK_KW_CONTINUE:
			lex_adv();
			node = astn_new(STMT, STMT_CONTINUE);
			break;
		case TOK_KW_BREAK:
			lex_adv();
			node = astn_new(STMT, STMT_BREAK);
			break;
		case TOK_KW_RETURN:
			lex_adv();
			node = astn_new(STMT, STMT_BREAK);
			
			if (lex_peek().type != TOK_SEM)
				node->dat.stmt.expr = parse_expr();
			break;
	}

	if (lex_peek().type != TOK_SEM) {
		//Expected ';' before...
	}
	lex_adv();

	return node;
}
