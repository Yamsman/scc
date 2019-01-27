#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include "ast.h"
#include "err.h"
#include "inst.h"
#include "gen.h"
#include "util/map.h"

void gen_stmt(asm_f *f, ast_n *n);
void gen_expr(asm_f *f, ast_n *n);

char *gen_numlabel(asm_f *f) {
	//Generate a label and store it in the map
	int num = f->lnum++;
	char *buf = malloc(16);
	snprintf(buf, 16, ".L%i", num);
	map_insert(&f->labels, buf, buf);
	return buf;
}

void gen_decl_init(asm_f *f, ast_n *n) {
	
}

void gen_decl(asm_f *f, ast_n *n) {
	
}

void gen_expr_logic_andor(asm_f *f, ast_n *n) {
	//Logical-and expressions can short circuit if the LHS is 0,
	//and logical-or expressions can if the LHS is 1
	int jmp_i = -1;
	int pass_val = -1;
	char *lbl_sc = gen_numlabel(f);
	char *lbl_end = gen_numlabel(f);
	switch (n->dat.expr.kind) {
		case EXPR_LOGIC_AND: jmp_i = INST_JNZ; pass_val = 0; break;
		case EXPR_LOGIC_OR:  jmp_i = INST_JZ;  pass_val = 1; break;
	}

	//Generate LHS, compare the pass_value, and short-circuit if possible
	gen_expr(f, n->dat.expr.left);
	asmf_add_inst(f, mk_inst(INST_TEST, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_REG, RAX)
	));
	asmf_add_inst(f, mk_inst(INST_JZ, 1, mk_oprd_label(lbl_sc)));

	//Generate RHS, compare the pass_value, and short-circuit if possible
	gen_expr(f, n->dat.expr.right);
	asmf_add_inst(f, mk_inst(INST_TEST, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_REG, RAX)
	));
	asmf_add_inst(f, mk_inst(INST_JZ, 1, mk_oprd_label(lbl_sc)));

	//If this point was reached, 'and' is true or 'or' is false
	asmf_add_inst(f, mk_inst(INST_MOV, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_IMM, !pass_val)
	));
	asmf_add_inst(f, mk_inst(INST_JZ, 1, mk_oprd_label(lbl_end)));

	//Short-circuit result
	asmf_add_inst(f, mk_label(lbl_sc));
	asmf_add_inst(f, mk_inst(INST_MOV, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_IMM, pass_val)
	));
	asmf_add_inst(f, mk_label(lbl_end));

	return;
}

void gen_expr_cmp(asm_f *f, ast_n *n) {
	int set_i = -1;
	switch (n->dat.expr.kind) {
		case EXPR_EQ:  set_i = INST_SETE;	break;
		case EXPR_NEQ: set_i = INST_SETNE;	break;
		case EXPR_GTH: set_i = INST_SETG;	break;
		case EXPR_LTH: set_i = INST_SETL;	break;
		case EXPR_GEQ: set_i = INST_SETGE;	break;
		case EXPR_LEQ: set_i = INST_SETLE;	break;
	}
	
	//Generate LHS into RAX and push the result
	gen_expr(f, n->dat.expr.left);
	asmf_add_inst(f, mk_inst(INST_PUSH, 1, mk_oprd(OPRD_REG, RAX)));

	//Generate RHS into RAX, pop LHS into register for comparison
	gen_expr(f, n->dat.expr.right);
	asmf_add_inst(f, mk_inst(INST_POP, 1, mk_oprd(OPRD_REG, RBX)));

	//Compare the two results and set EAX based on comparison type
	asmf_add_inst(f, mk_inst(INST_CMP, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_REG, RBX)
	));
	asmf_add_inst(f, mk_inst(set_i, 1, mk_oprd(OPRD_REG, AL)));
	asmf_add_inst(f, mk_inst(INST_MOVZX, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_REG, AL)	
	));

	return;
}

void gen_expr_bitwise(asm_f *f, ast_n *n) {
	int bit_i = -1;
	switch (n->dat.expr.kind) {
		case EXPR_AND: bit_i = INST_AND; break;
		case EXPR_OR:  bit_i = INST_OR;  break;
		case EXPR_XOR: bit_i = INST_XOR; break;
	}

	//Perform bitwise operation
	gen_expr(f, n->dat.expr.left);
	asmf_add_inst(f, mk_inst(INST_PUSH, 1, mk_oprd(OPRD_REG, RAX)));
	gen_expr(f, n->dat.expr.right);
	asmf_add_inst(f, mk_inst(INST_POP, 1, mk_oprd(OPRD_REG, RBX)));
	asmf_add_inst(f, mk_inst(bit_i, 2, 
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_REG, RBX)
	));

	return;
}

