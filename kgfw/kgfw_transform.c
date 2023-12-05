#include "kgfw_transform.h"
#include <string.h>

void kgfw_transform_identity(kgfw_transform_t * transform) {
	memset(transform, 0, sizeof(kgfw_transform_t));
	transform->scale[0] = 1.0f;
	transform->scale[1] = 1.0f;
	transform->scale[2] = 1.0f;
}
