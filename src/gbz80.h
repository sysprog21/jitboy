#ifndef JITBOY_GBZ80_H
#define JITBOY_GBZ80_H

#include "emit.h"

/* compiles block starting at start_address to gb_block */
bool compile(gb_block *block,
             gb_memory *mem,
             uint16_t start_address,
             int opt_level);

bool optimize_block(GList **instructions, int opt_level);

#endif
