#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "map.h"

map *map_new(int size) {
	map *m = malloc(sizeof(struct MAP));
	map_init(m, size);
	return m;
}

void map_del(map *m) {
	map_close(m);
	free(m);
	return;
}

void map_init(map *m, int size) {
	m->table = calloc(size, sizeof(void*));
	m->keys = calloc(size, sizeof(char*));
	m->count = 0;
	m->max = size;
	return;
}

void map_close(map *m) {
	free(m->table);
	free(m->keys);
}

unsigned int str_hash(const char *key) {
	//Jenkins one-at-a-time hash function
	unsigned int hash = 0;
	for (int i=0; i<strlen(key); i++) {
		hash += key[i++];
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;

	return hash;
}

void map_expand(map *m) {
	//Allocate space for new table
	int n_max = m->max * 2;
	void **n_table = calloc(n_max, sizeof(void*));
	const char **n_keys = calloc(n_max, sizeof(char*));

	//Relocate all keys into new table
	for (int i=0; i<m->max; i++) {
		if (m->table[i] != NULL) {
			int n_index = str_hash(m->keys[i]) % n_max;
			while (n_keys[n_index] != NULL)
				n_index = (n_index + 1) % n_max;
			n_table[n_index] = m->table[i];
			n_keys[n_index] = m->keys[i];
		}
	}

	//Free old tables and switch to new ones
	free(m->table);
	free(m->keys);
	m->table = n_table;
	m->keys = n_keys;
	m->max = n_max;

	return;
}

void *map_get(map *m, const char *key) {
	int index = str_hash(key) % m->max;
	while (m->keys[index] != NULL) {
		if (!strcmp(m->keys[index], key)) {
			return m->table[index];
		}
		index = (index + 1) % m->max;
	}

	return NULL;
}

void map_insert(map *m, const char *key, void *val) {
	//Expand the table capacity if load factor > 0.75
	if (m->count / (float)m->max > 0.75)
		map_expand(m);

	//Hash the key and perform linear probing if necessary
	int index = str_hash(key) % m->max;
	while (m->keys[index] != NULL) {
		if (m->table[index] != NULL)
			index = (index + 1) % m->max;
	}

	//Insert the value and key
	m->table[index] = val;
	m->keys[index] = key;
	m->count++;

	return;
}

void map_remove(map *m, const char *key) {
	int index = str_hash(key) % m->max;
	while (m->keys[index] != NULL) {
		/* The data pointer is set to NULL, but the key isn't.
		 * As a result, get operations will return NULL, and insertion
		 * does an additional check to see if the space has been
		 * previously removed. If not reused, these spaces disappear
		 * upon resizing.
		 */
		if (!strcmp(m->keys[index], key)) {
			m->table[index] = NULL;
			break;
		}
		index = (index + 1) % m->max;
	}

	return;
}
