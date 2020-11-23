#ifndef JITBOY_LCD_H
#define JITBOY_LCD_H

#include <SDL.h>
#include <stdbool.h>

typedef struct {
    SDL_Window *win;
    SDL_mutex *vblank_mutex;
    SDL_cond *vblank_cond;
    SDL_Thread *thread;
    bool exit;
} gb_lcd;

bool init_window(gb_lcd *lcd);
void deinit_window(gb_lcd *lcd);
void update_line(uint8_t *mem);

#endif
