#ifndef KRISVERS_KGFW_UUID_H
#define KRISVERS_KGFW_UUID_H

#include "kgfw_defines.h"

typedef unsigned long long int kgfw_uuid_t;

KGFW_PUBLIC kgfw_uuid_t kgfw_uuid_gen(void);

#endif