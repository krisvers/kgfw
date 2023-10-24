#include "kgfw_hash.h"

unsigned long long int kgfw_hash(char * string) {
	unsigned long long int hash = 5381;

	for (unsigned long long int i = 0; string[i] != '\0'; ++i) {
		hash = (hash << 5) + hash + string[i];
	}

	return hash;
}

unsigned long long int kgfw_hash_length(char * string, unsigned long long int length) {
	unsigned long long int hash = 5381;

	for (unsigned long long int i = 0; i < length; ++i) {
		hash = (hash << 5) + hash + string[i];
	}

	return hash;
}
