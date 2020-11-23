#include "optimize.h"
#include "emit.h"

/* The instruction always has the same result deterministically regardless of
 * memory, special registers, etc.
 */
static bool is_const(gbz80_inst *inst, int opt_level)
{
    switch (inst->opcode) {
    case NOP:
        return true;
    case LD:
        if (inst->op1 == MEM_HL || inst->op2 == MEM_HL ||
            inst->op1 == MEM_INC_HL || inst->op2 == MEM_INC_HL ||
            inst->op1 == MEM_DEC_HL || inst->op2 == MEM_DEC_HL ||
            inst->op1 == MEM_C || inst->op2 == MEM_C)
            return false;

        if ((inst->op1 == MEM_8 || inst->op2 == MEM_8) &&
            (inst->args[1] < 0x80 || inst->args[1] == 0xff))
            return false;

        return true;
    case AND:
    case CP:
        return inst->op2 == MEM_HL ? false : true;
    default:
        return false;
    }
}

/* Instruction without memory access (read / write) or other side effects
 * that can be influenced by an interrupt.
 */
static bool is_no_mem_access(gbz80_inst *inst, int opt_level)
{
    switch (inst->opcode) {
    case NOP:
    case ADD16:
    case SWAP:
    case DAA:
    case CP:
    case BIT:
    case RST:
    case JR:
    case JP:
    case RET:
    case CALL:
    case JP_BWD:
    case JP_TARGET:
    case LD16:
    case RR:
    case RL:
    case RRC:
    case RLC:
        return true;
    case LD:
        if (inst->op2 == MEM_C || inst->op2 == MEM_8)
            return false;

        /* mostly in connection with a memcpy */
        if (opt_level == 3 && inst->op1 == MEM_INC_DE)
            return true;

        /* Writing access and reading from HL is usually fine */
        if (opt_level < 3 &&
            (inst->op1 == MEM_HL || inst->op2 == MEM_HL ||
             inst->op1 == MEM_INC_HL || inst->op2 == MEM_INC_HL ||
             inst->op1 == MEM_DEC_HL || inst->op2 == MEM_DEC_HL ||
             inst->op1 == MEM_C || inst->op1 == MEM_8))
            return false;

        return true;
    case AND:
    case OR:
    case XOR:
        return inst->op2 == MEM_HL ? false : true;
    case ADD:
    case SUB:
        return true;
    case PUSH:
    case POP:
        return true;
    case INC:
    case DEC:
    case INC16:
    case DEC16:
        return (opt_level == 3) ? true : ((inst->op1 == MEM_HL) ? false : true);
    default:
        return false;
    }
}

static bool is_jump_to_start(gbz80_inst *inst, int byte_offset)
{
    if (inst->opcode == JR && inst->op2 == IMM8 &&
        (int8_t) inst->args[1] + byte_offset == 0) {
        return true;
    }
    return false;
}

