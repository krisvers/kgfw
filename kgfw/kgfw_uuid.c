#include "kgfw_uuid.h"
#include <stdlib.h>

kgfw_uuid_t kgfw_uuid_gen(void) {
	kgfw_uuid_t uuid = rand() | (((kgfw_uuid_t) rand()) << 32);

	return uuid;
}