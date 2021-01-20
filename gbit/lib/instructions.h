#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

/*
 * Lists of instructions to test: the case opcode, which operands it has and how
 * to encode them, and what inputs affect the behavior of the instruction.
 *
 * The following fields exist for each instruction:
 *  - En: Whether the instruction is enabled (during testing).
 *  - Mnemonic: String to describe the (general) instruction.
 *  - OP: Base opcode of the instruction. Operands such as registers may be ORed
 *        into this.
 *  - CB: Whether this instruction is part of the CB prefix instruction set.
 *  - IMM: Size in bytes of the immediate operand (if any). Immediate operands
 *         follow the opcode byte in the instruction stream.
 *  - R8: Bit position of the 8-bit register operand (or -1 if none).
 *  - R8 (2nd): Bit position of the 2nd 8-bit register operand (or -1 if none).
 *  - R16: Bit position of the 16-bit register operand (or -1 if none).
 *  - CC: Bit position of the condition (Z/C/NZ/NC) operand (or -1 if none).
 *  - BIT: Bit position of the bit-number operand (or -1 if none).
 *  - F: Whether the status flags (ZNHC) have an impact on this instruction.
 *  - BC/DE/HL/SP: Whether the value of this register has an impact on the
 *                 behavior of this instruction.
 *  - IME: Whether the interrupt mask enable (IME) flag is modified by this
 *         instruction.
 *
 */

