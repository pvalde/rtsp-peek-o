#include "rtsp_stream.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#define BYTE_ALIGNMENT 32

/**
 * @brief Represents an RTSP video stream and its decoding state.
 *
 * This structure encapsulates all FFmpeg contexts and buffers required to
 * receive, decode, and scale video frames from an RTSP source.
 *
 * The structure is managed by the rtsp_stream_* API. Its fields should be
 * considered internal and must not be modified directly unless explicitly
 * documented.
 */
struct Rtsp_Stream {
    AVFormatContext *format_ctx; /**< Input format context (demuxer). */
    const AVCodec *codec;        /**< Video codec used for decoding. */
    AVCodecContext *codec_ctx;   /**< Codec context for the video stream. */
    int video_stream_id;         /**< Index of the selected video stream. */

    char *url; /**< RTSP stream URL. */

    AVFrame *decoding_frame; /**< Internal frame used for decoded data. */
    enum AVPixelFormat
        frame_out_pix_fmt; /**< Target pixel format for scaling. */

    uint8_t *buffer;     /**< Buffer for scaled frame data. */
    int frame_buff_size; /**< Size of the allocated buffer. */

    AVPacket *packet; /**< Packet used for reading encoded data. */

    struct SwsContext *sws_ctx; /**< Scaling/conversion context (libswscale). */
};

/**
 * @brief Allocate and initialize an AVFrame for scaled output.
 *
 * Creates a FFmpeg's AVFrame configured according to the parameters in the
 * given Rtsp_Stream object. The frame's data pointers and linesizes are
 * allocated and set up to hold a scaled image, so it can be filled using
 * rtsp_stream_fill_scaled_frame().
 *
 * The returned frame must be freed with av_frame_free() when no longer needed.

 * @param stream Pointer to the Rtsp_Stream object that defines the output
 *        format, dimensions, and scaling parameters.
 * @return Pointer to a FFmpeg's AVFrame object on success, or NULL on failure.
 */
static AVFrame *rtsp_stream_set_scale_avframe(struct Rtsp_Stream *stream);

/**
 * @brief Decode the next available video frame from the stream's input.
 *
 * Retrieves the next packet from the stream's internal RTSP/network queue
 * sends it to the decoder, and stores any resulting decoded video frame in the
 * stream's internal frame buffers.
 *
 * This function is typically called in a loop, as multiple packets may be
 * required to produce a single decoded frame.
 *
 * @param stream Pointer to the Rtsp_Stream object managing decoding state and
 *               internal buffers.
 * @return 0 if a frame was successfully decoded and stored,
 *        -1 if an error occurred during packet retrieval or decoding,
 *        -2 if no packet is currently available in the input queue.
 */
static int rtsp_stream_decode_frame(struct Rtsp_Stream *stream);

/**
 * @brief Scale and convert the decoded video frame into a destination AVFrame.
 *
 * Uses libswscale to transform the most recently decoded video frame stored in
 * the stream's internal buffers into the format and dimensions specified by
 * `dst`.
 *
 * This function requires that a decoded video frame is already available in the
 * stream (e.g., after a successful call to rtsp_stream_decode_frame()).
 *
 * The destination frame must be pre-allocated and properly configured
 * (data pointers, linesizes, width, height, and pixel format), see
 * stream_set_scale_avframe().
 *
 * @param dst    Pointer to a pre-allocated AVFrame that will receive the
 *               scaled and converted image data.
 * @param stream Pointer to the Rtsp_Stream containing the source frame and
 *               scaling context.
 * @return 0 on success,
 *        -1 if scaling or conversion failed.
 */
static int rtsp_stream_fill_scaled_frame(AVFrame *dst,
                                         struct Rtsp_Stream *stream);

