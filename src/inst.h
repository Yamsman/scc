#ifndef INST_H
#define INST_H

#include "util/map.h"

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
	OPRD_IMM,
	OPRD_LBL
};

enum OPRD_DEREF {
	NODEREF,	//No dereferencing
	DEREF, 		//Dereference val
	DEREF_REG, 	//Dereference val+reg
	DEREF_IMM, 	//Dereference val+immd
	DEREF_LBL, 	//Dereference val+label
};

typedef struct OPRD {
	int type;
	union {
		int ival;
		char *sval;
	} val;
	int deref;
	union {
		int ival;
		char *sval;
	} deref_ofs;
} oprd;

typedef struct INST {
	int op;
	char *lbl;
	struct OPRD oprd[3];	
	struct INST *next;
} inst_n;

/*
 * Assembly file data
 */

//TODO: 'text' -> vector
typedef struct ASM {
	struct INST *text;	//Beginning of instruction list
	struct INST *text_cur;	//Last instruction in list

	char *cont_tgt;		//Target label for 'continue'
	char *break_tgt;	//Target label for 'break'
	struct SYMBOL *fsym;	//Symbol of current function, if applicable

	int lnum;		//Number used for next generated label
	struct MAP labels;	//Map for storing label strings
} asm_f;

struct OPRD mk_oprd(int type, int val);
struct OPRD mk_oprd_ex(int type, int val, int deref, int ofs);
struct OPRD mk_oprd_label(char *lbl);
struct INST *mk_inst(int op_id, int argc, ...);
struct INST *mk_label(char *str);
void inst_del(struct INST *in);
void inst_str(struct INST *in);

void asmf_init(struct ASM *f);
void asmf_close(struct ASM *f);
void asmf_add_inst(struct ASM *f, inst_n *in);

#endif
