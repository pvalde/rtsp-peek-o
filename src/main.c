#include "cli_args.h"
#include "rtsp_playback.h"
#include "video_player.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdio.h>

#define PROG_NAME "rtsp-peek"

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  bool sdl_initialized;
} SDL_Context;

void SDL_clean_up(SDL_Context *ctx);

int main(int argc, char *argv[]) {
  int ret;
  Cli_Args args = {0};
  Video_Player *players = NULL;
  int n_players_initialized = 0;
  SDL_Context sdl_ctx = {
      .window = NULL, .renderer = NULL, .sdl_initialized = false};

  ret = parse_args(argc, argv, &args);

  if (ret == PARSE_ERROR) {
    return -1;
  } else if (ret == PARSE_HELP) {
    return 0;
  }

  players = calloc(args.n_of_rtsp_urls, sizeof(Video_Player));
  if (!players) {
    fprintf(stderr, "Failed to allocate memory for video players\n");
    ret = -1;
    goto cleanup;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    ret = -1;
    goto cleanup;
  }
  sdl_ctx.sdl_initialized = true;

  avformat_network_init();

  sdl_ctx.window =
      SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_RESIZABLE);

  if (!sdl_ctx.window) {
    fprintf(stderr, "SDL: could not set video mode - exiting.\n");
    ret = -1;
    goto cleanup;
  }

  sdl_ctx.renderer =
      SDL_CreateRenderer(sdl_ctx.window, -1, SDL_RENDERER_ACCELERATED);

  if (!sdl_ctx.renderer) {
    fprintf(stderr, "SDL: could not create renderer - exiting.\n");
    ret = -1;
    goto cleanup;
  }

  SDL_GL_SetSwapInterval(1);

  for (int i = 0; i < args.n_of_rtsp_urls; i++) {
    if (Video_Player_init(&players[i], &sdl_ctx.renderer, args.rtsp_urls[i]) <
        0) {
      ret = -1;
      goto cleanup;
    } else {
      n_players_initialized++;
    }
  }

  ret = display_videos(players, args.n_of_rtsp_urls, sdl_ctx.renderer);

cleanup:
  if (players) {
    for (int i = 0; i < n_players_initialized; i++) {
      Video_Player_clean_up(&players[i]);
    }
    free(players);
  }
  SDL_clean_up(&sdl_ctx);
  Cli_args_clean_up(&args);
  avformat_network_deinit();
  if (ret == 0)
    printf("=============" PROG_NAME " CLOSED NORMALLY============\n");
  return ret < 0 ? -1 : 0;
}

void SDL_clean_up(SDL_Context *ctx) {
  if (!ctx)
    return;

  if (ctx->renderer) {
    SDL_DestroyRenderer(ctx->renderer);
    ctx->renderer = NULL;
  }

  if (ctx->window) {
    SDL_DestroyWindow(ctx->window);
    ctx->window = NULL;
  }

  if (ctx->sdl_initialized) {
    SDL_Quit();
    ctx->sdl_initialized = false;
  }
}