struct Rtsp_Stream *rtsp_stream_create(const char *url,
                                       enum Transport_Protocol protocol) {

    if (url == NULL) {
        fprintf(stderr, "ERROR: url is NULL\n");
        return NULL;
    }

    int err_code = 0;
    char *transport_protocol = NULL;
    char err_msg[AV_ERROR_MAX_STRING_SIZE];
    AVDictionary *opts = NULL;

    struct Rtsp_Stream *stream = calloc(1, sizeof(*stream));
    if (stream == NULL) {
        fprintf(
            stderr,
            "ERROR: Could not allocate memory for stream object. URL: '%s'\n",
            url);
        goto error_cleanup;
    }

    stream->frame_out_pix_fmt = AV_PIX_FMT_YUV420P;
    stream->video_stream_id = -1;

    stream->url = strdup(url);
    if (stream->url == NULL)
        goto error_cleanup;

    if (protocol == UDP)
        transport_protocol = "udp";
    else if (protocol == TCP)
        transport_protocol = "tcp";
    else {
        fprintf(stderr, "WARNING: invalid protocol, defaulting to UDP\n");
        transport_protocol = "udp";
    }

    // set stream options
    if (av_dict_set(&opts, "stimeout", "1000000", 0) < 0) {
        fprintf(stderr,
                "ERROR: There was a problem while setting timeout options for "
                "'%s'\n",
                stream->url);
        goto error_cleanup;
    }
    if (av_dict_set(&opts, "rtsp_transport", transport_protocol, 0) < 0) {
        fprintf(stderr,
                "ERROR: There was a problem while setting transport protocol "
                "options for "
                "'%s'\n",
                stream->url);
        goto error_cleanup;
    }

    // read stream header
    err_code = avformat_open_input(&(stream->format_ctx), url, NULL, &opts);
    if (err_code < 0) {
        char custom_err_msg[] = "ERROR: Could not open stream";
        if (av_strerror(err_code, err_msg, sizeof(err_msg)) == 0) {
            fprintf(stderr, "%s '%s': %s\n", custom_err_msg, stream->url,
                    err_msg);
        } else {
            fprintf(stderr, "%s '%s': unknown error\n", custom_err_msg,
                    stream->url);
        }
        goto error_cleanup;
    }
    av_dict_free(&opts);
    opts = NULL;

    // retrieve stream info
    err_code = avformat_find_stream_info(stream->format_ctx, NULL);
    if (err_code < 0) {
        char custom_err_msg[] = "ERROR: Could not find stream info for";
        if (av_strerror(err_code, err_msg, sizeof(err_msg)) == 0) {
            fprintf(stderr, "%s '%s': %s\n", custom_err_msg, stream->url,
                    err_msg);
        } else {
            fprintf(stderr, "%s '%s': unknown error\n", custom_err_msg,
                    stream->url);
        }
        goto error_cleanup;
    }

    // display info
    av_dump_format(stream->format_ctx, 0, url, 0);

    // save id of encoded data of type video
    for (unsigned int i = 0; i < stream->format_ctx->nb_streams; i++) {
        if (stream->format_ctx->streams[i]->codecpar->codec_type ==
            AVMEDIA_TYPE_VIDEO) {
            stream->video_stream_id = i;
            break;
        }
    }
    if (stream->video_stream_id < 0) {
        fprintf(stderr, "ERROR: video stream not found for '%s'\n",
                stream->url);
        goto error_cleanup;
    }

    // find decoder that matches codec id
    stream->codec = avcodec_find_decoder(
        stream->format_ctx->streams[stream->video_stream_id]
            ->codecpar->codec_id);
    if (stream->codec == NULL) {
        fprintf(stderr, "ERROR: Unsupported codec for '%s'\n", stream->url);
        goto error_cleanup;
    }

    // get AVCodecContext object with default values for the given codec.
    stream->codec_ctx = avcodec_alloc_context3(stream->codec);
    if (stream->codec_ctx == NULL) {
        fprintf(stderr, "ERROR: codec_ctx could not be allocated for '%s'\n",
                stream->url);
        goto error_cleanup;
    }

    // fill codec_ctx according to codecpar(armeters)
    err_code = avcodec_parameters_to_context(
        stream->codec_ctx,
        stream->format_ctx->streams[stream->video_stream_id]->codecpar);

    if (err_code < 0) {
        char custom_err_msg[] = "ERROR: Could not fill codec_ctx fields for";
        if (av_strerror(err_code, err_msg, sizeof(err_msg)) == 0) {
            fprintf(stderr, "%s '%s': %s\n", custom_err_msg, stream->url,
                    err_msg);
        } else {
            fprintf(stderr, "%s '%s': unknown error\n", custom_err_msg,
                    stream->url);
        }
        goto error_cleanup;
    }

    // initialize codec_ctx
    if (avcodec_open2(stream->codec_ctx, stream->codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec for '%s'\n", stream->url);
        goto error_cleanup;
    }

    stream->decoding_frame = av_frame_alloc();
    if (stream->decoding_frame == NULL) {
        fprintf(stderr, "Could not allocate frame_in for '%s'\n", stream->url);
        goto error_cleanup;
    }

    stream->frame_buff_size = av_image_get_buffer_size(
        stream->frame_out_pix_fmt, stream->codec_ctx->width,
        stream->codec_ctx->height, BYTE_ALIGNMENT);

    if (stream->frame_buff_size < 0) {
        fprintf(stderr,
                "ERROR: not possible to define a number of bytes for frame "
                "buffer in '%s'\n",
                stream->url);
        goto error_cleanup;
    }

    stream->buffer = av_malloc(stream->frame_buff_size * sizeof(uint8_t));
    // uint8_t = 1 byte
    if (stream->buffer == NULL) {
        fprintf(stderr, "ERROR: Could not allocate frame_buffer for '%s'\n",
                stream->url);
        goto error_cleanup;
    }

    stream->packet = av_packet_alloc();
    if (stream->packet == NULL) {
        fprintf(stderr, "ERROR: Could not allocate avpacket for '%s'\n",
                stream->url);
        goto error_cleanup;
    }

    // get scaling context
    stream->sws_ctx =
        sws_getContext(stream->codec_ctx->width, stream->codec_ctx->height,
                       stream->codec_ctx->pix_fmt, stream->codec_ctx->width,
                       stream->codec_ctx->height, stream->frame_out_pix_fmt,
                       SWS_BILINEAR, NULL, NULL, NULL);
    if (stream->sws_ctx == NULL) {
        fprintf(stderr, "ERROR: Failed to get sws_ctx for '%s'\n", stream->url);
        goto error_cleanup;
    }

    return stream;

error_cleanup:

    if (!(opts == NULL)) {
        av_dict_free(&opts);
        opts = NULL;
    }
    rtsp_stream_destroy(&stream);

    return NULL;
}

void rtsp_stream_destroy(struct Rtsp_Stream **stream) {
    if (!stream || !*stream)
        return;

    struct Rtsp_Stream *s = *stream;

    if (s) {

        sws_freeContext(s->sws_ctx);
        s->sws_ctx = NULL;

        if (s->packet) {
            av_packet_free(&(s->packet));
        }

        av_freep(&(s->buffer));

        if (s->decoding_frame) {
            av_frame_free(&(s->decoding_frame));
        }

        if (s->codec_ctx) {
            avcodec_free_context(&(s->codec_ctx));
        }

        if (s->format_ctx) {
            avformat_close_input(&(s->format_ctx));
        }

        free(s->url);
        s->url = NULL;

        free(s);
        s = NULL;
    }
    *stream = NULL;
}

static AVFrame *rtsp_stream_set_scale_avframe(struct Rtsp_Stream *stream) {
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate memory for AVFrame object\n");
        return NULL;
    }

    int ret = av_image_fill_arrays(
        frame->data, frame->linesize, stream->buffer, stream->frame_out_pix_fmt,
        stream->codec_ctx->width, stream->codec_ctx->height, BYTE_ALIGNMENT);
    if (ret < 0) {
        fprintf(stderr, "Could not fill AVFrame object\n");
        return NULL;
    }
    return frame;
}

