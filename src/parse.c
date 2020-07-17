#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include "ast.h"
#include "err.h"
#include "lex.h"
#include "eval.h"
#include "parse.h"

//TODO: remove intermediate decl/expr/stmt variables if only used a few times
//TODO: list length less than zero initial loop check
//TODO: if/else -> switch for branch and iteration stmt functions
//TODO: condense lex_adv(lx) calls for switch statements (possibly lex_consume?)

ast_n *parse_decl(lexer *lx);
s_type *parse_decl_spec(lexer *lx);
s_type *parse_decltr(lexer *lx, s_type *type, token *vname);
ast_n *parse_decl_body(lexer *lx, s_type *type);
ast_n *parse_decl_init(lexer *lx);

s_param *parse_fparam(lexer *lx);
vector parse_fparam_list(lexer *lx);
ast_n *parse_fdef(lexer *lx);

vector parse_smemb(lexer *lx);
s_type *parse_sdef(lexer *lx);
void parse_edef(lexer *lx);

ast_n *parse_expr(lexer *lx);
int parse_constexpr(lexer *lx);
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
					c_error(&lx->tgt->loc, "Premature end of input\n");
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
		if (node == NULL) continue;
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
ast_n *parse_decl(lexer *lx) {
	//Get specifiers
	s_type *type = parse_decl_spec(lx);
	if (!type)
		return NULL;
	
	//Check if specifiers were immediately followed by a semicolon
	token t = lex_peek(lx);
	if (t.type == TOK_SEM) {
		//Check for blank definition
		if (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION)
			c_error(&t.loc, "Declaration declares nothing\n");
		lex_adv(lx);
		type_del(type);
		return NULL;
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
			t = lex_peek(lx);
			if (t.type != TOK_SEM) {
				c_error(&t.loc, "Expected ';' after declaration\n");
			}
			lex_adv(lx);
			} break;

		case TOK_SEM:
			lex_adv(lx);
			break;

		case TOK_LBR:
			break;
	}
	type_del(type);

	return node;
}

