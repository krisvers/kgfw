#ifndef KRISVERS_KGFW_HASH_H
#define KRISVERS_KGFW_HASH_H

#include "kgfw_defines.h"

typedef unsigned long long int kgfw_hash_t;

KGFW_PUBLIC kgfw_hash_t kgfw_hash(char * string);
KGFW_PUBLIC kgfw_hash_t kgfw_hash_length(char * string, unsigned long long int length);

#endif
