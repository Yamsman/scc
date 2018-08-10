#include <stdio.h>
#include <stdlib.h>
#include "sym.h"
#include "ast.h"
#include "inst.h"
#include "gen.h"

void gen_stmt(asm_f *f, ast_n *n);

void gen_decl_init(asm_f *f, ast_n *n) {

}

void gen_decl(asm_f *f, ast_n *n) {
	
}

void gen_expr(asm_f *f, ast_n *n) {

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

}

void gen_stmt_switch(asm_f *f, ast_n *n) {

}

void gen_stmt_loop(asm_f *f, ast_n *n) {

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
			printf("ERROR: 'continue' not inside loop\n");
			break;
		case STMT_BREAK:
			printf("ERROR: 'break' not inside loop or switch\n");
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
			mk_oprd(OPRD_IMMD, vsize)
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