s_type *parse_decl_spec(lexer *lx) {
	s_type *type = type_new(TYPE_UNDEF);

	/*
	 * Loop and process specifiers
	 */
	int from_typedef = 0;
	int is_long = 0, is_signed = 0, is_unsigned = 0;
	s_pos begin = lex_peek(lx).loc;
	for (;;) {
		token t = lex_peek(lx);

		//Check for conflicts
		switch (t.type) {
			case TOK_KW_VOID: case TOK_KW_CHAR: case TOK_KW_SHORT:
			case TOK_KW_INT: case TOK_KW_FLOAT: case TOK_KW_DOUBLE:
			case TOK_KW_STRUCT: case TOK_KW_UNION: case TOK_KW_ENUM:
				if (type->kind != TYPE_UNDEF) {
					c_error(&t.loc, "Multiple data types in declaration\n");
				}
				break;
			case TOK_KW_EXTERN: case TOK_KW_STATIC: case TOK_KW_AUTO:
			case TOK_KW_REGISTER: case TOK_KW_TYPEDEF:
				if (type->s_class != CLASS_UNDEF) {
					c_error(&t.loc, "Multiple storage classes in declaration\n");
				}
				break;
			case TOK_KW_SIGNED:
				if (is_signed > 0)
					c_error(&t.loc, "Duplicate 'signed' qualifier\n");
				break;
			case TOK_KW_UNSIGNED:
				if (is_unsigned > 0)
					c_error(&t.loc, "Duplicate 'unsigned' qualifier\n");
				break;
			case TOK_KW_CONST:
				if (type->is_const == 1)
					c_error(&t.loc, "Duplicate 'const' qualifier\n");
				break;
			case TOK_KW_VOLATILE:
				if (type->is_volatile == 1)
					c_error(&t.loc, "Duplicate 'volatile' qualifier\n");
				break;
		} 

		//Check for a user defined type
		if (t.type == TOK_IDENT) {
			symbol *td = symtable_search(&lx->stb, t.dat.sval);
			if (type->kind != TYPE_UNDEF) goto ts_end;
			if (td == NULL || td->btype->s_class != CLASS_TYPEDEF) goto ts_end;

			//Clone the predefined type
			s_type *usr_type = type_clone(td->type);
			s_type *usr_btype = usr_type;
			while (usr_btype->ref != NULL)
				usr_btype = usr_btype->ref;
			usr_btype->s_class = CLASS_UNDEF;

			//Copy over class/qualifiers if needed
			usr_btype->s_class = type->s_class;
			usr_btype->is_const = type->is_const;
			usr_btype->is_volatile = type->is_volatile;
			
			type_del(type);
			type = usr_type;
			lex_adv(lx);
			continue;
		} 

		//Process specifier
		s_type *su_type;
		switch (t.type) {
			case TOK_KW_VOID: 	type->kind = TYPE_VOID; break;
			case TOK_KW_CHAR: 	type->kind = TYPE_INT; type->size = 1; break;
			case TOK_KW_SHORT: 	type->kind = TYPE_INT; type->size = 2; break;
			case TOK_KW_FLOAT:	type->kind = TYPE_FLOAT; type->size = 4; break;
			case TOK_KW_TYPEDEF:	type->s_class = CLASS_TYPEDEF;	break;
			case TOK_KW_EXTERN:	type->s_class = CLASS_EXTERN;	break;
			case TOK_KW_STATIC:	type->s_class = CLASS_STATIC;	break;
			case TOK_KW_AUTO:	type->s_class = CLASS_AUTO; 	break;
			case TOK_KW_REGISTER:	type->s_class = CLASS_REGISTER;	break;
			case TOK_KW_SIGNED:	type->is_signed = 1;		break;
			case TOK_KW_UNSIGNED:	type->is_signed = 0;		break;
			case TOK_KW_CONST:	type->is_const = 1;		break;
			case TOK_KW_VOLATILE:	type->is_volatile = 1;		break;
			case TOK_KW_INT: 	
				if (is_long == 0 || is_long == 1)
					type->size = 4;
				else if (is_long == 2)
					type->size = 8;
				else
					c_error(&t.loc, "Too many 'long' for data type 'int'\n");
				type->kind = TYPE_INT;
				break;
			case TOK_KW_DOUBLE:
				if (is_long == 0)
					type->size = 8;
				else if (is_long == 1)
					type->size = 16;
				else
					c_error(&t.loc, "Too many 'long' for data type 'double'\n");
				type->kind = TYPE_FLOAT;
				break;
			case TOK_KW_LONG:
				//Either set the data type to int, or modify the current type
				if (type->kind == TYPE_UNDEF) {
					type->size = 4;
					if (is_long > 0)
						type->size = 8;
				} else if (type->kind == TYPE_INT) {
					if (is_long == 1)
						type->size = 8;
					else if (is_long > 1)
						c_error(&t.loc,
							"Too many 'long' for data type 'int'\n");
				} else if (type->kind == TYPE_FLOAT && type->size == 8) {
					if (is_long == 0)
						type->size = 16;
					else if (is_long > 0)
						c_error(&t.loc,
							"Too many 'long' for data type 'double'\n");
				} else {
					c_error(&t.loc, "Invalid use of 'long'\n");
				}
				is_long++;
				break;
			case TOK_KW_STRUCT:
			case TOK_KW_UNION:
				//Read struct/union type
				su_type = parse_sdef(lx);
				lex_unget(lx, make_tok_node(lex_peek(lx)));
				if (su_type == NULL) break;

				//Merge with current type
				su_type->s_class = type->s_class;
				su_type->is_const = type->is_const;
				su_type->is_volatile = type->is_volatile;
				type_del(type);
				type = su_type; 
				break;
			case TOK_KW_ENUM:
				parse_edef(lx);
				break;
			default:
				goto ts_end;
		}
		lex_adv(lx);
	}

	/*
	 * Error checking
	 */
	//Warning for no specified data type (unless 'long' is specified)
ts_end:	if (type->kind == TYPE_UNDEF) {
		if (is_long == 0)
			c_warn(&begin, "Declaration defaults to 'int'\n");
		if (is_long > 2)
			c_error(&begin, "Too many 'long' for data type 'int'\n");
		type->kind = TYPE_INT;
	}

	//Ensure signed/unsigned is only used with integer data types
	if (is_signed == 1 && type->kind != TYPE_INT)
		c_error(&begin, "Improper usage of 'signed' qualifier\n");
	if (is_unsigned == 1 && type->kind != TYPE_INT)
		c_error(&begin, "Improper usage of 'unsigned' qualifier\n");

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

		type = type_new(TYPE_PTR);
		type->ref_len = parse_constexpr(lx);

		//Missing ']'
		token t = lex_peek(lx);
		if (t.type != TOK_RBK)
			c_error(&t.loc, "Missing ']' after array size\n");
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

s_type *parse_decltr(lexer *lx, s_type *type, token *vname) {
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
		
		//Missing ')'
		token t = lex_peek(lx);
		if (lex_peek(lx).type != TOK_RPR)
			c_error(&t.loc, "Missing ')' in declarator\n");
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
		*vname = t;
	}
	return parse_decltr_back(lx, type);
}