bool optimize_block(GList **instructions, int opt_level)
{
    if (opt_level == 0) /* no optimization */
        return true;

    for (GList *inst = *instructions; inst != NULL; inst = inst->next) {
        uint32_t *a = (uint32_t *) DATA(inst)->args;

        /* pattern is LD A,HL+; LD (DE),A; INC DE -> LD DE+, HL+ */
        if ((*a & 0xffffff) == 0x13122a) {
            printf("optimizing block @%#x (3)\n", DATA(*instructions)->address);
            DATA(inst)->op1 = MEM_INC_DE;
            DATA(inst)->cycles = 6;
            DATA(inst)->bytes = 3;
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
        }

        /* pattern f0 41 e6 03 20 fa -> wait for stat mode 3 */
        if ((*a) == 0x03e641f0 && (*(a + 1) & 0xffff) == 0xfa20) {
            printf("optimizing block @%#x (4)\n", DATA(*instructions)->address);
            DATA(inst)->opcode = HALT;
            DATA(inst)->op1 = WAIT_STAT3;
            DATA(inst)->cycles = 0;
            DATA(inst)->bytes = 6;
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
        }

        /* pattern f0 44 fe ?? 20 fa -> wait for ly */
        if ((*a & 0x00ffffff) == 0xfe44f0 && (*(a + 1) & 0xffff) == 0xfa20) {
            printf("optimizing block @%#x (5)\n", DATA(*instructions)->address);
            DATA(inst)->opcode = HALT;
            DATA(inst)->op1 = WAIT_LY;
            DATA(inst)->cycles = 0;
            DATA(inst)->bytes = 6;
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
        }

        /* pattern f0 00 f0 00 -> repeated read of jopad register */
        if ((*a) == 0x00f000f0) {
            printf("optimizing block @%#x (6)\n", DATA(*instructions)->address);
            DATA(inst)->cycles += 3;
            DATA(inst)->bytes += 2;
            DATA(inst)->args = DATA(inst->next)->args;
            g_free(inst->next->data);
            *instructions = g_list_delete_link(*instructions, inst->next);
            if (inst->prev) /* apply pattern to this instruction again */
                inst = inst->prev;
        }
    }

    int byte_offset = 0;
    for (GList *inst = *instructions; inst != NULL; inst = inst->next) {
        byte_offset += DATA(inst)->bytes;
        if (is_jump_to_start(DATA(inst), byte_offset)) {
            bool can_optimize1 = true;
            bool can_optimize2 = true;
            for (GList *inst2 = *instructions; inst2 != inst;
                 inst2 = inst2->next) {
                if (!is_const(DATA(inst2), opt_level))
                    can_optimize1 = false;

                if (!is_no_mem_access(DATA(inst2), opt_level))
                    can_optimize2 = false;
            }

            if (can_optimize1) {
                printf("optimizing block @%#x (1)\n",
                       DATA(*instructions)->address);
                /* optimize  2: ... JP <2
                 * to        JP >1 2: HALT 1: ... JP <2
                 *
                 * insert jump target at the position of the old start
                 * instruction.
                 */
                gbz80_inst *jp_target = g_new(gbz80_inst, 1);
                *jp_target = (gbz80_inst){
                    JP_TARGET, TARGET_1, NONE, 0, DATA(*instructions)->address,
                    0,         0,        0,    0};
                *instructions = g_list_prepend(*instructions, jp_target);
                /* prepend halt, as the loop cannot change the break condition
                 */
                gbz80_inst *halt_inst = g_new(gbz80_inst, 1);
                *halt_inst = (gbz80_inst){HALT,
                                          NONE,
                                          NONE,
                                          0,
                                          DATA(*instructions)->address,
                                          1,
                                          1,
                                          1,
                                          INST_FLAG_ENDS_BLOCK};
                *instructions = g_list_prepend(*instructions, halt_inst);
                /* prepend jump target before halt instruction */
                gbz80_inst *jp_target2 = g_new(gbz80_inst, 1);
                *jp_target2 = (gbz80_inst){
                    JP_TARGET, TARGET_2, NONE, 0, DATA(*instructions)->address,
                    0,         0,        0,    0};
                *instructions = g_list_prepend(*instructions, jp_target2);
                /* prepend jp, to jump to the old start instruction */
                gbz80_inst *jp_inst = g_new(gbz80_inst, 1);
                *jp_inst = (gbz80_inst){
                    JP_FWD, NONE, TARGET_1, 0, DATA(*instructions)->address,
                    0,      0,    0,        0};
                *instructions = g_list_prepend(*instructions, jp_inst);
                /* modify jump to point to the halt instruction */
                DATA(inst)->opcode = JP_BWD;
                DATA(inst)->op2 = TARGET_2;
                DATA(inst)->flags &= ~INST_FLAG_ENDS_BLOCK;
                break;
            } else if (can_optimize2) {
                if (DATA(*instructions)->address < 0xff00)
                    printf("optimizing block @%#x (2)\n",
                           DATA(*instructions)->address);

                /* insert jump target at the position of the old start
                 * instruction.
                 */
                gbz80_inst *jp_target = g_new(gbz80_inst, 1);
                *jp_target = (gbz80_inst){
                    JP_TARGET, TARGET_1, NONE, 0, DATA(*instructions)->address,
                    0,         0,        0,    0};
                *instructions = g_list_prepend(*instructions, jp_target);

                DATA(inst)->opcode = JP_BWD;
                DATA(inst)->op2 = TARGET_1;
            } else {
                printf("jp to start detected, could not optimize %#x\n",
                       DATA(*instructions)->address);
            }
        }
    }

    return true;
}
