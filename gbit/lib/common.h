#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

struct test_inst {
    bool enabled;

    char *mnem;
    u8 opcode;
    bool is_cb_prefix;
    int imm_size;

    /* Presence and position of operand encoded in opcode (-1 if not used). */
    int reg8_bitpos, reg8_bitpos2;
    int reg16_bitpos;
    int cond_bitpos;
    int bit_bitpos;

    /* Whether to vary the specific register during tests. */
    bool test_F, test_BC, test_DE, test_HL, test_SP, test_IME;
};


#define MEM_ACCESS_READ 0
#define MEM_ACCESS_WRITE 1

struct mem_access {
    int type;
    u16 addr;
    u8 val;
};

struct state {
    union {
        struct {
            u16 AF, BC, DE, HL;
        } reg16;
        struct {
            u8 F, A, C, B, E, D, L, H;
        } reg8;
    };
    u16 SP;
    u16 PC;
    bool halted;
    bool interrupts_master_enabled;

    int num_mem_accesses;
    struct mem_access mem_accesses[16];
};

#define BIT(val, bitpos) (((val) >> (bitpos)) & 1)

#endif
