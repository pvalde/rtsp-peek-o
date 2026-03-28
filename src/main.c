#include "cli_args.h"
#include <stdbool.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "menu_bar.h"
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"
#include "stream.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>

#define PROG_NAME "rtsp-peek"
#define MENU_BAR_HEIGHT 35

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

    pthread_t threads[args.n_of_rtsp_urls];
    struct Stream_Create_Params threads_result[args.n_of_rtsp_urls];
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

    sdl_ctx.renderer = SDL_CreateRenderer(sdl_ctx.window, -1,
                                          SDL_RENDERER_ACCELERATED |
                                              SDL_RENDERER_PRESENTVSYNC);

    if (!sdl_ctx.renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting.\n");
        ret = -1;
        goto cleanup;
    }

    SDL_GL_SetSwapInterval(1);

    enum Stream_Protocol protocol = UDP;
    /* for (int i = 0; i < args.n_of_rtsp_urls; i++) { */
    /*     streams_arr.streams[i] = stream_create(args.rtsp_urls[i], protocol);
     */
    /*     if (streams_arr.streams[i] == NULL) { */
    /*         ret = -1; */
    /*         goto cleanup; */
    /*     } */
    /*     streams_arr.length++; */
    /* } */

    // using threads

    for (int i = 0; i < args.n_of_rtsp_urls; i++) {
        threads_result[i].in_protocol = protocol;
        threads_result[i].out_stream = NULL;
        threads_result[i].in_url = args.rtsp_urls[i];
        threads_result[i].out_return_val = 1;
        pthread_create(&threads[i], NULL, stream_create_thread,
                       &threads_result[i]);
    }

    for (int i = 0; i < args.n_of_rtsp_urls; i++) {
        pthread_join(threads[i], NULL);
    }

    for (int i = 0; i < args.n_of_rtsp_urls; i++) {
        streams_arr.streams[i] = threads_result[i].out_stream;
        streams_arr.length++;
    }

    for (int i = 0; i < args.n_of_rtsp_urls; i++) {
        if (threads_result[i].out_return_val < 0) {
            ret = -1;
            goto cleanup;
        }
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

void main_loop(Graphics_context *sdl_ctx, Streams_arr *s_arr) {
    int running = 1;
    int ren_w, ren_h;
    SDL_GetRendererOutputSize(sdl_ctx->renderer, &ren_w, &ren_h);

    float height_weight = 1.0 / (float)s_arr->length;
    SDL_Rect pos_rects[s_arr->length];
    struct Stream_Frame_Data frame_data[s_arr->length];

    SDL_Texture **ind_textures = calloc(s_arr->length, sizeof(SDL_Texture *));
    for (int i = 0; i < s_arr->length; i++) {
        ind_textures[i] =
            stream_get_sdl_texture(s_arr->streams[i], sdl_ctx->renderer);
    }

    SDL_Event event;

    /* NK GUI */
    struct nk_rect menu_bound = nk_rect(0, 0, ren_w, MENU_BAR_HEIGHT);
    nk_flags window_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR;

    struct nk_context *nk_ctx = nk_sdl_init(sdl_ctx->window, sdl_ctx->renderer);
    struct nk_font_atlas *atlas;
    struct nk_font *font;

    nk_sdl_font_stash_begin(&atlas);
    font = nk_font_atlas_add_default(atlas, 14, 0);
    nk_sdl_font_stash_end();
    nk_style_set_font(nk_ctx, &font->handle);

    while (running) {
        nk_input_begin(nk_ctx);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_GetRendererOutputSize(sdl_ctx->renderer, &ren_w, &ren_h);
                menu_bound = nk_rect(0, 0, ren_w, MENU_BAR_HEIGHT);
            }
            nk_sdl_handle_event(&event);
        }
        nk_sdl_handle_grab();
        nk_input_end(nk_ctx);

        SDL_RenderClear(sdl_ctx->renderer);

        /* GUI */

        /* Top menu */
        if (nk_begin(nk_ctx, "MENU_BAR", menu_bound, window_flags)) {

            nk_layout_row_begin(nk_ctx, NK_STATIC, 25, 5);

            nk_layout_row_push(nk_ctx, 60);
            if (nk_menu_begin_label(nk_ctx, "MENU", NK_TEXT_LEFT,
                                    nk_vec2(200, 600))) {

                menu_quit(nk_ctx, &running);
                nk_menu_end(nk_ctx);
            }
        }

        nk_end(nk_ctx);

        for (int i = 0; i < s_arr->length; i++) {
            stream_get_decoded_frame(&(frame_data[i]), s_arr->streams[i]);
        }

        int height =
            (int)(round)((((float)ren_h) - MENU_BAR_HEIGHT) * height_weight);
        for (int i = 0; i < s_arr->length; i++) {

            // make space
            pos_rects[i].x = 0;
            pos_rects[i].y = (i * height) + MENU_BAR_HEIGHT;
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

                SDL_RenderCopy(sdl_ctx->renderer, ind_textures[i], NULL,
                               &pos_rects[i]);
            }
        }

        nk_sdl_render(NK_ANTI_ALIASING_ON);

        SDL_RenderPresent(sdl_ctx->renderer);
    }

    for (int i = 0; i < s_arr->length; i++) {
        if (ind_textures[i])
            SDL_DestroyTexture(ind_textures[i]);
    }

    nk_sdl_shutdown();
    free(ind_textures);
    ind_textures = NULL;
}
