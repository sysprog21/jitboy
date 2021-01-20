#ifndef REF_CPU_H
#define REF_CPU_H

#include "common.h"

void rcpu_init(size_t tester_instruction_mem_size, u8 *tester_instruction_mem);
void rcpu_reset(struct state *state);
void rcpu_get_state(struct state *state);
int rcpu_step(void);

#if 0
struct gb_state {
    /* Registers: allow access to 8-bit and 16-bit regs, and via array. */
    union {
        u8 regs[8];
        struct {
            u16 BC, DE, HL, AF;
        } reg16;
        struct { /* little-endian of x86 is not nice here. */
            u8 C, B, E, D, L, H, F, A;
        } reg8;
        struct __attribute__((packed)) {
            char padding[6];
            u8 pad1:1;
            u8 pad2:1;
            u8 pad3:1;
            u8 pad4:1;
            u8 CF:1;
            u8 HF:1;
            u8 NF:1;
            u8 ZF:1;
        } flags;
    };

    u16 sp;
    u16 pc;

    int halted;
    int interrupts_master_enabled;
};

void rmmu_write(struct gb_state *s, u16 addr, u8 val);
u8   rmmu_read(struct gb_state *s, u16 addr);
u16  rmmu_read16(struct gb_state *s, u16 location);
void rmmu_write16(struct gb_state *s, u16 location, u16 value);
u16  rmmu_pop16(struct gb_state *s);
void rmmu_push16(struct gb_state *s, u16 value);

void ref_cpu_init_luts(struct gb_state *s);
int ref_cpu_do_instruction(struct gb_state *s);
#endif

#endif
