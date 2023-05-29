#ifndef JITBOY_CORE_H
#define JITBOY_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "audio.h"
#include "gbz80.h"
#include "lcd.h"
#include "memory.h"

#define MAX_ROM_BANKS 256
#define MAX_RAM_BANKS 16

typedef struct {
    gb_state state;
    gb_memory memory;
    gb_block compiled_blocks[MAX_ROM_BANKS][0x4000];  // bank, start address
    gb_block highmem_blocks[0x80];
    gb_lcd lcd;
    gb_audio audio;
    bool draw_frame;
    unsigned next_frame_time;

    int frame_cnt;
    unsigned time_busy;
    unsigned last_time;

    int opt_level;
} gb_vm;

void free_block(gb_block *block);

bool init_vm(gb_vm *vm,
             const char *filename,
             int opt_level,
             int scale,
             bool init_render,
             bool init_sound);
bool run_vm(gb_vm *vm, bool turbo);
bool free_vm(gb_vm *vm);

#endif
