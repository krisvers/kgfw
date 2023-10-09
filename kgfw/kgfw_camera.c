#include "kgfw_camera.h"
#include <linmath.h>

void kgfw_camera_perspective(kgfw_camera_t * camera, mat4x4 outm) {
	if (camera->ortho) {
		mat4x4_ortho(outm, (camera->ratio - 1) / 2.0f, (camera->ratio + 1) / 2.0f, -1, 1, camera->nplane, camera->fplane);
	} else {
	}
	
	mat4x4_perspective(outm, camera->fov * 3.141592f / 180.0f, camera->ratio, camera->nplane, camera->fplane);

	mat4x4_rotate_X(outm, outm, -camera->rot[0] * 3.141592f / 180.0f);
	mat4x4_rotate_Y(outm, outm, camera->rot[1] * 3.141592f / 180.0f);
	mat4x4_rotate_Z(outm, outm, -camera->rot[2] * 3.141592f / 180.0f);
}

void kgfw_camera_view(kgfw_camera_t * camera, mat4x4 outm) {
	mat4x4_translate(outm, -camera->pos[0], -camera->pos[1], camera->pos[2]);
}
