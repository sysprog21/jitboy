
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "disassembler.h"
#include "inputstate.h"
#include "instructions.h"
#include "ref_cpu.h"
#include "tester.h"

#define INSTRUCTION_MEM_SIZE 4
static u8 instruction_mem[INSTRUCTION_MEM_SIZE];

static unsigned long num_tests = 0;
static bool tested_op[256] = {0};
static bool tested_op_cb[256] = {0};

static struct tester_operations *tcpu_ops;
static struct tester_flags *flags;

static void dump_state(struct state *state)
{
    printf(
        " PC   SP   AF   BC   DE   HL  ZNHC hlt IME\n"
        "%04x %04x %04x %04x %04x %04x %d%d%d%d  %d   %d\n",
        state->PC, state->SP, state->reg16.AF, state->reg16.BC, state->reg16.DE,
        state->reg16.HL, BIT(state->reg16.AF, 7), BIT(state->reg16.AF, 6),
        BIT(state->reg16.AF, 5), BIT(state->reg16.AF, 4), state->halted,
        state->interrupts_master_enabled);

    for (int i = 0; i < state->num_mem_accesses; i++)
        printf("  Mem %s: addr=%04x val=%02x\n",
               state->mem_accesses[i].type ? "write" : "read",
               state->mem_accesses[i].addr, state->mem_accesses[i].val);
    printf("\n");
}

