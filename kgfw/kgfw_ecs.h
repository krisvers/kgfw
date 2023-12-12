#ifndef KRISVERS_KGFW_ECS_H
#define KRISVERS_KGFW_ECS_H

#include "kgfw_defines.h"
#include "kgfw_uuid.h"
#include "kgfw_hash.h"
#include "kgfw_transform.h"

typedef struct kgfw_component {
	void (*update)(struct kgfw_component * self);
	void (*start)(struct kgfw_component * self);
	void (*destroy)(struct kgfw_component * self);
	/* identifier for component instance */
	kgfw_uuid_t instance_id;
	/* id for the component type */
	kgfw_uuid_t type_id;
	struct kgfw_entity * entity;
} kgfw_component_t;

typedef struct kgfw_component_node {
	kgfw_component_t * component;
	struct kgfw_component_node * next;
} kgfw_component_node_t;

typedef struct kgfw_component_collection {
	unsigned long long int count;
	kgfw_component_node_t * handles;
} kgfw_component_collection_t;

typedef struct kgfw_entity {
	/* entity id */
	kgfw_uuid_t id;
	/* heap-allocated c-string owned by ECS system */
	const char * name;
	kgfw_transform_t transform;
	kgfw_component_collection_t components;
} kgfw_entity_t;

/* implementation of an ECS system */
typedef struct kgfw_system {
	void (*update)(struct kgfw_system * self, kgfw_component_node_t * components);
	void (*start)(struct kgfw_system * self, kgfw_component_node_t * components);
	void (*destroy)(struct kgfw_system * self);
} kgfw_system_t;

typedef void (*kgfw_system_update_f)(struct kgfw_system * self, kgfw_component_node_t * components);
typedef void (*kgfw_system_start_f)(struct kgfw_system * self, kgfw_component_node_t * components);
typedef void (*kgfw_component_start_f)(struct kgfw_component * self);
typedef void (*kgfw_component_update_f)(struct kgfw_component * self);

#define KGFW_ECS_INVALID_ID 0

KGFW_PUBLIC int kgfw_ecs_init(void);
KGFW_PUBLIC void kgfw_ecs_deinit(void);
KGFW_PUBLIC void kgfw_ecs_update(void);
/* if name == NULL, the name of the entity will be "Entity [entity.id]" */
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_new(const char * name);
/* if name == NULL, the name of the entity will be "Entity [entity.id]" */
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_copy(const char * name, kgfw_entity_t * source);
KGFW_PUBLIC void kgfw_entity_destroy(kgfw_entity_t * entity);
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_get(kgfw_uuid_t id);
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_get_via_name(const char * name);
KGFW_PUBLIC kgfw_component_t * kgfw_entity_get_component(kgfw_uuid_t type_id);
/*
	default component system_id is 0 (only update, start, and destroy function pointers)
	returns KGFW_ECS_INVALID_ID on error
 */
KGFW_PUBLIC kgfw_uuid_t kgfw_component_construct(const char * name, unsigned long long int component_size, void * component_data, kgfw_uuid_t system_id);
/* returns KGFW_ECS_INVALID_ID on error */
KGFW_PUBLIC kgfw_uuid_t kgfw_system_construct(const char * name, unsigned long long int system_size, void * system_data);
KGFW_PUBLIC kgfw_component_t * kgfw_entity_attach_component(kgfw_entity_t * entity, kgfw_uuid_t type_id);
KGFW_PUBLIC void kgfw_component_destroy(kgfw_component_t * component);
KGFW_PUBLIC const char * kgfw_component_type_get_name(kgfw_uuid_t type_id);
KGFW_PUBLIC kgfw_uuid_t kgfw_component_type_get_id(const char * type_name);

#endif
