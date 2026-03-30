#ifndef RTSP_STREAM_H
#define RTSP_STREAM_H

#include <SDL2/SDL_render.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <stdatomic.h>
#include <stdbool.h>

/**
 * @enum Transport_Protocol
 * @brief Transport protocol used for opening a Rtsp_Stream.
 *
 * Specifies whether the stream should use TCP or UDP transport.
 */
enum Transport_Protocol { TCP, UDP };

/**
 * @typedef Rtsp_Stream
 * @brief Opaque type representing a rtsp video stream.
 *
 * The Stream struct holds all internal FFmpeg contexts, buffers, and state
 * required to read, decode, and process video frames.
 * Use rtsp_stream_create() to allocate and rtsp_stream_destroy() to free.
 */
typedef struct Rtsp_Stream Rtsp_Stream;

/**
 * @struct Rtsp_Stream_Create_Params
 * @brief Input/output structure used by a thread to create a Stream.
 *
 * This structure is used as the argument to `rtsp_stream_create_thread()`.
 * The thread fills `out_stream` with the created Rtsp_Stream pointer (or NULL
 * on failure) and sets `out_return_val` to 0 on success or -1 on failure.
 */
struct Rtsp_Stream_Create_Params {
    int out_return_val; /**< Status code: 0 = success, -1 = failure */
    enum Transport_Protocol
        in_protocol;    /**< Input protocol for stream creation */
    const char *in_url; /**< Input URL of the stream */
    struct Rtsp_Stream
        *out_stream; /**< Output Rtsp_Stream pointer (NULL if failed) */
};

/**
 * @struct Rtsp_Stream_Get_Frame_Params
 * @brief Parameters and context for retrieving a video frame in a separate
 * thread.
 *
 * This structure is used as the argument to `rtsp_stream_threaded_get_frame()`
 * when running a thread that continuously reads frames from an `Rtsp_Stream`.
 *
 * The fields provide the thread with the necessary input, output, and
 * synchronization mechanisms.
 *
 * @note All atomic fields are used for thread-safe communication between the
 *       main thread and the worker thread.
 *
 * @pre The caller must allocate and initialize the structure, set
 *      `out_frame_data` to NULL before starting the thread, `stop` to zero, and
 *      initialize the `mutex`.
 *
 */
struct Rtsp_Stream_Get_Frame_Params {
    Rtsp_Stream *stream;     /**< Pointer to the Rtsp_Stream to read frames
                                  from. */
    pthread_mutex_t *mutex;  /**< Mutex used to protect shared resources during
                                  frame retrieval. */
    atomic_int stop;         /**< Input flag. Set to non-zero by the main
                                  thread to request the worker thread to stop.
                                  */
    AVFrame *out_frame_data; /**< Output. Pointer to a pre-allocated AVFrame
                                  where the worker thread writes the latest
                                  video frame. The worker thread owns the data,
                                  main thread should treat it as read-only and
                                  access using `mutex`. It should be initialized
                                  as NULL before calling the thread for safety
                                  reasons. */

    atomic_int thread_status; /**< Thread Status (written by worker thread)
                                    1 = initialization unfinished,
                                    0 = running,
                                   -1 = runtime error,
                                   -2 = initialization failed). */

    atomic_int thread_finished; /**< Output flag. Set to non-zero by the thread
                                   when it has finished execution. */
};

/**
 * @brief Creates a Rtsp_Stream object from a URL.
 *
 * Allocates and initializes a Rtsp_Stream structure, populating all it fields
 * with the corresponding ffmpeg library's functions. Among other reasons, it
 * will fail if no video stream is found for the stream defined by url.
 *
 * @param url The URL to open the stream from. It will be duplicated, and the
 * copy owned by the stream struct. It cannot be NULL.
 * @param protocol Transport protocol for the stream.
 * @return Pointer to a Rtsp_Stream object, or NULL on failure.
 */
Rtsp_Stream *rtsp_stream_create(const char *url, enum Transport_Protocol);

/**
 * @brief Frees all resources owned by a Rtsp_Stream and sets pointer to NULL.
 *
 * Safe for partially initialized Rtsp_Stream objects. Frees all FFmpeg
 * contexts, buffers, and allocated memory owned by the Rtsp_Stream.
 *
 * If the pointer itself is NULL, or points to NULL, the function does
 * nothing.
 *
 * @param stream Pointer to Rtsp_Stream pointer. Becomes NULL after cleanup.
 */
void rtsp_stream_destroy(Rtsp_Stream **stream);

/**
 * @brief Create an SDL_Texture for a Rtsp_Stream's video frames.
 *
 * Creates an SDL streaming texture in IYUV format matching the Stream's
 * width and height, suitable for updating with video frames.
 *
 * @param stream Pointer to the Rtsp_Stream object with codec info.
 * @param renderer SDL_Renderer used to create the texture.
 * @return Pointer to the new SDL_Texture, or NULL on failure.
 *
 * @note Caller must destroy the texture with SDL_DestroyTexture().
 */
SDL_Texture *rtsp_stream_get_sdl_texture(Rtsp_Stream *stream,
                                         SDL_Renderer *renderer);

/**
 * @brief Thread routine to create a Stream object.
 *
 * Creates a Stream using the URL and protocol from the provided structure,
 * storing the resulting Stream pointer and a status code in the same
 * structure.
 *
 * @param arg Pointer to a struct Stream_Create_Params containing input
 * values (in_url, in_protocol) and output fields (out_stream,
 *            out_return_val).
 * @return Always returns NULL. The created video frame is written to
 *         out_stream (NULL on failure), and out_return_val is set to 0 on
 *         success or -1 on failure.
 *
 * @note Each thread must receive a unique structure to avoid data races.
 */
void *rtsp_stream_threaded_create(void *arg);

/**
 * @brief Thread function to continuously retrieve video frames from an RTSP
stream.
 *
 * This function is intended to run as a separate thread. It continuously
 * reads frames from the given `Rtsp_Stream` and writes the latest frame into
 * the `out_frame_data` field of the provided `Rtsp_Stream_Get_Frame_Params`
 * structure. Thread-safe access to shared resources is ensured via the
 * provided `mutex`.
 *
 * The function monitors the `stop` atomic flag. When it is set to non-zero by
 * the main thread, the worker thread will terminate gracefully.
 *
 * Thread progress and status are communicated back to the main thread through
 * the `thread_status` and `thread_finished` atomic fields.
 *
 * @param arg Pointer to an initialized `Rtsp_Stream_Get_Frame_Params`
 *            structure.
 *            The caller must ensure that the structure is properly initialized
 *            (e.g., `out_frame_data` set to NULL, `stop` set to 0, and
 *            `mutex` initialized) before starting the thread.
 *
 * @return Always returns NULL.
 *
 * @note The caller must not modify `out_frame_data` under any circumstances
 *       and should only read it under the protection of `mutex`. All atomic
 *       fields are used to safely communicate between threads.
 *
 * @warning Ensure proper cleanup of the thread and resources to avoid memory
 *          leaks or race conditions.
 */
void *rtsp_stream_threaded_get_frame(void *arg);

#endif // RTSP_STREAM_H
