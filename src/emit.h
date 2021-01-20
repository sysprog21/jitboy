#ifndef JITBOY_EMIT_H
#define JITBOY_EMIT_H

#define DATA(X) ((gbz80_inst *) (X)->data)

#include "glib.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "memory.h"

typedef struct {
    enum {
        NOP,
        LD16,
        LD,
        INC16,
        INC,
        DEC16,
        DEC,
        RLC,
        RLCA,
        ADD16,
        ADD,
        RRC,
        RRCA,
        STOP,
        RL,
        RLA,
        JR,
        RR,
        RRA,
        DAA,
        CPL,
        SCF,
        CCF,
        HALT,
        ADC,
        SUB,
        SBC,
        AND,
        XOR,
        OR,
        CP,
        RET,
        POP,
        JP,
        CALL,
        PUSH,
        RST,
        RETI,
        DI,
        EI,
        SLA,
        SRA,
        SWAP,
        SRL,
        BIT,
        RES,
        SET,
        JP_TARGET,
        JP_BWD,
        JP_FWD,
        ERROR
    } opcode;
    enum {
        NONE,
        REG_A,
        REG_B,
        REG_C,
        REG_D,
        REG_E,
        REG_H,
        REG_L /* = 7 */,
        REG_AF,
        REG_BC,
        REG_DE,
        REG_HL,
        REG_SP,
        IMM8,
        IMM16 /* = 14 */,
        MEM_BC,
        MEM_DE,
        MEM_HL,
        MEM_16,
        MEM_8,
        MEM_C,
        MEM_INC_DE,
        MEM_INC_HL,
        MEM_DEC_HL /*=22*/,
        CC_Z,
        CC_C,
        CC_NZ,
        CC_NC,
        MEM_0x00,
        MEM_0x08,
        MEM_0x10,
        MEM_0x18,
        MEM_0x20,
        MEM_0x28,
        MEM_0x30,
        MEM_0x38,
        BIT_0,
        BIT_1,
        BIT_2,
        BIT_3,
        BIT_4,
        BIT_5,
        BIT_6,
        BIT_7,
        TARGET_1,
        TARGET_2,
        WAIT_LY,
        WAIT_STAT3
    } op1,
        op2;
    uint8_t *args;
    uint16_t address;
    uint8_t bytes;
    uint8_t cycles, alt_cycles;
    enum {
        INST_FLAG_NONE = 0x00,
        INST_FLAG_PRESERVE_CC = 0x01,
        INST_FLAG_PERS_WRITE = 0x02,
        INST_FLAG_USES_CC = 0x04,
        INST_FLAG_AFFECTS_CC = 0x08,
        INST_FLAG_ENDS_BLOCK = 0x10,
        INST_FLAG_SAVE_CC = 0x20,
        INST_FLAG_RESTORE_CC = 0x40
    } flags;
} gbz80_inst;

typedef struct {
    uint16_t (*func)(gb_state *);
    unsigned exec_count;
    uint16_t end_address;
    size_t size;
    void *mem;
} gb_block;

bool emit(gb_block *block, GList *inst);

#endif
