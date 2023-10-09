#ifndef KRISVERS_KGFW_ECS_H
#define KRISVERS_KGFW_ECS_H

#include "kgfw_defines.h"

#define KGFW_DEFAULT_COMPONENT_STATE_MEMBERS unsigned long long int size;

#define KGFW_DEFAULT_COMPONENT_MEMBERS unsigned long long int size;	\
	int (*init)(void * self, void * state);							\
	int (*update)(void * self, void * state);

typedef struct kgfw_ecs_component {
	KGFW_DEFAULT_COMPONENT_MEMBERS
} kgfw_ecs_component_t;

typedef struct kgfw_ecs_component_state {
	KGFW_DEFAULT_COMPONENT_STATE_MEMBERS
} kgfw_ecs_component_state_t;

/* returns ID of successfully registered component and returns zero on failure */
KGFW_PUBLIC unsigned long long int kgfw_ecs_register(kgfw_ecs_component_t * component, const char * name);
KGFW_PUBLIC unsigned long long int kgfw_ecs_fetch_id(const char * name);
KGFW_PUBLIC void * kgfw_ecs_get(unsigned long long int id);
KGFW_PUBLIC void kgfw_ecs_cleanup(void);
KGFW_PUBLIC void kgfw_ecs_update(void);

#endif
