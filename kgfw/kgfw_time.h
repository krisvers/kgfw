#ifndef KRISVERS_KGFW_TIME_H
#define KRISVERS_KGFW_TIME_H

#include "kgfw_defines.h"

KGFW_PUBLIC void kgfw_time_scale(float scale);
KGFW_PUBLIC float kgfw_time_get_scale(void);
KGFW_PUBLIC float kgfw_time_get(void);
KGFW_PUBLIC void kgfw_time_update(void);
KGFW_PUBLIC float kgfw_time_delta(void);
KGFW_PUBLIC void kgfw_time_start(void);
KGFW_PUBLIC void kgfw_time_end(void);
KGFW_PUBLIC void kgfw_time_init(void);

#endif
