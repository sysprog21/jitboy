#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "core.h"

static void usage(const char *exe)
{
    printf("usage: %s [-O LEVEL] file.gb\n", exe);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    int opt_level = 0;
    int c;
    while ((c = getopt(argc, argv, "O:")) != -1) {
        switch (c) {
        case 'O':
            sscanf(optarg, "%i", &opt_level);
            break;
        case '?':
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (optind >= argc) {
        usage(argv[0]);
        return -1;
    }

    if (opt_level > 3)
        opt_level = 3;
    if (opt_level < 0)
        opt_level = 0;

    /* initialize memory */
    gb_vm *vm = malloc(sizeof(gb_vm));
    if (!init_vm(vm, argv[optind], opt_level, true)) {
        fprintf(stderr, "FATAL: fail to initialize\n");
        exit(1);
    }

#ifdef DEBUG
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
#endif

    SDL_Event evt;

    /* start emulation */
    while (run_vm(vm)) {
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
            case SDL_KEYUP:
                switch (evt.key.keysym.sym) {
                case SDLK_x:
                    vm->state.keys.state &= ~GB_KEY_A;
                    break;
                case SDLK_z:
                    vm->state.keys.state &= ~GB_KEY_B;
                    break;
                case SDLK_UP:
                    vm->state.keys.state &= ~GB_KEY_UP;
                    break;
                case SDLK_DOWN:
                    vm->state.keys.state &= ~GB_KEY_DOWN;
                    break;
                case SDLK_LEFT:
                    vm->state.keys.state &= ~GB_KEY_LEFT;
                    break;
                case SDLK_RIGHT:
                    vm->state.keys.state &= ~GB_KEY_RIGHT;
                    break;
                case SDLK_RETURN: /* start button */
                    vm->state.keys.state &= ~GB_KEY_START;
                    break;
                case SDLK_BACKSPACE: /* select button */
                    vm->state.keys.state &= ~GB_KEY_SELECT;
                    break;
                default:
                    break;
                }
                break;
            case SDL_KEYDOWN:
                switch (evt.key.keysym.sym) {
                case SDLK_x:
                    vm->state.keys.state |= GB_KEY_A;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_z:
                    vm->state.keys.state |= GB_KEY_B;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_UP:
                    vm->state.keys.state |= GB_KEY_UP;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_DOWN:
                    vm->state.keys.state |= GB_KEY_DOWN;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_LEFT:
                    vm->state.keys.state |= GB_KEY_LEFT;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_RIGHT:
                    vm->state.keys.state |= GB_KEY_RIGHT;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_RETURN: /* start button */
                    vm->state.keys.state |= GB_KEY_START;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_BACKSPACE: /* select button */
                    vm->state.keys.state |= GB_KEY_SELECT;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDLK_ESCAPE:
                    goto end_program;
                default:
                    break;
                }
                break;
            case SDL_QUIT:
                goto end_program;
            default:
                break;
            }
        }
    }

end_program:
    printf("terminating ...\n");

    free_vm(vm);
    free(vm);
    return 0;
}
