#include "kgfw_ecs.h"
#include "kgfw_hash.h"
#include "kgfw_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct entity_node {
	kgfw_entity_t entity;
	kgfw_hash_t hash;
	struct entity_node * next;
} entity_node_t;

typedef struct component_types {
	kgfw_uuid_t * type_ids;
	kgfw_uuid_t * system_ids;
	void ** datas;
	unsigned long long int * sizes;
	const char ** names;
	kgfw_hash_t * hashes;
	unsigned long long int count;
} component_types_t;

typedef struct component_node {
	kgfw_component_t * component;
	struct component_node * next;
	unsigned long long int type_index;
} component_node_t;

typedef struct systems {
	kgfw_uuid_t * ids;
	kgfw_system_t ** datas;
	unsigned long long int * sizes;
	const char ** names;
	kgfw_hash_t * hashes;
	unsigned long long int count;
} systems_t;

struct {
	entity_node_t * entities;
	/*
		array of linked-lists with [component_types.count] elements
		one element = one linked-list of instances of the same type of component
	*/
	component_node_t ** components;
	component_types_t component_types;
	systems_t systems;
} static state = {
	NULL,
	NULL,
	{
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		0
	},
	{
		NULL,
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
	for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "type %llu: %p", i, state.components[i]);
		while (state.components[i] != NULL) {
			kgfw_component_destroy(state.components[i]->component);
		}
	}

	while (state.entities != NULL) {
		kgfw_entity_destroy((kgfw_entity_t *) state.entities);
	}

	if (state.component_types.type_ids != NULL) {
		free(state.component_types.type_ids);
	}
	if (state.component_types.system_ids != NULL) {
		free(state.component_types.system_ids);
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

	for (unsigned long long int i = 0; i < state.systems.count; ++i) {
		state.systems.datas[i]->destroy(state.systems.datas[i]);
	}

	if (state.systems.ids != NULL) {
		free(state.systems.ids);
	}
	if (state.systems.datas != NULL) {
		for (unsigned long long int i = 0; i < state.systems.count; ++i) {
			free(state.systems.datas[i]);
		}
		free(state.systems.datas);
	}
	if (state.systems.sizes != NULL) {
		free(state.systems.sizes);
	}
	if (state.systems.names != NULL) {
		for (unsigned long long int i = 0; i < state.systems.count; ++i) {
			free((void *) state.systems.names[i]);
		}
		free(state.systems.names);
	}
	state.systems.count = 0;
}

void kgfw_ecs_update(void) {
	for (unsigned long long int i = 0; i < state.systems.count; ++i) {
		unsigned long long int comp_index = 0;
		for (unsigned long long int j = 0; j < state.component_types.count; ++j) {
			if (state.component_types.system_ids[j] == state.systems.ids[i]) {
				state.systems.datas[i]->update(state.systems.datas[i], (kgfw_component_node_t *) state.components[comp_index]);
				break;
			}
		}
	}
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
		if (state.entities == node) {
			state.entities = NULL;
		}
	} else {
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
	while (id == KGFW_ECS_INVALID_ID) {
		id = kgfw_uuid_gen();
	}

	void * data = malloc(component_size);
	if (data == NULL) {
		return 0;
	}
	memcpy(data, component_data, component_size);
	void ** datas = realloc(state.component_types.datas, sizeof(void *) * (state.component_types.count + 1));
	if (datas == NULL) {
		return 0;
	}
	state.component_types.datas = datas;
	state.component_types.datas[state.component_types.count] = data;

	kgfw_uuid_t * type_ids = realloc(state.component_types.type_ids, sizeof(kgfw_uuid_t) * (state.component_types.count + 1));
	if (type_ids == NULL) {
		return 0;
	}
	state.component_types.type_ids = type_ids;
	state.component_types.type_ids[state.component_types.count] = id;

	kgfw_uuid_t * system_ids = realloc(state.component_types.system_ids, sizeof(kgfw_uuid_t) * (state.component_types.count + 1));
	if (system_ids == NULL) {
		return 0;
	}
	state.component_types.system_ids = system_ids;
	state.component_types.system_ids[state.component_types.count] = system_id;

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

	kgfw_hash_t * hashes = realloc(state.component_types.hashes, sizeof(kgfw_hash_t) * (state.component_types.count + 1));
	if (hashes == NULL) {
		return 0;
	}
	state.component_types.hashes = hashes;
	state.component_types.hashes[state.component_types.count] = kgfw_hash(n);

	component_node_t ** nodes = realloc(state.components, sizeof(component_node_t *) * (state.component_types.count + 1));
	if (sizes == NULL) {
		return 0;
	}
	state.components = nodes;
	state.components[state.component_types.count] = NULL;

	++state.component_types.count;

	return id;
}

