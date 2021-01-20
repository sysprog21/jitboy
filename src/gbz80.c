#include <SDL.h>

#include "gbz80.h"
#include "gbz80_instr.h"

bool optimize_cc(GList *inst)
{
    for (; inst; inst = inst->next) {
        if (DATA(inst)->flags & INST_FLAG_AFFECTS_CC)
            DATA(inst)->flags |= INST_FLAG_SAVE_CC;

        if (DATA(inst)->flags & INST_FLAG_USES_CC)
            DATA(inst)->flags |= INST_FLAG_RESTORE_CC;
    }

    return true;
}

/* compiles block starting at start_address to gb_block */
bool compile(gb_block *block,
             gb_memory *mem,
             uint16_t start_address,
             int opt_level)
{
    LOG_DEBUG("compile new block @%#x\n", start_address);

    GList *instructions = NULL;

    uint16_t i = start_address;
    for (;;) {
        gbz80_inst *inst = g_new(gbz80_inst, 1);

        uint8_t opcode = mem->mem[i];
        if (opcode != 0xcb) {
            *inst = inst_table[opcode];
        } else {
            opcode = mem->mem[i + 1];
            *inst = cb_table[opcode];
        }

        inst->args = mem->mem + i;
        inst->address = i;
        i += inst->bytes;

        if (inst->opcode == ERROR) {
            LOG_ERROR("Invalid Opcode! (%#x)\n", opcode);
            return false;
        }
        LOG_DEBUG("inst: %i @%#x\n", inst->opcode, inst->address);

        instructions = g_list_prepend(instructions, inst);

        if (inst->flags & INST_FLAG_ENDS_BLOCK)
            break;
    }

    instructions = g_list_reverse(instructions);

    if (!optimize_block(&instructions, opt_level))
        return false;

    if (!optimize_cc(instructions))
        return false;

    bool result = emit(block, instructions);

    g_list_free_full(instructions, g_free);

    return result;
}
