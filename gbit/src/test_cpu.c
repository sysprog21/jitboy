/*
 * Placeholder interface implementation for the testing framework.
 *
 * The four functions starting with mycpu_ should be provided by your CPU to be
 * tested.
 *
 * Additionally, this file contains an example implementation of the mock MMU
 * that maps the tester's instruction memory area read-only, and tracks all
 * write operations.
 */

#include <stdlib.h>
#include <string.h>

#include "../lib/tester.h"
#include "src/core.h"
#include "src/gbz80_instr.h"
#include "test_cpu.h"

static size_t instruction_mem_size;
static uint8_t *instruction_mem;

static uint8_t flag_args[1];

static int num_mem_accesses;
static struct mem_access mem_accesses[16];

static gb_vm *vm = NULL;

/* since jitboy only update pc when doing instruction jmp/call
 * so we trickly use a fake one and count it independently to pass gbit
 */
static uint16_t pc;
static uint8_t flag;

void cleanup()
{
    free_vm(vm);
    free(vm);
}

/*
 * Called once during startup. The area of memory pointed to by
 * tester_instruction_mem will contain instructions the tester will inject, and
 * should be mapped read-only at addresses [0,tester_instruction_mem_size).
 */
static void mycpu_init(size_t tester_instruction_mem_size,
                       uint8_t *tester_instruction_mem)
{
    instruction_mem_size = tester_instruction_mem_size;
    instruction_mem = tester_instruction_mem;

    /* ... Initialize your CPU here ... */
    vm = malloc(sizeof(gb_vm));
    if (!init_vm(vm, NULL, 0, false)) {
        LOG_ERROR("Fail to initialize\n");
        exit(1);
    }

#ifdef DEBUG
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
#endif
}

/*
 * Resets the CPU state (e.g., registers) to a given state state.
 */
static void mycpu_set_state(struct state *state)
{
    /* ... Load your CPU with state as described (e.g., registers) ... */
    memset(vm->memory.mem, 0xaa, 0x10000);
    memcpy(vm->memory.mem, instruction_mem, instruction_mem_size);

    vm->state.a = state->reg8.A;
    vm->state.b = state->reg8.B;
    vm->state.c = state->reg8.C;
    vm->state.d = state->reg8.D;
    vm->state.e = state->reg8.E;

    flag = state->reg8.F;

    flag_args[0] = 0;
    // carry
    flag_args[0] |= (flag & 0x10) >> 4;
    // adjust
    flag_args[0] |= (flag & 0x20) >> 1;
    // zero
    flag_args[0] |= (flag & 0x80) >> 1;

    vm->state.f_subtract = ((flag >> 6) & 1) == 1 ? true : false;

    vm->state.h = state->reg8.H;
    vm->state.l = state->reg8.L;

    vm->state._sp = state->SP;
    pc = state->PC;

    vm->state.halt = state->halted;
    vm->state.ime = state->interrupts_master_enabled;
}

/*
 * Query the current state of the CPU.
 */
static void mycpu_get_state(struct state *state)
{
    state->num_mem_accesses = num_mem_accesses;
    memcpy(state->mem_accesses, mem_accesses, sizeof(mem_accesses));

    /* ... Copy your current CPU state into the provided struct ... */
    state->reg8.A = vm->state.a;
    state->reg8.B = vm->state.b;
    state->reg8.C = vm->state.c;
    state->reg8.D = vm->state.d;
    state->reg8.E = vm->state.e;
    state->reg8.F = flag;
    state->reg8.H = vm->state.h;
    state->reg8.L = vm->state.l;

    state->SP = vm->state._sp;
    state->PC = pc;

    state->halted = vm->state.halt;
    state->interrupts_master_enabled = vm->state.ime;

    num_mem_accesses = 0;
}

static void load_flag(void)
{
    gb_block *block = &vm->compiled_blocks[2][0];

    GList *instructions = NULL;

    /* run the fake instruction for flag load */
    gbz80_inst *inst_last = g_new(gbz80_inst, 1);
    *inst_last = inst_table[0xfd];
    inst_last->args = NULL;
    inst_last->address = -1;
    instructions = g_list_prepend(instructions, inst_last);
    instructions = g_list_reverse(instructions);

    if (!optimize_cc(instructions)) {
        exit(1);
    }

    bool result = emit(block, instructions);
    g_list_free_full(instructions, g_free);

    if (result == false) {
        LOG_ERROR("Fail to compile instruction\n");
        exit(1);
    }

    block->func(&vm->state);
    free_block(block);
}
/*
 * Step a single instruction of the CPU. Returns the amount of cycles this took
 * (e.g., NOP should return 4).
 */
