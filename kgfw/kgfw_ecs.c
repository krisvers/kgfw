#include "kgfw_ecs.h"

typedef struct {
	unsigned long long int size;
	kgfw_component_t * data;
} components_t;

typedef struct {
	unsigned long long int size;
	kgfw_system_t * data;
} systems_t;

struct {
	components_t * components;
	unsigned long long int components_count;
	components_t * systems;
	unsigned long long int systems_count;
} static state = {
	NULL,
	0,
	NULL,
	0,
};

int kgfw_ecs_init(void) {

}