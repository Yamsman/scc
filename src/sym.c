#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sym.h"
#include "err.h"
#include "util/map.h"
#include "util/vector.h"

/*
 * Symtable functions
 */
void symtable_init(symtable *stb) {
	stb->s_global = malloc(sizeof(struct SCOPE));
	stb->s_cur = stb->s_global;
	map_init(&stb->s_global->table, MAP_DEFAULT);
	stb->s_global->prev = NULL;
	stb->func = NULL;
	return;
}

#define PREDEF_MACRO(STB, NAME, EXP)\
	symbol *s_##NAME = symtable_def(STB, #NAME, type_new(TYPE_MACRO), NULL);\
	s_##NAME->mac_exp = EXP;\
	s_##NAME->is_predef = 1;

void symtable_predef(symtable *stb) {
	//Get date and time strings
	static char sdate[16];
	static char stime[16];
	time_t c_time = time(NULL);
	struct tm *l_time = localtime(&c_time);
	strftime(sdate, 16, "\"%b %d %Y\"", l_time);
	strftime(stime, 16, "\"%H:%M:%S\"", l_time);

	//Static buffer for __FILE__ and __LINE__ (filled in lex_expand_macro)
	static char sfili[256];
	
	//Predefine macros
	PREDEF_MACRO(stb, __DATE__, sdate);
	PREDEF_MACRO(stb, __FILE__, sfili);
	PREDEF_MACRO(stb, __LINE__, sfili);
	PREDEF_MACRO(stb, __STDC__, "1");
	PREDEF_MACRO(stb, __STDC_HOSTED__, "0");
	PREDEF_MACRO(stb, __STDC_VERSION__, "199901L");
	PREDEF_MACRO(stb, __TIME__, stime);
	return;
}

void symtable_close(symtable *stb) {
	scope *sc = stb->s_cur;
	while (sc != NULL) {
		for (int i=0; i<sc->table.max; i++) {
			if (sc->table.table[i] != NULL)
				sym_del(sc->table.table[i]);
		}
		map_close(&sc->table);

		scope *prev = sc->prev;
		free(sc);
		sc = prev;
	}
	return;
}

void symtable_scope_enter(symtable *stb) {
	scope *sc = malloc(sizeof(struct SCOPE));
	sc->prev = stb->s_cur;
	map_init(&sc->table, MAP_DEFAULT);
	stb->s_cur = sc;
	return;
}

void symtable_scope_leave(symtable *stb) {
	scope *sc = stb->s_cur;
	map_close(&sc->table);
	stb->s_cur = sc->prev;
	free(sc);
	return;
}

symbol *symtable_def(symtable *stb, char *name, s_type *type, s_pos *loc) {
	//Check for conflicts in current scope
	symbol *dup = map_get(&stb->s_cur->table, name);
	if (dup != NULL) {
		if (!type_compare(dup->type, type)) {
			c_error(loc, "Conflicting types for '%s'\n", name);
			return NULL;
		}

		if (stb->s_cur == stb->s_global) {
			return dup;
		} else {
			c_error(loc, "Redeclaration of '%s'\n", name);
			return NULL;
		}
	}

	//Create a new symbol and add it to the table
	//Macros always go in global scope
	symbol *s = sym_new(name, type);
	if (type->kind == TYPE_MACRO) {
		map_insert(&stb->s_global->table, name, s);
	} else {
		map_insert(&stb->s_cur->table, name, s);
	}
	if (stb->func != NULL)
		vector_add(&stb->func->lvars, s);

	return s;
}

void symtable_def_label(symtable *stb, char *name, s_pos *loc) {
	if (stb->func == NULL) {
		c_error(loc, "Label '%s' declared outside of function\n", name);
		return;
	}
	
	//Check for duplicates
	symbol *dup = map_get(&stb->func->labels, name);
	if (dup != NULL) {
		c_error(loc, "Label '%s' is already declared\n", name);
		return;
	}

	map_insert(&stb->func->labels, name, name);
	return;
}


void symtable_undef(symtable *stb, char *name) {
	
}

symbol *symtable_search(symtable *stb, char *name) {
	scope *sc_ptr = stb->s_cur;
	while (sc_ptr != NULL) {
		symbol *s = map_get(&sc_ptr->table, name);
		if (s != NULL) return s;
		sc_ptr = sc_ptr->prev;
	}
	return NULL;
}

void symtable_print() {

	return;
}

symbol *sym_new(char *name, s_type *type) {
	symbol *s = calloc(1, sizeof(struct SYMBOL));
	s->name = name;
	s->type = type;
	if (type->kind == TYPE_FUNC) {
		vector_init(&s->lvars, VECTOR_EMPTY);
		map_init(&s->labels, MAP_DEFAULT);
	}
	s->mac_exp = NULL;
	s->is_predef = 0;

	//Get base type from chain
	s_type *bt_ptr = type;
	while (bt_ptr->ref != NULL) 
		bt_ptr = bt_ptr->ref;
	s->btype = bt_ptr;

	return s;
}

