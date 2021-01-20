/*
 * Reference CPU implementation.
 *
 * Game Boy CPU emulator in C.
 * From https://github.com/koenk/gbc.git
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "ref_cpu.h"

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
            u8 pad1 : 1;
            u8 pad2 : 1;
            u8 pad3 : 1;
            u8 pad4 : 1;
            u8 CF : 1;
            u8 HF : 1;
            u8 NF : 1;
            u8 ZF : 1;
        } flags;
    };

    u16 sp;
    u16 pc;

    int halted;
    int interrupts_master_enabled;
};

static struct gb_state emu_state;

static size_t instruction_mem_size;
static u8 *instruction_mem;

static int num_mem_accesses;
static struct mem_access mem_accesses[16];

#define FLAG_C 0x10
#define FLAG_H 0x20
#define FLAG_N 0x40
#define FLAG_Z 0x80

static const u8 flagmasks[] = {FLAG_Z, FLAG_Z, FLAG_C, FLAG_C};

static int cycles_per_instruction[] = {
    /* 0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f       */
    4,  12, 8,  8,  4,  4,  8,  4,  20, 8,  8,  8, 4,  4,  8, 4,  /* 0 */
    4,  12, 8,  8,  4,  4,  8,  4,  12, 8,  8,  8, 4,  4,  8, 4,  /* 1 */
    8,  12, 8,  8,  4,  4,  8,  4,  8,  8,  8,  8, 4,  4,  8, 4,  /* 2 */
    8,  12, 8,  8,  12, 12, 12, 4,  8,  8,  8,  8, 4,  4,  8, 4,  /* 3 */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 4,  4,  8, 4,  /* 4 */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 4,  4,  8, 4,  /* 5 */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 4,  4,  8, 4,  /* 6 */
    8,  8,  8,  8,  8,  8,  4,  8,  4,  4,  4,  4, 4,  4,  8, 4,  /* 7 */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 4,  4,  8, 4,  /* 8 */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 4,  4,  8, 4,  /* 9 */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 4,  4,  8, 4,  /* a */
    4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4, 8,  4,  8, 4,  /* b */
    8,  12, 12, 16, 12, 16, 8,  16, 8,  16, 12, 0, 12, 24, 8, 16, /* c */
    8,  12, 12, 4,  12, 16, 8,  16, 8,  16, 12, 4, 12, 4,  8, 16, /* d */
    12, 12, 8,  4,  4,  16, 8,  16, 16, 4,  16, 4, 4,  4,  8, 16, /* e */
    12, 12, 8,  4,  4,  16, 8,  16, 12, 8,  16, 4, 0,  4,  8, 16, /* f */
};

static int cycles_per_instruction_cb[] = {
    /* 0   1   2   3   4   5   6   7   8   9   a   b   c   d   e   f       */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* 0 */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* 1 */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* 2 */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* 3 */
    8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8, /* 4 */
    8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8, /* 5 */
    8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8, /* 6 */
    8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8, /* 7 */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* 8 */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* 9 */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* a */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* b */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* c */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* d */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* e */
    8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8, /* f */
};

struct emu_luts {
    /* Lookup tables for the reg-index encoded in instructions to ptr to reg. */
    u8 *reg8_lut[9];
    u16 *reg16_lut[4];
    u16 *reg16s_lut[4];
};

static struct emu_luts luts;

static void cpu_init_luts(struct gb_state *s)
{
    luts.reg8_lut[0] = &s->reg8.B;
    luts.reg8_lut[1] = &s->reg8.C;
    luts.reg8_lut[2] = &s->reg8.D;
    luts.reg8_lut[3] = &s->reg8.E;
    luts.reg8_lut[4] = &s->reg8.H;
    luts.reg8_lut[5] = &s->reg8.L;
    luts.reg8_lut[6] = NULL;
    luts.reg8_lut[7] = &s->reg8.A;
    luts.reg16_lut[0] = &s->reg16.BC;
    luts.reg16_lut[1] = &s->reg16.DE;
    luts.reg16_lut[2] = &s->reg16.HL;
    luts.reg16_lut[3] = &s->sp;
    luts.reg16s_lut[0] = &s->reg16.BC;
    luts.reg16s_lut[1] = &s->reg16.DE;
    luts.reg16s_lut[2] = &s->reg16.HL;
    luts.reg16s_lut[3] = &s->reg16.AF;
}