void gen_expr_arith(asm_f *f, ast_n *n) {
	gen_expr(f, n->dat.expr.left);

}

void gen_expr_ref(asm_f *f, ast_n *n) {

}

void gen_expr_deref(asm_f *f, ast_n *n) {
	gen_expr(f, n->dat.expr.left);
	asmf_add_inst(f, mk_inst(INST_MOV, 2, 
		mk_oprd(OPRD_REG, RAX),
		mk_oprd_ex(OPRD_REG, RAX, DEREF, 0)
	));

	return;
}

void gen_expr_array(asm_f *f, ast_n *n) {
	gen_expr(f, n->dat.expr.left);
	asmf_add_inst(f, mk_inst(INST_MOV, 2, 
		mk_oprd(OPRD_REG, RBX),
		mk_oprd(OPRD_REG, RAX)
	));

	ast_n *ofs = n->dat.expr.right;
	if (ofs->dat.expr.kind == EXPR_CONST) {
		if (ofs->dat.expr.sym->type->kind != TYPE_INT) {
			c_error(NULL, "Array offset must be an integer\n");
			return;
		}
		asmf_add_inst(f, mk_inst(INST_MOV, 2, 
			mk_oprd(OPRD_REG, RAX),
			mk_oprd_ex(OPRD_REG, RAX, DEREF_IMM, 0) //placeholder
		));
	}

	return;
}

void gen_expr_call(asm_f *f, ast_n *n) {
	/*
	symbol *fsym = n->dat.decl.sym;

	//Push args to stack
	vector params = fsym->type->param;
	ast_n *arg = n->dat.expr.left;
	for (int i=params.len-1; i>=0; i++) {
		symbol *psym = params.table[i];	//Symbol of defined parameter
		symbol *asym = arg->dat.expr.left->dat.expr.sym; //Symbol of call arg

		//Check types
		if (psym->type->kind != asym->type->kind)
			c_error(NULL, "Mismatching types in function call\n");

		//Generate the expression
		gen_expr(f, arg);

		//Push the argument
		
		arg = arg->next;
	}

	//call function
	return;
	*/
}

void gen_expr_immd(asm_f *f, ast_n *n) {
	return;
}

void gen_expr(asm_f *f, ast_n *n) {
	switch (n->dat.expr.kind) {
		case EXPR_ASSIGN:
			break;
		case EXPR_LOGIC_AND:
		case EXPR_LOGIC_OR:
			gen_expr_logic_andor(f, n);
			break;
		case EXPR_LOGIC_NOT:
			break;
		case EXPR_EQ:
		case EXPR_NEQ:
		case EXPR_GTH:
		case EXPR_LTH:
		case EXPR_GEQ:
		case EXPR_LEQ:
			gen_expr_cmp(f, n);
			break;
		case EXPR_AND:
		case EXPR_OR:
		case EXPR_XOR:
			gen_expr_bitwise(f, n);
			break;
		case EXPR_NOT:
			gen_expr(f, n->dat.expr.left);
			asmf_add_inst(f, mk_inst(INST_NOT, 1,
				mk_oprd(OPRD_REG, EAX)
			));
			break;
		case EXPR_ADD:
		case EXPR_SUB:
		case EXPR_MUL:
		case EXPR_DIV:
		case EXPR_MOD:
		case EXPR_LSHIFT:
		case EXPR_RSHIFT:
			//gen_expr_arith(
			break;
		case EXPR_CAST:
		case EXPR_REF:
			gen_expr_ref(f, n);
			break;
		case EXPR_DEREF:
			gen_expr_deref(f, n);
			break;
		case EXPR_POS:
		case EXPR_NEG:
		case EXPR_INC_PRE:
		case EXPR_DEC_PRE:
		case EXPR_ARRAY:
			gen_expr_array(f, n);
			break;
		case EXPR_CALL:
			gen_expr_call(f, n);
			break;
		case EXPR_MEMB:
		case EXPR_PTR_MEMB:
		case EXPR_INC_POST:
		case EXPR_DEC_POST:
		case EXPR_IDENT:
			//gen_expr_var
			break;
		case EXPR_CONST:
		case EXPR_STR:
			gen_expr_immd(f, n);
			break;
	}

	return;
}

