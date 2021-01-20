#ifndef TEST_CPU_H
#define TEST_CPU_H

uint8_t mymmu_read(uint16_t address);
void mymmu_write(uint16_t address, uint8_t data);
void cleanup();
void gbit_restore_flag(uint8_t gb_flag);

#endif