/*
 * Mock MMU implementation that records memory writes, and reads from the mock
 * memory passed in during initialization.
 */
static void mmu_write(struct gb_state *s, u16 addr, u8 val)
{
    (void) s;
    struct mem_access *access = &mem_accesses[num_mem_accesses++];
    access->type = MEM_ACCESS_WRITE;
    access->addr = addr;
    access->val = val;
}
static u8 mmu_read(struct gb_state *s, u16 addr)
{
    (void) s;
    // struct mem_acccess *access = &mem_accesses[num_mem_accesses++];
    u8 ret;

    if (addr < instruction_mem_size)
        ret = instruction_mem[addr];
    else
        ret = 0xaa;

    /*
    access->type = MEM_ACCESS_READ;
    access->addr = addr;
    access->val = ret;
    */

    return ret;
}
static u16 mmu_read16(struct gb_state *s, u16 location)
{
    return mmu_read(s, location) | ((u16) mmu_read(s, location + 1) << 8);
}
static void mmu_write16(struct gb_state *s, u16 location, u16 value)
{
    mmu_write(s, location, value & 0xff);
    mmu_write(s, location + 1, value >> 8);
}
static u16 mmu_pop16(struct gb_state *s)
{
    u16 val = mmu_read16(s, s->sp);
    s->sp += 2;
    return val;
}
void mmu_push16(struct gb_state *s, u16 value)
{
    s->sp -= 2;
    mmu_write16(s, s->sp, value);
}


#define CF s->flags.CF
#define HF s->flags.HF
#define NF s->flags.NF
#define ZF s->flags.ZF
#define A s->reg8.A
#define F s->reg8.F
#define B s->reg8.B
#define C s->reg8.C
#define D s->reg8.D
#define E s->reg8.E
#define H s->reg8.H
#define L s->reg8.L
#define AF s->reg16.AF
#define BC s->reg16.BC
#define DE s->reg16.DE
#define HL s->reg16.HL
#define M(op, value, mask) (((op) & (mask)) == (value))
#define mem(loc) (mmu_read(s, loc))
#define IMM8 (mmu_read(s, s->pc))
#define IMM16 (mmu_read(s, s->pc) | (mmu_read(s, s->pc + 1) << 8))
#define REG8(bitpos) luts.reg8_lut[(op >> bitpos) & 7]
#define REG16(bitpos) luts.reg16_lut[((op >> bitpos) & 3)]
#define REG16S(bitpos) luts.reg16s_lut[((op >> bitpos) & 3)]
#define FLAG(bitpos) ((op >> bitpos) & 3)

