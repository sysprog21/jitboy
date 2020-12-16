#include "lcd.h"

struct __attribute__((__packed__)) OAMentry {
    uint8_t y;
    uint8_t x;
    uint8_t tile;
    uint8_t flags;
};

static void render_back(uint32_t *buf, uint8_t *addr_sp)
{
    uint32_t pal_grey[] = {0xffffff, 0xaaaaaa, 0x555555, 0x000000};

    /* point to tile map */
    uint8_t *ptr_map = addr_sp;
    if (addr_sp[0xff40] & 0x8)
        ptr_map += 0x9c00;
    else
        ptr_map += 0x9800;

    /* Current line + SCROLL Y */
    uint8_t y = addr_sp[0xff44] + addr_sp[0xff42];
    /* SCROLL X */
    int j = addr_sp[0xff43];
    uint8_t x1 = j >> 3;

    /* Advance to row in tile map */
    ptr_map += ((y >> 3) << 5) & 0x3ff;

    int i = addr_sp[0xff44] * 160;  // 0;
    j &= 7;
    uint8_t x = 8 - j;
    uint8_t shift_factor = ((uint8_t)(~j)) % 8;
    for (; x < 168; x += 8) {
        uint16_t tile_num = ptr_map[x1++ & 0x1f];
        if (!(addr_sp[0xff40] & 0x10))
            tile_num = 256 + (signed char) tile_num;
        /* point to tile.
         * Each tile is 8 * 8 * 2 = 128 bits = 16 bytes
         */
        uint8_t *ptr_data = addr_sp + 0x8000 + (tile_num << 4);
        /* point to row in tile depending on LY and SCROLL Y.
         * Each row is 8 * 2 = 16 bits = 2 bytes
         */
        ptr_data += (y & 7) << 1;
        for (; j < 8 && (x + j) < 168; shift_factor--, j++) {
            uint8_t indx = ((ptr_data[0] >> shift_factor) & 1) |
                           ((((ptr_data[1] >> shift_factor)) & 1) << 1);
            /* if bit 0 in LCDC is not set, screen is blank */
            buf[i] = (addr_sp[0xff40] & 0x01)
                         ? pal_grey[(addr_sp[0xff47] >> (indx << 1)) & 3]
                         : (unsigned) -1;
            i++;
        }
        j = 0;
        shift_factor = 7;
    }

    if (addr_sp[0xff40] & 0x20) {
        uint8_t wx = addr_sp[0xff4b] - 7;
        uint8_t wy = addr_sp[0xff4a];

        y = addr_sp[0xff44];  // current line to update
        uint8_t *tile_map_ptr = addr_sp +
                                ((addr_sp[0xff40] & 0x40) ? 0x9c00 : 0x9800) +
                                (y - wy) / 8 * 32;
        uint8_t *tile_data_ptr =
            addr_sp + ((addr_sp[0xff40] & 0x10) ? 0x8000 : 0x9000);
        i = y * 160;

        for (x = 0; x < 160; ++x) {
            if (x < wx || y < wy)
                continue;

            uint8_t *tile = tile_data_ptr +
                            16 * ((addr_sp[0xff40] & 0x10)
                                      ? tile_map_ptr[(x - wx) / 8]
                                      : (int8_t) tile_map_ptr[(x - wx) / 8]);
            tile += (y - wy) % 8 * 2;

            int col = ((*tile >> (7 - (x - wx) % 8)) & 1) +
                      (((*(tile + 1) >> (7 - (x - wx) % 8)) & 1) << 1);

            buf[i + x] = pal_grey[(addr_sp[0xff47] >> (col << 1)) & 3];
        }
    }

    /* TODO: prioritize sprite */
    if ((addr_sp[0xff40] & 0x02) == 0)
        return;

    struct OAMentry *objs[10];
    int num_objs = 0;
    uint8_t obj_tile_height = addr_sp[0xff40] & 0x04 ? 16 : 8;
    y = addr_sp[0xff44];

    for (int i = 0; i < 40; i++) {
        struct OAMentry *obj = (struct OAMentry *) (addr_sp + 0xfe00 + 4 * i);

        if ((obj->y > 0) && (obj->y < 160)) {
            if ((y >= obj->y - 16) && (y < obj->y - 16 + obj_tile_height)) {
                uint8_t pos = num_objs;

                /* Sprites are ordered in array from low priority to high
                 * priority. So the low priority sprite will be drawn first,
                 * meaning that the high priority one may overlap it.
                 *
                 * Priority of sprites follow the rule:
                 * The smaller the X coordinate, the higher the priority. For
                 * two objects with same X coordinate, the one with lower OAM
                 * address has higher priority.
                 */
                while (pos > 0 && objs[pos - 1]->x <= obj->x) {
                    if (pos < 10) {
                        objs[pos] = objs[pos - 1];
                    }
                    pos--;
                }

                objs[pos] = obj;

                num_objs++;
            }
        }

        if (num_objs >= 10)
            break;
    }

    for (int sprite = 0; sprite < num_objs; ++sprite) {
        int sposy = objs[sprite]->y - 16;
        int sposx = objs[sprite]->x - 8;

        uint8_t flags = objs[sprite]->flags;
        uint8_t tile_idx = (obj_tile_height == 16)
                               ? ((flags & 0x40) ? objs[sprite]->tile | 0x01
                                                 : objs[sprite]->tile & ~0x01)
                               : objs[sprite]->tile;
        uint8_t obp = ((flags & 0x10) ? addr_sp[0xff49] : addr_sp[0xff48]);

        if (sposy > y - obj_tile_height && sposy <= y) {
            /* sprite is displayed in a line */
            for (x = 0; x < 8; ++x) {
                int px_x = ((flags & 0x20) ? 7 - x : x) + sposx;
                int px_y = ((flags & 0x40) ? obj_tile_height - 1 - y + sposy
                                           : y - sposy);

                int col =
                    ((addr_sp[0x8000 + 16 * tile_idx + 2 * px_y] >> (7 - x)) &
                     1) +
                    (((addr_sp[0x8001 + 16 * tile_idx + 2 * px_y] >> (7 - x)) &
                      1)
                     << 1);

                if (col != 0 && px_x >= 0 && px_x < 160) {
                    if (!(flags & 0x80) || buf[y * 160 + px_x] == pal_grey[0])
                        buf[y * 160 + px_x] = pal_grey[obp >> (col << 1) & 3];
                }
            }
        }
    }
}