ast_n *parse_decl_body(lexer *lx, s_type *base_type) {
	//Parse declarator
	token vname = {0}; 
	s_type *vtype = parse_decltr(lx, type_clone(base_type), &vname);

	//Create a new symbol
	//parse_decltr may not set vname in order to implement parameters
	if (vname.dat.sval == NULL)
		c_error(&vname.loc, "Missing variable name in declaration\n");

	symbol *s = symtable_def(&lx->stb, vname.dat.sval, vtype, &vname.loc);
	if (!s) { //If duplicate exists
		free(vname.dat.sval);
		type_del(vtype);
	}

	ast_n *node = astn_new(DECL, DECL_STD, lex_peek(lx));
	ast_decl *decl_n = &node->dat.decl;
	decl_n->sym = s;

	//Check for and parse initialization
	token t = lex_peek(lx);
	if (t.type == TOK_ASSIGN) {
		lex_adv(lx);
		//decl_n->init = parse_decl_init(lx, node);
		
		//Typedefs cannot be initialized
		if (base_type->s_class == CLASS_TYPEDEF)
			c_error(&t.loc, "Attempt to initialize a typedef\n");
	}

	return node;
}

//Parse an initialization designation
ast_n *parse_init_desi(lexer *lx) {
	
}

//Parses a list of initializations
ast_n *parse_init_list(lexer *lx) {
	ast_n *list = NULL;
	ast_n *cur = NULL;

	//Parse first initialization
	

	//Parse subsequent elements
	while (lex_peek(lx).type != TOK_RBR) {
		//Check for premature end of input
		if (lex_peek(lx).type == TOK_END) {
			return NULL;
		}
	}

	return list;
}

//Parse an initializer for a given object
ast_n *parse_decl_init(lexer *lx) {
	ast_n *node;
	if (lex_peek(lx).type == TOK_LBR) {
		//Read initializer list
		token lbr = lex_peek(lx);
		lex_adv(lx);
		node = parse_init_list(lx);

		//Optional ',' after list
		if (lex_peek(lx).type == TOK_CMM)
			lex_adv(lx);

		//Expect '}'
		token rbr = lex_peek(lx);
		if (rbr.type == TOK_RBR) {
			lex_adv(lx);
		} else if (rbr.type == TOK_END) {
			c_error(&lbr.loc, "Unexpected end of input in initializer\n");
		} else {
			c_error(&rbr.loc, "Expected '}' after initializer list\n");
		}
	} else {
		node = parse_expr_assign(lx);
	}

	return node;
}

s_param *parse_fparam(lexer *lx) {
	//Parse type
	token vname = {0};
	s_type *base_type = parse_decl_spec(lx);

	//Parse declarator
	s_type *type = parse_decltr(lx, base_type, &vname);

	//Create parameter and return
	return param_new(type, vname.dat.sval);
}

vector parse_fparam_list(lexer *lx) {
	//Initialize vector
	vector params;
	vector_init(&params, VECTOR_EMPTY);

	//Return NULL for a blank list
	if (lex_peek(lx).type == TOK_RPR)
		return params;

	//Parse initial item
	s_param *p = parse_fparam(lx);
	vector_add(&params, p);

	//Parse multiple declarations
	while (lex_peek(lx).type == TOK_CMM) {
		lex_adv(lx);
		p = parse_fparam(lx);
		vector_add(&params, p);
	}

	return params;
}

//Parse a function definition
ast_n *parse_fdef(lexer *lx) {
	//Get return type
	s_type *r_type = parse_decl_spec(lx);

	//Get declarator
	token f_name = {0};
	s_type *f_type = parse_decltr(lx, r_type, &f_name);
	f_type->ret = r_type;

	//Create symbol (or reuse unless there is a redefinition conflict)
	symbol *s = symtable_def(&lx->stb, f_name.dat.sval, f_type, &f_name.loc);
	if (s->fbody != NULL)
		c_error(&f_name.loc, "Redefinition of '%s'\n", f_name.dat.sval);

	//Enter function scope
	symtable_scope_enter(&lx->stb);
	lx->stb.func = s;
	for (int i=0; i<f_type->param.len; i++) {
		s_param *p = f_type->param.table[i];
		symtable_def(&lx->stb, p->name, p->type, NULL);
	}

	//Create node and parse body
	ast_n *node = astn_new(DECL, DECL_FUNC, lex_peek(lx));
	node->dat.decl.sym = s;
	node->dat.decl.block = parse_stmt_cmpd(lx);

	//Leave function scope
	symtable_scope_leave(&lx->stb);
	lx->stb.func = NULL;

	return node;
}

vector parse_sdef_body(lexer *lx) {
	//Advance past '{'
	lex_adv(lx);

	//Parse list of declarations
	vector memb;
	vector_init(&memb, VECTOR_EMPTY);
	while (lex_peek(lx).type != TOK_RBR) {
		if (lex_peek(lx).type == TOK_END) {
			c_error(&lx->tgt->loc, "Expected '}' before end of file\n");
		}

		//Get type
		//TODO: no type classes when parsing for specifier
		token mname = {0};
		s_type *base_type = parse_decl_spec(lx);

		//Parse declarator
		//TODO: bitfields
		s_type *mtype = parse_decltr(lx, type_clone(base_type), &mname);
		if (mname.dat.sval != NULL) {
			//Create new member
			s_param *m = param_new(mtype, mname.dat.sval);
			vector_add(&memb, m);
		} else {
			c_error(&mname.loc, "Declaration declares nothing\n");
		}
		type_del(base_type);

		//Expect ';'
		token t = lex_peek(lx);
		if (t.type != TOK_SEM)
			c_error(&t.loc, "Missing ';' after member definition\n");
		lex_adv(lx);
	}

	//Missing '}'
	token t = lex_peek(lx);
	if (t.type != TOK_RBR)
		c_error(&t.loc, "Missing '}' after struct/union declaration\n");
	lex_adv(lx);

	return memb;
}

