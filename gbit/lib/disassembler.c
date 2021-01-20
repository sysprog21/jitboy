/*
 * Originally adapted from VisualBoyAdvance.
 */

#include "disassembler.h"

#include <stdio.h>

typedef struct {
    u8 mask;
    u8 value;
    const char *mnem;
} GBOPCODE;

static const char *registers[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};

static const char *registers16[] = {
    "BC", "DE", "HL", "SP",  /* for some operations */
    "BC", "DE", "HL", "AF"}; /* for push/pop */

static const char *conditions[] = {"NZ", "Z", "NC", "C"};

static GBOPCODE opcodes[] = {{0xff, 0x00, "NOP"},
                             {0xcf, 0x01, "LD %R4, %W"},
                             {0xff, 0x02, "LD (BC), A"},
                             {0xcf, 0x03, "INC %R4"},
                             {0xc7, 0x04, "INC %r3"},
                             {0xc7, 0x05, "DEC %r3"},
                             {0xc7, 0x06, "LD %r3, %B"},
                             {0xff, 0x07, "RLCA"},
                             {0xff, 0x08, "LD (%W), SP"},
                             {0xcf, 0x09, "ADD HL, %R4"},
                             {0xff, 0x0a, "LD A, (BC)"},
                             {0xcf, 0x0b, "DEC %R4"},
                             {0xff, 0x0f, "RRCA"},
                             {0xff, 0x10, "STOP"},
                             {0xff, 0x12, "LD (DE), A"},
                             {0xff, 0x17, "RLA"},
                             {0xff, 0x18, "JR %d"},
                             {0xff, 0x1a, "LD A, (DE)"},
                             {0xff, 0x1f, "RRA"},
                             {0xe7, 0x20, "JR %c3, %d"},
                             {0xff, 0x22, "LDI (HL), A"},
                             {0xff, 0x27, "DAA"},
                             {0xff, 0x2a, "LD A, (HL+)"},
                             {0xff, 0x2f, "CPL"},
                             {0xff, 0x32, "LDD (HL), A"},
                             {0xff, 0x37, "SCF"},
                             {0xff, 0x3a, "LD A, (HL-)"},
                             {0xff, 0x3f, "CCF"},
                             {0xff, 0x76, "HALT"},
                             {0xc0, 0x40, "LD %r3, %r0"},
                             {0xf8, 0x80, "ADD A, %r0"},
                             {0xf8, 0x88, "ADC A, %r0"},
                             {0xf8, 0x90, "SUB %r0"},
                             {0xf8, 0x98, "SBC A, %r0"},
                             {0xf8, 0xa0, "AND %r0"},
                             {0xf8, 0xa8, "XOR %r0"},
                             {0xf8, 0xb0, "OR %r0"},
                             {0xf8, 0xb8, "CP %r0"},
                             {0xe7, 0xc0, "RET %c3"},
                             {0xcf, 0xc1, "POP %t4"},
                             {0xe7, 0xc2, "JP %c3, %W"},
                             {0xff, 0xc3, "JP %W"},
                             {0xe7, 0xc4, "CALL %c3, %W"},
                             {0xcf, 0xc5, "PUSH %t4"},
                             {0xff, 0xc6, "ADD A, %B"},
                             {0xc7, 0xc7, "RST %P"},
                             {0xff, 0xc9, "RET"},
                             {0xff, 0xcd, "CALL %W"},
                             {0xff, 0xce, "ADC %B"},
                             {0xff, 0xd6, "SUB %B"},
                             {0xff, 0xd9, "RETI"},
                             {0xff, 0xde, "SBC %B"},
                             {0xff, 0xe0, "LD (0xff%n), A"},
                             {0xff, 0xe2, "LD (0xff00h+C), A"},
                             {0xff, 0xe6, "AND %B"},
                             {0xff, 0xe8, "ADD SP,%d"},
                             {0xff, 0xe9, "LD PC,HL"},
                             {0xff, 0xea, "LD (%W),A"},
                             {0xff, 0xee, "XOR %B"},
                             {0xff, 0xf0, "LD A, (0xff%n)"},
                             {0xff, 0xf2, "LD A, (0xff00h+C)"},
                             {0xff, 0xf3, "DI"},
                             {0xff, 0xf6, "OR %B"},
                             {0xff, 0xf8, "LD HL, SP + %d"},
                             {0xff, 0xf9, "LD SP, HL"},
                             {0xff, 0xfa, "LD A, (%W)"},
                             {0xff, 0xfb, "EI"},
                             {0xff, 0xfe, "CP %B"},
                             {0x00, 0x00, "DB %B"}};

static GBOPCODE cbOpcodes[] = {
    {0xf8, 0x00, "RLC %r0"},     {0xf8, 0x08, "RRC %r0"},
    {0xf8, 0x10, "RL %r0"},      {0xf8, 0x18, "RR %r0"},
    {0xf8, 0x20, "SLA %r0"},     {0xf8, 0x28, "SRA %r0"},
    {0xf8, 0x30, "SWAP %r0"},    {0xf8, 0x38, "SRL %r0"},
    {0xc0, 0x40, "BIT %b, %r0"}, {0xc0, 0x80, "RES %b, %r0"},
    {0xc0, 0xc0, "SET %b, %r0"}, {0x00, 0x00, "DB CBh, %B"}};


int disassemble(u8 *data)
{
    u16 pc = 0;
    u8 opcode = data[pc++];
    GBOPCODE *op = NULL;
    const char *mnem = NULL;

    if (opcode == 0xcb) {
        /* extended instruction */
        op = cbOpcodes;
        opcode = data[pc++];
    } else {
        op = opcodes;
    }

    while ((opcode & op->mask) != op->value)
        op++;
    mnem = op->mnem;

    u8 temp1, temp2;
    s8 stemp;

    while (*mnem) {
        if (*mnem == '%') {
            mnem++;
            switch (*mnem) {
            case 'B': /* Single byte */
                temp1 = data[pc++];
                printf("0x%x", temp1);
                break;
            case 'W': /* Word (two bytes) */
                temp1 = data[pc++];
                temp2 = data[pc++];
                printf("0x%x", temp1 | (temp2 << 8));
                break;
            case 'd': /* Signed displacement (one byte) */
                stemp = data[pc++];
                printf("%d", stemp);
                break;
            case 'n': /* Single byte, no 0x prefix */
                temp1 = data[pc++];
                printf("%02x", temp1);
                break;
            case 'r': /* Register name */
                temp1 = *(++mnem) - '0';
                printf("%s", registers[(opcode >> temp1) & 7]);
                break;
            case 'R': /* 16 bit register name (double reg) */
                temp1 = *(++mnem) - '0';
                printf("%s", registers16[(opcode >> temp1) & 3]);
                break;
            case 't': /* 16 bit register name (double reg) for push/pop */
                temp1 = *(++mnem) - '0';
                printf("%s", registers16[4 + ((opcode >> temp1) & 3)]);
                break;
            case 'c': /* condition flag name */
                temp1 = *(++mnem) - '0';
                printf("%s", conditions[(opcode >> temp1) & 3]);
                break;
            case 'b': /* bit number of CB bit instruction */
                temp1 = (opcode >> 3) & 7;
                printf("%x", temp1);
                break;
            case 'P': /* RST address */
                temp1 = ((opcode >> 3) & 7) * 8;
                printf("0x%x", temp1);
                break;
            default:
                printf("%%%c", *mnem);
            }
        } else {
            putc(*mnem, stdout);
        }
        mnem++;
    }
    putc('\n', stdout);
    return pc;
}
