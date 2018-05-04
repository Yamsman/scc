#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"

//TODO: sym.scope_id -> sym.scope_count, *sym.scope_cur
//TODO: properly free types/type chains when removing symbols
struct SYMBOL_TABLE sym;

void symtable_init() {
	sym.table = malloc(sizeof(struct SYMBOL*)*100);
	sym.count = 0;
	sym.max = 100;

	sym.scope = malloc(sizeof(int)*10);
	sym.scope_index = 0;
	sym.scope_max = 10;

	sym.scope_nextid = 1;
	*sym.scope = 0;
	return;
}

void symtable_close() {
	if (sym.table != NULL)
		free(sym.table);
	if (sym.scope != NULL)
		free(sym.scope);
	return;
}

void symtable_scope_enter() {
	sym.scope_index++;
	if (sym.scope_index == sym.scope_max) {
		sym.scope_max *= 2;
		sym.scope = realloc(sym.scope, sizeof(int)*sym.scope_max);
	}

	sym.scope[sym.scope_index] = sym.scope_nextid;

	sym.scope_nextid++;
	return;
}

void symtable_scope_leave() {
	sym.scope_index--;
	if (sym.scope_index < 0) {
		puts("ERROR: Scope stack underflow");
		sym.scope_index = 0;
	}

	return;
}

//TODO: types
symbol *symtable_add(char *name) {
	//Search current scope for conflicts
	if (symtable_search(name, SCOPE_CURRENT)) {
		printf("ERROR: Redefinition of variable '%s'\n", name);
		return NULL;
	}

	//Create and add a new symbol
	symbol *s = malloc(sizeof(struct SYMBOL));
	s->scope_id = sym.scope[sym.scope_index];
	s->name = name;
	s->type = NULL;
	s->btype = NULL;

	sym.table[sym.count++] = s;
	if (sym.count >= sym.max) {
		sym.max *= 2;
		sym.table = realloc(sym.table, sym.max);
	}

	return s;
}

//TODO: rewrite
symbol *symtable_search(char *name, int s_type) {
	int range = -1;
	switch (s_type) {
		case SCOPE_CURRENT: range = 1; break;
		case SCOPE_VISIBLE: range = sym.scope_index+1; break;
	}

	for (int i=0; i<sym.count; i++) {
		int *scope_id = &sym.scope[sym.scope_index];
		int scope_match = 0;
		for (int j=range; j>0; j--) {
			if (*scope_id == sym.table[i]->scope_id) {
				scope_match = 1;
				break;
			}
			scope_id--;	
		}

		if ((scope_match || s_type == SCOPE_ALL) && !strcmp(name, sym.table[i]->name))
			return sym.table[i];
	}

	return NULL;
}

void symtable_print() {
	for (int i=0; i<sym.count; i++) {
		symbol *s = sym.table[i];
		printf("%i: name: \"%s\", scope_id: %i\n", i, s->name, s->scope_id);
	}
	return;
}

s_type *type_new(int kind) {
	s_type *type = malloc(sizeof(struct TYPE));
	type->kind = kind;
	type->s_class = CLASS_UNDEF;
	type->is_signed = 0;
	type->is_const = 0;
	type->is_volatile = 0;

	type->ref = NULL;
	type->ref_len = 0;

	type->param = NULL;
	type->ret = NULL;

	type->memb = NULL;
	
	return type;
}

s_type *type_clone(s_type *from) {
	s_type *type = malloc(sizeof(struct TYPE));
	*type = *from;
	return type;
}

void type_del(s_type *t) {
	if (t->param != NULL)
		param_del(t->param);
	if (t->ref != NULL)
		type_del(t->ref);
	free(t);
	return;
}

s_param *param_new(s_type *type, char *name) {
	s_param *param = malloc(sizeof(struct PARAM));
	param->type = type;
	param->name = name;
	param->next = NULL;

	return param;
}

void param_del(s_param *p) {
	if (p->type != NULL)
		type_del(p->type);
	if (p->name != NULL)
		free(p->name);
	if (p->next != NULL)
		param_del(p->next);
	free(p);
}
