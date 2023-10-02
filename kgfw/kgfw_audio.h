#ifndef KRISVERS_KGFW_AUDIO_H
#define KRISVERS_KGFW_AUDIO_H

#include "kgfw_defines.h"

KGFW_PUBLIC int kgfw_audio_play_sound(char * name, float x, float y, float z, float gain, float pitch, unsigned char loop, unsigned char relative);
KGFW_PUBLIC void kgfw_audio_update(void);
KGFW_PUBLIC int kgfw_audio_init(void);
KGFW_PUBLIC void kgfw_audio_deinit(void);

#endif
