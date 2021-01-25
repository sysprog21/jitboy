#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gbit/lib/tester.h"
#include "instr_test.h"
#include "src/core.h"


static size_t instruction_mem_size;
static uint8_t *instruction_mem;

static int num_mem_accesses;
static struct mem_access mem_accesses[16];

static gb_vm *vm = NULL;

/* since jitboy only update pc when doing instruction jmp/call
 * so we introduce an indirect layer for memory/register manipulation,
 * counting it independently to pass gbit
 */
static uint16_t pc;
static uint8_t flag;

/* flag_args is used to carry the arguments for the initial status flag
 * which is required for codegen
 */
static uint8_t *flag_args;
/* the block for flag loading will be created at the initial and then cached */
static gb_block *load_flag_block;

static void gbz80_init(size_t tester_instruction_mem_size,
                       uint8_t *tester_instruction_mem)
{
    instruction_mem_size = tester_instruction_mem_size;
    instruction_mem = tester_instruction_mem;

    vm = malloc(sizeof(gb_vm));
    if (!init_vm(vm, NULL, 0, false)) {
        LOG_ERROR("Fail to initialize\n");
        exit(1);
    }
    flag_args = malloc(sizeof(uint8_t));
    load_flag_block = &vm->compiled_blocks[2][0];
    build_load_flag_block(load_flag_block);
}

static void gbz80_close()
{
    free(flag_args);
    free_block(load_flag_block);
    free_vm(vm);
    free(vm);
}

static void gbz80_set_state(struct state *state)
{
    /* setup memory content as the tester requires */
    memset(vm->memory.mem, 0xaa, 0x10000);
    memcpy(vm->memory.mem, instruction_mem, instruction_mem_size);

    vm->state.a = state->reg8.A;
    vm->state.b = state->reg8.B;
    vm->state.c = state->reg8.C;
    vm->state.d = state->reg8.D;
    vm->state.e = state->reg8.E;

    flag = state->reg8.F;

    /* mapping gbz80's status flag to x86's */
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

static void gbz80_get_state(struct state *state)
{
    state->num_mem_accesses = num_mem_accesses;
    memcpy(state->mem_accesses, mem_accesses, sizeof(mem_accesses));

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

static int gbz80_step(void)
{
    gb_block *block = &vm->compiled_blocks[1][0];

    struct instr_info inst_info =
        compile_and_run(block, &vm->memory, pc, flag_args);

    bool is_cb = inst_info.is_cb;
    uint8_t opcode = inst_info.opcode;
    uint8_t bytes = inst_info.bytes;

    uint16_t ret = block->func(&vm->state);

    /* Due to the JIT compilation, not each instruction is executed at one time.
     * But this may violate the expectation of instructions tester. If we want
     * to do this on jitboy, we can't always get the correct program counter on
     * the JIT codes' return. Since jitboy cares about the address after
     * JMP/CALL type instructions only, we let the tester check this. Otherwise
     * we'll introduce an indirect layer for memory/register manipulation, using
     * the program counter that doesn't directly come from jitboy to pass the
     * tester.
     */
    if (ret == 0xFFFF) {
        if (!is_cb && (opcode == 0xc0 || opcode == 0xc2 || opcode == 0xc4) &&
            !(flag & 0x80))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xd0 || opcode == 0xd2 || opcode == 0xd4) &&
                 !(flag & 0x10))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xc8 || opcode == 0xca || opcode == 0xcc) &&
                 (flag & 0x80))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xd8 || opcode == 0xda || opcode == 0xdc) &&
                 (flag & 0x10))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xc9 || opcode == 0xc3 || opcode == 0xe9 ||
                  opcode == 0xcd || opcode == 0xc7 || opcode == 0xd7 ||
                  opcode == 0xe7 || opcode == 0xcf || opcode == 0xdf ||
                  opcode == 0xef || opcode == 0xff || opcode == 0xd9))
            pc = ret;
        else
            pc += bytes;
    } else if (ret == pc) {
        if (!is_cb && (opcode == 0xc0 || opcode == 0xc2 || opcode == 0xc4) &&
            !(flag & 0x80))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xd0 || opcode == 0xd2 || opcode == 0xd4) &&
                 !(flag & 0x10))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xc8 || opcode == 0xca || opcode == 0xcc) &&
                 (flag & 0x80))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xd8 || opcode == 0xda || opcode == 0xdc) &&
                 (flag & 0x10))
            pc = ret;
        else if (!is_cb &&
                 (opcode == 0xc9 || opcode == 0xc3 || opcode == 0xe9 ||
                  opcode == 0xcd || opcode == 0xc7 || opcode == 0xd7 ||
                  opcode == 0xe7 || opcode == 0xcf || opcode == 0xdf ||
                  opcode == 0xef || opcode == 0xff || opcode == 0xd9))
            pc = ret;
        else  //  stop
            pc += 1;
    } else {
        pc = ret;
    }

    load_flag_block->func(&vm->state);
    free_block(block);

    return inst_info.cycles;
}


