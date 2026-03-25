#ifndef STREAM_PRIVATE_H
#define STREAM_PRIVATE_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdbool.h>

enum Stream_Protocol { TCP, UDP };

struct Stream {
    AVFormatContext *format_ctx;
    const AVCodec *codec;
    AVCodecContext *codec_ctx;
    int video_stream_id;
    char *url;
    AVFrame *frame_in;
    AVFrame *frame_out;
    enum AVPixelFormat frame_out_pix_fmt;
    uint8_t *buffer;
    int frame_buff_size;
    AVPacket *packet;
    struct SwsContext *sws_ctx;
};

/**
 * @brief Creates a Stream object from a URL.
 *
 * Allocates and initializes a Stream structure, populating all it fields with
 * the corresponding ffmpeg library's functions. Among other reasons, it will
 * fail if no video stream is found for the stream defined by url.
 *
 * @param url The URL to open the stream from. It will be duplicated, and the
 * copy owned by the stream struct.
 * @param protocol Transport protocol for the stream.
 * @return Pointer to a Stream object, or NULL on failure.
 */
struct Stream *stream_create(const char *url, enum Stream_Protocol protocol);

/**
 * @brief Frees all resources owned by a Stream and sets pointer to NULL.
 *
 * Safe for partially initialized Stream objects. Frees all FFmpeg contexts,
 * buffers, and allocated memory owned by the Stream.
 *
 * @param stream Pointer to Stream pointer. Becomes NULL after cleanup.
 */
void stream_destroy(struct Stream **stream);

#endif // STREAM_PRIVATE_H
