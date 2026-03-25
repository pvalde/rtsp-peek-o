#include "video_player.h"

static int Renderer_init(Video_Player *pPlayer, SDL_Renderer **renderer);

static void Renderer_clean_up(Video_Player *pPlayer);

int Video_Player_init(Video_Player *pPlayer, SDL_Renderer **renderer,
                      const char *url) {
    int ret;
    enum Stream_Protocol protocol = UDP;
    pPlayer->stream = stream_create(url, protocol);
    if (pPlayer->stream == NULL) {
        return -1;
    }

    ret = Renderer_init(pPlayer, renderer);
    if (ret < 0) {
        return -1;
    }
    return 0;
}

void Video_Player_clean_up(Video_Player *pPlayer) {

    stream_destroy(&(pPlayer->stream));
    Renderer_clean_up(pPlayer);
}

static int Renderer_init(Video_Player *pPlayer, SDL_Renderer **renderer) {
    pPlayer->renderer.sdl_renderer = *renderer;

    pPlayer->renderer.sdl_texture = SDL_CreateTexture(
        pPlayer->renderer.sdl_renderer, SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING, pPlayer->stream->codec_ctx->width,
        pPlayer->stream->codec_ctx->height);

    return 0;
}

static void Renderer_clean_up(Video_Player *pPlayer) {
    SDL_DestroyRenderer(pPlayer->renderer.sdl_renderer);
}
