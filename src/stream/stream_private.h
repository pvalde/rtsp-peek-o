#ifndef STREAM_PRIVATE_H
#define STREAM_PRIVATE_H

#include "stream.h"

struct Stream {
    AVFormatContext *format_ctx;
    const AVCodec *codec;
    AVCodecContext *codec_ctx;
    int video_stream_id;
    char *url;
    AVFrame *frame_in;
    AVFrame *frame_out;
    AVFrame *frame_tmp;
    enum AVPixelFormat frame_out_pix_fmt;
    uint8_t *buffer;
    int frame_buff_size;
    AVPacket *packet;
    struct SwsContext *sws_ctx;
};

#endif // STREAM_PRIVATE_H
