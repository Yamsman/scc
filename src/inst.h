#ifndef INST_H
#define INST_H

/* 
 * Registers and opcodes
 */

enum X86_64_IDS {
	#define reg(id, str) id,
	#define opc(id, str) id,
	#include "asm.inc"
	#undef opc
	#undef reg
};

extern const char *x86_64_str[];

/*
 * Operand/Instruction
 */

enum OPRD_TYPE {
	OPRD_REG,
	OPRD_IMMD
};

enum OPRD_DEREF {
	NODEREF,
	DEREF, //Dereference val
	DEREF_REG, //Dereference val+reg
	DEREF_IMMD, //Dereference val+immd
};

typedef struct OPRD {
	int type;
	int val;
	int deref;
	int deref_ofs;
} oprd;

typedef struct INST {
	int op;
	struct OPRD oprd[3];
	struct INST *jmp_to;
	char *lbl;

	struct INST *next;
} inst_n;

/*
 * Assembly file data
 */

typedef struct ASM {
	struct INST *text;
	struct INST *text_cur;
} asm_f;

oprd mk_oprd(int type, int val);
oprd mk_oprd_ex(int type, int val, int deref, int ofs);
inst_n *mk_inst(int op_id, int argc, ...);
inst_n *mk_label(char *str);
void inst_str(inst_n *in);

void asmf_init(asm_f *f);
void asmf_close(asm_f *f);
void asmf_add_inst(asm_f *f, inst_n *in);

#endif