void gen_stmt_cmpd(asm_f *f, ast_n *n) {
	ast_n *n_cur = n->next;
	while (n_cur != NULL) {
		switch (n_cur->kind) {
			case STMT: gen_stmt(f, n_cur); break;
			case DECL: gen_decl(f, n_cur); break;
		}
		n_cur = n_cur->next;
	}
	return;
}

void gen_stmt_if(asm_f *f, ast_n *n) {
	//Jump to end if not true
	char *lbl_end = gen_numlabel(f);
	gen_expr(f, n->dat.stmt.expr);
	asmf_add_inst(f, mk_inst(INST_TEST, 2,
		mk_oprd(OPRD_REG, RAX),
		mk_oprd(OPRD_REG, RAX)
	));
	asmf_add_inst(f, mk_inst(INST_JZ, 1, mk_oprd_label(lbl_end)));

	//Generate if body
	if (n->dat.stmt.body != NULL)
		gen_stmt(f, n->dat.stmt.body);

	//Generate else body
	if (n->dat.stmt.else_body != NULL) {
		//The main if will jump to here if the expression was false
		//Otherwise, jump past the else block
		char *lbl_else_end = gen_numlabel(f);
		asmf_add_inst(f, mk_inst(INST_JMP, 1,
			mk_oprd_label(lbl_else_end)
		));
		asmf_add_inst(f, mk_label(lbl_end));
		lbl_end = lbl_else_end;

		gen_stmt(f, n->dat.stmt.else_body);
	}
	asmf_add_inst(f, mk_label(lbl_end));
	
	return;
}

void gen_stmt_switch(asm_f *f, ast_n *n) {

}

void gen_stmt_loop(asm_f *f, ast_n *n) {
	char *lbl_beg = gen_numlabel(f);
	char *lbl_end = NULL;
	asmf_add_inst(f, mk_label(lbl_beg));

	//Condition for while and for loops
	if (n->dat.stmt.kind == STMT_WHILE) {
		lbl_end = gen_numlabel(f);
		
		//Does not apply for converted for loops with no condition
		if (n->dat.stmt.expr != NULL) {
			gen_expr(f, n->dat.stmt.expr);
			asmf_add_inst(f, mk_inst(INST_TEST, 2,
				mk_oprd(OPRD_REG, RAX),
				mk_oprd(OPRD_REG, RAX)
			));
			asmf_add_inst(f, mk_inst(INST_JZ, 1,
				mk_oprd_label(lbl_end)
			));
		}
	}
	
	//Generate loop body by hand to account for break/continue
	ast_n *n_cur = n->dat.stmt.body;
	if (n_cur->dat.stmt.kind == STMT_CMPD)
		n_cur = n_cur->next;
	while (n_cur != NULL) {
		f->cont_tgt = lbl_beg;
		f->break_tgt = lbl_end;
		switch (n_cur->kind) {
			case DECL: gen_decl(f, n_cur); break;
			case STMT: gen_stmt(f, n_cur); break;
		}
		n_cur = n_cur->next;
	}
	f->cont_tgt = NULL;
	f->break_tgt = NULL;

	//Condition for do-while loop
	if (n->dat.stmt.kind == STMT_DO_WHILE) {
		gen_expr(f, n->dat.stmt.expr);
		asmf_add_inst(f, mk_inst(INST_TEST, 2,
			mk_oprd(OPRD_REG, RAX),
			mk_oprd(OPRD_REG, RAX)
		));
		asmf_add_inst(f, mk_inst(INST_JNZ, 1,
			mk_oprd_label(lbl_beg)
		));
	} else {
		//Jump to beginning for while/for loops
		asmf_add_inst(f, mk_inst(INST_JMP, 1,
			mk_oprd_label(lbl_beg)
		));
	}

	//End label
	if (lbl_end != NULL)
		asmf_add_inst(f, mk_label(lbl_end));
	return;
}

void gen_stmt_label(asm_f *f, ast_n *n) {
	//Copy the symbol name to a new string
	char *lsrc = n->dat.stmt.lbl; //here

	//If the label isn't already in the map, store it
	char *lbl = map_get(&f->labels, lsrc);
	if (lbl == NULL) {
		int len = strlen(lsrc);
		char *str = malloc(len+1);
		strncpy(str, lsrc, len);
		str[len] = '\0';

		//Store the copy
		map_insert(&f->labels, str, str);
		lbl = str;
	}

	asmf_add_inst(f, mk_label(lbl));
	return;
}

