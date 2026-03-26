#include "cli_args.h"
#include "stream.h"
#include <SDL2/SDL.h>
#include <stdio.h>

#define PROG_NAME "rtsp-peek"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool sdl_initialized;
} Graphics_context;

typedef struct {
    Stream **streams;
    int length;
} Streams_arr;

void graphics_context_cleanup(Graphics_context *ctx);

void main_loop(Graphics_context *ctx, Streams_arr *s_arr);

int main(int argc, char *argv[]) {

    Streams_arr streams_arr = {.streams = NULL, .length = 0};

    int ret;
    Cli_Args args = {0};
    Graphics_context sdl_ctx = {
        .window = NULL, .renderer = NULL, .sdl_initialized = false};

    ret = parse_args(argc, argv, &args);

    if (ret == PARSE_ERROR) {
        return -1;
    } else if (ret == PARSE_HELP) {
        return 0;
    }

    streams_arr.streams = calloc(args.n_of_rtsp_urls, sizeof(Stream *));
    if (!streams_arr.streams) {
        fprintf(stderr, "Failed to allocate memory for streams\n");
        ret = -1;
        goto cleanup;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        ret = -1;
        goto cleanup;
    }
    sdl_ctx.sdl_initialized = true;

    avformat_network_init(); // TODO check if remove is safe

    sdl_ctx.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED, 800, 600,
                                      SDL_WINDOW_RESIZABLE);

    if (!sdl_ctx.window) {
        fprintf(stderr, "SDL: could not set video mode - exiting.\n");
        ret = -1;
        goto cleanup;
    }

    sdl_ctx.renderer =
        SDL_CreateRenderer(sdl_ctx.window, -1, SDL_RENDERER_ACCELERATED);

    if (!sdl_ctx.renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting.\n");
        ret = -1;
        goto cleanup;
    }

    SDL_GL_SetSwapInterval(1);

    enum Stream_Protocol protocol = TCP;
    for (int i = 0; i < args.n_of_rtsp_urls; i++) {
        streams_arr.streams[i] = stream_create(args.rtsp_urls[i], protocol);
        if (streams_arr.streams[i] == NULL) {
            ret = -1;
            goto cleanup;
        }
        streams_arr.length++;
    }

    main_loop(&sdl_ctx, &streams_arr);

cleanup:
    for (int i = 0; i < streams_arr.length; i++) {
        stream_destroy(&(streams_arr.streams[i]));
        streams_arr.streams[i] = NULL;
    }
    if (streams_arr.streams != NULL) {
        free(streams_arr.streams);
        streams_arr.streams = NULL;
    }

    graphics_context_cleanup(&sdl_ctx);
    Cli_args_clean_up(&args);
    avformat_network_deinit();
    if (ret == 0)
        printf("=============" PROG_NAME " CLOSED NORMALLY============\n");
    return ret < 0 ? -1 : 0;
}

void graphics_context_cleanup(Graphics_context *ctx) {
    if (!ctx)
        return;

    if (ctx->renderer) {
        SDL_DestroyRenderer(ctx->renderer);
        ctx->renderer = NULL;
    }

    if (ctx->window) {
        SDL_DestroyWindow(ctx->window);
        ctx->window = NULL;
    }

    if (ctx->sdl_initialized) {
        SDL_Quit();
        ctx->sdl_initialized = false;
    }
}

void main_loop(Graphics_context *g_ctx, Streams_arr *s_arr) {
    int running = 1;
    int ren_w, ren_h;
    SDL_GetRendererOutputSize(g_ctx->renderer, &ren_w, &ren_h);

    float height_weight = 1.0 / (float)s_arr->length;
    SDL_Rect pos_rects[s_arr->length];
    struct Stream_Frame_Data frame_data[s_arr->length];

    SDL_Texture **ind_textures = calloc(s_arr->length, sizeof(SDL_Texture *));
    for (int i = 0; i < s_arr->length; i++) {
        ind_textures[i] =
            stream_get_sdl_texture(s_arr->streams[i], g_ctx->renderer);
    }

    SDL_Event event;
    while (running) {

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_GetRendererOutputSize(g_ctx->renderer, &ren_w, &ren_h);
            }
        }

        for (int i = 0; i < s_arr->length; i++) {
            stream_get_decoded_frame(&(frame_data[i]), s_arr->streams[i]);
        }

        SDL_RenderClear(g_ctx->renderer);

        int height = (int)(round)((float)ren_h * height_weight);
        for (int i = 0; i < s_arr->length; i++) {

            // make space
            pos_rects[i].x = 0;
            pos_rects[i].y = i * height;
            pos_rects[i].w = ren_w;
            pos_rects[i].h = height;

            if (frame_data[i].frame != NULL) {
                SDL_UpdateYUVTexture(ind_textures[i], NULL,
                                     frame_data[i].frame->data[0],
                                     frame_data[i].frame->linesize[0],
                                     frame_data[i].frame->data[1],
                                     frame_data[i].frame->linesize[1],
                                     frame_data[i].frame->data[2],
                                     frame_data[i].frame->linesize[2]);

                SDL_RenderCopy(g_ctx->renderer, ind_textures[i], NULL,
                               &pos_rects[i]);
            }
        }

        SDL_RenderPresent(g_ctx->renderer);
    }

    for (int i = 0; i < s_arr->length; i++) {
        if (ind_textures[i])
            SDL_DestroyTexture(ind_textures[i]);
    }

    free(ind_textures);
    ind_textures = NULL;
}
