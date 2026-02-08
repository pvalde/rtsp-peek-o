#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

typedef struct {
  AVFormatContext *pFormatCtx;
  AVCodec *pCodec;
  AVCodecContext *pCodecCtx;
  int videoStream;
  char *url;
  AVFrame *pFrameIn;
  AVFrame *pFrameOut;
  uint8_t *buffer;
  int numBytes;
  AVPacket *pPacket;
  struct SwsContext *sws_ctx;
} Stream;

typedef struct {
  SDL_Renderer *sdl_renderer;
  SDL_Texture *sdl_texture;
} Renderer;

typedef struct {
  Stream stream;
  Renderer renderer;
} VideoPlayer;

int VideoPlayer_init(VideoPlayer *pPlayer, SDL_Renderer **renderer,
                     const char *url);
void VideoPlayer_clean_up(VideoPlayer *pPlayer);

#endif // VIDEO_PLAYER_H
