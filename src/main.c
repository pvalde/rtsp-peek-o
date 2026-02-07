#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdio.h>

#include "video_player.h"

void printHelpMenu();
int decode_frames(VideoPlayer *pPlayer);

int main(int argc, char *argv[]) {
  int ret;
  VideoPlayer player = {0};

  if (!(argc == 2)) {
    printf("n of args: %d\n", argc);
    printHelpMenu();

    return -1;
  }

  /* ******************** init SDL ******************** */
  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
  if (ret != 0) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    return -1;
  }

  avformat_network_init();

  SDL_Window *screen =
      SDL_CreateWindow("DVR VIEWER", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_RESIZABLE);

  if (!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting.\n");
    return -1;
  }

  SDL_GL_SetSwapInterval(1);

  /* ******************** video player init ******************** */
  ret = VideoPlayer_init(&player, &screen, argv[1]);
  if (ret < 0) {
    return -1;
  }

  ret = decode_frames(&player);
  if (ret < 0) {
    return -1;
  }

  VideoPlayer_clean_up(&player);

  SDL_DestroyWindow(screen);
  SDL_Quit();

  printf("=============NO ERRORS!============\n");
  return 0;
}

void printHelpMenu() {
  printf("Invalid arguments.\n\n");
  printf("Usage: ./dvr-viewer <filename>\n\n");
}

int decode_frames(VideoPlayer *pPlayer) {
  int running = 1;
  int ret;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
      }
    }
    // TODO: add reconnecting logic
    if (av_read_frame(pPlayer->stream.pFormatCtx, pPlayer->stream.pPacket) >=
        0) {
      // if the packet is from video stream
      if (pPlayer->stream.pPacket->stream_index ==
          pPlayer->stream.videoStream) {

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

          /* // get clip fps */
          /* double fps = av_q2d( */
          /*     pPlayer->stream.pFormatCtx->streams[pPlayer->stream.videoStream]
           */
          /*         ->r_frame_rate); */

          /* // get clip sleep time */
          /* double sleep_time = 1.0 / (double)fps; */
          /* SDL_Delay((1000 * sleep_time) - 10); */

          SDL_Rect rect;
          rect.x = 0;
          rect.y = 0;
          rect.w = pPlayer->stream.pCodecCtx->width;
          rect.h = pPlayer->stream.pCodecCtx->height;

          SDL_UpdateYUVTexture(pPlayer->renderer.sdl_texture, &rect,
                               pPlayer->stream.pFrameOut->data[0],
                               pPlayer->stream.pFrameOut->linesize[0],
                               pPlayer->stream.pFrameOut->data[1],
                               pPlayer->stream.pFrameOut->linesize[1],
                               pPlayer->stream.pFrameOut->data[2],
                               pPlayer->stream.pFrameOut->linesize[2]);

          SDL_RenderClear(pPlayer->renderer.sdl_renderer);

          // copy portion of the texture to the current rendering target
          SDL_RenderCopy(pPlayer->renderer.sdl_renderer,
                         pPlayer->renderer.sdl_texture, NULL, NULL);

          SDL_RenderPresent(pPlayer->renderer.sdl_renderer);
        }
      }

      av_packet_unref(pPlayer->stream.pPacket);
    }
  }

  return 0;
}
