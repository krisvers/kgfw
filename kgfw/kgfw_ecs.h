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

const char * kgfw_entity_get_name(kgfw_entity_t * this);
const char * kgfw_component_get_name(kgfw_component_t * this);

#endif
