#ifndef KRISVERS_KGFW_H
#define KRISVERS_KGFW_H

#include "kgfw_audio.h"
#include "kgfw_camera.h"
#include "kgfw_commands.h"
#include "kgfw_console.h"
#include "kgfw_ecs.h"
#include "kgfw_graphics.h"
#include "kgfw_hash.h"
#include "kgfw_input.h"
#include "kgfw_log.h"
#include "kgfw_list.h"
#include "kgfw_time.h"
#include "kgfw_transform.h"
#include "kgfw_uuid.h"
#include "kgfw_window.h"

#include "kgfw_defines.h"

KGFW_PUBLIC int kgfw_init(void);
KGFW_PUBLIC void kgfw_deinit(void);
KGFW_PUBLIC int kgfw_update(void);

#endif