//Parse a struct type
s_type *parse_sdef(lexer *lx) {
	s_type *type = type_new(TYPE_UNDEF);
	switch (lex_peek(lx).type) {
		case TOK_KW_STRUCT:
			type->kind = TYPE_STRUCT;
			break;
		case TOK_KW_UNION:
			type->kind = TYPE_UNION;
			break;
		default:
			type_del(type);
			return NULL;
	}
	lex_adv(lx);

	//Get identifier and member list
	char *tname = NULL;
	s_pos nloc, aloc;
	if (lex_peek(lx).type == TOK_IDENT) {
		tname = lex_peek(lx).dat.sval;
		nloc = lex_peek(lx).loc;
		lex_adv(lx);
	}
	aloc = lex_peek(lx).loc;
	if (lex_peek(lx).type == TOK_LBR)
		type->param = parse_sdef_body(lx);

	/* 
	 * Four cases:
	 * struct is anonymous (list but no name)
	 * struct is already defined (name but no list)
	 * struct needs to be defined (name and list)
	 * invalid (no name and no list)
	 */
	if (tname == NULL) {
		if (type->param.len == VECTOR_EMPTY) {
			c_error(&aloc, "Expected '{' or identifier before ...\n");
			type_del(type);
			return NULL;
		} 
	} else {
		symbol *s = NULL;
		if (type->param.len == VECTOR_EMPTY) {
			//Search the symbol table for an existing definition
			s = symtable_search(&lx->stb, tname);
			type_del(type);
			if (s == NULL) {
				c_error(&nloc, "Undefined struct '%s'\n", tname);
				free(tname);
				return NULL;
			}
			free(tname);
			type = type_clone(s->type);
		} else {
			s = symtable_def(&lx->stb, tname, type_clone(type), &nloc);
		}
	}

	return type;
}

//Parses a single enumeration
void parse_enum(lexer *lx, int *val) {
	//Read name
	token t = lex_peek(lx);
	if (t.type != TOK_IDENT) {
		c_error(&t.loc, "Invalid input in enumeration\n");
		return;
	}
	lex_adv(lx);

	//Read optional expression
	if (lex_peek(lx).type == TOK_ASSIGN) {
		lex_adv(lx);
		*val = parse_constexpr(lx);
	}

	//Define a macro for the enumeration
	char buf[64];
	snprintf(buf, 64, "%i", *val);
	symbol *sym = symtable_def(&lx->stb, t.dat.sval, type_new(TYPE_MACRO), &t.loc);

	//Copy the expansion to a new string
	char *exp = malloc(strlen(buf)+1);
	strncpy(exp, buf, strlen(buf));
	sym->mac_exp = exp;
	
	(*val)++;
	return;
}

//Parse an enum type
void parse_edef(lexer *lx) {
	lex_adv(lx);

	//Read identifier if available
	char *tname = NULL;
	s_pos nloc, aloc;
	if (lex_peek(lx).type == TOK_IDENT) {
		tname = lex_peek(lx).dat.sval;
		nloc = lex_peek(lx).loc;
		lex_adv(lx);
	}

	//Read list of enumerations
	aloc = lex_peek(lx).loc;
	if (lex_peek(lx).type == TOK_LBR) {
		int e_val = 0;
		lex_adv(lx);
		token t = lex_peek(lx);
		while (t.type != TOK_RBR) {
			//Check for premature end of input
			if (t.type == TOK_END) {
				c_error(&aloc, "Expected '}' before end of input\n");
				break;
			}

			//Parse a single enumeration
			parse_enum(lx, &e_val);

			//Check for comma
			t = lex_peek(lx);
			if (t.type == TOK_IDENT) {
				c_error(&t.loc, "Expected ',' before identifier\n");
				continue;
			} else if (t.type != TOK_CMM && t.type != TOK_RBR) {
				c_error(&t.loc, "Invalid token in enum definition\n");
			}
			lex_adv(lx);
		}
	}

	return;
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

int parse_constexpr(lexer *lx) {
	//Read constant expression
	ast_n *expr = parse_expr_cond(lx);
	
	//Evaluate the expression
	int err = 0;
	int res = eval_constexpr_ast(expr, &err);

	astn_del(expr);
	return res;
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
base:			n_node = astn_new(EXPR, EXPR_ASSIGN, lex_peek(lx));
			lex_adv(lx);
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
		ast_n *opr = astn_new(EXPR, atype, lex_peek(lx));
		opr->dat.expr.left = node;
		opr->dat.expr.right = n_node->dat.expr.right;
		n_node->dat.expr.right = opr;
	}

	return n_node;
}

