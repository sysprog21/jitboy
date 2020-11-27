#ifndef JITBOY_COMMON_H
#define JITBOY_COMMON_H

#ifdef DEBUG
/* Simple log messages with categories and priorities */
#include <SDL_log.h>
#define LOG_ERROR(...) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#define LOG_DEBUG(...) SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)

#else
#include <stdio.h>
#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define LOG_DEBUG(...)

#endif

#endif