void gbz80_mmu_write(uint16_t address, uint8_t data)
{
    struct mem_access *access = &mem_accesses[num_mem_accesses++];
    access->type = MEM_ACCESS_WRITE;
    access->addr = address;
    access->val = data;
}

void gbz80_restore_flag(uint8_t gb_flag)
{
    flag = vm->state.f_subtract == true ? 0x40 : 0;

    /* mapping x86's status flag back to gbz80's */
    // carry
    flag |= ((gb_flag) &0x01) << 4;
    // adjust
    flag |= ((gb_flag) &0x10) << 1;
    // zero
    flag |= ((gb_flag) &0x40) << 1;
}

static struct tester_flags flags = {
    .keep_going_on_mismatch = 0,
    .enable_cb_instruction_testing = 1,
    .print_tested_instruction = 0,
    .print_verbose_inputs = 0,
};

struct tester_operations myops = {
    .init = gbz80_init,
    .set_state = gbz80_set_state,
    .get_state = gbz80_get_state,
    .step = gbz80_step,
};

static void print_usage(char *progname)
{
    printf("Usage: %s [option]...\n\n", progname);
    printf("Game Boy Instruction Tester.\n\n");
    printf("Options:\n");
    printf(
        " -k, --keep-going       Skip to the next instruction on a mismatch "
        "(instead of aborting all tests).\n");
    printf(
        " -c, --no-enable-cb     Disable testing of CB prefixed "
        "instructions.\n");
    printf(" -p, --print-inst       Print instruction undergoing tests.\n");
    printf(" -v, --print-input      Print every inputstate that is tested.\n");
    printf(" -h, --help             Show this help.\n");
}

static bool parse_args(int argc, char **argv)
{
    while (1) {
        static struct option long_options[] = {
            {"keep-going", no_argument, 0, 'k'},
            {"no-enable-cb", no_argument, 0, 'c'},
            {"print-inst", no_argument, 0, 'p'},
            {"print-input", no_argument, 0, 'v'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}};

        char c = getopt_long(argc, argv, "kcpvh", long_options, NULL);

        if (c == -1)
            break;

        switch (c) {
        case 'k':
            flags.keep_going_on_mismatch = 1;
            break;
        case 'c':
            flags.enable_cb_instruction_testing = 0;
            break;
        case 'p':
            flags.print_tested_instruction = 1;
            break;
        case 'v':
            flags.print_verbose_inputs = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            exit(0);
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (optind != argc) {
        /* We should not have any leftover arguments. */
        print_usage(argv[0]);
        return true;
    }

    return false;
}

int main(int argc, char **argv)
{
    if (parse_args(argc, argv))
        return 1;

    int ret = tester_run(&flags, &myops);
    gbz80_close();

    return ret;
}
