#include <stdio.h>
#include <stdlib.h>
#include "sym.h"
#include "ast.h"
#include "inst.h"
#include "gen.h"

void gen_stmt(asm_f *f, ast_n *n);
void gen_expr(asm_f *f, ast_n *n);

char *gen_numlabel(asm_f *f) {
	int num = f->lnum++;
	char *buf = malloc(16);
	snprintf(buf, 16, ".L%i", num);
	return buf;
}

void gen_decl_init(asm_f *f, ast_n *n) {

}

void gen_decl(asm_f *f, ast_n *n) {
	
}

void gen_expr_logic_andor(asm_f *f, ast_n *n) {
	//Logical and exppass_valsions can short circuit if the LHS is 0,
	//and logical or exppass_valsions can if the LHS is 1
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
		case EXPR_DEREF:
		case EXPR_POS:
		case EXPR_NEG:
		case EXPR_INC_PRE:
		case EXPR_DEC_PRE:
		case EXPR_ARRAY:
		case EXPR_CALL:
		case EXPR_MEMB:
		case EXPR_PTR_MEMB:
		case EXPR_INC_POST:
		case EXPR_DEC_POST:
		case EXPR_IDENT:
		case EXPR_CONST:
		case EXPR_STR:
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
		gen_expr(f, n->dat.stmt.expr);
	} else if (n->dat.stmt.kind == STMT_FOR) {
		//...
	}
	if (n->dat.stmt.kind != STMT_DO_WHILE) {
		lbl_end = gen_numlabel(f);
		asmf_add_inst(f, mk_inst(INST_TEST, 2,
			mk_oprd(OPRD_REG, RAX),
			mk_oprd(OPRD_REG, RAX)
		));
		asmf_add_inst(f, mk_inst(INST_JZ, 1,
			mk_oprd_label(lbl_end)
		));
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
	char *lname = n->dat.expr.sym->name;
	asmf_add_inst(f, mk_label(lname));
	return;
}

void gen_stmt_goto(asm_f *f, ast_n *n) {

}

void gen_stmt_return(asm_f *f, ast_n *n) {

}

void gen_stmt(asm_f *f, ast_n *n) {
	switch (n->dat.stmt.kind) {
		case STMT_EXPR: gen_expr(f, n);			break;
		case STMT_CMPD: gen_stmt_cmpd(f, n); 		break;
		case STMT_IF: 	gen_stmt_if(f, n);		break;
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
			printf("ERROR: Case label not inside switch statement\n");
			break;
		case STMT_DEFAULT:
			printf("ERROR: 'default' label not inside switch statement\n");
			break;
		case STMT_GOTO: gen_stmt_goto(f, n);		break;
		case STMT_CONTINUE:
			if (f->cont_tgt == NULL) {
				printf("ERROR: 'continue' not inside loop\n");
				break;
			}
			asmf_add_inst(f, mk_inst(INST_JMP, 1,
				mk_oprd_label(f->cont_tgt)
			));
			break;
		case STMT_BREAK:
			if (f->break_tgt == NULL) {
				printf("ERROR: 'break' not inside loop or switch\n");
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

	//Function preamble
	asmf_add_inst(f, mk_inst(INST_PUSH, 1,
		mk_oprd(OPRD_REG, RBP)
	));
	asmf_add_inst(f, mk_inst(INST_MOV, 2,
		mk_oprd(OPRD_REG, RBP),	
		mk_oprd(OPRD_REG, RSP)
	));

	//Calculate local variable area size
	int vsize = 8;
	//...

	if (vsize > 0) {
		asmf_add_inst(f, mk_inst(INST_SUB, 2,
			mk_oprd(OPRD_REG, RSP),
			mk_oprd(OPRD_IMM, vsize)
		));
	}

	//Function body
	gen_stmt_cmpd(f, n->dat.decl.block);

	//Function end
	asmf_add_inst(f, mk_inst(INST_LEAVE, 0));
	asmf_add_inst(f, mk_inst(INST_RET, 0));
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
