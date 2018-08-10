#ifndef VECTOR_H
#define VECTOR_H

typedef struct VECTOR {
	void **table;
	int len;
	int max;
} vector;

struct VECTOR *vector_new(int size);
void vector_del(struct VECTOR *v);
void vector_init(struct VECTOR *v, int size);
void vector_close(struct VECTOR *v);

void *vector_get(struct VECTOR *v, int index);
void vector_add(struct VECTOR *v, void *val);
void vector_remove(struct VECTOR *v, int index);
void vector_expand(struct VECTOR *v);

#endif

