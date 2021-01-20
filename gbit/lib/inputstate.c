#include <assert.h>
#include <stdlib.h>

#include "inputstate.h"

static size_t array_u8_find(u8 elem, u8 *arr, size_t size)
{
    for (size_t i = 0; i < size; i++)
        if (arr[i] == elem)
            return i;
    assert(0 && "Element not found");
    return -1;
}

static size_t array_u16_find(u16 elem, u16 *arr, size_t size)
{
    for (size_t i = 0; i < size; i++)
        if (arr[i] == elem)
            return i;
    assert(0 && "Element not found");
    return -1;
}

static int next_flags(bool test_individually, u8 *val)
{
    u8 vals_full[] = {0x00, 0x10, 0x20, 0x40, 0x80};
    u8 vals_simple[] = {0x00, 0xf0};
    u8 *vals;
    size_t len;
    size_t idx;

    if (test_individually) {
        vals = vals_full;
        len = sizeof(vals_full);
    } else {
        vals = vals_simple;
        len = sizeof(vals_simple);
    }

    idx = array_u8_find(*val, vals, len);

    if (idx == len - 1)
        *val = vals[0];
    else
        *val = vals[idx + 1];

    return idx != len - 1;
}

static int next_val8(u8 *val)
{
    u8 vals[] = {0x00, 0x01, 0x0f, 0x10, 0xef, 0xf0, 0xff};
    size_t len = sizeof(vals);
    size_t idx;

    idx = array_u8_find(*val, vals, len);

    if (idx == len - 1)
        *val = vals[0];
    else
        *val = vals[idx + 1];

    return idx != len - 1;
}

static int next_val16(bool enabled, u16 *val)
{
    u16 vals[] = {0x0000, 0x0001, 0x000f, 0x0010, 0x00ff,
                  0x0100, 0x0fff, 0x1000, 0xffff};
    size_t len = sizeof(vals) / sizeof(vals[0]);
    size_t idx;

    if (!enabled)
        return 0;

    idx = array_u16_find(*val, vals, len);

    if (idx == len - 1)
        *val = vals[0];
    else
        *val = vals[idx + 1];

    return idx != len - 1;
}

static int next_bool(bool enabled, bool *val)
{
    if (!enabled)
        return 0;

    *val = !*val;

    return *val == 0;
}

static int next_oper(bool enabled, int lim, int *val)
{
    if (!enabled)
        return 0;

    if (++(*val) == lim) {
        *val = 0;
        return 0;
    }
    return 1;
}

static int next_imm(int imm_size, u16 *val)
{
    if (imm_size == 0)
        return 0;
    else if (imm_size == 1)
        return next_val8((u8 *) val);
    else
        return next_val16(1, val);
}

int next_state(struct test_inst *inst,
               struct op_state *op_state,
               struct state *state)
{
    return next_bool(inst->test_IME, &state->interrupts_master_enabled)
               ? 0
               : next_flags(inst->test_F, &state->reg8.F)
                     ? 0
                     : next_val8(&state->reg8.A)
                           ? 0
                           : next_val16(inst->test_BC, &state->reg16.BC)
                                 ? 0
                                 : next_val16(inst->test_DE, &state->reg16.DE)
                                       ? 0
                                       : next_val16(inst->test_HL,
                                                    &state->reg16.HL)
                                             ? 0
                                             : next_val16(inst->test_SP,
                                                          &state->SP)
                                                   ? 0
                                                   : next_imm(inst->imm_size,
                                                              &op_state->imm)
                                                         ? 0
                                                         : next_oper(
                                                               inst->cond_bitpos !=
                                                                   -1,
                                                               4,
                                                               &op_state->cond)
                                                               ? 0
                                                               : next_oper(
                                                                     inst->reg16_bitpos !=
                                                                         -1,
                                                                     4,
                                                                     &op_state
                                                                          ->reg16)
                                                                     ? 0
                                                                     : next_oper(
                                                                           inst->reg8_bitpos !=
                                                                               -1,
                                                                           8,
                                                                           &op_state
                                                                                ->reg8)
                                                                           ? 0
                                                                           : next_oper(
                                                                                 inst->reg8_bitpos2 !=
                                                                                     -1,
                                                                                 8,
                                                                                 &op_state
                                                                                      ->reg8_2)
                                                                                 ? 0
                                                                                 : next_oper(
                                                                                       inst->bit_bitpos !=
                                                                                           -1,
                                                                                       8,
                                                                                       &op_state
                                                                                            ->bit)
                                                                                       ? 0
                                                                                       : 1;
}