static int rtsp_stream_decode_frame(struct Rtsp_Stream *stream) {
    int status = 0;
    int has_new_frame = 0;

    while (!has_new_frame) {

        // get decoded frame
        status =
            avcodec_receive_frame(stream->codec_ctx, stream->decoding_frame);
        if (status == AVERROR(EAGAIN)) {
            // read a packet from the stream
            if (av_read_frame(stream->format_ctx, stream->packet) < 0) {
                return -2; // no more packets
            }

            // send packet only if it is a video frame
            if (stream->packet->stream_index == stream->video_stream_id) {
                status = avcodec_send_packet(stream->codec_ctx, stream->packet);
                if (status < 0) {
                    fprintf(stderr, "ERROR: could not send packet ('%s').\n",
                            stream->url);
                    av_packet_unref(stream->packet);
                    return -1;
                }
            }
            av_packet_unref(stream->packet);
        } else if (status < 0) {
            fprintf(stderr, "ERROR: decoding failed ('%s').\n", stream->url);
            return -1;
        } else {
            // frame received successfully
            has_new_frame = 1;
        }
    }
    return 0;
}

static int rtsp_stream_fill_scaled_frame(AVFrame *dst,
                                         struct Rtsp_Stream *stream) {
    int ret = 0;
    ret = sws_scale(stream->sws_ctx,
                    (uint8_t const *const *)stream->decoding_frame->data,
                    stream->decoding_frame->linesize, 0,
                    stream->codec_ctx->height, dst->data, dst->linesize);

    if (ret != dst->height) {
        return -1;
    }
    return 0;
}

