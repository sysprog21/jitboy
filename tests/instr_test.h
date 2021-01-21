#ifndef TEST_CPU_H
#define TEST_CPU_H

void gbz80_mmu_write(uint16_t address, uint8_t data);
void gbz80_restore_flag(uint8_t gb_flag);

#endif