static int mycpu_step(void)
{
    /* ... Execute a single instruction in your CPU ... */
    gb_block *block = &vm->compiled_blocks[1][0];

    GList *instructions = NULL;

    /* run the fake instruction for flag reset */
    gbz80_inst *inst_0 = g_new(gbz80_inst, 1);
    *inst_0 = inst_table[0xfc];
    inst_0->args = flag_args;
    inst_0->address = -1;
    instructions = g_list_prepend(instructions, inst_0);

    gbz80_inst *inst = g_new(gbz80_inst, 1);

    uint8_t opcode = mymmu_read(pc);
    bool iscb = false;
    if (opcode != 0xcb) {
        *inst = inst_table[opcode];
    } else {
        iscb = true;
        opcode = mymmu_read(pc + 1);
        *inst = cb_table[opcode];
    }

    inst->args = instruction_mem + pc;
    inst->address = pc;

    if (inst->opcode == ERROR) {
        LOG_ERROR("Invalid Opcode! (%#x)\n", opcode);
        return false;
    }
    LOG_DEBUG("inst: %i @%#x\n", inst->opcode, inst->address);

    instructions = g_list_prepend(instructions, inst);
    instructions = g_list_reverse(instructions);

    if (!optimize_cc(instructions)) {
        exit(1);
    }

    bool result = emit(block, instructions);
    g_list_free_full(instructions, g_free);

    if (result == false) {
        LOG_ERROR("Fail to compile instruction\n");
        exit(1);
    }

    uint16_t ret = block->func(&vm->state);
    if (ret == 0xFFFF) {
        if (!iscb && (opcode == 0xc0 || opcode == 0xc2 || opcode == 0xc4) &&
            !(flag & 0x80))
            pc = ret;
        else if (!iscb &&
                 (opcode == 0xd0 || opcode == 0xd2 || opcode == 0xd4) &&
                 !(flag & 0x10))
            pc = ret;
        else if (!iscb &&
                 (opcode == 0xc8 || opcode == 0xca || opcode == 0xcc) &&
                 (flag & 0x80))
            pc = ret;
        else if (!iscb &&
                 (opcode == 0xd8 || opcode == 0xda || opcode == 0xdc) &&
                 (flag & 0x10))
            pc = ret;
        else if (!iscb && (opcode == 0xc9 || opcode == 0xc3 || opcode == 0xe9 ||
                           opcode == 0xcd || opcode == 0xc7 || opcode == 0xd7 ||
                           opcode == 0xe7 || opcode == 0xcf || opcode == 0xdf ||
                           opcode == 0xef || opcode == 0xff || opcode == 0xd9))
            pc = ret;
        else
            pc += inst->bytes;
    } else if (ret == pc) {
        if (!iscb && (opcode == 0xc0 || opcode == 0xc2 || opcode == 0xc4) &&
            !(flag & 0x80))
            pc = ret;
        else if (!iscb &&
                 (opcode == 0xd0 || opcode == 0xd2 || opcode == 0xd4) &&
                 !(flag & 0x10))
            pc = ret;
        else if (!iscb &&
                 (opcode == 0xc8 || opcode == 0xca || opcode == 0xcc) &&
                 (flag & 0x80))
            pc = ret;
        else if (!iscb &&
                 (opcode == 0xd8 || opcode == 0xda || opcode == 0xdc) &&
                 (flag & 0x10))
            pc = ret;
        else if (!iscb && (opcode == 0xc9 || opcode == 0xc3 || opcode == 0xe9 ||
                           opcode == 0xcd || opcode == 0xc7 || opcode == 0xd7 ||
                           opcode == 0xe7 || opcode == 0xcf || opcode == 0xdf ||
                           opcode == 0xef || opcode == 0xff || opcode == 0xd9))
            pc = ret;
        else  //  stop
            pc += 1;
    } else {
        pc = ret;
    }

    free_block(block);
    load_flag();

    return inst->cycles;
}

struct tester_operations myops = {
    .init = mycpu_init,
    .set_state = mycpu_set_state,
    .get_state = mycpu_get_state,
    .step = mycpu_step,
};


/*
 * Example mock MMU implementation, mapping the tester's instruction memory
 * read-only at address 0, and logging all writes.
 */

uint8_t mymmu_read(uint16_t address)
{
    if (address < instruction_mem_size)
        return instruction_mem[address];
    else
        return 0xaa;
}

void mymmu_write(uint16_t address, uint8_t data)
{
    struct mem_access *access = &mem_accesses[num_mem_accesses++];
    access->type = MEM_ACCESS_WRITE;
    access->addr = address;
    access->val = data;
}

void gbit_restore_flag(uint8_t gb_flag)
{
    flag = vm->state.f_subtract == true ? 0x40 : 0;

    // carry
    flag |= ((gb_flag) &0x01) << 4;
    // adjust
    flag |= ((gb_flag) &0x10) << 1;
    // zero
    flag |= ((gb_flag) &0x40) << 1;
}
