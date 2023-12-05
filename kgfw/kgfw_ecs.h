#ifndef KRISVERS_KGFW_ECS_H
#define KRISVERS_KGFW_ECS_H

#include "kgfw_defines.h"
#include "kgfw_uuid.h"
#include "kgfw_hash.h"
#include "kgfw_transform.h"

typedef struct kgfw_component {
	/* component-system type id, to be the same as the system which acts unto this component */
	kgfw_uuid_t type_id;
	void (*update)(struct kgfw_component * this);
	void (*start)(struct kgfw_component * this);
} kgfw_component_t;

typedef struct kgfw_system {
	/* component-system type id, to be the same as the component which this system acts unto */
	kgfw_uuid_t type_id;
	void (*update)(struct kgfw_system * this, unsigned long long int components_count, kgfw_component_t * components);
	void (*start)(struct kgfw_system * this, unsigned long long int components_count, kgfw_component_t * components);
} kgfw_system_t;

typedef unsigned long long int kgfw_component_handle_t;
typedef struct kgfw_component_collection {
	unsigned long long int components_count;
	kgfw_component_handle_t * components;
} kgfw_component_collection_t;

typedef struct kgfw_entity {
	kgfw_uuid_t id;
	/* heap-allocated c-string owned by ECS system */
	const char * name;
	kgfw_transform_t transform;
	kgfw_component_collection_t components;
} kgfw_entity_t;

KGFW_PUBLIC int kgfw_ecs_init(void);
KGFW_PUBLIC void kgfw_ecs_deinit(void);
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_new(const char * name);
KGFW_PUBLIC void kgfw_entity_destroy(kgfw_entity_t * entity);
KGFW_PUBLIC void kgfw_component_system_construct(const char * name, unsigned long long int component_size, void * component_data, unsigned long long int system_size, void * system_data, kgfw_uuid_t system_type_id);
/* default component type_id is 0 (only update and start function pointers) */
KGFW_PUBLIC kgfw_component_handle_t kgfw_component_new(kgfw_uuid_t type_id);
KGFW_PUBLIC const char * kgfw_entity_get_name(kgfw_uuid_t id);
KGFW_PUBLIC const char * kgfw_component_get_type_name(kgfw_uuid_t type_id);
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_get(kgfw_uuid_t id);
KGFW_PUBLIC kgfw_entity_t * kgfw_entity_get_via_name(const char * name);
KGFW_PUBLIC kgfw_component_handle_t kgfw_entity_get_component(kgfw_uuid_t type_id);

#endif