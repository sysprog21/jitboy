#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "core.h"
#include "save.h"

static void usage(const char *exe)
{
    printf(
        "Usage: %s [OPTIONS]\n"
        "Options:\n"
        "  -O, --opt-level=LEVEL   Set the optimization level (default: 0)\n"
        "  -s, --scale=SCALE       Set the scale of the window (default: 3)\n",
        exe);
}

static void banner()
{
#define ENDL "\n"
    printf(
        "                _n_________________" ENDL
        "                |_|_______________|_|" ENDL
        "                |  ,-------------.  |" ENDL
        "                | |  .---------.  | |" ENDL
        "                | |  |         |  | |" ENDL
        "                | |  |         |  | |" ENDL
        "                | |  |         |  | |" ENDL
        "                | |  |         |  | |" ENDL
        "                | |  `---------'  | |" ENDL
        "                | `---------------' |" ENDL
        "                |   _ GAME BOY      |" ENDL
        "   Up           | _| |_         ,-. | ----> Z" ENDL
        "Left/Right <--- ||_ O _|   ,-. \"._,\"|" ENDL
        "  Down          |  |_|    \"._,\"   A | ----> X" ENDL
        "                |    _  _    B      |" ENDL
        "                |   // //           |" ENDL
        "                |  // //    \\\\\\\\\\\\  | ----> Enter/BackSpace" ENDL
        "                |  `  `      \\\\\\\\\\\\ ," ENDL
        "                |________...______,\"" ENDL);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }

    int opt_level = 0;
    int scale = 3;
    int c;
    const struct option long_options[] = {
        {"opt-level", required_argument, NULL, 'O'},
        {"scale", required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}  // Terminating element
    };
    while ((c = getopt_long(argc, argv, "O:s:", long_options, NULL)) != -1) {
        switch (c) {
        case 'O':
            sscanf(optarg, "%i", &opt_level);
            break;
        case 's':
            sscanf(optarg, "%i", &scale);
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
    if (!init_vm(vm, argv[optind], opt_level, scale, true)) {
        LOG_ERROR("Fail to initialize\n");
        exit(1);
    }

    banner();
#ifdef DEBUG
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE);
#endif

    SDL_Event evt;

    /* start emulation */
    while (run_vm(vm)) {
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
            case SDL_KEYUP:
                switch (evt.key.keysym.scancode) {
                case SDL_SCANCODE_X:
                    vm->state.keys.state &= ~GB_KEY_A;
                    break;
                case SDL_SCANCODE_Z:
                    vm->state.keys.state &= ~GB_KEY_B;
                    break;
                case SDL_SCANCODE_UP:
                    vm->state.keys.state &= ~GB_KEY_UP;
                    break;
                case SDL_SCANCODE_DOWN:
                    vm->state.keys.state &= ~GB_KEY_DOWN;
                    break;
                case SDL_SCANCODE_LEFT:
                    vm->state.keys.state &= ~GB_KEY_LEFT;
                    break;
                case SDL_SCANCODE_RIGHT:
                    vm->state.keys.state &= ~GB_KEY_RIGHT;
                    break;
                case SDL_SCANCODE_RETURN: /* start button */
                    vm->state.keys.state &= ~GB_KEY_START;
                    break;
                case SDL_SCANCODE_BACKSPACE: /* select button */
                    vm->state.keys.state &= ~GB_KEY_SELECT;
                    break;
                default:
                    break;
                }
                break;
            case SDL_KEYDOWN:
                switch (evt.key.keysym.scancode) {
                case SDL_SCANCODE_X:
                    vm->state.keys.state |= GB_KEY_A;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_Z:
                    vm->state.keys.state |= GB_KEY_B;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_UP:
                    vm->state.keys.state |= GB_KEY_UP;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_DOWN:
                    vm->state.keys.state |= GB_KEY_DOWN;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_LEFT:
                    vm->state.keys.state |= GB_KEY_LEFT;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_RIGHT:
                    vm->state.keys.state |= GB_KEY_RIGHT;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_RETURN: /* start button */
                    if (evt.key.keysym.mod & KMOD_LALT ||
                        evt.key.keysym.mod & KMOD_RALT)
                        toggle_fullscreen(&vm->lcd);
                    else {
                        vm->state.keys.state |= GB_KEY_START;
                        vm->memory.mem[0xff0f] |= 0x10;
                    }
                    break;
                case SDL_SCANCODE_BACKSPACE: /* select button */
                    vm->state.keys.state |= GB_KEY_SELECT;
                    vm->memory.mem[0xff0f] |= 0x10;
                    break;
                case SDL_SCANCODE_ESCAPE:
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
    LOG_DEBUG("terminating ...\n");

    if (!write_battery(vm->memory.savname, &vm->memory)) {
        LOG_DEBUG("Failed to save battery\n");
    }

    free_vm(vm);
    free(vm);
    return 0;
}