kgfw_component_t * kgfw_entity_attach_component(kgfw_entity_t * entity, kgfw_uuid_t type_id) {
	if (state.component_types.count == 0 || entity == NULL) {
		return NULL;
	}

	for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
		if (state.component_types.type_ids[i] == type_id) {
			component_node_t * node = malloc(sizeof(component_node_t));
			if (node == NULL) {
				return NULL;
			}

			if (state.components[i] == NULL) {
				state.components[i] = node;
			} else {
				node->next = state.components[i];
				state.components[i] = node;
			}

			node->component = malloc(state.component_types.sizes[i]);
			if (node->component == NULL) {
				free(node);
				return NULL;
			}
			memcpy(node->component, state.component_types.datas[i], state.component_types.sizes[i]);

			node->component->instance_id = kgfw_uuid_gen();
			node->component->entity = entity;
			node->type_index = i;

			kgfw_component_node_t * cnode = malloc(sizeof(kgfw_component_node_t));
			if (cnode == NULL) {
				return NULL;
			}
			cnode->next = entity->components.handles;
			entity->components.handles = cnode;
			++entity->components.count;

			node->component->start(node->component);
			return node->component;
		}
	}

	return NULL;
}

void kgfw_component_destroy(kgfw_component_t * component) {
	if (component == NULL) {
		return;
	}

	unsigned long long int type_index;
	for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
		if (state.component_types.type_ids[i] == component->type_id) {
			type_index = i;
			goto type_found;
		}
	}
	return;

type_found:;

	kgfw_component_node_t * prev = component->entity->components.handles;
	kgfw_component_node_t * cnode = component->entity->components.handles;
	while (cnode->component != component) {
		prev = cnode;
		cnode = cnode->next;

		if (cnode == NULL) {
			return;
		}
	}
	
	component->destroy(component);

	prev->next = cnode->next;
	free(cnode);
	//free(node);
}

const char * kgfw_component_type_get_name(kgfw_uuid_t type_id) {
	for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
		if (state.component_types.type_ids[i] == type_id) {
			return state.component_types.names[i];
		}
	}

	return NULL;
}

kgfw_uuid_t kgfw_component_type_get_id(const char * type_name) {
	kgfw_hash_t hash = kgfw_hash(type_name);
	for (unsigned long long int i = 0; i < state.component_types.count; ++i) {
		if (state.component_types.hashes[i] == hash) {
			return state.component_types.type_ids[i];
		}
	}

	return 0;
}

kgfw_uuid_t kgfw_system_construct(const char * name, unsigned long long int system_size, void * system_data) {
	if (system_size == 0 || system_data == NULL) {
		return 0;
	}

	kgfw_uuid_t id = 0;
	while (id == KGFW_ECS_INVALID_ID) {
		id = kgfw_uuid_gen();
	}

	kgfw_system_t * data = malloc(system_size);
	if (data == NULL) {
		return 0;
	}
	memcpy(data, system_data, system_size);
	kgfw_system_t ** datas = realloc(state.systems.datas, sizeof(kgfw_system_t *) * (state.systems.count + 1));
	if (datas == NULL) {
		return 0;
	}
	state.systems.datas = datas;
	state.systems.datas[state.systems.count] = data;

	kgfw_uuid_t * ids = realloc(state.systems.ids, sizeof(kgfw_uuid_t) * (state.systems.count + 1));
	if (ids == NULL) {
		return 0;
	}
	state.systems.ids = ids;
	state.systems.ids[state.systems.count] = id;

	unsigned long long int * sizes = realloc(state.systems.sizes, sizeof(unsigned long long int) * (state.systems.count + 1));
	if (sizes == NULL) {
		return 0;
	}
	state.systems.sizes = sizes;
	state.systems.sizes[state.systems.count] = system_size;

	const char ** names = realloc(state.systems.names, sizeof(const char *) * (state.systems.count + 1));
	if (names == NULL) {
		return 0;
	}
	state.systems.names = names;

	const char * n = NULL;
	if (name == NULL) {
		unsigned long long int len = snprintf(NULL, 0, "System 0x%llx", id);
		if (len < 0) {
			return 0;
		}

		n = malloc(sizeof(char) * (len + 1));
		if (n == NULL) {
			return 0;
		}
		sprintf((char *) n, "System 0x%llx", id);
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
	state.systems.names[state.systems.count] = n;

	kgfw_hash_t * hashes = realloc(state.systems.hashes, sizeof(kgfw_hash_t) * (state.systems.count + 1));
	if (hashes == NULL) {
		return 0;
	}
	state.systems.hashes = hashes;
	state.systems.hashes[state.systems.count] = kgfw_hash(n);

	unsigned long long int comp_index = 0;
	for (unsigned long long int j = 0; j < state.component_types.count; ++j) {
		if (state.component_types.system_ids[j] == state.systems.ids[state.systems.count]) {
			data->start(data, (kgfw_component_node_t *) state.components[comp_index]);
			break;
		}
	}

	++state.systems.count;

	return id;
}