struct test_inst instructions[] = {
    /*En  Mnemonic               OP  CB IMM  R8  R8  R16 CC  BIT  F BC DE HL SP
       IME */
    {1, "NOP", 0x00, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD r16, imm16", 0x01, 0, 2, -1, -1, 4, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD (BC), A", 0x02, 0, 0, -1, -1, -1, -1, -1, 0, 1, 0, 0, 0, 0},
    {1, "INC r16", 0x03, 0, 0, -1, -1, 4, -1, -1, 0, 1, 1, 1, 1, 0},
    {1, "INC r8", 0x04, 0, 0, 3, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "DEC r8", 0x05, 0, 0, 3, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "LD r8, imm8", 0x06, 0, 1, 3, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "RLCA", 0x07, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "LD (imm16), SP", 0x08, 0, 2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 1, 0},
    {1, "ADD HL, r16", 0x09, 0, 0, -1, -1, 4, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "LD A, (BC)", 0x0a, 0, 0, -1, -1, -1, -1, -1, 0, 1, 0, 0, 0, 0},
    {1, "DEC r16", 0x0b, 0, 0, -1, -1, 4, -1, -1, 0, 1, 1, 1, 1, 0},
    {1, "RRCA", 0x0f, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "STOP", 0x10, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD (DE), A", 0x12, 0, 0, -1, -1, -1, -1, -1, 0, 0, 1, 0, 0, 0},
    {1, "RLA", 0x17, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "JR off8", 0x18, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD A, (DE)", 0x1a, 0, 0, -1, -1, -1, -1, -1, 0, 0, 1, 0, 0, 0},
    {1, "RRA", 0x1f, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "JR cc, off8", 0x20, 0, 1, -1, -1, -1, 3, -1, 1, 0, 0, 0, 0, 0},
    {1, "LD (HL+), A", 0x22, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 0, 0},
    {1, "DAA", 0x27, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "LD A, (HL+)", 0x2a, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 0, 0},
    {1, "CPL", 0x2f, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD (HL-), A", 0x32, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 0, 0},
    {1, "SCF", 0x37, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "LD A, (HL-)", 0x3a, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 0, 0},
    {1, "CCF", 0x3f, 0, 0, -1, -1, -1, -1, -1, 1, 0, 0, 0, 0, 0},
    {1, "LD r8, r8", 0x40, 0, 0, 0, 3, -1, -1, -1, 0, 1, 1, 1, 0,
     0},  // Also generates HALT (for LD (HL), (HL) slot)
    {1, "ADD r8", 0x80, 0, 0, 0, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "ADC r8", 0x88, 0, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "SUB r8", 0x90, 0, 0, 0, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "SBC r8", 0x98, 0, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "AND r8", 0xa0, 0, 0, 0, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "XOR r8", 0xa8, 0, 0, 0, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "OR r8", 0xb0, 0, 0, 0, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "CP r8", 0xb8, 0, 0, 0, -1, -1, -1, -1, 0, 1, 1, 1, 0, 0},
    {1, "RET cc", 0xc0, 0, 0, -1, -1, -1, 3, -1, 1, 0, 0, 0, 1, 0},
    {1, "POP r16", 0xc1, 0, 0, -1, -1, 4, -1, -1, 0, 1, 1, 1, 1, 0},
    {1, "JP cc, imm16", 0xc2, 0, 2, -1, -1, -1, 3, -1, 1, 0, 0, 0, 0, 0},
    {1, "JP imm16", 0xc3, 0, 2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "CALL cc, imm16", 0xc4, 0, 2, -1, -1, -1, 3, -1, 1, 0, 0, 0, 1, 0},
    {1, "PUSH r16", 0xc5, 0, 0, -1, -1, 4, -1, -1, 0, 1, 1, 1, 1, 0},
    {1, "RST vec", 0xc7, 0, 0, -1, -1, -1, -1, 3, 0, 0, 0, 0, 1, 0},
    {1, "RET", 0xc9, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 1, 0},
    {1, "CALL imm16", 0xcd, 0, 2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 1, 0},
    {1, "ADD imm8", 0xc6, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "ADC imm8", 0xce, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "SUB imm8", 0xd6, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "RETI", 0xd9, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 1, 1},
    {1, "SBC imm8", 0xde, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD ($ff00+imm8), A", 0xe0, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD ($ff00+C), A", 0xe2, 0, 0, -1, -1, -1, -1, -1, 0, 1, 0, 0, 0, 0},
    {1, "AND imm8", 0xe6, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "ADD SP, imm8", 0xe8, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 1, 0},
    {1, "JP HL", 0xe9, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 0, 0},
    {1, "LD (imm16), A", 0xea, 0, 2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "XOR imm8", 0xee, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD A, ($ff00+imm8)", 0xf0, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD A, ($ff00+C)", 0xf2, 0, 0, -1, -1, -1, -1, -1, 0, 1, 0, 0, 0, 0},
    {1, "DI", 0xf3, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 1},
    {1, "OR imm8", 0xf6, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "LD HL, SP + imm8", 0xf8, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 1, 1, 0},
    {1, "LD SP, HL", 0xf9, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 1, 1, 0},
    {1, "LD A, (imm16)", 0xfa, 0, 2, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
    {1, "EI", 0xfb, 0, 0, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 1},
    {1, "CP imm8", 0xfe, 0, 1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0},
};

struct test_inst cb_instructions[] = {
    /*En  Mnemonic           OP  CB IMM  R8  R8  R16 CC  BIT  F BC DE HL SP IME
     */
    {1, "RLC r8", 0x00, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "RRC r8", 0x08, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "RL r8", 0x10, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "RR r8", 0x18, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "SLA r8", 0x20, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "SRA r8", 0x28, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "SWAP r8", 0x30, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "SRL r8", 0x38, 1, 0, 0, -1, -1, -1, -1, 1, 1, 1, 1, 0, 0},
    {1, "BIT n, r8", 0x40, 1, 0, 0, -1, -1, -1, 3, 0, 1, 1, 1, 0, 0},
    {1, "RES n, r8", 0x80, 1, 0, 0, -1, -1, -1, 3, 0, 1, 1, 1, 0, 0},
    {1, "SET n, r8", 0xc0, 1, 0, 0, -1, -1, -1, 3, 0, 1, 1, 1, 0, 0},
};

#endif
