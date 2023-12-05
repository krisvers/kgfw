#ifndef KRISVERS_KGFW_TRANSFORM_H
#define KRISVERS_KGFW_TRANSFORM_H

#include "kgfw_defines.h"

typedef struct kgfw_transform {
	float pos[3];
	float rot[3];
	float scale[3];
} kgfw_transform_t;

KGFW_PUBLIC void kgfw_transform_identity(kgfw_transform_t * transform);

#endif
