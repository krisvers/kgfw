#include "kgfw_hash.h"

kgfw_hash_t kgfw_hash(const char * string) {
	kgfw_hash_t hash = 5381;

	for (unsigned long long int i = 0; string[i] != '\0'; ++i) {
		hash = (hash << 5) + hash + string[i];
	}

	return hash;
}

kgfw_hash_t kgfw_hash_length(const char * string, unsigned long long int length) {
	kgfw_hash_t hash = 5381;

	for (unsigned long long int i = 0; i < length; ++i) {
		hash = (hash << 5) + hash + string[i];
	}

	return hash;
}