ast_n *parse_expr_cond(lexer *lx) {
	ast_n *node = parse_expr_logic_or(lx);

	if (lex_peek(lx).type == TOK_QMK) {
		//Construct ternary node
		ast_n *n_node = astn_new(EXPR, EXPR_COND, lex_peek(lx));
		ast_expr *n_expr = &n_node->dat.expr;
		n_expr->left = node;
		lex_adv(lx);

		//Read left result and check for ';'
		ast_n *res_true = parse_expr(lx);
		token t = lex_peek(lx);
		if (t.type != TOK_COL)
			c_error(&t.loc, "Expected ':' in ternary expression\n");
		lex_adv(lx);

		//Read right result and construct result node
		ast_n *res = astn_new(EXPR, EXPR_COND_RES, t);
		ast_n *res_false = parse_expr_cond(lx);
		res->dat.expr.left = res_true;
		res->dat.expr.right = res_false;
		n_expr->right = res;
		return n_node;
	}

	return node;
}

ast_n *parse_expr_logic_or(lexer *lx) {
	ast_n *node = parse_expr_logic_and(lx);

	if (lex_peek(lx).type == TOK_LOGIC_OR) {
		ast_n *n_node = astn_new(EXPR, EXPR_LOGIC_OR, lex_peek(lx));
		lex_adv(lx);
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
		ast_n *n_node = astn_new(EXPR, EXPR_LOGIC_AND, lex_peek(lx));
		lex_adv(lx);
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
		ast_n *n_node = astn_new(EXPR, EXPR_OR, lex_peek(lx));
		lex_adv(lx);
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
		ast_n *n_node = astn_new(EXPR, EXPR_XOR, lex_peek(lx));
		lex_adv(lx);
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
		ast_n *n_node = astn_new(EXPR, EXPR_AND, lex_peek(lx));
		lex_adv(lx);
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
			n_node = astn_new(EXPR, EXPR_EQ, t);
			lex_adv(lx);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_relation(lx);
			return n_node;
		case TOK_NEQ:
			n_node = astn_new(EXPR, EXPR_NEQ, t);
			lex_adv(lx);
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
			n_node = astn_new(EXPR, EXPR_LTH, t);
			lex_adv(lx);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
		case TOK_GTH:
			n_node = astn_new(EXPR, EXPR_GTH, t);
			lex_adv(lx);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
		case TOK_LEQ:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_LEQ, t);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_shift(lx);
			return n_node;
		case TOK_GEQ:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_GEQ, t);
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
			n_node = astn_new(EXPR, EXPR_LSHIFT, t);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt(lx);
			return n_node;
		case TOK_RSHIFT:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_RSHIFT, t);
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
			n_node = astn_new(EXPR, EXPR_ADD, t);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_addt(lx);
			return n_node;
		case TOK_SUB:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_SUB, t);
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
			n_node = astn_new(EXPR, EXPR_MUL, t);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast(lx);
			return n_node;
		case TOK_DIV:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_DIV, t);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast(lx);
			return n_node;
		case TOK_MOD:
			lex_adv(lx);
			n_node = astn_new(EXPR, EXPR_MOD, t);
			n_expr = &n_node->dat.expr;
			n_expr->left = node;
			n_expr->right = parse_expr_cast(lx);
			return n_node;
	}

	return node;
}

ast_n *parse_expr_cast(lexer *lx) {
	ast_n *node = parse_expr_unary(lx);
	
	ast_n *n_node;
	token t = lex_peek(lx);
	if (t.type == TOK_LPR) {
		lex_adv(lx);
		
		//Parse type
		token vname = BLANK_TOKEN;
		s_type *base_type = parse_decl_spec(lx);
		s_type *type = parse_decltr(lx, base_type, &vname);
		
		//Ensure the type didn't contain a variable name
		if (vname.type != -1)
			c_error(&vname.loc, "Unexpected identifier in cast expression\n");

		//Check for ')'
		token t = lex_peek(lx);
		if (t.type != TOK_RPR)
			c_error(&t.loc, "Missing ')' after parameter list\n");
		lex_adv(lx);

		//Parse following the type cast
		ast_n *n_node = astn_new(EXPR, EXPR_CAST, t);
		n_node->dat.expr.type = type;
		n_node->dat.expr.left = parse_expr_cast(lx);

		//Join to results of parse_expr_unary if needed
		//If the operation is a reference or inc/dec, flag an error
		if (node != NULL) {
			ast_expr *n_expr = &node->dat.expr;
			if (n_expr->kind == EXPR_INC_PRE)
				c_error(NULL, "lvalue required for pre-increment\n");
			if (n_expr->kind == EXPR_DEC_PRE)
				c_error(NULL, "lvalue required for pre-decrement\n");
			if (n_expr->kind == EXPR_REF)
				c_error(NULL, "lvalue required for reference operation\n");
			n_expr->left = n_node;
			return node;
		}
		return n_node;
	}
	
	return node; 
}

