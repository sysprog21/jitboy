#include <inttypes.h>
#include <sys/mman.h>

#include "core.h"
#include "interrupt.h"
#include "save.h"

void free_block(gb_block *block)
{
    munmap(block->mem, block->size);
}

bool init_vm(gb_vm *vm,
             const char *filename,
             int opt_level,
             int scale,
             bool init_io)
{
    if (!gb_memory_init(&vm->memory, filename))
        return false;

    dump_header_info(&vm->memory);

    vm->state.mem = &vm->memory;
    vm->state.a = 0x01;
    vm->state.b = 0x00;
    vm->state.c = 0x13;
    vm->state.d = 0x00;
    vm->state.e = 0xd8;
    vm->state.h = 0x01;
    vm->state.l = 0x4d;
    vm->state._sp = 0xfffe;
    vm->state.pc = 0x100;
    vm->state.f_subtract = false;

    vm->state.flags = 0x0;

    vm->state.inst_count = 0;
    vm->state.ly_count = 0;
    vm->state.tima_count = 0;
    vm->state.div_count = 0;
    vm->state.next_update = 0;

    vm->state.ime = true;
    vm->state.halt = false;

    vm->state.trap_reason = 0;

    vm->memory.mem[0xff05] = 0x00;
    vm->memory.mem[0xff06] = 0x00;
    vm->memory.mem[0xff07] = 0x00;
    vm->memory.mem[0xff10] = 0x80;
    vm->memory.mem[0xff11] = 0xBF;
    vm->memory.mem[0xff12] = 0xF3;
    vm->memory.mem[0xff14] = 0xBF;
    vm->memory.mem[0xff16] = 0x3F;
    vm->memory.mem[0xff17] = 0x00;
    vm->memory.mem[0xff19] = 0xBF;
    vm->memory.mem[0xff1a] = 0x7F;
    vm->memory.mem[0xff1b] = 0xFF;
    vm->memory.mem[0xff1c] = 0x9F;
    vm->memory.mem[0xff1e] = 0xbf;
    vm->memory.mem[0xff20] = 0xff;
    vm->memory.mem[0xff21] = 0x00;
    vm->memory.mem[0xff22] = 0x00;
    vm->memory.mem[0xff23] = 0xbf;
    vm->memory.mem[0xff24] = 0x77;
    vm->memory.mem[0xff25] = 0xf3;
    vm->memory.mem[0xff26] = 0xf1;
    vm->memory.mem[0xff40] = 0x91;
    vm->memory.mem[0xff42] = 0x00;
    vm->memory.mem[0xff43] = 0x00;
    vm->memory.mem[0xff45] = 0x00;
    vm->memory.mem[0xff47] = 0xfc;
    vm->memory.mem[0xff48] = 0xff;
    vm->memory.mem[0xff49] = 0xff;
    vm->memory.mem[0xff4a] = 0x00;
    vm->memory.mem[0xff4b] = 0x00;
    vm->memory.mem[0xffff] = 0x00;

    for (int block = 0; block < MAX_ROM_BANKS; ++block)
        for (int i = 0; i < 0x4000; ++i) {
            vm->compiled_blocks[block][i].exec_count = 0;
            vm->compiled_blocks[block][i].func = 0;
        }

    for (int i = 0; i < 0x80; ++i) {
        vm->highmem_blocks[i].exec_count = 0;
        vm->highmem_blocks[i].func = 0;
    }

    if (!read_battery(vm->memory.savname, &vm->memory))
        LOG_ERROR("Fail to read battery\n");

    if (init_io) {
        /* both audio and lcd will be initialized if init_io is true*/
        if (!init_window(&vm->lcd, scale))
            return false;

        vm->draw_frame = true;
        vm->next_frame_time = SDL_GetTicks();
        vm->time_busy = 0;
        vm->last_time = 0;
        vm->frame_cnt = 0;

        vm->opt_level = opt_level;

        audio_init(&vm->audio, &vm->memory);
    }

    return true;
}

