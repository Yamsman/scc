#ifndef MAP_H
#define MAP_H

typedef struct MAP {
	void **table;
	const char **keys;
	int count;
	int max;
} map;

struct MAP *map_new(int size);
void map_del(struct MAP *m);
void map_init(struct MAP *m, int size);
void map_close(struct MAP *m);

void *map_get(struct MAP *m, const char *key);
void map_insert(struct MAP *m, const char *key, void *val);
void map_remove(struct MAP *m, const char *key);
void map_expand(struct MAP *m);

#endif
