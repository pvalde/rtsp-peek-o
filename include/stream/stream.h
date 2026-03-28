#ifndef STREAM_H
#define STREAM_H

#include "SDL_render.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdbool.h>

/**
 * @enum Stream_Protocol
 * @brief Transport protocol used for opening a stream.
 *
 * Specifies whether the stream should use TCP or UDP transport.
 */
enum Stream_Protocol { TCP, UDP };

/**
 * @typedef Stream
 * @brief Opaque type representing a video stream.
 *
 * The Stream struct holds all internal FFmpeg contexts, buffers, and state
 * required to read, decode, and process video frames.
 * Use stream_create() to allocate and stream_destroy() to free.
 */
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
    const AVFrame *frame; /**< Pointer to the decoded frame (read-only) */
    int x;                /**< top-left x coordinate of the frame */
    int y;                /**< top-left y coordinate of the frame */
    int w;                /**< width of the frame */
    int h;                /**< height of the frame */
};

/**
 * @struct Stream_Create_Params
 * @brief Input/output structure used by a thread to create a Stream.
 *
 * This structure is passed to stream_create_thread().
 * The thread fills `out_stream` with the created Stream pointer (or NULL on
 * failure) and sets `out_return_val` to 0 on success or -1 on failure.
 */
struct Stream_Create_Params {
    int out_return_val; /**< Status code: 0 = success, -1 = failure */
    enum Stream_Protocol in_protocol; /**< Input protocol for stream creation */
    const char *in_url;               /**< Input URL of the stream */
    struct Stream *out_stream; /**< Output Stream pointer (NULL if failed) */
};

/**
 * @brief Creates a Stream object from a URL.
 *
 * Allocates and initializes a Stream structure, populating all it fields with
 * the corresponding ffmpeg library's functions. Among other reasons, it will
 * fail if no video stream is found for the stream defined by url.
 *
 * @param url The URL to open the stream from. It will be duplicated, and the
 * copy owned by the stream struct. It cannot be NULL.
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
 * If the pointer itself is NULL, or points to NULL, the function does nothing.
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

/**
 * @brief Create an SDL_Texture for a Stream's video frames.
 *
 * Creates an SDL streaming texture in IYUV format matching the Stream's
 * width and height, suitable for updating with video frames.
 *
 * @param stream Pointer to the Stream object with codec info.
 * @param renderer SDL_Renderer used to create the texture.
 * @return Pointer to the new SDL_Texture, or NULL on failure.
 *
 * @note Caller must destroy the texture with SDL_DestroyTexture().
 */
SDL_Texture *stream_get_sdl_texture(struct Stream *stream,
                                    SDL_Renderer *renderer);

/**
 * @brief Thread routine to create a Stream object.
 *
 * Creates a Stream using the URL and protocol from the provided structure,
 * storing the resulting Stream pointer and a status code in the same structure.
 *
 * @param arg Pointer to a struct Stream_Create_Params containing input values
 *            (in_url, in_protocol) and output fields (out_stream,
 *            out_return_val).
 * @return Always returns NULL. The created Stream is written to
 *         out_stream (NULL on failure), and out_return_val is set to 0 on
 *         success or -1 on failure.
 *
 * @note Each thread must receive a unique structure to avoid data races.
 */
void *stream_create_thread(void *input);

#endif // STREAM_H