static int cpu_do_cb_instruction(struct gb_state *s)
{
    u8 op = mmu_read(s, s->pc++);

    if (M(op, 0x00, 0xf8)) { /* RLC reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        u8 res = (val << 1) | (val >> 7);
        ZF = res == 0;
        NF = 0;
        HF = 0;
        CF = val >> 7;
        if (reg)
            *reg = res;
        else
            mmu_write(s, HL, res);
    } else if (M(op, 0x08, 0xf8)) { /* RRC reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        u8 res = (val >> 1) | ((val & 1) << 7);
        ZF = res == 0;
        NF = 0;
        HF = 0;
        CF = val & 1;
        if (reg)
            *reg = res;
        else
            mmu_write(s, HL, res);
    } else if (M(op, 0x10, 0xf8)) { /* RL reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        u8 res = (val << 1) | (CF ? 1 : 0);
        ZF = res == 0;
        NF = 0;
        HF = 0;
        CF = val >> 7;
        if (reg)
            *reg = res;
        else
            mmu_write(s, HL, res);
    } else if (M(op, 0x18, 0xf8)) { /* RR reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        u8 res = (val >> 1) | (CF << 7);
        ZF = res == 0;
        NF = 0;
        HF = 0;
        CF = val & 0x1;
        if (reg)
            *reg = res;
        else
            mmu_write(s, HL, res);
    } else if (M(op, 0x20, 0xf8)) { /* SLA reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        CF = val >> 7;
        val = val << 1;
        ZF = val == 0;
        NF = 0;
        HF = 0;
        if (reg)
            *reg = val;
        else
            mmu_write(s, HL, val);
    } else if (M(op, 0x28, 0xf8)) { /* SRA reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        CF = val & 0x1;
        val = (val >> 1) | (val & (1 << 7));
        ZF = val == 0;
        NF = 0;
        HF = 0;
        if (reg)
            *reg = val;
        else
            mmu_write(s, HL, val);
    } else if (M(op, 0x30, 0xf8)) { /* SWAP reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        u8 res = ((val << 4) & 0xf0) | ((val >> 4) & 0xf);
        F = res == 0 ? FLAG_Z : 0;
        if (reg)
            *reg = res;
        else
            mmu_write(s, HL, res);
    } else if (M(op, 0x38, 0xf8)) { /* SRL reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        CF = val & 0x1;
        val = val >> 1;
        ZF = val == 0;
        NF = 0;
        HF = 0;
        if (reg)
            *reg = val;
        else
            mmu_write(s, HL, val);
    } else if (M(op, 0x40, 0xc0)) { /* BIT bit, reg8 */
        u8 bit = (op >> 3) & 7;
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        ZF = ((val >> bit) & 1) == 0;
        NF = 0;
        HF = 1;
    } else if (M(op, 0x80, 0xc0)) { /* RES bit, reg8 */
        u8 bit = (op >> 3) & 7;
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        val = val & ~(1 << bit);
        if (reg)
            *reg = val;
        else
            mmu_write(s, HL, val);
    } else if (M(op, 0xc0, 0xc0)) { /* SET bit, reg8 */
        u8 bit = (op >> 3) & 7;
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        val |= (1 << bit);
        if (reg)
            *reg = val;
        else
            mmu_write(s, HL, val);
    } else {
        s->pc -= 2;
        printf("Unknown opcode %02x", op);
        return -1;
    }
    return 0;
}

