#include "kgfw_ecs.h"
#include "kgfw_log.h"
#include <string.h>
#include <stdlib.h>

static unsigned long long int length = 0;
static unsigned long long int size = 0;
static void * components = NULL;
static char ** names = NULL;

void kgfw_ecs_cleanup(void) {
	if (components != NULL) {
		free(components);
	}

	if (names != NULL) {
		for (unsigned long long int i = 0; i < length; ++i) {
			free(names[i]);
		}
		free(names);
	}

	length = 0;
	size = 0;
}

unsigned long long int kgfw_ecs_register(kgfw_ecs_component_t * component, const char * name) {
	if (component == NULL || name == NULL) {
		return 0;
	}

	++length;
	size += component->size;
	if (components == NULL) {
		components = malloc(size);
		if (components == NULL) {
			return 0;
		}
		names = malloc(sizeof(char *));
		if (names == NULL) {
			return 0;
		}
	} else {
		components = realloc(components, size);
		if (components == NULL) {
			return 0;
		}
		names = realloc(names, length * sizeof(char *));
		if (names == NULL) {
			return 0;
		}
	}

	memcpy((void *) ((unsigned long long int) components + size - component->size), component, component->size);
	names[length - 1] = malloc(strlen(name) + 1);
	if (names[length - 1] == NULL) {
		return 0;
	}
	memcpy(names[length - 1], name, strlen(name));

	return length;
}

unsigned long long int kgfw_ecs_fetch(const char * name) {
	for (unsigned long long int i = 0; i < length; ++i) {
		if (strcmp(name, names[i]) == 0) {
			return i + 1;
		}
	}

	return 0;
}

void * kgfw_ecs_get(unsigned long long int id) {
	if (id - 1 < length) {
		void * p = components;
		for (unsigned long long int i = 0; i < id - 1; ++i) {
			p = (void *) ((unsigned long long int) p + ((kgfw_ecs_component_t *) p)->size);
		}
		return p;
	}

	return NULL;
}


void kgfw_ecs_update(void) {
	
}
