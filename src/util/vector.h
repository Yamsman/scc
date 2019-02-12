#ifndef VECTOR_H
#define VECTOR_H

//Predefined sizes
#define VECTOR_DEFAULT 0x10
#define VECTOR_EMPTY 0x00

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

//"Stack" functions
void vector_push(struct VECTOR *v, void *val);
void *vector_pop(struct VECTOR *v);
void *vector_top(struct VECTOR *v);

#endif