static int cpu_do_instruction(struct gb_state *s)
{
    u8 op = mmu_read(s, s->pc++);
    if (M(op, 0x00, 0xff)) {        /* NOP */
    } else if (M(op, 0x01, 0xcf)) { /* LD reg16, u16 */
        u16 *dst = REG16(4);
        *dst = IMM16;
        s->pc += 2;
    } else if (M(op, 0x02, 0xff)) { /* LD (BC), A */
        mmu_write(s, BC, A);
    } else if (M(op, 0x03, 0xcf)) { /* INC reg16 */
        u16 *reg = REG16(4);
        *reg += 1;
    } else if (M(op, 0x04, 0xc7)) { /* INC reg8 */
        u8 *reg = REG8(3);
        u8 val = reg ? *reg : mem(HL);
        u8 res = val + 1;
        ZF = res == 0;
        NF = 0;
        HF = (val & 0xf) == 0xf;
        if (reg)
            *reg = res;
        else
            mmu_write(s, HL, res);
    } else if (M(op, 0x05, 0xc7)) { /* DEC reg8 */
        u8 *reg = REG8(3);
        u8 val = reg ? *reg : mem(HL);
        val--;
        NF = 1;
        ZF = val == 0;
        HF = (val & 0x0F) == 0x0F;
        if (reg)
            *reg = val;
        else
            mmu_write(s, HL, val);
    } else if (M(op, 0x06, 0xc7)) { /* LD reg8, imm8 */
        u8 *dst = REG8(3);
        u8 src = IMM8;
        s->pc++;
        if (dst)
            *dst = src;
        else
            mmu_write(s, HL, src);
    } else if (M(op, 0x07, 0xff)) { /* RLCA */
        u8 res = (A << 1) | (A >> 7);
        F = (A >> 7) ? FLAG_C : 0;
        A = res;

    } else if (M(op, 0x08, 0xff)) { /* LD (imm16), SP */
        mmu_write16(s, IMM16, s->sp);
        s->pc += 2;

    } else if (M(op, 0x09, 0xcf)) { /* ADD HL, reg16 */
        u16 *src = REG16(4);
        u32 tmp = HL + *src;
        NF = 0;
        HF = (((HL & 0xfff) + (*src & 0xfff)) & 0x1000) ? 1 : 0;
        CF = tmp > 0xffff;
        HL = tmp;
    } else if (M(op, 0x0a, 0xff)) { /* LD A, (BC) */
        A = mem(BC);
    } else if (M(op, 0x0b, 0xcf)) { /* DEC reg16 */
        u16 *reg = REG16(4);
        *reg -= 1;
    } else if (M(op, 0x0f, 0xff)) { /* RRCA */
        F = (A & 1) ? FLAG_C : 0;
        A = (A >> 1) | ((A & 1) << 7);
    } else if (M(op, 0x10, 0xff)) { /* STOP */
        s->halted = 1;
    } else if (M(op, 0x12, 0xff)) { /* LD (DE), A */
        mmu_write(s, DE, A);
    } else if (M(op, 0x17, 0xff)) { /* RLA */
        u8 res = A << 1 | (CF ? 1 : 0);
        F = (A & (1 << 7)) ? FLAG_C : 0;
        A = res;
    } else if (M(op, 0x18, 0xff)) { /* JR off8 */
        s->pc += (s8) IMM8 + 1;
    } else if (M(op, 0x1a, 0xff)) { /* LD A, (DE) */
        A = mem(DE);
    } else if (M(op, 0x1f, 0xff)) { /* RRA */
        u8 res = (A >> 1) | (CF << 7);
        ZF = 0;
        NF = 0;
        HF = 0;
        CF = A & 0x1;
        A = res;
    } else if (M(op, 0x20, 0xe7)) { /* JR cond, off8 */
        u8 flag = (op >> 3) & 3;
        if (((F & flagmasks[flag]) ? 1 : 0) == (flag & 1))
            s->pc += (s8) IMM8;
        s->pc++;
    } else if (M(op, 0x22, 0xff)) { /* LDI (HL), A */
        mmu_write(s, HL, A);
        HL++;
    } else if (M(op, 0x27, 0xff)) { /* DAA */
        /* When adding/subtracting two numbers in BCD form, this instructions
         * brings the results back to BCD form too. In BCD form the decimals 0-9
         * are encoded in a fixed number of bits (4). E.g., 0x93 actually means
         * 93 decimal. Adding/subtracting such numbers takes them out of this
         * form since they can results in values where each digit is >9.
         * E.g., 0x9 + 0x1 = 0xA, but should be 0x10. The important thing to
         * note here is that per 4 bits we 'skip' 6 values (0xA-0xF), and thus
         * by adding 0x6 we get: 0xA + 0x6 = 0x10, the correct answer. The same
         * works for the upper byte (add 0x60).
         * So: If the lower byte is >9, we need to add 0x6.
         * If the upper byte is >9, we need to add 0x60.
         * Furthermore, if we carried the lower part (HF, 0x9+0x9=0x12) we
         * should also add 0x6 (0x12+0x6=0x18).
         * Similarly for the upper byte (CF, 0x90+0x90=0x120, +0x60=0x180).
         *
         * For subtractions (we know it was a subtraction by looking at the NF
         * flag) we simiarly need to *subtract* 0x06/0x60/0x66 to again skip the
         * unused 6 values in each byte. The GB does this by only looking at the
         * NF and CF flags then.
         */
        s8 add = 0;
        if ((!NF && (A & 0xf) > 0x9) || HF)
            add |= 0x6;
        if ((!NF && A > 0x99) || CF) {
            add |= 0x60;
            CF = 1;
        }
        A += NF ? -add : add;
        ZF = A == 0;
        HF = 0;
    } else if (M(op, 0x2a, 0xff)) { /* LDI A, (HL) */
        A = mmu_read(s, HL);
        HL++;
    } else if (M(op, 0x2f, 0xff)) { /* CPL */
        A = ~A;
        NF = 1;
        HF = 1;
    } else if (M(op, 0x32, 0xff)) { /* LDD (HL), A */
        mmu_write(s, HL, A);
        HL--;
    } else if (M(op, 0x37, 0xff)) { /* SCF */
        NF = 0;
        HF = 0;
        CF = 1;
    } else if (M(op, 0x3a, 0xff)) { /* LDD A, (HL) */
        A = mmu_read(s, HL);
        HL--;
    } else if (M(op, 0x3f, 0xff)) { /* CCF */
        CF = CF ? 0 : 1;
        NF = 0;
        HF = 0;
    } else if (M(op, 0x76, 0xff)) { /* HALT */
        s->halted = 1;
    } else if (M(op, 0x40, 0xc0)) { /* LD reg8, reg8 */
        u8 *src = REG8(0);
        u8 *dst = REG8(3);
        u8 srcval = src ? *src : mem(HL);
        if (dst)
            *dst = srcval;
        else
            mmu_write(s, HL, srcval);
    } else if (M(op, 0x80, 0xf8)) { /* ADD A, reg8 */
        u8 *src = REG8(0);
        u8 srcval = src ? *src : mem(HL);
        u16 res = A + srcval;
        ZF = (u8) res == 0;
        NF = 0;
        HF = (A ^ srcval ^ res) & 0x10 ? 1 : 0;
        CF = res & 0x100 ? 1 : 0;
        A = (u8) res;
    } else if (M(op, 0x88, 0xf8)) { /* ADC A, reg8 */
        u8 *src = REG8(0);
        u8 srcval = src ? *src : mem(HL);
        u16 res = A + srcval + CF;
        ZF = (u8) res == 0;
        NF = 0;
        HF = (A ^ srcval ^ res) & 0x10 ? 1 : 0;
        CF = res & 0x100 ? 1 : 0;
        A = (u8) res;

    } else if (M(op, 0x90, 0xf8)) { /* SUB reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        u8 res = A - val;
        ZF = res == 0;
        NF = 1;
        HF = ((s32) A & 0xf) - (val & 0xf) < 0;
        CF = A < val;
        A = res;
    } else if (M(op, 0x98, 0xf8)) { /* SBC A, reg8 */
        u8 *reg = REG8(0);
        u8 regval = reg ? *reg : mem(HL);
        u8 res = A - regval - CF;
        ZF = res == 0;
        NF = 1;
        HF = ((s32) A & 0xf) - (regval & 0xf) - CF < 0;
        CF = A < regval + CF;
        A = res;
    } else if (M(op, 0xa0, 0xf8)) { /* AND reg8 */
        u8 *reg = REG8(0);
        u8 val = reg ? *reg : mem(HL);
        A = A & val;
        ZF = A == 0;
        NF = 0;
        HF = 1;
        CF = 0;
    } else if (M(op, 0xa8, 0xf8)) { /* XOR reg8 */
        u8 *src = REG8(0);
        u8 srcval = src ? *src : mem(HL);
        A ^= srcval;
        F = A ? 0 : FLAG_Z;
    } else if (M(op, 0xb0, 0xf8)) { /* OR reg8 */
        u8 *src = REG8(0);
        u8 srcval = src ? *src : mem(HL);
        A |= srcval;
        F = A ? 0 : FLAG_Z;
    } else if (M(op, 0xb8, 0xf8)) { /* CP reg8 */
        u8 *reg = REG8(0);
        u8 regval = reg ? *reg : mem(HL);
        ZF = A == regval;
        NF = 1;
        HF = (A & 0xf) < (regval & 0xf);
        CF = A < regval;
    } else if (M(op, 0xc0, 0xe7)) { /* RET cond */
        /* TODO cyclecount depends on taken or not */

        u8 flag = (op >> 3) & 3;
        if (((F & flagmasks[flag]) ? 1 : 0) == (flag & 1))
            s->pc = mmu_pop16(s);

    } else if (M(op, 0xc1, 0xcf)) { /* POP reg16 */
        u16 *dst = REG16S(4);
        *dst = mmu_pop16(s);
        F = F & 0xf0;
    } else if (M(op, 0xc2, 0xe7)) { /* JP cond, imm16 */
        u8 flag = (op >> 3) & 3;
        if (((F & flagmasks[flag]) ? 1 : 0) == (flag & 1))
            s->pc = IMM16;
        else
            s->pc += 2;
    } else if (M(op, 0xc3, 0xff)) { /* JP imm16 */
        s->pc = IMM16;
    } else if (M(op, 0xc4, 0xe7)) { /* CALL cond, imm16 */
        u16 dst = IMM16;
        s->pc += 2;
        u8 flag = (op >> 3) & 3;
        if (((F & flagmasks[flag]) ? 1 : 0) == (flag & 1)) {
            mmu_push16(s, s->pc);
            s->pc = dst;
        }
    } else if (M(op, 0xc5, 0xcf)) { /* PUSH reg16 */
        u16 *src = REG16S(4);
        mmu_push16(s, *src);
    } else if (M(op, 0xc6, 0xff)) { /* ADD A, imm8 */
        u16 res = A + IMM8;
        ZF = (u8) res == 0;
        NF = 0;
        HF = (A ^ IMM8 ^ res) & 0x10 ? 1 : 0;
        CF = res & 0x100 ? 1 : 0;
        A = (u8) res;
        s->pc++;
    } else if (M(op, 0xc7, 0xc7)) { /* RST imm8 */
        mmu_push16(s, s->pc);
        s->pc = ((op >> 3) & 7) * 8;
    } else if (M(op, 0xc9, 0xff)) { /* RET */
        s->pc = mmu_pop16(s);
    } else if (M(op, 0xcd, 0xff)) { /* CALL imm16 */
        u16 dst = IMM16;
        mmu_push16(s, s->pc + 2);
        s->pc = dst;
    } else if (M(op, 0xce, 0xff)) { /* ADC imm8 */
        u16 res = A + IMM8 + CF;
        ZF = (u8) res == 0;
        NF = 0;
        HF = (A ^ IMM8 ^ res) & 0x10 ? 1 : 0;
        CF = res & 0x100 ? 1 : 0;
        A = (u8) res;
        s->pc++;
    } else if (M(op, 0xd6, 0xff)) { /* SUB imm8 */
        u8 res = A - IMM8;
        ZF = res == 0;
        NF = 1;
        HF = ((s32) A & 0xf) - (IMM8 & 0xf) < 0;
        CF = A < IMM8;
        A = res;
        s->pc++;
    } else if (M(op, 0xd9, 0xff)) { /* RETI */
        s->pc = mmu_pop16(s);
        s->interrupts_master_enabled = 1;
    } else if (M(op, 0xde, 0xff)) { /* SBC imm8 */
        u8 res = A - IMM8 - CF;
        ZF = res == 0;
        NF = 1;
        HF = ((s32) A & 0xf) - (IMM8 & 0xf) - CF < 0;
        CF = A < IMM8 + CF;
        A = res;
        s->pc++;
    } else if (M(op, 0xe0, 0xff)) { /* LD (0xff00 + imm8), A */
        mmu_write(s, 0xff00 + IMM8, A);
        s->pc++;
    } else if (M(op, 0xe2, 0xff)) { /* LD (0xff00 + C), A */
        mmu_write(s, 0xff00 + C, A);
    } else if (M(op, 0xe6, 0xff)) { /* AND imm8 */
        A = A & IMM8;
        s->pc++;
        ZF = A == 0;
        NF = 0;
        HF = 1;
        CF = 0;
    } else if (M(op, 0xe8, 0xff)) { /* ADD SP, imm8s */
        s8 off = (s8) IMM8;
        u32 res = s->sp + off;
        ZF = 0;
        NF = 0;
        HF = (s->sp & 0xf) + (IMM8 & 0xf) > 0xf;
        CF = (s->sp & 0xff) + (IMM8 & 0xff) > 0xff;
        s->sp = res;
        s->pc++;
    } else if (M(op, 0xe9, 0xff)) { /* LD PC, HL (or JP (HL) ) */
        s->pc = HL;
    } else if (M(op, 0xea, 0xff)) { /* LD (imm16), A */
        mmu_write(s, IMM16, A);
        s->pc += 2;
    } else if (M(op, 0xcb, 0xff)) { /* CB-prefixed extended instructions */
        return cpu_do_cb_instruction(s);
    } else if (M(op, 0xee, 0xff)) { /* XOR imm8 */
        A ^= IMM8;
        s->pc++;
        F = A ? 0 : FLAG_Z;
    } else if (M(op, 0xf0, 0xff)) { /* LD A, (0xff00 + imm8) */
        A = mmu_read(s, 0xff00 + IMM8);
        s->pc++;
    } else if (M(op, 0xf2, 0xff)) { /* LD A, (0xff00 + C) */
        A = mmu_read(s, 0xff00 + C);
    } else if (M(op, 0xf3, 0xff)) { /* DI */
        s->interrupts_master_enabled = 0;
    } else if (M(op, 0xf6, 0xff)) { /* OR imm8 */
        A |= IMM8;
        F = A ? 0 : FLAG_Z;
        s->pc++;
    } else if (M(op, 0xf8, 0xff)) { /* LD HL, SP + imm8 */
        u32 res = (u32) s->sp + (s8) IMM8;
        ZF = 0;
        NF = 0;
        HF = (s->sp & 0xf) + (IMM8 & 0xf) > 0xf;
        CF = (s->sp & 0xff) + (IMM8 & 0xff) > 0xff;
        HL = (u16) res;
        s->pc++;
    } else if (M(op, 0xf9, 0xff)) { /* LD SP, HL */
        s->sp = HL;
    } else if (M(op, 0xfa, 0xff)) { /* LD A, (imm16) */
        A = mmu_read(s, IMM16);
        s->pc += 2;
    } else if (M(op, 0xfb, 0xff)) { /* EI */
        s->interrupts_master_enabled = 1;
    } else if (M(op, 0xfe, 0xff)) { /* CP imm8 */
        u8 n = IMM8;
        ZF = A == n;
        NF = 1;
        HF = (A & 0xf) < (n & 0xf);
        CF = A < n;
        s->pc++;
    } else {
        s->pc--;
        printf("Unknown opcode %02x", op);
        return -1;
    }
    return 0;
}