ast_n *parse_expr_unary(lexer *lx) {
	ast_n *n_node = NULL;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_AND: lex_adv(lx); n_node = astn_new(EXPR, EXPR_REF, t); 	break;
		case TOK_ASR: lex_adv(lx); n_node = astn_new(EXPR, EXPR_DEREF, t); 	break;
		case TOK_ADD: lex_adv(lx); n_node = astn_new(EXPR, EXPR_POS, t); 	break;
		case TOK_SUB: lex_adv(lx); n_node = astn_new(EXPR, EXPR_NEG, t); 	break;
		case TOK_NOT: lex_adv(lx); n_node = astn_new(EXPR, EXPR_LOGIC_NOT, t); 	break;
		case TOK_TLD: lex_adv(lx); n_node = astn_new(EXPR, EXPR_NOT, t); 	break;
		case TOK_INC: lex_adv(lx); n_node = astn_new(EXPR, EXPR_INC_PRE, t);	break;
		case TOK_DEC: lex_adv(lx); n_node = astn_new(EXPR, EXPR_DEC_PRE, t); 	break;
		//TODO case TOK_KW_SIZEOF:
	}

	ast_n *node = parse_expr_postfix(lx);
	if (n_node != NULL) {
		n_node->dat.expr.left = node;
		return n_node;
	}

	return node;
}

ast_n *parse_expr_arglist(lexer *lx) {
	ast_n *node = parse_expr_assign(lx);

	//Arguments will be evaluated in reverse order, so the list will be backwards
	ast_n *head = node;
	while (lex_peek(lx).type == TOK_CMM) {
		lex_adv(lx);
		node = parse_expr_assign(lx);
		node->next = head;
		head = node;
	}

	return head;
}

ast_n *parse_expr_postfix(lexer *lx) {
	ast_n *node = parse_expr_primary(lx);

	ast_n *n_node;
	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_LBK: //Array accessor
			n_node = astn_new(EXPR, EXPR_ARRAY, t);
			n_node->dat.expr.left = node;
			lex_adv(lx);

			//Get offset
			n_node->dat.expr.right = parse_expr(lx);
			//n_node->dat.expr.right = parse_expr_primary(lx);
			
			t = lex_peek(lx);
			if (t.type != TOK_RBK)
				c_error(&t.loc, "Missing ']' after array offset\n");
			lex_adv(lx);

			return n_node;
		case TOK_LPR: //Function calls
			//Check for a cast expression
			lex_adv(lx);
			if (is_type_spec(lx, lex_peek(lx))) {
				lex_unget(lx, make_tok_node(lex_peek(lx)));
				lex_unget(lx, make_tok_node(t));
				break;
			}
			n_node = astn_new(EXPR, EXPR_CALL, t);
			n_node->dat.expr.left = node;

			//Empty parameter list
			if (lex_peek(lx).type == TOK_RPR) {
				lex_adv(lx);
				return n_node;
			}

			//Get parameter list
			n_node->dat.expr.right = parse_expr_arglist(lx);
			
			t = lex_peek(lx);
			if (t.type != TOK_RPR)
				c_error(&t.loc, "Missing ')' after parameter list\n");
			lex_adv(lx);
			s_type *type = astn_type(n_node);

			return n_node;
		case TOK_PRD:
		case TOK_PTR:
			n_node = astn_new(EXPR, (t.type == TOK_PRD) ? EXPR_MEMB : EXPR_PTR_MEMB, t);
			lex_adv(lx);

			//Check for identifier
			t = lex_peek(lx);
			if (t.type != TOK_IDENT) {
				c_error(&t.loc, "Expected identifier after '%s'\n",
					(t.type == TOK_PRD) ? "." : "->");
			}
			n_node->dat.expr.left = node;
			n_node->dat.expr.right = astn_new(EXPR, EXPR_IDENT, t);
			lex_adv(lx);
			return n_node;
		case TOK_INC:
			n_node = astn_new(EXPR, EXPR_INC_POST, t);
			n_node->dat.expr.left = node;
			lex_adv(lx);
			return n_node;
		case TOK_DEC:
			n_node = astn_new(EXPR, EXPR_INC_POST, t);
			n_node->dat.expr.left = node;
			lex_adv(lx);
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
			symbol *s = symtable_search(&lx->stb, t.dat.sval);
			node = astn_new(EXPR, EXPR_IDENT, t);
			if (s != NULL) {
				node->dat.expr.sym = s;
			} else {
				c_error(&t.loc, "Undeclared variable \"%s\"\n", t.dat.sval);
			}
			free(t.dat.sval);

			return node;
		case TOK_CONST:
			lex_adv(lx);
			node = astn_new(EXPR, EXPR_CONST, t);
			return node;
		case TOK_STR:
			lex_adv(lx);
			node = astn_new(EXPR, EXPR_STR, t);
			return node;
		case TOK_LPR:
			lex_adv(lx);
			
			//Check for a cast expression
			if (is_type_spec(lx, lex_peek(lx))) {
				lex_unget(lx, make_tok_node(lex_peek(lx)));
				lex_unget(lx, make_tok_node(t));
				break;
			}
			
			node = parse_expr(lx);
			lex_adv(lx); //expect ')'
			return node;
	}
	return NULL;
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
		case TOK_IDENT: {
			//Check for label
			//TODO: clean
			token_n *tn = make_tok_node(lex_peek(lx));
			lex_adv(lx);
			token t = lex_peek(lx);
			lex_unget(lx, make_tok_node(t));
			lex_unget(lx, tn);
			if (t.type == TOK_COL) {
				node = parse_stmt_label(lx);
				break;
			}
			} //Fall through
		default:
			node = parse_stmt_expr(lx);
			break;
	}
	return node;
}