static void dump_op_state(struct test_inst *inst, struct op_state *op_state)
{
    const char *reg8_names[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
    const char *reg16_names[] = {"BC", "DE", "HL", "SP/AF"};
    const char *cond_names[] = {"NZ", "Z", "NC", "C"};

    if (inst->imm_size == 1)
        printf("imm: %02x\n", op_state->imm);
    else if (inst->imm_size == 2)
        printf("imm: %04x\n", op_state->imm);

    if (inst->reg8_bitpos != -1)
        printf("r8: %s (%d)\n", reg8_names[op_state->reg8], op_state->reg8);

    if (inst->reg8_bitpos2 != -1)
        printf("r8: %s (%d)\n", reg8_names[op_state->reg8_2], op_state->reg8_2);

    if (inst->reg16_bitpos != -1)
        printf("r16: %s (%d)\n", reg16_names[op_state->reg16], op_state->reg16);

    if (inst->cond_bitpos != -1)
        printf("cond: %s (%d)\n", cond_names[op_state->cond], op_state->cond);

    if (inst->bit_bitpos != -1)
        printf("bit: %d\n", op_state->bit);
}

static int mem_access_cmp(const void *p1, const void *p2)
{
    u16 addr1 = ((struct mem_access *) p1)->addr;
    u16 addr2 = ((struct mem_access *) p2)->addr;
    if (addr1 < addr2)
        return -1;
    if (addr1 > addr2)
        return 1;
    return 0;
}

static int states_mem_accesses_eq(struct state *s1, struct state *s2)
{
    int i;

    if (s1->num_mem_accesses != s2->num_mem_accesses)
        return 0;

    /* Sort memory accesses by address before comparing them. Usually the order
     * of memory accesses do not matter (e.g., the order of bytes of a 16-bit
     * store), so this reduces false positives. */
    qsort(s1->mem_accesses, s1->num_mem_accesses, sizeof(struct mem_access),
          mem_access_cmp);
    qsort(s2->mem_accesses, s2->num_mem_accesses, sizeof(struct mem_access),
          mem_access_cmp);

    for (i = 0; i < s1->num_mem_accesses; i++)
        if (s1->mem_accesses[0].type != s2->mem_accesses[0].type ||
            s1->mem_accesses[0].addr != s2->mem_accesses[0].addr ||
            s1->mem_accesses[0].val != s2->mem_accesses[0].val)
            return 0;

    return 1;
}

static int states_eq(struct state *s1, struct state *s2)
{
    return s1->reg16.AF == s2->reg16.AF && s1->reg16.BC == s2->reg16.BC &&
           s1->reg16.DE == s2->reg16.DE && s1->reg16.HL == s2->reg16.HL &&
           s1->PC == s2->PC && s1->SP == s2->SP && s1->halted == s2->halted &&
           s1->interrupts_master_enabled == s2->interrupts_master_enabled &&
           states_mem_accesses_eq(s1, s2);
}

static void state_reset(struct state *s)
{
    memset(s, 0, sizeof(*s));
}

static void op_state_reset(struct op_state *op_state)
{
    memset(op_state, 0, sizeof(*op_state));
}


static int run_state(struct state *state)
{
    struct state tcpu_out_state, rcpu_out_state;

    tcpu_ops->set_state(state);
    rcpu_reset(state);

    tcpu_ops->step();
    rcpu_step();

    tcpu_ops->get_state(&tcpu_out_state);
    rcpu_get_state(&rcpu_out_state);

    if (!states_eq(&tcpu_out_state, &rcpu_out_state)) {
        printf("\n  === STATE MISMATCH ===\n");
        printf("\n - Instruction -\n");
        disassemble(instruction_mem);
        printf("\n - Input state -\n");
        dump_state(state);
        printf("\n - Test-CPU output state -\n");
        dump_state(&tcpu_out_state);
        printf("\n - Correct output state -\n");
        dump_state(&rcpu_out_state);
        return 1;
    }

    return 0;
}

static void assemble(u8 *out, struct test_inst *inst, struct op_state *op_state)
{
    u8 opcode = inst->opcode;
    int idx = 0;

    if (inst->reg8_bitpos >= 0)
        opcode |= op_state->reg8 << inst->reg8_bitpos;
    if (inst->reg8_bitpos2 >= 0)
        opcode |= op_state->reg8_2 << inst->reg8_bitpos2;
    if (inst->reg16_bitpos >= 0)
        opcode |= op_state->reg16 << inst->reg16_bitpos;
    if (inst->cond_bitpos >= 0)
        opcode |= op_state->cond << inst->cond_bitpos;
    if (inst->bit_bitpos >= 0)
        opcode |= op_state->bit << inst->bit_bitpos;

    if (inst->is_cb_prefix)
        out[idx++] = 0xcb;

    out[idx++] = opcode;

    if (inst->imm_size >= 1)
        out[idx++] = op_state->imm & 0xff;
    if (inst->imm_size >= 2)
        out[idx++] = (op_state->imm >> 8) & 0xff;
}

static int test_instruction(struct test_inst *inst)
{
    struct op_state op_state;
    struct state state;
    bool *op_success_table;
    bool had_failure = 0;
    bool last_op_had_failure = 0;
    u8 last_op = 0;

    op_success_table = inst->is_cb_prefix ? tested_op_cb : tested_op;

    state_reset(&state);
    op_state_reset(&op_state);
    do {
        u8 opcode;

        assemble(instruction_mem, inst, &op_state);
        opcode = instruction_mem[inst->is_cb_prefix ? 1 : 0];

        if (opcode != last_op)
            last_op_had_failure = false;

        if (last_op_had_failure)
            continue;

        num_tests++;

        if (flags->print_verbose_inputs) {
            dump_op_state(inst, &op_state);
            disassemble(instruction_mem);
        }

        last_op_had_failure = run_state(&state);

        last_op = opcode;
        op_success_table[opcode] = !last_op_had_failure;

        if (last_op_had_failure) {
            had_failure = true;
            if (!flags->keep_going_on_mismatch)
                return 1;
        }
    } while (!next_state(inst, &op_state, &state));

    return had_failure;
}

static int test_instructions(size_t num_instructions,
                             struct test_inst *insts,
                             const char *prefix)
{
    size_t num_instructions_tested = 0;
    size_t num_instructions_passed = 0;

    for (size_t i = 0; i < num_instructions; i++) {
        bool failure;

        if (flags->print_tested_instruction) {
            printf("%s   ", insts[i].mnem);
            if (insts[i].is_cb_prefix)
                printf("(CB prefix)");
            printf("\n");
        }

        if (!insts[i].enabled) {
            if (flags->print_tested_instruction)
                printf(" Skipping\n");
            continue;
        }

        failure = test_instruction(&insts[i]);
        if (failure && !flags->keep_going_on_mismatch)
            return 1;

        if (flags->print_tested_instruction)
            printf(" Ran %lu permutations\n", num_tests);

        if (!failure)
            num_instructions_passed++;
        num_instructions_tested++;
        num_tests = 0;
    }

    if (flags->print_tested_instruction)
        printf("\n");

    printf("Tested %zu/%zu %sinstructions", num_instructions_tested,
           num_instructions, prefix);
    if (num_instructions_passed != num_instructions_tested)
        printf(", %zu passed and %zu failed", num_instructions_passed,
               num_instructions_tested - num_instructions_passed);
    printf("\n");
    if (flags->print_tested_instruction)
        printf("\n");

    return 0;
}

static bool is_valid_op(u8 op)
{
    u8 invalid_ops[] = {0xcb,  // Special case (prefix)
                        0xd3, 0xdb, 0xdd, 0xe3, 0xe4, 0xeb,
                        0xec, 0xed, 0xf4, 0xfc, 0xfd};

    for (size_t i = 0; i < sizeof(invalid_ops); i++)
        if (invalid_ops[i] == op)
            return false;

    return true;
}

static bool is_valid_op_cb(u8 op)
{
    (void) op;
    return true;
}

static void print_coverage(bool *op_table,
                           bool (*is_valid)(u8),
                           const char *prefix)
{
    unsigned num_tested_ops, total_valid_ops;

    num_tested_ops = 0, total_valid_ops = 0;
    for (int op = 0; op <= 0xff; op++) {
        if (op_table[op])
            num_tested_ops++;
        if (is_valid(op))
            total_valid_ops++;
    }

    printf("Successfully tested %d/%d %sopcodes\n", num_tested_ops,
           total_valid_ops, prefix);

    if (num_tested_ops < total_valid_ops) {
        printf("\nTable of untested/incorrect %sopcodes:\n", prefix);
        for (int op = 0; op <= 0xff; op++) {
            if (op_table[op] || !is_valid(op))
                printf("-- ");
            else
                printf("%02x ", op);
            if ((op & 0xf) == 0xf)
                printf("\n");
        }
        printf("\n");
    }
}

static int test_all_instructions(void)
{
    size_t num_instructions = sizeof(instructions) / sizeof(instructions[0]);
    size_t num_cb_instructions =
        sizeof(cb_instructions) / sizeof(cb_instructions[0]);

    if (test_instructions(num_instructions, instructions, ""))
        return 1;

    if (flags->enable_cb_instruction_testing)
        if (test_instructions(num_cb_instructions, cb_instructions, "CB "))
            return 1;

    print_coverage(tested_op, is_valid_op, "");
    if (flags->enable_cb_instruction_testing)
        print_coverage(tested_op_cb, is_valid_op_cb, "CB ");

    return 0;
}


int tester_run(struct tester_flags *app_flags,
               struct tester_operations *app_tcpu_ops)
{
    flags = app_flags;
    tcpu_ops = app_tcpu_ops;

    tcpu_ops->init(INSTRUCTION_MEM_SIZE, instruction_mem);
    rcpu_init(INSTRUCTION_MEM_SIZE, instruction_mem);

    return test_all_instructions();
}