#undef AF
#undef BC
#undef DE
#undef HL

/*
 * Resets the CPU state (registers and such) to the state at bootup.
 */
void rcpu_reset(struct state *state)
{
    emu_state.halted = 0;
    emu_state.interrupts_master_enabled = state->interrupts_master_enabled;
    emu_state.pc = state->PC;
    emu_state.sp = state->SP;
    emu_state.reg16.AF = state->reg16.AF;
    emu_state.reg16.BC = state->reg16.BC;
    emu_state.reg16.DE = state->reg16.DE;
    emu_state.reg16.HL = state->reg16.HL;

    num_mem_accesses = 0;
}

/*
 * Query the current state of the CPU.
 */
void rcpu_get_state(struct state *state)
{
    state->PC = emu_state.pc;
    state->SP = emu_state.sp;
    state->reg16.AF = emu_state.reg16.AF;
    state->reg16.BC = emu_state.reg16.BC;
    state->reg16.DE = emu_state.reg16.DE;
    state->reg16.HL = emu_state.reg16.HL;
    state->halted = emu_state.halted;
    state->interrupts_master_enabled = emu_state.interrupts_master_enabled;
    state->num_mem_accesses = num_mem_accesses;
    memcpy(state->mem_accesses, mem_accesses, sizeof(mem_accesses));
}

/*
 * Step a single instruction of the CPU. Returns the amount of cycles this took
 * (e.g., NOP should return 4.
 */
int rcpu_step(void)
{
    struct gb_state *s = &emu_state;
    u8 op;
    int cycles = 0;

    op = mmu_read(s, s->pc);
    cycles = cycles_per_instruction[op];
    if (op == 0xcb) {
        op = mmu_read(s, s->pc + 1);
        cycles = cycles_per_instruction_cb[op];
    }

    if (cpu_do_instruction(s))
        return -1;
    return cycles;
}

/*
 * Called once during startup. The area of memory pointed to by
 * tester_instruction_mem will contain instructions the tester will inject, and
 * should be mapped read-only at addresses [0,tester_instruction_mem_size).
 */
void rcpu_init(size_t tester_instruction_mem_size, u8 *tester_instruction_mem)
{
    cpu_init_luts(&emu_state);
    instruction_mem_size = tester_instruction_mem_size;
    instruction_mem = tester_instruction_mem;
}
