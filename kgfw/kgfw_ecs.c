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

typedef struct component_node {
	kgfw_component_t entity;
	kgfw_hash_t hash;
	struct component_node * next;
	struct component_node * prev;
} component_node_t;

struct {
	entity_node_t * entities;

} static state = {
	NULL,
};

int kgfw_ecs_init(void) {
	return 0;
}

void kgfw_ecs_deinit(void) {
	while (state.entities != NULL) {
		kgfw_entity_destroy((kgfw_entity_t *) state.entities);
	}
	return;
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
		int len = snprintf(NULL, 0, "Entity 0x%llx", e->id);
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
		e->components.handles = malloc(sizeof(kgfw_component_handle_t) * e->components.count);
		if (e->components.handles == NULL) {
			free((void *) e->name);
			free(e);
			return NULL;
		}

		memcpy(e->components.handles, source->components.handles, sizeof(kgfw_component_handle_t) * e->components.count);
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