bool run_vm(gb_vm *vm, bool turbo)
{
    uint16_t prev_pc = vm->state.last_pc;
    vm->state.last_pc = vm->state.pc;

    /* compile next block / get cached block */
    if (vm->state.pc < 0x4000) { /* first block */
        if (vm->compiled_blocks[0][vm->state.pc].exec_count == 0) {
            if (!compile(&vm->compiled_blocks[0][vm->state.pc], &vm->memory,
                         vm->state.pc, vm->opt_level))
                goto compile_error;
        }
        LOG_DEBUG("execute function @%#x (count %i)\n", vm->state.pc,
                  vm->compiled_blocks[0][vm->state.pc].exec_count);
        vm->compiled_blocks[0][vm->state.pc].exec_count++;
        vm->state.pc = vm->compiled_blocks[0][vm->state.pc].func(&vm->state);
        LOG_DEBUG("finished\n");
    } else if (vm->state.pc < 0x8000) { /* execute function in ROM */
        uint8_t bank = vm->memory.current_rom_bank;
        if (vm->compiled_blocks[bank][vm->state.pc - 0x4000].exec_count == 0) {
            if (!compile(&vm->compiled_blocks[bank][vm->state.pc - 0x4000],
                         &vm->memory, vm->state.pc, vm->opt_level))
                goto compile_error;
        }
        LOG_DEBUG("execute function @%#x (count %i)\n", vm->state.pc,
                  vm->compiled_blocks[bank][vm->state.pc - 0x4000].exec_count);
        vm->compiled_blocks[bank][vm->state.pc - 0x4000].exec_count++;
        vm->state.pc =
            vm->compiled_blocks[bank][vm->state.pc - 0x4000].func(&vm->state);
        LOG_DEBUG("finished\n");
    } else if (vm->state.pc >=
               0xff80) { /* execute function in internal RAM, e.g. for DMA */
        if (vm->highmem_blocks[vm->state.pc - 0xff80].exec_count == 0) {
            if (!compile(&vm->highmem_blocks[vm->state.pc - 0xff80],
                         &vm->memory, vm->state.pc, vm->opt_level))
                goto compile_error;
        }
        LOG_DEBUG("execute function @%#x (count %i)\n", vm->state.pc,
                  vm->highmem_blocks[vm->state.pc - 0xff80].exec_count);
        vm->highmem_blocks[vm->state.pc - 0xff80].exec_count++;
        vm->state.pc =
            vm->highmem_blocks[vm->state.pc - 0xff80].func(&vm->state);
        LOG_DEBUG("finished\n");
    } else { /* execute function in RAM */
        gb_block temp = {0};
        if (!compile(&temp, &vm->memory, vm->state.pc, vm->opt_level))
            goto compile_error;
        LOG_DEBUG("execute function in ram\n");
        vm->state.pc = temp.func(&vm->state);
        LOG_DEBUG("finished\n");
        free_block(&temp);
    }

    LOG_DEBUG("ioregs: STAT=%02x LY=%02x IF=%02x IE=%02x\n",
              vm->memory.mem[0xff41], vm->memory.mem[0xff44],
              vm->memory.mem[0xff0f], vm->memory.mem[0xffff]);
    LOG_DEBUG(

        "register: A=%02x, BC=%02x%02x, DE=%02x%02x, HL=%02x%02x, SP=%04x\n",
        vm->state.a, vm->state.b, vm->state.c, vm->state.d, vm->state.e,
        vm->state.h, vm->state.l, vm->state._sp);
    LOG_DEBUG("previous address: %#x\n", prev_pc);
    LOG_DEBUG("next address: %#x\n", vm->state.pc);

    LOG_DEBUG("pc = %i\n", vm->state.pc);

    do {
        if (vm->state.inst_count >= vm->state.next_update) {
            /* check interrupts */
            update_ioregs(&vm->state);

            if (vm->memory.mem[0xff44] == 144) {
                if (vm->draw_frame) {
                    unsigned time = SDL_GetTicks();
                    if (!turbo) {
                        if (!SDL_TICKS_PASSED(time, vm->next_frame_time)) {
                            SDL_Delay(vm->next_frame_time - time);
                        }
                        vm->next_frame_time += 17; /* 17ms until next frame */
                    }

                    vm->time_busy += time - vm->last_time;
                    vm->last_time = SDL_GetTicks();

                    if (++(vm->frame_cnt) == 60) {
                        vm->frame_cnt = 0;
                        float load = (vm->time_busy) / (60 * 17.0);
                        char title[15];
                        sprintf(title, "load: %.2f", load);
                        SDL_SetWindowTitle(vm->lcd.win, title);
                        vm->time_busy = 0;
                    }

                    SDL_CondBroadcast(vm->lcd.vblank_cond);
                    vm->draw_frame = false;
                }
            } else {
                vm->draw_frame = true;
            }

            uint16_t interrupt_addr = start_interrupt(&vm->state);
            if (interrupt_addr) {
                LOG_DEBUG("interrupt from %i to %i\n", vm->state.pc,
                          interrupt_addr);

                /* end halt mode */
                if (vm->state.halt == 1)
                    vm->state.halt = 0;

                /* save PC to stack */
                vm->state._sp -= 2;
                *(uint16_t *) (&vm->state.mem->mem[vm->state._sp]) =
                    vm->state.pc;
                // jump to interrupt address
                vm->state.pc = interrupt_addr;
            }

            if (vm->state.halt == WAIT_STAT3 &&
                (vm->memory.mem[0xff41] & 0x3) == 0x3) {
                vm->state.halt = 0; /* end wait for stat mode 3 */
            }

            if (vm->state.halt == WAIT_LY &&
                vm->memory.mem[0xff44] == vm->state.halt_arg) {
                vm->state.halt = 0; /* end wait for ly */
            }

            vm->state.next_update = next_update_time(&vm->state);
        }

        if (vm->state.halt != 0) {
            vm->state.inst_count = (vm->state.inst_count < vm->state.next_update
                                        ? vm->state.next_update
                                        : vm->state.inst_count + 16);
        }
    } while (vm->state.halt != 0);

    return true;

compile_error:
    LOG_ERROR("an error occurred while compiling the function @%#x.\n",
              vm->state.pc);

    LOG_ERROR("ioregs: STAT=%02x LY=%02x IF=%02x IE=%02x\n",
              vm->memory.mem[0xff41], vm->memory.mem[0xff44],
              vm->memory.mem[0xff0f], vm->memory.mem[0xffff]);
    LOG_ERROR(
        "register: A=%02x, BC=%02x%02x, DE=%02x%02x, HL=%02x%02x, SP=%04x\n",
        vm->state.a, vm->state.b, vm->state.c, vm->state.d, vm->state.e,
        vm->state.h, vm->state.l, vm->state._sp);
    LOG_ERROR("previous address: %#x\n", prev_pc);
    LOG_ERROR("next address: %#x\n", vm->state.pc);

    return false;
}

