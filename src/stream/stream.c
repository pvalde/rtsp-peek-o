#include "stream_private.h"
#include <assert.h>
#include <stdio.h>

#define BYTE_ALIGNMENT 32

enum AVPixelFormat get_qsv_format(AVCodecContext *ctx,
                                  const enum AVPixelFormat *pix_fmts);

struct Stream *stream_create(const char *url, enum Stream_Protocol protocol) {

    if (url == NULL) {
        fprintf(stderr, "ERROR: url is NULL\n");
        return NULL;
    }

    int err_code = 0;
    char *transport_protocol = NULL;
    char err_msg[AV_ERROR_MAX_STRING_SIZE];
    AVDictionary *opts = NULL;

    struct Stream *stream = calloc(1, sizeof(*stream));
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

    stream->frame_tmp = av_frame_alloc();
    if (stream->frame_tmp == NULL) {
        goto error_cleanup;
    }

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

    stream->frame_in = av_frame_alloc();
    if (stream->frame_in == NULL) {
        fprintf(stderr, "Could not allocate frame_in for '%s'\n", stream->url);
        goto error_cleanup;
    }
    stream->frame_out = av_frame_alloc();
    if (stream->frame_out == NULL) {
        fprintf(stderr, "Could not allocate frame_out for '%s'\n", stream->url);
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

    err_code = av_image_fill_arrays(
        stream->frame_out->data, stream->frame_out->linesize, stream->buffer,
        stream->frame_out_pix_fmt, stream->codec_ctx->width,
        stream->codec_ctx->height, BYTE_ALIGNMENT);

    if (err_code < 0) {
        fprintf(stderr, "ERROR: Failed to setup frame_buffer for '%s'\n",
                stream->url);
        goto error_cleanup;
    }

    stream->packet = av_packet_alloc();
    if (stream->packet == NULL) {
        fprintf(stderr, "ERROR: Could not allocate avpacket for '%s'\n",
                stream->url);
        goto error_cleanup;
    }

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
    stream_destroy(&stream);

    return NULL;
}

void stream_destroy(struct Stream **stream) {
    if (!stream || !*stream)
        return;

    struct Stream *s = *stream;

    if (s != NULL) {

        sws_freeContext(s->sws_ctx);
        s->sws_ctx = NULL;

        if (s->packet != NULL) {
            av_packet_free(&(s->packet));
            assert(s->packet == NULL);
        }

        av_freep(&(s->buffer));
        assert(s->buffer == NULL);

        if (s->frame_out != NULL) {
            av_frame_free(&(s->frame_out));
            assert(s->frame_out == NULL);
        }

        if (s->frame_in != NULL) {
            av_frame_free(&(s->frame_in));
            assert(s->frame_in == NULL);
        }

        if (s->codec_ctx != NULL) {
            avcodec_free_context(&(s->codec_ctx));
            assert(s->codec_ctx == NULL);
        }

        if (s->format_ctx != NULL) {
            avformat_close_input(&(s->format_ctx));
            assert(s->format_ctx == NULL);
        }

        if (s->frame_tmp != NULL) {
            av_frame_free(&(s->frame_tmp));
            s->frame_tmp = NULL;
        }

        free(s->url);
        s->url = NULL;

        free(s);
        s = NULL;
    }
    *stream = NULL;
}

int stream_get_decoded_frame(struct Stream_Frame_Data *out_frame_data,
                             struct Stream *stream) {
    int status = 0;
    int has_new_frame = 0;
    AVFrame *last_frame = stream->frame_tmp;

    while (!has_new_frame) {

        // get decoded frame
        status = avcodec_receive_frame(stream->codec_ctx, stream->frame_in);
        if (status == AVERROR(EAGAIN)) {
            // read a packet from the stream
            if (av_read_frame(stream->format_ctx, stream->packet) < 0) {
                return -1; // no more packets
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
            av_frame_unref(last_frame);
            av_frame_ref(last_frame, stream->frame_in);
        }
    }

    // scale and populate output
    if (has_new_frame) {
        sws_scale(stream->sws_ctx, (uint8_t const *const *)last_frame->data,
                  last_frame->linesize, 0, stream->codec_ctx->height,
                  stream->frame_out->data, stream->frame_out->linesize);

        out_frame_data->frame =
            stream->frame_out; // must not be freed or modified from outside.
        out_frame_data->x = 0;
        out_frame_data->y = 0;
        out_frame_data->w = stream->codec_ctx->width;
        out_frame_data->h = stream->codec_ctx->height;
    } else {
        out_frame_data->frame = NULL;
    }

    if (has_new_frame) {
        return 0;
    } else {
        return -1;
    }
}

SDL_Texture *stream_get_sdl_texture(struct Stream *stream,
                                    SDL_Renderer *renderer) {
    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        stream->codec_ctx->width, stream->codec_ctx->height);
    return texture;
}

void *stream_create_thread(void *arg) {
    struct Stream_Create_Params *shared_vals =
        (struct Stream_Create_Params *)arg;

    shared_vals->out_stream =
        stream_create(shared_vals->in_url, shared_vals->in_protocol);

    shared_vals->out_return_val = (shared_vals->out_stream == NULL) ? -1 : 0;

    return NULL;
}