static uint32_t imgbuf[2][160 * 144];
static int cur_imgbuf = 0;

static void render_frame(gb_lcd *lcd);

static gb_lcd *g_lcd = NULL;
static int render_thread_function(void *ptr)
{
    gb_lcd *lcd = (gb_lcd *) ptr;
    g_lcd = lcd;

    SDL_CreateRenderer(lcd->win, -1, SDL_RENDERER_ACCELERATED);

    while (!lcd->exit) {
        SDL_LockMutex(lcd->vblank_mutex);
        SDL_CondWait(lcd->vblank_cond, lcd->vblank_mutex);
        cur_imgbuf = (cur_imgbuf + 1) % 2;
        SDL_UnlockMutex(lcd->vblank_mutex);
        render_frame(lcd);
    }

    SDL_Renderer *renderer = SDL_GetRenderer(lcd->win);
    SDL_DestroyRenderer(renderer);

    return 0;
}

bool init_window(gb_lcd *lcd)
{
    SDL_Init(SDL_INIT_VIDEO);
    lcd->win = SDL_CreateWindow("jitboy", SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, 160 * 3, 144 * 3,
                                SDL_WINDOW_OPENGL);
    if (!lcd->win) {
        LOG_ERROR("Window could not be created! SDL_Error: %s\n",
                  SDL_GetError());
        return false;
    }

    lcd->vblank_mutex = SDL_CreateMutex();
    lcd->vblank_cond = SDL_CreateCond();

    lcd->exit = false;

    lcd->thread =
        SDL_CreateThread(render_thread_function, "Render Thread", (void *) lcd);

    return true;
}

void deinit_window(gb_lcd *lcd)
{
    SDL_LockMutex(lcd->vblank_mutex);
    lcd->exit = true;
    SDL_UnlockMutex(lcd->vblank_mutex);
    SDL_CondBroadcast(lcd->vblank_cond);

    SDL_WaitThread(lcd->thread, 0);
    SDL_DestroyWindow(lcd->win);

    SDL_DestroyCond(lcd->vblank_cond);
    SDL_DestroyMutex(lcd->vblank_mutex);

    SDL_Quit();
}

static void lock()
{
    if (!g_lcd)
        return;
    SDL_LockMutex(g_lcd->vblank_mutex);
}

static void unlock()
{
    if (!g_lcd)
        return;
    SDL_UnlockMutex(g_lcd->vblank_mutex);
}

void update_line(uint8_t *mem)
{
    lock();
    render_back(imgbuf[cur_imgbuf], mem);
    unlock();
}

static void render_frame(gb_lcd *lcd)
{
    SDL_Renderer *renderer = SDL_GetRenderer(lcd->win);
    static SDL_Texture *bitmapTex = NULL;
    if (!bitmapTex) {
        bitmapTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, 160, 144);
    }
    void *pixels = NULL;
    int pitch = 0;
    SDL_LockTexture(bitmapTex, NULL, &pixels, &pitch);
    memcpy(pixels, imgbuf[(cur_imgbuf + 1) % 2], 160 * 144 * sizeof(uint32_t));
    SDL_UnlockTexture(bitmapTex);

    SDL_RenderCopy(renderer, bitmapTex, NULL, NULL);
    SDL_RenderPresent(renderer);
}
