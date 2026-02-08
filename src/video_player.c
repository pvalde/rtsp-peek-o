#include "video_player.h"

void log_ffmpeg_error(const char *err_msg, char *stream_name, int err);

static int Stream_init(Stream *pStream, const char *url);
static void Stream_clean_up(Stream *pStream);

static int Renderer_init(VideoPlayer *pPlayer, SDL_Renderer **renderer);

static void Renderer_clean_up(VideoPlayer *pPlayer);
static int load_avformat_ctx(AVFormatContext **pFormatCtx, char *url);
static int set_video_stream(AVFormatContext **pFormatCtx, int *videoStream);
static int find_decoder(AVCodec **pCodec, int *videoStream,
                        AVFormatContext **pFormatCtx);
static int set_defaults_for_codec_ctx(AVCodecContext **pCodecCtx,
                                      AVCodec **pCodec,
                                      AVFormatContext **pFormatCtx,
                                      int *videoStream);
static int initialize_codec_ctx(AVCodecContext **pCodecCtx, AVCodec **pCodec,
                                AVFormatContext **pFormatCtx, int *videoStream);
static int allocate_video_frame(AVFrame **pFrame);

static int allocate_raw_data_buffer(uint8_t **buffer, int *numBytes,
                                    AVCodecContext **pCodecCtx,
                                    enum AVPixelFormat pix_format);

static void initialize_sws_ctx_for_software_scaling(struct SwsContext **sws_ctx,
                                                    AVCodecContext **pCodecCtx,
                                                    enum AVPixelFormat pix_fmt);

static void set_avframe_for_encoding_decoding(AVFrame **pFrame,
                                              enum AVPixelFormat pix_format,
                                              uint8_t **raw_buffer,
                                              AVCodecContext **pCodecCtx);

int VideoPlayer_init(VideoPlayer *pPlayer, SDL_Renderer **renderer,
                     const char *url) {
  int ret;
  ret = Stream_init(&(pPlayer->stream), url);
  if (ret < 0) {
    return -1;
  }

  ret = Renderer_init(pPlayer, renderer);
  if (ret < 0) {
    return -1;
  }
  return 0;
}

void VideoPlayer_clean_up(VideoPlayer *pPlayer) {
  Stream_clean_up(&(pPlayer->stream));
  Renderer_clean_up(pPlayer);
}

static int Stream_init(Stream *pStream, const char *url) {
  pStream->url = url;

  enum AVPixelFormat pix_fmt = AV_PIX_FMT_YUV420P;
  int ret;

  ret = load_avformat_ctx(&(pStream->pFormatCtx), pStream->url);
  if (ret < 0) {
    return -1;
  }

  ret = set_video_stream(&(pStream->pFormatCtx), &(pStream->videoStream));
  if (ret < 0) {
    return -1;
  }
  printf("video stream: %d\n", pStream->videoStream);

  ret = find_decoder(&(pStream->pCodec), &(pStream->videoStream),
                     &(pStream->pFormatCtx));
  if (ret < 0) {
    return -1;
  }

  ret = set_defaults_for_codec_ctx(&(pStream->pCodecCtx), &(pStream->pCodec),
                                   &(pStream->pFormatCtx),
                                   &(pStream->videoStream));

  if (ret < 0) {
    return -1;
  }

  ret = initialize_codec_ctx(&(pStream->pCodecCtx), &(pStream->pCodec),
                             &(pStream->pFormatCtx), &(pStream->videoStream));

  ret = allocate_video_frame(&(pStream->pFrameIn));
  if (ret < 0) {
    return -1;
  }

  ret = allocate_video_frame(&(pStream->pFrameOut));
  if (ret < 0) {
    return -1;
  }

  ret = allocate_raw_data_buffer(&(pStream->buffer), &(pStream->numBytes),
                                 &(pStream->pCodecCtx), pix_fmt);
  if (ret < 0) {
    return -1;
  }

  set_avframe_for_encoding_decoding(&(pStream->pFrameOut), pix_fmt,
                                    &(pStream->buffer), &(pStream->pCodecCtx));

  pStream->pPacket = av_packet_alloc();
  if (pStream->pPacket == NULL) {
    fprintf(stderr, "Could not alloc packet.\n");
    return -1;
  }

  initialize_sws_ctx_for_software_scaling(&(pStream->sws_ctx),
                                          &(pStream->pCodecCtx), pix_fmt);

  return 0;
}

static void Stream_clean_up(Stream *pStream) {
  sws_freeContext(pStream->sws_ctx);
  av_free(pStream->buffer);
  av_frame_free(&(pStream->pFrameOut));
  av_free(pStream->pFrameOut);
  av_frame_free(&(pStream->pFrameIn));
  av_free(pStream->pFrameIn);

  // Close the codecs
  avcodec_free_context(&(pStream->pCodecCtx));

  // Close the video file
  avformat_close_input(&(pStream->pFormatCtx));
}

static int Renderer_init(VideoPlayer *pPlayer, SDL_Renderer **renderer) {
  pPlayer->renderer.sdl_renderer = *renderer;

  pPlayer->renderer.sdl_texture = SDL_CreateTexture(
      pPlayer->renderer.sdl_renderer, SDL_PIXELFORMAT_IYUV,
      SDL_TEXTUREACCESS_STREAMING, pPlayer->stream.pCodecCtx->width,
      pPlayer->stream.pCodecCtx->height);

  return 0;
}

