#ifndef STREAM_H
#define STREAM_H

#include "SDL_render.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdbool.h>

enum Stream_Protocol { TCP, UDP };

typedef struct Stream Stream;

/**
 * @struct Stream_Frame_Data
 * @brief Represents a decoded video frame from a stream.
 *
 * The `frame` pointer is a **borrowed reference** to an internal frame buffer.
 * It is valid only until the next call to `stream_get_decoded_frame`.
 * Do NOT modify the frame data.
 */
struct Stream_Frame_Data {
    const AVFrame *frame; /** Pointer to the decoded frame (read-only) */
    int x;                /** top-left x coordinate of the frame */
    int y;                /** top-left y coordinate of the frame */
    int w;                /** width of the frame */
    int h;                /** height of the frame */
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

/**
 * @brief Decodes the next frame from a video stream.
 *
 * @param[out] out_frame_data Pointer to a Stream_Frame_Data struct to fill.
 * @param[in]  stream         Pointer to the Stream to decode from.
 *
 * @return 0 if a new frame is available,
 *        <0 if no frame is available.
 *
 * @note The frame pointer in out_frame_data is a **borrowed reference**:
 *       - Valid only until the next call to this function.
 *       - The caller must not free or modify the frame.
 *       - Use it immediately for processing (e.g., rendering or conversion).
 * @note If no frame is available, out_frame_data->frame is NULL.
 *       The caller must check for NULL before using the frame.
 */
int stream_get_decoded_frame(struct Stream_Frame_Data *out_frame_data,
                             struct Stream *stream);

SDL_Texture *stream_get_sdl_texture(struct Stream *stream,
                                    SDL_Renderer *renderer);

#endif // STREAM_H