static void show_statistics(gb_vm *vm)
{
    printf("\nStatistics:\n");

    uint64_t compiled_functions = 0;
    uint64_t most_executed = 0;
    uint64_t most_executed_addr = 0;
    uint64_t total_executed = 0;

    for (int block = 0; block < MAX_ROM_BANKS; ++block)
        for (int i = 0; i < 0x4000; ++i) {
            if (vm->compiled_blocks[block][i].exec_count > 0) {
                ++compiled_functions;
                if (vm->compiled_blocks[block][i].exec_count > most_executed) {
                    most_executed_addr = block * 0x4000 + i;
                    most_executed = vm->compiled_blocks[block][i].exec_count;
                }
                total_executed += vm->compiled_blocks[block][i].exec_count;
            }
        }

    printf("- total compiled rom functions: %" PRIu64 "\n", compiled_functions);
    printf("- most frequent executed block @%" PRIx64 ", %" PRIu64
           " times executed\n",
           most_executed_addr, most_executed);
    int cnt = vm->compiled_blocks[0][0x40].exec_count
                  ? vm->compiled_blocks[0][0x40].exec_count
                  : 1;
    printf("- executed blocks total / per frame: %" PRIu64 " / %" PRIu64 "\n",
           total_executed, total_executed / cnt);
    printf("- frames: %u\n", vm->compiled_blocks[0][0x40].exec_count);
}

bool free_vm(gb_vm *vm)
{
    show_statistics(vm);

    for (int block = 0; block < MAX_ROM_BANKS; ++block)
        for (int i = 0; i < 0x4000; ++i)
            if (vm->compiled_blocks[block][i].exec_count > 0)
                free_block(&vm->compiled_blocks[block][i]);

    for (int i = 0; i < 0x80; ++i)
        if (vm->highmem_blocks[i].exec_count > 0)
            free_block(&vm->highmem_blocks[i]);

    /* destroy window */
    deinit_window(&vm->lcd);

    SDL_CloseAudioDevice(vm->audio.dev);

    return gb_memory_free(&vm->memory);
}