static void Renderer_clean_up(VideoPlayer *pPlayer) {
  SDL_DestroyRenderer(pPlayer->renderer.sdl_renderer);
}

void log_ffmpeg_error(const char *err_msg, char *stream_name, int err) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE];
  av_strerror(err, errbuf, sizeof(errbuf));
  if (stream_name == NULL) {
    fprintf(stderr, "%s: %s\n", err_msg, errbuf);
  } else {
    fprintf(stderr, "%s '%s': %s\n", err_msg, stream_name, errbuf);
  }
}

static int load_avformat_ctx(AVFormatContext **pFormatContext, char *url) {
  // open file (get header)
  AVDictionary *opts = NULL;
  av_dict_set(&opts, "stimeout", "10000000", 0);
  int ret = avformat_open_input(&(*pFormatContext), url, NULL, &opts);
  av_dict_free(&opts);
  if (ret < 0) {
    log_ffmpeg_error("Could not open stream", url, ret);
    return -1;
  }

  // retrieve stream info
  ret = avformat_find_stream_info(*pFormatContext, NULL);
  if (ret < 0) {
    log_ffmpeg_error("Could not find stream info", url, ret);
    return -1;
  }

  // debug function
  av_dump_format(*pFormatContext, 0, url, 0);

  return 0;
}

static int set_video_stream(AVFormatContext **pFormatCtx, int *videoStream) {
  for (int i = 0; i < (*pFormatCtx)->nb_streams; i++) {
    // Check general type of the encoded data to match AVMEDIA_TYPE_VIDEO
    if ((*pFormatCtx)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      *videoStream = i;
      break;
    }
  }
  if (*videoStream == -1) {
    fprintf(stderr, "video stream not found");
    return -1;
  }
  return 0;
}

// find decoder that matches codec id
static int find_decoder(AVCodec **pCodec, int *videoStream,
                        AVFormatContext **pFormatCtx) {
  *pCodec = avcodec_find_decoder(
      (*pFormatCtx)->streams[*videoStream]->codecpar->codec_id);

  if (*pCodec == NULL) {
    // codec not found
    fprintf(stderr, "Unsupported coded!\n");
    return -1;
  }
  return 0;
}

// get AVCodecContext with default values for the given codec.
static int set_defaults_for_codec_ctx(AVCodecContext **pCodecCtx,
                                      AVCodec **pCodec,
                                      AVFormatContext **pFormatCtx,
                                      int *videoStream) {
  int ret;
  *pCodecCtx = avcodec_alloc_context3(*pCodec);

  ret = avcodec_parameters_to_context(
      *pCodecCtx, (*pFormatCtx)->streams[*videoStream]->codecpar);

  if (ret != 0) {
    // error copying codec context
    fprintf(stderr, "Could not copy codec context.\n");
    return -1;
  }
  return 0;
}

// Open codec
// the AVCodecContext is initialized to use the given AVCodec
static int initialize_codec_ctx(AVCodecContext **pCodecCtx, AVCodec **pCodec,
                                AVFormatContext **pFormatCtx,
                                int *videoStream) {
  int ret;
  *pCodecCtx = avcodec_alloc_context3(*pCodec);
  ret = avcodec_parameters_to_context(
      *pCodecCtx, (*pFormatCtx)->streams[*videoStream]->codecpar);

  if (ret != 0) {
    // error copying codec context
    fprintf(stderr, "Could not copy codec context.\n");
    return -1;
  }

  ret = avcodec_open2(*pCodecCtx, *pCodec, NULL);
  if (ret < 0) {
    // Could not open codec
    fprintf(stderr, "Could not open codec.\n");
    return -1;
  }
  return 0;
}

static int allocate_video_frame(AVFrame **pFrame) {
  *pFrame = av_frame_alloc();
  if (*pFrame == NULL) {
    fprintf(stderr, "Could not allocate frame");
    return -1;
  }
  return 0;
}

static int allocate_raw_data_buffer(uint8_t **buffer, int *numBytes,
                                    AVCodecContext **pCodecCtx,
                                    enum AVPixelFormat pix_format) {

  *numBytes = av_image_get_buffer_size(pix_format, (*pCodecCtx)->width,
                                       (*pCodecCtx)->height, 32);

  *buffer = (uint8_t *)av_malloc(*numBytes * sizeof(uint8_t));
  if (*buffer == NULL) {
    // Could not allocate buffer
    fprintf(stderr, "Could not allocate buffer\n");
    return -1;
  }
  return 0;
}

static void set_avframe_for_encoding_decoding(AVFrame **pFrame,
                                              enum AVPixelFormat pix_format,
                                              uint8_t **raw_buffer,
                                              AVCodecContext **pCodecCtx) {

  av_image_fill_arrays((*pFrame)->data, (*pFrame)->linesize, *raw_buffer,
                       pix_format, (*pCodecCtx)->width, (*pCodecCtx)->height,
                       32);
}

static void
initialize_sws_ctx_for_software_scaling(struct SwsContext **sws_ctx,
                                        AVCodecContext **pCodecCtx,
                                        enum AVPixelFormat pix_fmt) {

  *sws_ctx = sws_getContext((*pCodecCtx)->width, (*pCodecCtx)->height,
                            (*pCodecCtx)->pix_fmt, (*pCodecCtx)->width,
                            (*pCodecCtx)->height, pix_fmt, SWS_BILINEAR, NULL,
                            NULL, NULL);
}
