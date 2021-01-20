#ifndef INPUTSTATE_H
#define INPUTSTATE_H

#include "common.h"

/* Describes state of operands, which can be used during assembly. */
struct op_state {
    int reg8, reg8_2, reg16, cond, bit;
    u16 imm;
};

int next_state(struct test_inst *inst,
               struct op_state *op_state,
               struct state *state);

#endif