void gen_stmt_goto(asm_f *f, ast_n *n) {
	//Verify label exists in function scope
	symbol *fsym = f->fsym;
	char *lbl = NULL;
	if ((lbl = map_get(&fsym->labels, n->dat.stmt.lbl)) == NULL) {
		c_error(NULL, "Label '%s' used but not defined\n",
			n->dat.stmt.lbl);
		return;
	}

	asmf_add_inst(f, mk_inst(INST_JMP, 1,
		mk_oprd_label(lbl)
	));
	return;
}

void gen_stmt_return(asm_f *f, ast_n *n) {
	//TODO: check for matching return type
	if (n->dat.stmt.expr != NULL) 
		gen_expr(f, n->dat.stmt.expr);
	asmf_add_inst(f, mk_inst(INST_LEAVE, 0));
	asmf_add_inst(f, mk_inst(INST_RET, 0));
}

void gen_stmt(asm_f *f, ast_n *n) {
	switch (n->dat.stmt.kind) {
		case STMT_EXPR:   gen_expr(f, n);		break;
		case STMT_CMPD:   gen_stmt_cmpd(f, n); 		break;
		case STMT_IF: 	  gen_stmt_if(f, n);		break;
		case STMT_SWITCH: gen_stmt_switch(f, n);	break;
		case STMT_WHILE:
		case STMT_FOR:
		case STMT_DO_WHILE:
			gen_stmt_loop(f, n);
			break;
		case STMT_LABEL:
			gen_stmt_label(f, n);
			break;
		case STMT_CASE:
			c_error(NULL, "Case label not inside switch statement\n");
			break;
		case STMT_DEFAULT:
			c_error(NULL, "'default' label not inside switch statement\n");
			break;
		case STMT_GOTO: gen_stmt_goto(f, n);		break;
		case STMT_CONTINUE:
			if (f->cont_tgt == NULL) {
				c_error(NULL, "'continue' not inside loop\n");
				break;
			}
			asmf_add_inst(f, mk_inst(INST_JMP, 1,
				mk_oprd_label(f->cont_tgt)
			));
			break;
		case STMT_BREAK:
			if (f->break_tgt == NULL) {
				c_error(NULL, "'break' not inside loop or switch\n");
				break;
			}
			asmf_add_inst(f, mk_inst(INST_JMP, 1,
				mk_oprd_label(f->break_tgt)
			));
			break;
		case STMT_RETURN: 
			gen_stmt_return(f, n);
			break;
	}
	return;
}

void gen_fdef(asm_f *f, ast_n *n) {
	//Make label
	symbol *fsym = n->dat.decl.sym;
	char *fnc_name = fsym->name;
	asmf_add_inst(f, mk_label(fnc_name));
	f->fsym = fsym;

	//Function preamble
	asmf_add_inst(f, mk_inst(INST_PUSH, 1,
		mk_oprd(OPRD_REG, RBP)
	));
	asmf_add_inst(f, mk_inst(INST_MOV, 2,
		mk_oprd(OPRD_REG, RBP),	
		mk_oprd(OPRD_REG, RSP)
	));

	//Calculate local variable area size
	int vsize = 0;
	for (int i=0; i<fsym->lvars.len; i++) {
		vsize += 8; //temporary
	}

	if (vsize > 0) {
		asmf_add_inst(f, mk_inst(INST_SUB, 2,
			mk_oprd(OPRD_REG, RSP),
			mk_oprd(OPRD_IMM, vsize)
		));
	}

	//Function body
	gen_stmt_cmpd(f, n->dat.decl.block);

	//Function end
	//Only generate a return statemnt if one isn't present
	if (f->text_cur->op != INST_RET) {
		//Only 'main' has a defined return value of zero
		//if no return statement is present
		if (!strcmp(fsym->name, "main")) {
			asmf_add_inst(f, mk_inst(INST_MOV, 2,
				mk_oprd(OPRD_REG, RAX),
				mk_oprd(OPRD_IMM, 0)
			));
		}

		asmf_add_inst(f, mk_inst(INST_LEAVE, 0));
		asmf_add_inst(f, mk_inst(INST_RET, 0));
	}
	f->fsym = NULL;
	return;
}

/*
 * 
 */

void gen_data(asm_f *f, ast_n *n) {

}

void gen_text(asm_f *f, ast_n *n) {
	//Loop for each declaration and function definition
	ast_n *n_cur = n;
	while (n_cur != NULL) {
		if (n_cur->dat.decl.kind == DECL_STD) {
			gen_decl(f, n_cur);
		} else {
			gen_fdef(f, n_cur);
		}
		n_cur = n_cur->next;
	}
	return;
}
