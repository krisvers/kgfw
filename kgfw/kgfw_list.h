#ifndef KRISVERS_KGFW_LIST_H
#define KRISVERS_KGFW_LIST_H

#include "kgfw_defines.h"

#define kgfw_list_new(T) _kgfw_list_new(sizeof(T))
#define kgfw_list_reserve(list, capacity) _kgfw_list_reserve(list, capacity)
#define kgfw_list_destroy(list) _kgfw_list_destroy(list)

KGFW_PUBLIC void * _kgfw_list_new(unsigned long long int type_size);
KGFW_PUBLIC void * _kgfw_list_reserve(void * list, unsigned long long int capacity);
KGFW_PUBLIC void _kgfw_list_destroy(void * list);

#endif
