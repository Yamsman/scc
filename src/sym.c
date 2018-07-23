#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"

void symtable_init(symtable *stb) {
	stb->s_global = malloc(sizeof(struct SCOPE));
	stb->s_cur = stb->s_global;
	map_init(&stb->s_global->table, 16);
	return;
}

//TODO: CLEAN UP PROPERLY
void symtable_close(symtable *stb) {
	scope *sc = stb->s_cur;
	while (sc != NULL) {
		scope *prev = sc->prev;
		free(sc);
		sc = prev;
	}
	return;
}

void symtable_scope_enter(symtable *stb) {
	scope *sc = malloc(sizeof(struct SCOPE));
	sc->prev = stb->s_cur;
	map_init(&sc->table, 16);
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

symbol *symtable_add(symtable *stb, char *name, s_type *type) {
	//Check for conflicts in current scope
	symbol *dup = map_get(&stb->s_cur->table, name);
	if (dup != NULL) {
		if (!type_compare(dup->type, type)) {
			printf("ERROR: Conflicting types for '%s'\n", name);
			return NULL;
		}

		if (stb->s_cur == stb->s_global) {
			return dup;
		} else {
			printf("ERROR: Redeclaration of '%s'\n", name);
			return NULL;
		}
	}

	//Create a new symbol
	symbol *s = malloc(sizeof(struct SYMBOL));
	s->name = name;
	s->type = type;
	if (type->kind == TYPE_FUNC) {
		s->fbody = NULL;
		map_init(&s->lvars, 16);
	}

	//Get base type from chain
	s_type *bt_ptr = type;
	while (bt_ptr->ref != NULL) 
		bt_ptr = bt_ptr->ref;
	s->btype = bt_ptr;

	//Add symbol to table
	map_insert(&stb->s_cur->table, name, s);
	if (stb->func != NULL)
		map_insert(&stb->func->lvars, name, s);

	return s;
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

int type_compare_noref(s_type *a, s_type *b) {
	if (a->kind != b->kind) return 0;
	if (a->s_class != b->s_class) return 0;

	//TODO: qualifiers
	if (a->ref_len != b->ref_len) return 0;

	if (a->kind == TYPE_FUNC) {
		s_param *i = a->param;
		s_param *j = b->param;
		while (i != NULL) {
			if (j == NULL) return 0;
			if (!type_compare(i->type, j->type)) return 0;
			i = i->next;
			j = j->next;
		}
		if (j != NULL) return 0;
		if (!type_compare(a->ret, b->ret)) return 0;
	} else if (a->memb != NULL) {
		s_param *i = a->memb;
		s_param *j = b->memb;
		while (i != NULL) {
			if (j == NULL) return 0;
			if (!type_compare(i->type, j->type)) return 0;
			i = i->next;
			j = j->next;
		}
		if (j != NULL) return 0;
	}

	return 1;	
}

int type_compare(s_type *a, s_type *b) {
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
