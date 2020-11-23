#ifndef JITBOY_INTERRUPT_H
#define JITBOY_INTERRUPT_H

#include <stdint.h>

#include "emit.h"

uint64_t next_update_time(gb_state *state);

void update_ioregs(gb_state *state);
uint16_t start_interrupt(gb_state *state);

#endif
