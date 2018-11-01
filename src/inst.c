#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "inst.h"
#include "util/map.h"

const char *x86_64_str[] = {
	#define reg(id, str) str,
	#define inst(id, str) str,
	#include "asm.inc"
	#undef inst
	#undef reg
};

/*
 * Operand functions
 */

oprd mk_oprd(int type, int val) {
	oprd o;
	o.type = type;
	o.val.ival = val;
	o.deref = NODEREF;
	return o;
}

oprd mk_oprd_ex(int type, int val, int deref, int ofs) {
	oprd o;
	o.type = type;
	o.val.ival = val;
	o.deref = deref;
	o.deref_ofs.ival = ofs;
	return o;
}

oprd mk_oprd_label(char *str) {
	oprd o = mk_oprd(OPRD_LBL, 0);
	o.val.sval = str;
	return o;
}

/*
 * Instruction functions
 */

//TODO: move oprd count (argc) to asm.inc
inst_n *mk_inst(int op_id, int argc, ...) {
	inst_n *in = malloc(sizeof(struct INST));
	in->op = op_id;
	in->lbl = NULL;
	in->next = NULL;

	//Initialize operands
	va_list args;
	va_start(args, argc);
	for (int i=0; i<3; i++) {
		//Initialize other operands as blank
		if (i >= argc) {
			in->oprd[i].type = -1;
			continue;
		}
		in->oprd[i] = va_arg(args, oprd);
	}
	va_end(args);

	return in;
}

inst_n *mk_label(char *str) {
	inst_n *in = mk_inst(-1, 0);
	in->lbl = str;
	return in;
}

void inst_del(inst_n *in) {
	//Labels are freed alongside the asm struct
	free(in);
	return;
}

void inst_str(inst_n *in) {
	//Print label
	if (in->lbl != NULL) {
		printf("%s:", in->lbl);
		if (in->next->lbl != NULL) printf("\n");
		return;
	}

	//Print operation
	printf("\t%s", x86_64_str[in->op]);

	//Print operands
	for (int i=0; i<3; i++) {
		oprd o = in->oprd[i];
		if (o.type < 0)
			break;

		if (i > 0) printf(", ");
		else printf("\t");
		if (o.deref != NODEREF)
			printf("[");
		if (o.type == OPRD_REG)
			printf("%s", x86_64_str[o.val.ival]);
		else if (o.type == OPRD_LBL)
			printf("%s", o.val.sval);
		else
			printf("%i", o.val.ival);

		//Dereferencing
		//TODO: DWORD PTR ...
		switch (o.deref) {
			case DEREF:	
				printf("]");
				break;
			case DEREF_REG:
				printf("+%s]", x86_64_str[o.deref_ofs.ival]);
				break;
			case DEREF_IMM: 
				printf("+%i]", o.deref_ofs.ival);
				break;
			case DEREF_LBL: 
				printf("+%s]", o.deref_ofs.sval);
				break;
		}
	}

	printf("\n");
	return;
}

/*
 * Assembly file functions
 */

void asmf_init(asm_f *f) {
	//f->data = NULL;
	//f->data_cur = NULL;
	f->text = NULL;
	f->text_cur = NULL;
	f->cont_tgt = NULL;
	f->break_tgt = NULL;
	f->lnum = 0;
	map_init(&f->labels, 10);
	return;
}

void asmf_close(asm_f *f) {
	struct INST *cur = f->text;
	while (cur != NULL) {
		struct INST *next = cur->next;
		inst_del(cur);
		cur = next;
	}

	//Free labels
	for (int i=0; i<f->labels.max; i++) {
		if (f->labels.table[i] != NULL)
			free(f->labels.table[i]);
	}
	map_close(&f->labels);
	return;
}

void asmf_add_inst(asm_f *f, inst_n *in) {
	if (f->text == NULL) {
		f->text = in;
		f->text_cur = in;
	} else {
		f->text_cur->next = in;
		f->text_cur = in;
	}

	return;
}
