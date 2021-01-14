#ifndef JITBOY_SAV_H
#define JITBOY_SAV_H

/* Most Game Boy cartridges that allow games to be saved contain a small
 * internal battery to store the save states. We emulate how it works
 * on jitboy.
 */
bool read_battery(char *savfile, gb_memory *mem);
bool write_battery(char *savfile, gb_memory *mem);

#endif
