#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include "stream_private.h"
#include <SDL2/SDL.h>

typedef struct Stream Stream;

typedef struct {
    SDL_Renderer *sdl_renderer;
    SDL_Texture *sdl_texture;
} Renderer;

typedef struct {
    Stream *stream;
    Renderer renderer;
} Video_Player;

int Video_Player_init(Video_Player *pPlayer, SDL_Renderer **renderer,
                      const char *url);
void Video_Player_clean_up(Video_Player *pPlayer);

#endif // VIDEO_PLAYER_H
