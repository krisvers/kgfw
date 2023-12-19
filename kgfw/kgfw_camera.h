#ifndef KRISVERS_KGFW_CAMERA_H
#define KRISVERS_KGFW_CAMERA_H

#include "kgfw_defines.h"
#include <linmath.h>

typedef struct kgfw_camera {
	vec3 pos;
	vec3 rot;
	vec2 scale;

	float fov;
	float nplane;
	float fplane;
	float ratio;
	unsigned char ortho;
	unsigned char tp;
	vec3 focus;
} kgfw_camera_t;

void kgfw_camera_perspective(kgfw_camera_t * camera, mat4x4 outm);
void kgfw_camera_view(kgfw_camera_t * camera, mat4x4 outm);

#endif
