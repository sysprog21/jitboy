#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"

bool read_battery(char *savfile, gb_memory *mem)
{
    if (!savfile)
        return false;

    FILE *fp = fopen(savfile, "rb");

    if (!fp) {
        LOG_ERROR("Fail to open file %s\n", savfile);
        return false;
    }

    /* get file size and allocate size accordingly */
    fseek(fp, 0, SEEK_END);
    size_t sz = ftell(fp) * sizeof(uint8_t);
    rewind(fp);

    size_t read_size = fread(mem->ram_banks, sizeof(uint8_t), sz, fp);
    fclose(fp);

    if (read_size != mem->max_ram_banks_num * 0x2000) {
        LOG_ERROR("Size mismatch between savfile and cartridge RAM\n");
        return false;
    }
    return true;
}

bool write_battery(char *savfile, gb_memory *mem)
{
    if (!savfile)
        return false;

    gb_memory_ram_flush(mem);

    FILE *fp = fopen(savfile, "wb");
    if (!fp) {
        LOG_ERROR("Failed to open file %s\n", savfile);
        return false;
    }

    size_t ramsize = mem->max_ram_banks_num * 0x2000;
    size_t write_size = fwrite(mem->ram_banks, sizeof(uint8_t), ramsize, fp);
    fclose(fp);

    if (write_size != ramsize) {
        LOG_ERROR("Size mismatch between savfile and cartridge RAM\n");
        return false;
    }

    return true;
}
