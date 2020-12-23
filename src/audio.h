#ifndef JITBOY_AUDIO_H
#define JITBOY_AUDIO_H

#include <SDL.h>
#include <stdlib.h>

#include "memory.h"

#define AUDIO_SAMPLE_RATE 48000.0

#define DMG_CLOCK_FREQ 4194304.0
#define SCREEN_REFRESH_CYCLES 70224.0
#define VERTICAL_SYNC (DMG_CLOCK_FREQ / SCREEN_REFRESH_CYCLES)

#define AUDIO_SAMPLES ((unsigned) (AUDIO_SAMPLE_RATE / VERTICAL_SYNC))

typedef struct {
    SDL_AudioDeviceID dev;
} gb_audio;

void audio_init(gb_audio *audio, gb_memory *mem);
void channel_update(const uint16_t addr, const uint8_t val);
void lock_audio_dev();
void unlock_audio_dev();

#endif
