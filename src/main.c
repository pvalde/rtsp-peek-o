#include "video_player.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>

#define PROG_NAME "rtsp-peek"

void printHelpMenu();
int decode_frames(VideoPlayer *pPlayer);
int decode_frames2(VideoPlayer players[], int length);
void render_sdl_rect(SDL_Rect *rect, VideoPlayer *pPlayer, SDL_Rect *pos_rect);
int update_rect(VideoPlayer *pPlayer, SDL_Rect *rect);
int display_videos(VideoPlayer pPlayer[], int players_length,
                   SDL_Renderer *renderer);

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

  SDL_DestroyWindow(window);
  SDL_Quit();

  printf("=============" PROG_NAME " CLOSED NORMALLY============\n");
  return 0;
}

void printHelpMenu() {
  printf("Usage: " PROG_NAME " <rtsp_url> [<rtsp_url> ...]\n\n");
}

int update_rect(VideoPlayer *pPlayer, SDL_Rect *rect) {
  int ret;
  if (av_read_frame(pPlayer->stream.pFormatCtx, pPlayer->stream.pPacket) >= 0) {
    // if the packet is from video stream
    if (pPlayer->stream.pPacket->stream_index == pPlayer->stream.videoStream) {

      // decode packet
      ret = avcodec_send_packet(pPlayer->stream.pCodecCtx,
                                pPlayer->stream.pPacket);
      if (ret < 0) {
        fprintf(stderr, "Error sending packet for decoding.\n");
        return -1;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(pPlayer->stream.pCodecCtx,
                                    pPlayer->stream.pFrameIn);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          // EOF exit loop
          break;
        } else if (ret < 0) {
          fprintf(stderr, "Error while decoding.\n");
          return -1;
        }

        // Convert the image into YUV format that SDL uses:
        sws_scale(pPlayer->stream.sws_ctx,
                  (uint8_t const *const *)pPlayer->stream.pFrameIn->data,
                  pPlayer->stream.pFrameIn->linesize, 0,
                  pPlayer->stream.pCodecCtx->height,
                  pPlayer->stream.pFrameOut->data,
                  pPlayer->stream.pFrameOut->linesize);

        rect->x = 0;
        rect->y = 0;
        rect->w = pPlayer->stream.pCodecCtx->width;
        rect->h = pPlayer->stream.pCodecCtx->height;
      }
    }
  }
  return 0;
}

void render_sdl_rect(SDL_Rect *rect, VideoPlayer *pPlayer, SDL_Rect *pos_rect) {
  SDL_UpdateYUVTexture(pPlayer->renderer.sdl_texture, rect,
                       pPlayer->stream.pFrameOut->data[0],
                       pPlayer->stream.pFrameOut->linesize[0],
                       pPlayer->stream.pFrameOut->data[1],
                       pPlayer->stream.pFrameOut->linesize[1],
                       pPlayer->stream.pFrameOut->data[2],
                       pPlayer->stream.pFrameOut->linesize[2]);

  // copy portion of the texture to the current rendering target
  SDL_RenderCopy(pPlayer->renderer.sdl_renderer, pPlayer->renderer.sdl_texture,
                 NULL, pos_rect);
}

int display_videos(VideoPlayer players[], int players_length,
                   SDL_Renderer *renderer) {
  int running = 1;
  int ret;

  SDL_Rect texture_rect;
  SDL_Rect pos_rects[players_length];

  int ren_w, ren_h;
  SDL_GetRendererOutputSize(renderer, &ren_w, &ren_h);

  // dimensions for 1 column
  float heigth_weight = 1.0 / (float)players_length;

  while (running) {

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = 0;
      } else if (e.type == SDL_WINDOWEVENT &&
                 e.window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_GetRendererOutputSize(renderer, &ren_w, &ren_h);
      }
    }

    for (int i = 0; i < players_length; i++) {
      ret = update_rect(&players[i], &texture_rect);
      if (ret < 0) {
        return -1;
      }
    }

    SDL_RenderClear(players->renderer.sdl_renderer);
    int height = (int)(round((float)ren_h * heigth_weight));

    for (int i = 0; i < players_length; i++) {
      pos_rects[i].x = 0;
      pos_rects[i].y = i * height;
      pos_rects[i].w = ren_w;
      pos_rects[i].h = height;
      render_sdl_rect(&texture_rect, &players[i], &pos_rects[i]);
    }

    SDL_RenderPresent(players->renderer.sdl_renderer);
    av_packet_unref(players->stream.pPacket);
  }
  return 0;
}
