#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vector.h"

vector *vector_new(int size) {
	vector *v = malloc(sizeof(struct VECTOR));
	vector_init(v, size);
	return v;
}

void vector_del(vector *v) {
	vector_close(v);
	free(v);
	return;
}

void vector_init(vector *v, int size) {
	v->table = malloc(sizeof(void*)*size);
	v->len = 0;
	v->max = size;
	return;
}

void vector_close(vector *v) {
	free(v->table);
	return;
}

void vector_expand(vector *v) {
	int n_max = v->max * 2;
	v->table = realloc(v->table, sizeof(void*)*n_max);
	v->max = n_max;
	return;
}

void *vector_get(vector *v, int index) {
	//Check bounds
	if (index < 0 || index >= v->len)
		return NULL;

	return v->table[index];
}

void vector_add(vector *v, void *val) {
	//Expand the table if there is no room left
	if (v->len >= v->max)
		vector_expand(v);

	//Add value to end of table
	int index = v->len-1;
	v->table[index] = val;
	v->len++;

	return;
}

void vector_remove(vector *v, int index) {
	//Shift all entries if neccesary
	for (int i=index; i<v->len-1; i++)
		v->table[i] = v->table[i+1];
	v->table[v->len-1] = NULL;

	return;
}
