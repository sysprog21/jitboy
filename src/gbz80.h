#ifndef JITBOY_GBZ80_H
#define JITBOY_GBZ80_H

#include "emit.h"

/* compiles block starting at start_address to gb_block */
bool compile(gb_block *block,
             gb_memory *mem,
             uint16_t start_address,
             int opt_level);

bool optimize_block(GList **instructions, int opt_level);

#ifdef INSTRUCTION_TEST
struct instr_info {
    bool is_cb;
    uint8_t opcode;
    uint8_t bytes;
    int cycles;
};

struct instr_info compile_and_run(gb_block *block,
                                  gb_memory *mem,
                                  uint16_t pc,
                                  uint8_t *flag_args);

/* build_load_flag_block is used for generating JIT codes to load status flag.
 * Because the JIT codes for flag loading doesn't need any arguments, so we can
 * build it at the init stage and then cache it for reusing to avoid spending
 * time on avoidable codegen.
 */
void build_load_flag_block(gb_block *block);
#endif
#endif