//Parses a parentheses-enclosed expression used in if statments, while loops, etc
ast_n *parse_condition(lexer *lx, char *of) {
	if (lex_peek(lx).type == TOK_LPR) {
		lex_adv(lx);
	} else {
		c_error(&lx->tgt->loc, "Missing '(' before condition body\n");
	}

	ast_n *cond = parse_expr(lx);
	if (cond == NULL)
		c_error(&lx->tgt->loc, "Expected expression in %s condition\n", of);

	if (lex_peek(lx).type == TOK_RPR) {
		lex_adv(lx);
	} else {
		c_error(&lx->tgt->loc, "Missing ')' after condition body\n");
	}

	return cond;
}

ast_n *parse_stmt_cmpd(lexer *lx) {
	//Expect '{'
	token t = lex_peek(lx);
	if (t.type == TOK_LBR)
		lex_adv(lx);

	//Parse list of declarations/statements here
	ast_n *base = NULL;
	ast_n *last = NULL;
	while (lex_peek(lx).type != TOK_RBR) {
		//Premature end of file
		token t = lex_peek(lx);
		if (t.type == TOK_END) {
			c_error(&t.loc, "Expected '}' before end of input!\n");
			break;
		}

		//Parse a statement or a declaration
		ast_n *node;
		if (is_type_spec(lx, lex_peek(lx))) {
			node = parse_decl(lx);
		} else {
			node = parse_stmt(lx);
		}
		if (node == NULL) continue;

		//Add the node to the list
		if (base == NULL) {
			base = node;
		} else {
			last->next = node;
		}

		//Move to the end of the list
		//If node already has a next, move to the end of it
		if (node != NULL) {
			last = node;
			while (last->next != NULL)
				last = last->next;
		}
	}

	//Expect '}'
	if (lex_peek(lx).type == TOK_RBR)
		lex_adv(lx);

	ast_n *head = astn_new(STMT, STMT_CMPD, t);
	head->next = base;
	return head;
}

ast_n *parse_stmt_expr(lexer *lx) {
	ast_n *node;
	if (lex_peek(lx).type == TOK_SEM) {
		lex_adv(lx);
		return NULL;
	}

	node = astn_new(STMT, STMT_EXPR, lex_peek(lx));
	node->dat.stmt.expr = parse_expr(lx);

	if (lex_peek(lx).type == TOK_SEM) {
		lex_adv(lx);
	} else {
		token t = lex_peek(lx);
		c_error(&t.loc, "Missing ';' after expression\n");
	}

	return node;
}

//TODO: switch statements
ast_n *parse_stmt_selection(lexer *lx) {
	ast_n *node;
	if (lex_peek(lx).type == TOK_KW_IF) {
		node = astn_new(STMT, STMT_IF, lex_peek(lx));
		lex_adv(lx);

		node->dat.stmt.expr = parse_condition(lx, "if statement");
		node->dat.stmt.body = parse_stmt(lx);
		if (lex_peek(lx).type == TOK_KW_ELSE) {
			lex_adv(lx);
			node->dat.stmt.else_body = parse_stmt(lx);
		}
	} else if (lex_peek(lx).type == TOK_KW_SWITCH) {
		node = astn_new(STMT, STMT_SWITCH, lex_peek(lx));
		lex_adv(lx);

		node->dat.stmt.expr = parse_condition(lx, "switch statement");
		node->dat.stmt.body = parse_stmt(lx);
	}

	return node;
}

ast_n *parse_stmt_while(lexer *lx) {
	ast_n *node = astn_new(STMT, STMT_WHILE, lex_peek(lx));
	node->dat.stmt.expr = parse_condition(lx, "while loop");
	node->dat.stmt.body = parse_stmt(lx);
	return node;
}

