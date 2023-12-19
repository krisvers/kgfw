#include "kgfw_list.h"
#include <stdlib.h>

typedef struct {
	unsigned long long int size;
	unsigned long long int capacity;
	unsigned long long int type_size;
} list_t;

void * _kgfw_list_new(unsigned long long int type_size) {
	list_t * list = malloc(sizeof(list_t) + type_size);
	if (list == NULL) {
		return NULL;
	}

	list->size = 0;
	list->capacity = 1;
	list->type_size = type_size;
	
	return (void *) (((unsigned long long int) list) + sizeof(list_t));
}

void * _kgfw_list_reserve(void * list, unsigned long long int capacity) {
	if (list == NULL) {
		return NULL;
	}

	list_t * l = (void *) (((unsigned long long int) list) - sizeof(list_t));
	void * p = realloc(l, sizeof(list_t) + l->type_size * capacity);
	if (p == NULL) {
		return NULL;
	}
	l = p;
	l->capacity = capacity;

	return (void *) (((unsigned long long int) p) + sizeof(list_t));
}

void * _kgfw_list_expand_from(void * list, unsigned long long int index) {
	if (list == NULL) {
		return NULL;
	}

	list_t * l = (void *) (((unsigned long long int) list) - sizeof(list_t));
}

void _kgfw_list_destroy(void * list) {
	if (list != NULL) {
		free((void *) (((unsigned long long int) list) - sizeof(list_t)));
	}
}
