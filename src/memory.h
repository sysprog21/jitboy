#ifndef JITBOY_MEMORY_H
#define JITBOY_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *mem;
    uint8_t *ram_banks;
    const char *filename;
    char *savname;
    uint8_t max_ram_banks_num;
    int fd;
    enum {
        MBC_NONE = 0x00,
        MBC1 = 0x01,
        MBC1_RAM_BAT = 0x03,
        MBC2 = 0x05,
        MBC2_BAT = 0x06,
        MBC3_TIMER_RAM_BAT = 0x10,
        MBC3 = 0x11,
        MBC3_RAM_BAT = 0x13,
        MBC5 = 0x19,
        MBC5_RAM_BAT = 0x1b
    } mbc;
    uint8_t mbc_mode, mbc_data;
    uint8_t current_rom_bank, current_ram_bank;
    bool rtc_access;
} gb_memory;

typedef struct {
    enum {
        GB_KEY_RIGHT = 0x01,
        GB_KEY_LEFT = 0x02,
        GB_KEY_UP = 0x04,
        GB_KEY_DOWN = 0x08,
        GB_KEY_A = 0x10,
        GB_KEY_B = 0x20,
        GB_KEY_SELECT = 0x40,
        GB_KEY_START = 0x80
    } state;
} gb_keys;

typedef struct {
    // memory
    gb_memory *mem;

    // register
    uint8_t a, b, c, d, e, h, l;
    bool f_subtract;
    uint16_t _sp;
    uint16_t pc;
    uint64_t flags;

    uint16_t last_pc;

    // keys
    gb_keys keys;

    // instruction count
    uint64_t inst_count;

    uint64_t ly_count;
    uint64_t tima_count;
    uint64_t div_count;

    uint64_t next_update;

    // interrupt timers etc
    bool ime;

    // cpu is in halt state
    uint32_t halt;
    uint8_t halt_arg;

    // flag to trace callstack
    enum {
        REASON_OTHER = 0,
        REASON_CALL = 1,
        REASON_RST = 2,
        REASON_INT = 4,
        REASON_RET = 8
    } trap_reason;
} gb_state;

/* flush back external RAM(0xa000 - 0xbfff) to ram_banks buffer */
void gb_memory_ram_flush(gb_memory *mem);

#ifdef GBIT
/* tell gbit if memory write happened by jit code */
void gb_memory_ld16(gb_state *state, uint64_t addr, uint64_t value);
#endif

/* emulate write through mbc */
void gb_memory_write(gb_state *state, uint64_t addr, uint64_t value);

/* initialize memory layout and map file filename */
bool gb_memory_init(gb_memory *mem, const char *filename);

/* Release memory */
bool gb_memory_free(gb_memory *mem);

/* dump ROM header information */
void dump_header_info(gb_memory *mem);

#endif
