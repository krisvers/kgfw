#include "kgfw_ecs.h"
#include "kgfw_hash.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct entity_node {
	kgfw_entity_t entity;
	kgfw_hash_t hash;
	struct entity_node * next;
	struct entity_node * prev;
} entity_node_t;

typedef struct component_types {
	kgfw_uuid_t * ids;
	void ** datas;
	unsigned long long int * sizes;
	const char ** names;
	unsigned long long int count;
} component_types_t;

struct {
	entity_node_t * entities;
	component_types_t component_types;
} static state = {
	NULL,
	{
		NULL,
		NULL,
		NULL,
		NULL,
		0
	},
};

int kgfw_ecs_init(void) {
	return 0;
}

void kgfw_ecs_deinit(void) {
	while (state.entities != NULL) {
		kgfw_entity_destroy((kgfw_entity_t *) state.entities);
	}

	if (state.component_types.ids != NULL) {
		free(state.component_types.ids);
	}
	if (state.component_types.datas != NULL) {
		for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
			free(state.component_types.datas[i]);
		}
		free(state.component_types.datas);
	}
	if (state.component_types.sizes != NULL) {
		free(state.component_types.sizes);
	}
	if (state.component_types.names != NULL) {
		for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
			free((void *) state.component_types.names[i]);
		}
		free(state.component_types.names);
	}
	state.component_types.count = 0;
}

kgfw_entity_t * kgfw_entity_new(const char * name) {
	kgfw_entity_t * e = NULL;
	entity_node_t * node = malloc(sizeof(entity_node_t));
	if (node == NULL) {
		return NULL;
	}

	memset(node, 0, sizeof(entity_node_t));

	if (state.entities == NULL) {
		state.entities = node;
	} else {
		state.entities->prev = node;
		node->next = state.entities;
		state.entities = node;
	}

	e = &node->entity;
	if (e == NULL) {
		free(node);
		return NULL;
	}

	memset(e, 0, sizeof(kgfw_entity_t));

	e->id = kgfw_uuid_gen();

	if (name == NULL) {
		unsigned long long int len = snprintf(NULL, 0, "Entity 0x%llx", e->id);
		if (len < 0) {
			free(node);
			return NULL;
		}

		e->name = malloc(sizeof(char) * (len + 1));
		if (e->name == NULL) {
			free(node);
			return NULL;
		}
		sprintf((char *) e->name, "Entity 0x%llx", e->id);
		((char *) e->name)[len] = '\0';
	} else {
		unsigned long long int len = strlen(name);
		e->name = malloc(sizeof(char) * (len + 1));
		if (e->name == NULL) {
			free(node);
			return NULL;
		}
		strncpy((char *) e->name, name, len);
		((char *) e->name)[len] = '\0';
	}

	node->hash = kgfw_hash(e->name);

	kgfw_transform_identity(&e->transform);
	return e;
}

kgfw_entity_t * kgfw_entity_copy(const char * name, kgfw_entity_t * source) {
	kgfw_entity_t * e = kgfw_entity_new(name);
	if (e == NULL) {
		 return NULL;
	}

	memcpy(&e->transform, &source->transform, sizeof(kgfw_transform_t));

	e->components.count = source->components.count;
	if (e->components.count != 0) {
		e->components.handles = malloc(sizeof(kgfw_component_t *) * e->components.count);
		if (e->components.handles == NULL) {
			free((void *) e->name);
			free(e);
			return NULL;
		}

		memcpy(e->components.handles, source->components.handles, sizeof(kgfw_component_t *) * e->components.count);
	}
	return e;
}

void kgfw_entity_destroy(kgfw_entity_t * entity) {
	if (entity == NULL) {
		return;
	}

	entity_node_t * node = (entity_node_t *) entity;
	if (node->next == NULL) {
		if (node->prev != NULL) {
			node->prev->next = NULL;
		}
		if (state.entities == node) {
			state.entities = NULL;
		}
	} else {
		if (node->prev != NULL) {
			node->prev->next = node->next;
			node->next->prev = node->prev;
		}
		if (state.entities == node) {
			state.entities = node->next;
		}
	}
	if (node->entity.components.handles != NULL) {
		free(node->entity.components.handles);
	}
	free(node);
}

kgfw_entity_t * kgfw_entity_get(kgfw_uuid_t id) {
	if (state.entities == NULL) {
		return NULL;
	}

	for (entity_node_t * n = state.entities; n->next != NULL; n = n->next) {
		if (n->entity.id == id) {
			return &n->entity;
		}
	}

	return NULL;
}

kgfw_entity_t * kgfw_entity_get_via_name(const char * name) {
	if (state.entities == NULL) {
		return NULL;
	}

	kgfw_hash_t hash = kgfw_hash(name);
	for (entity_node_t * n = state.entities;; n = n->next) {
		if (n == NULL) {
			return NULL;
		}

		if (n->hash == hash) {
			return &n->entity;
		}
	}

	return NULL;
}

kgfw_uuid_t kgfw_component_construct(const char * name, unsigned long long int component_size, void * component_data, kgfw_uuid_t system_id) {
	if (component_size == 0 || component_data == NULL) {
		return 0;
	}

	kgfw_uuid_t id = 0;
	while (id == 0) {
		id = kgfw_uuid_gen();
	}

	void * data = malloc(component_size);
	if (data == NULL) {
		return 0;
	}
	void ** datas = realloc(state.component_types.datas, sizeof(void *) * (state.component_types.count + 1) * component_size);
	if (datas == NULL) {
		return 0;
	}
	state.component_types.datas = datas;
	state.component_types.datas[state.component_types.count] = data;

	kgfw_uuid_t * ids = realloc(state.component_types.ids, sizeof(kgfw_uuid_t) * (state.component_types.count + 1));
	if (ids == NULL) {
		return 0;
	}
	state.component_types.ids = ids;
	state.component_types.ids[state.component_types.count] = id;

	unsigned long long int * sizes = realloc(state.component_types.sizes, sizeof(unsigned long long int) * (state.component_types.count + 1));
	if (sizes == NULL) {
		return 0;
	}
	state.component_types.sizes = sizes;
	state.component_types.sizes[state.component_types.count] = component_size;

	const char ** names = realloc(state.component_types.names, sizeof(const char *) * (state.component_types.count + 1));
	if (names == NULL) {
		return 0;
	}
	state.component_types.names = names;

	const char * n = NULL;
	if (name == NULL) {
		unsigned long long int len = snprintf(NULL, 0, "Component 0x%llx", id);
		if (len < 0) {
			return 0;
		}

		n = malloc(sizeof(char) * (len + 1));
		if (n == NULL) {
			return 0;
		}
		sprintf((char *) n, "Component 0x%llx", id);
		((char *) n)[len] = '\0';
	}
	else {
		unsigned long long int len = strlen(name);
		n = malloc(sizeof(char) * (len + 1));
		if (n == NULL) {
			return 0;
		}
		strncpy((char *) n, name, len);
		((char *) n)[len] = '\0';
	}

	state.component_types.names[state.component_types.count] = n;
	state.component_types.count++;

	return id;
}

kgfw_component_t * kgfw_component_new(kgfw_uuid_t type_id) {
	if (state.component_types.count == 0) {
		return NULL;
	}

	for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
		if (state.component_types.ids[i] == type_id) {
			kgfw_component_t * c = malloc(sizeof(kgfw_component_t));
			if (c == NULL) {
				return NULL;
			}

			memcpy(c, state.component_types.datas[i], sizeof(kgfw_component_t));
			c->instance_id = kgfw_uuid_gen();
			return c;
		}
	}

	return NULL;
}