ast_n *parse_stmt_for(lexer *lx) {
	//A for loop will become a while loop with two optional parts
	ast_n *node = astn_new(STMT, STMT_WHILE, lex_peek(lx));

	//Check for missing '('
	if (lex_peek(lx).type == TOK_LPR) {
		lex_adv(lx);
	} else {
		token t = lex_peek(lx);
		c_error(&t.loc, "Missing '(' before for-loop condition\n");
	}

	//Parse either a declaration or expression statement
	ast_n *cond[3] = {0};
	if (is_type_spec(lx, lex_peek(lx))) {
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
		token t = lex_peek(lx);
		c_error(&t.loc, "Missing '(' after for-loop condition\n");
	}

	//Parse the body and set the condition
	node->dat.stmt.expr = cond[1];
	node->dat.stmt.body = parse_stmt(lx);

	//If the last item isn't blank, add it to the end of the loop body
	if (cond[2] != NULL) {
		ast_n *end = node->dat.stmt.body;
		while (end->next != NULL)
			end = end->next;
		end->next = cond[2];
	}

	//If the first item isn't blank, set its next to the loop and return it
	if (cond[0] != NULL) {
		cond[0]->next = node;
		return cond[0];
	}
	return node;
}

ast_n *parse_stmt_do_while(lexer *lx) {
	ast_n *node = astn_new(STMT, STMT_DO_WHILE, lex_peek(lx));

	//Parse body
	node->dat.stmt.body = parse_stmt(lx);

	//Parse condition
	if (lex_peek(lx).type == TOK_KW_WHILE) {
		lex_adv(lx);
	} else {
		c_error(&lx->tgt->loc, "Expected 'while' after do-while body\n");
	}

	node->dat.stmt.expr = parse_condition(lx, "do-while loop");

	if (lex_peek(lx).type == TOK_SEM) {
		lex_adv(lx);
	} else {
		c_error(&lx->tgt->loc, "Missing ';' after do-while condition\n");
	}
	return node;
}

ast_n *parse_stmt_iteration(lexer *lx) {
	switch (lex_peek(lx).type) {
		case TOK_KW_WHILE: lex_adv(lx); return parse_stmt_while(lx);
		case TOK_KW_FOR:   lex_adv(lx); return parse_stmt_for(lx);
		case TOK_KW_DO:	   lex_adv(lx); return parse_stmt_do_while(lx);
	}
}

ast_n *parse_stmt_label(lexer *lx) {
	ast_n *node = NULL;

	token t = lex_peek(lx);
	switch (t.type) {
		case TOK_IDENT:
			node = astn_new(STMT, STMT_LABEL, t);
			symtable_def_label(&lx->stb, t.dat.sval, &t.loc);
			node->dat.stmt.lbl = t.dat.sval;
			lex_adv(lx);
			break;
		case TOK_KW_CASE:
			node = astn_new(STMT, STMT_CASE, t);
			lex_adv(lx);
			node->dat.stmt.expr = parse_expr_cond(lx); //TODO: parse_constexpr();
			break;
		case TOK_KW_DEFAULT:
			node = astn_new(STMT, STMT_DEFAULT, t);
			lex_adv(lx);
			break;
		default:
			return NULL;
	}

	//Missing ':'
	t = lex_peek(lx);
	if (t.type != TOK_COL)
		c_error(&t.loc, "Missing ':' after label name\n");
	lex_adv(lx);

	return node;
}

ast_n *parse_stmt_jump(lexer *lx) {
	ast_n *node = NULL;

	switch (lex_peek(lx).type) {
		case TOK_KW_GOTO:
			node = astn_new(STMT, STMT_GOTO, lex_peek(lx));
			lex_adv(lx);

			token t = lex_peek(lx);
			if (t.type != TOK_IDENT)
				c_error(&t.loc, "Expected identifier after 'goto'\n");
			lex_adv(lx);
			node->dat.stmt.lbl = t.dat.sval;
			break;
		case TOK_KW_CONTINUE:
			node = astn_new(STMT, STMT_CONTINUE, lex_peek(lx));
			lex_adv(lx);
			break;
		case TOK_KW_BREAK:
			node = astn_new(STMT, STMT_BREAK, lex_peek(lx));
			lex_adv(lx);
			break;
		case TOK_KW_RETURN:
			node = astn_new(STMT, STMT_RETURN, lex_peek(lx));
			lex_adv(lx);
			
			if (lex_peek(lx).type != TOK_SEM)
				node->dat.stmt.expr = parse_expr(lx);
			break;
	}

	//Missing ';'
	token t = lex_peek(lx);
	if (lex_peek(lx).type != TOK_SEM)
		c_error(&t.loc, "Missing ']' after statement\n");
	lex_adv(lx);

	return node;
}
