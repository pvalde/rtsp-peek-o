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

void printHelpMenu();

int main(int argc, char *argv[]) {
  int n_of_streams = argc - 1;
  int ret;
  VideoPlayer players[n_of_streams];

  for (int i = 0; i < n_of_streams; i++) {
    VideoPlayer player = {0};
    players[i] = player;
  }

  if (!(argc >= 2)) {
    printHelpMenu();

    return -1;
  }

  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
  if (ret != 0) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    return -1;
  }

  avformat_network_init();

  SDL_Window *window =
      SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_RESIZABLE);

  if (!window) {
    fprintf(stderr, "SDL: could not set video mode - exiting.\n");
    return -1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_GL_SetSwapInterval(1);

  for (int i = 0; i < n_of_streams; i++) {
    ret = VideoPlayer_init(&players[i], &renderer, argv[i + 1]);
    if (ret < 0) {
      return -1;
    }
  }

  ret = display_videos(players, sizeof(players) / sizeof(players[0]), renderer);
  if (ret < 0) {
    return -1;
  }

  for (int i = 0; i < n_of_streams; i++) {
    VideoPlayer_clean_up(&players[i]);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  printf("=============" PROG_NAME " CLOSED NORMALLY============\n");
  return 0;
}

void printHelpMenu() {
  printf("Usage: " PROG_NAME " <rtsp_url> [<rtsp_url> ...]\n\n");
}