SDL_Texture *rtsp_stream_get_sdl_texture(struct Rtsp_Stream *stream,
                                         SDL_Renderer *renderer) {
    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        stream->codec_ctx->width, stream->codec_ctx->height);
    return texture;
}

void *rtsp_stream_threaded_create(void *arg) {
    struct Rtsp_Stream_Create_Params *shared_vars =
        (struct Rtsp_Stream_Create_Params *)arg;

    shared_vars->out_stream =
        rtsp_stream_create(shared_vars->in_url, shared_vars->in_protocol);

    shared_vars->out_return_val = (shared_vars->out_stream == NULL) ? -1 : 0;

    return NULL;
}

void *rtsp_stream_threaded_get_frame(void *arg) {
    // TODO: use enum for detailed error code.

    struct Rtsp_Stream_Get_Frame_Params *shared_vars =
        (struct Rtsp_Stream_Get_Frame_Params *)arg;

    /* Initialization *********************************************************/

    atomic_store(&(shared_vars->thread_status), 1);
    pthread_mutex_lock(shared_vars->mutex);
    shared_vars->out_frame_data = NULL;
    pthread_mutex_unlock(shared_vars->mutex);

    int retry_count = 0;
    char *url = NULL;
    AVFrame *temp = NULL;
    AVFrame *internal_write_frame = NULL;
    AVFrame *internal_next_frame = NULL;

    url = strdup(shared_vars->stream->url);
    if (!url) {
        fprintf(stderr,
                "ERROR: Could not allocate memory in thread to get decoded "
                "frame from from '%s'\n",
                shared_vars->stream->url);
        goto error_init_cleanup;
    }

    internal_write_frame = rtsp_stream_set_scale_avframe(shared_vars->stream);
    internal_next_frame = rtsp_stream_set_scale_avframe(shared_vars->stream);

    if (!internal_write_frame || !internal_next_frame) {
        fprintf(stderr,
                "ERROR: could not allocate internal frames for decoding thread "
                ":%s\n",
                url);
        goto error_init_cleanup;
    }

    atomic_store(&(shared_vars->thread_status), 0); /* initialization done */

    /* Loop to get scaled frames **********************************************/
    while (!atomic_load(&(shared_vars->stop))) {
        int ret = rtsp_stream_decode_frame(shared_vars->stream);

        if (ret < 0) {
            if (retry_count < 100) {
                retry_count++;
                continue;
            } else {
                fprintf(
                    stderr,
                    "ERROR: could not get decoded frame in thread '%s' after "
                    "100 retries.\n",
                    url);
                goto error_cleanup;
            }
            continue;
        } else {
            rtsp_stream_fill_scaled_frame(internal_write_frame,
                                          shared_vars->stream);

            pthread_mutex_lock(shared_vars->mutex);

            // expose newly scaled frame
            shared_vars->out_frame_data = internal_write_frame;

            // next write for the next iteration
            temp = internal_write_frame;
            internal_write_frame = internal_next_frame;
            internal_next_frame = temp;
            temp = NULL;

            pthread_mutex_unlock(shared_vars->mutex);
        }
    }

    /* Cleanup ****************************************************************/
    atomic_store(&(shared_vars->thread_status), 0);
    goto cleanup;

error_cleanup:
    atomic_store(&(shared_vars->thread_status), -1);
    goto cleanup;

error_init_cleanup:
    atomic_store(&(shared_vars->thread_status), -2);
    goto cleanup;

cleanup:
    fprintf(stderr, "Closing thread: '%s'...\n", url);

    pthread_mutex_lock(shared_vars->mutex);
    shared_vars->out_frame_data = NULL;
    pthread_mutex_unlock(shared_vars->mutex);

    if (internal_write_frame) {
        av_frame_free(&internal_write_frame);
    }

    if (internal_next_frame) {
        av_frame_free(&internal_next_frame);
    }

    if (url) {
        free(url);
        url = NULL;
    }

    atomic_store(&(shared_vars->thread_finished), 1);
    return NULL;
}