void sym_del(symbol *s) {
	if (s->name != NULL && !s->is_predef)
		free(s->name);
	if (s->type != NULL)
		type_del(s->type);

	/*
	 * s->fbody is part of the AST, so it will be freed elsewhere.
	 * s->labels also contains strings owned by the AST.
	 */
	for (int i=0; i<s->lvars.len; i++) {
		if (s->lvars.table[i] != NULL)
			sym_del(s->lvars.table[i]);
	}
	vector_close(&s->lvars);
	map_close(&s->labels);

	if (s->mac_exp != NULL && !s->is_predef)
		free(s->mac_exp);

	free(s);
	return;
}

s_type *type_new(int kind) {
	s_type *type = malloc(sizeof(struct TYPE));
	type->kind = kind;
	type->size = -1;
	type->s_class = CLASS_UNDEF;
	type->is_signed = 1;
	type->is_const = 0;
	type->is_volatile = 0;

	type->ref = NULL;
	type->ref_len = 0;

	vector_init(&type->param, VECTOR_EMPTY);
	type->ret = NULL;
	
	return type;
}

void type_del(s_type *t) {
	//Reference data
	if (t->ref != NULL)
		type_del(t->ref);

	//Function/struct data
	for (int i=0; i<t->param.len; i++) {
		if (t->kind == TYPE_FUNC) {
			param_del(t->param.table[i]);
		} else {
			memb_del(t->param.table[i]);
		}
	}
	vector_close(&t->param);
	if (t->ret != NULL)
		type_del(t->ret);

	free(t);
	return;
}

s_type *type_clone(s_type *from) {
	if (from == NULL) return NULL;
	s_type *type = malloc(sizeof(struct TYPE));
	*type = *from;
	type->ref = type_clone(from->ref);
	type->ret = type_clone(from->ret);

	//Copy parameters
	vector_init(&type->param, VECTOR_EMPTY);
	for (int i=0; i<from->param.len; i++) {
		s_param *p = from->param.table[i];
		char *pname = malloc(strlen(p->name)+1);
		strncpy(pname, p->name, strlen(p->name)+1);
		s_param *pclone = param_new(type_clone(p->type), pname);
		vector_add(&type->param, pclone);
	}
	return type;
}

int type_compare_noref(s_type *a, s_type *b) {
	if (a->kind != b->kind) return 0;
	if (a->s_class != b->s_class) return 0;

	//TODO: qualifiers
	if (a->ref_len != b->ref_len) return 0;

	//Compare parameters/members
	if (a->param.len != b->param.len) return 0;
	for (int i=0; i<a->param.len; i++) {
		s_param *ap = a->param.table[i];
		s_param *bp = b->param.table[i];
		if (!type_compare(ap->type, bp->type)) return 0;
	}
	if (!type_compare(a->ret, b->ret)) return 0;

	return 1;	
}

int type_compare(s_type *a, s_type *b) {
	if (a == NULL && b == NULL) return 1;
	if (a == NULL || b == NULL) return 0;
	if (!type_compare_noref(a, b)) return 0;

	//Compare reference types
	s_type *i = a->ref;
	s_type *j = b->ref;
	while (i != NULL) {
		if (j == NULL) return 0;
		if (!type_compare_noref(i, j)) return 0;
		i = i->ref;
		j = j->ref;
	}
	if (j != NULL) return 0;

	return 1;
}

int type_sizeof(s_type *t) {
	//Base types
	switch (t->kind) {
		case TYPE_UNDEF:
		case TYPE_MACRO:
		case TYPE_LABEL:
			return -1;
		case TYPE_PTR:	
		case TYPE_FUNC:	
			return 8;
		case TYPE_VOID:
			return -1;
		case TYPE_INT:
		case TYPE_FLOAT:
			return t->size;
	}

	//User-defined types
	int ssum = 0;
	int smax = 0;
	for (int i=0; i<t->param.len; i++) {
		int msize = type_sizeof(t->param.table[i]);
		ssum += msize;
		if (msize > smax) smax = msize;
	}
	if (t->kind == TYPE_UNION)
		return smax;
	return ssum;
}

s_param *param_new(s_type *type, char *name) {
	s_param *param = malloc(sizeof(struct PARAM));
	param->type = type;
	param->name = name;

	//Get base type
	//TODO: just replace s_param with symbol
	s_type *bt_ptr = type;
	while (bt_ptr->ref != NULL) 
		bt_ptr = bt_ptr->ref;
	param->btype = bt_ptr;

	return param;
}

void param_del(s_param *p) {
	//Symbols created when parsing a function definition take "ownership"
	//of the name and type pointers; hence, they are not freed here
	free(p);
	return;
}

void memb_del(s_param *m) {
	if (m->type != NULL)
		type_del(m->type);
	if (m->name != NULL)
		free(m->name);
	free(m);
}
