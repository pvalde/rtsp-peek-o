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
#include "options.h"
#include "rtsp_stream.h"
#include "stream_manager.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#define PROG_NAME "rtsp-peek"
#define MENU_BAR_HEIGHT 35
#define MAX_URL_LENGTH 1024
#define MAX_URLS 16

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    bool sdl_initialized;
} Graphics_context;

typedef struct {
    int rows;
    int columns;
} Layout;

typedef struct {
    char **rtsp_urls;
    int rtsp_urls_count;
    Layout layout;
    SM_Data *sm_data;
} Main_State;

void graphics_context_cleanup(Graphics_context *ctx);

void main_loop(Graphics_context *sdl_ctx, Main_State *main_state);

int set_pos(SDL_Rect *out_pos_rect, int index, Layout layout,
            int renderer_height, int renderer_width, int padding_top);

void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS] [URL1 URL2 ...]\n\n", prog_name);
    printf("RTSP stream grid viewer.\n\n");
    printf("Options:\n");
    printf("  -g, --grid <cols>x<rows>  Specify the layout grid dimensions "
           "(e.g., --grid 3x2).\n");
    printf("                            Both dimensions must be integers "
           "greater than 0.\n");
    printf("  -h, --help                Show this help message and exit.\n\n");
    printf("Input Methods:\n");
    printf("  1. Command Line:  %s --grid 2x2 rtsp://cam1 rtsp://cam2\n",
           prog_name);
    printf("  2. Stdin Piping:  %s -g 2x2 < list_of_urls.txt\n\n", prog_name);
}

int main(int argc, char *argv[]) {

    int main_ret_val = 0;

    App_Options_T options = options_parse(argc, argv);

    if (options.help_requested) {
        print_usage(argv[0]);
        return 0;
    }

    if (options.parse_error) {
        fprintf(stderr, "Error: Invalid arguments or malformed grid layout "
                        "configuration.\n");
        print_usage(argv[0]);
        return 1;
    }

    Main_State main_state = {0};
    int max_capacity = options.columns * options.rows;
    max_capacity = max_capacity ? max_capacity : MAX_URLS;

    main_state.rtsp_urls = calloc(max_capacity, sizeof(char *));

    if (!main_state.rtsp_urls) {
        fprintf(stderr, "Fatal: Out of memory allocating URL pointer array.");
        main_ret_val = 1;
        goto cleanup;
    }

    if (options.url_count > 0) {

        main_state.rtsp_urls_count = options.url_count;
        for (int i = 0; i < options.url_count; i++) {
            main_state.rtsp_urls[i] = argv[options.url_start_index + i];
        }
        main_state.rtsp_urls_count = options.url_count;
    } else {

        int captured = 0;

        while (captured < max_capacity) {
            main_state.rtsp_urls[captured] =
                calloc(MAX_URL_LENGTH, sizeof(char));
            if (!main_state.rtsp_urls[captured]) {
                fprintf(stderr, "Fatal: Out of memory allocating URL buffer.");
                main_ret_val = 1;
                goto cleanup;
            }

            if (fgets(main_state.rtsp_urls[captured], MAX_URL_LENGTH, stdin) ==
                NULL) {
                free(main_state.rtsp_urls[captured]); // clean unused buffer
                break;
            }

            // strip trailing new line
            main_state.rtsp_urls[captured][strcspn(
                main_state.rtsp_urls[captured], "\n")] = '\0';

            // skip empty lines
            if (strlen(main_state.rtsp_urls[captured]) == 0) {
                free(main_state.rtsp_urls[captured]); // throw away mem for
                                                      // empty inputs
                continue; // overwrites this buffer slot on the next iteration
            }

            captured++;
        }
        main_state.rtsp_urls_count = captured;
    }

    if (main_state.rtsp_urls_count == 0) {
        fprintf(stderr,
                "Error: No RTSP URLs provided via arguments or stdin.\n");
        print_usage(argv[0]);
        main_ret_val = 1;
        goto cleanup;
    }

    Graphics_context sdl_ctx = {
        .window = NULL, .renderer = NULL, .sdl_initialized = false};

    if (options.rows < 1 || options.columns < 1) {
        main_state.layout.rows = main_state.rtsp_urls_count; // default value
        main_state.layout.columns = 1;                       // default value
    } else {
        main_state.layout.rows = options.rows;
        main_state.layout.columns = options.columns;
    }

    main_state.sm_data = stream_manager_init(main_state.rtsp_urls_count);
    if (!main_state.sm_data) {
        fprintf(stderr, "Failed to allocate memory for initial streams\n");
        main_ret_val = 1;
        goto cleanup;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        main_ret_val = 1;
        goto cleanup;
    }
    sdl_ctx.sdl_initialized = true;

    avformat_network_init(); // TODO check if remove is safe

    sdl_ctx.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED, 800, 600,
                                      SDL_WINDOW_RESIZABLE);

    if (!sdl_ctx.window) {
        fprintf(stderr, "SDL: could not set video mode - exiting.\n");
        main_ret_val = 1;
        goto cleanup;
    }

    sdl_ctx.renderer = SDL_CreateRenderer(sdl_ctx.window, -1,
                                          SDL_RENDERER_ACCELERATED |
                                              SDL_RENDERER_PRESENTVSYNC);

    if (!sdl_ctx.renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting.\n");
        main_ret_val = 1;
        goto cleanup;
    }

    SDL_GL_SetSwapInterval(1);

    enum Transport_Protocol protocol = TCP;

    int sm_status = stream_manager_rtsp_streams_init_th(
        main_state.sm_data, main_state.rtsp_urls, main_state.rtsp_urls_count,
        protocol);

    if (sm_status < 0) {
        fprintf(stderr, "sm failed to create initial streams\n");
        main_ret_val = 1;
        goto cleanup;
    }

    main_loop(&sdl_ctx, &main_state);

cleanup:
    stream_manager_cleanup(&main_state.sm_data);

    // clean main_state.rtsp_urls
    if (options.url_count == 0) { // Only free if we allocated via stdin
        for (int i = 0; i < main_state.rtsp_urls_count; i++) {
            free(main_state.rtsp_urls[i]);
        }
    }
    free(main_state.rtsp_urls); // This is always safe to free if calloc'd

    graphics_context_cleanup(&sdl_ctx);
    avformat_network_deinit();

    if (main_ret_val == 0)
        printf("=============" PROG_NAME " CLOSED NORMALLY============\n");
    return main_ret_val < 0 ? -1 : 0;
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

void main_loop(Graphics_context *sdl_ctx, Main_State *main_state) {

    int running = 1;
    int ren_w, ren_h;

    SDL_GetRendererOutputSize(sdl_ctx->renderer, &ren_w, &ren_h);

    int n_of_streams = stream_manager_get_length(main_state->sm_data);
    if (n_of_streams < 0) {
        fprintf(stderr, "FATAL: sm_data is null!");
        // TODO: MUST STOP EXECUTION HERE!
    }

    /* Create SDL_Rects */
    SDL_Rect pos_rects[n_of_streams];

    SDL_Texture **ind_textures = calloc(n_of_streams, sizeof(SDL_Texture *));
    for (int i = 0; i < n_of_streams; i++) {
        ind_textures[i] = rtsp_stream_get_sdl_texture(
            stream_manager_get_rtsp_stream(main_state->sm_data, i),
            sdl_ctx->renderer);
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

    /* start frame decoding in threads */
    pthread_t decoding_frame_threads[n_of_streams];
    pthread_mutex_t decoding_frame_mutexes[n_of_streams];
    struct Rtsp_Stream_Get_Frame_Params thread_args[n_of_streams];
    memset(thread_args, 0,
           sizeof(thread_args)); // zero out the entire array structre

    for (int i = 0; i < n_of_streams; i++) {
        // setting arguments
        thread_args[i].mutex = &(decoding_frame_mutexes[i]);
        atomic_store(&(thread_args[i].stop), 0);
        atomic_store(&(thread_args[i].thread_status), 1);
        thread_args[i].out_frame_data = NULL;
        thread_args[i].stream =
            stream_manager_get_rtsp_stream(main_state->sm_data, i);

        // init mutex
        pthread_mutex_init(&(decoding_frame_mutexes[i]), NULL);

        pthread_create(&(decoding_frame_threads[i]), NULL,
                       rtsp_stream_threaded_get_frame, &(thread_args[i]));
    }
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

        for (int i = 0; i < n_of_streams; i++) {
            switch (atomic_load(&(thread_args[i].thread_status))) {
            case 0:
                // thread is running normally
                break;
            case 1:
                fprintf(stderr,
                        "WARNING: decoding thread no. %d has not finished its "
                        "initialization yet.\n",
                        i);
                break;
            case -1:
                fprintf(stderr,
                        "ERROR: decoding thread no. %d "
                        "has failed\n",
                        i);
                running = 0;
                break;
            case -2:
                fprintf(stderr,
                        "ERROR: decoding thread no. %d has failed at "
                        "initialization stage\n",
                        i);
                running = 0;
                break;
            }
        }

        for (int i = 0; i < n_of_streams; i++) {
            if (set_pos(&(pos_rects[i]), i, main_state->layout, ren_h, ren_w,
                        MENU_BAR_HEIGHT) == 0) {

                pthread_mutex_lock(&(decoding_frame_mutexes[i]));

                AVFrame *frame = thread_args[i].out_frame_data;
                if (frame) {
                    SDL_UpdateYUVTexture(ind_textures[i], NULL, frame->data[0],
                                         frame->linesize[0], frame->data[1],
                                         frame->linesize[1], frame->data[2],
                                         frame->linesize[2]);

                    SDL_RenderCopy(sdl_ctx->renderer, ind_textures[i], NULL,
                                   &pos_rects[i]);
                }

                pthread_mutex_unlock(&(decoding_frame_mutexes[i]));
            }
        }

        nk_sdl_render(NK_ANTI_ALIASING_ON);

        SDL_RenderPresent(sdl_ctx->renderer);
    }

    // send stop signal to threads
    for (int i = 0; i < n_of_streams; i++) {
        atomic_store(&(thread_args[i].stop), 1);
    }

    // wait for threads to stop
    for (int i = 0; i < n_of_streams; i++) {
        pthread_join(decoding_frame_threads[i], NULL);
    }

    for (int i = 0; i < n_of_streams; i++) {
        if (ind_textures[i])
            SDL_DestroyTexture(ind_textures[i]);
    }

    nk_sdl_shutdown();
    free(ind_textures);
    ind_textures = NULL;
}

/**
 * @brief Computes the position and size of a grid cell in a renderer.
 *
 * Maps a linear index (row-major order) into a 2D grid defined by the
 * given layout, and writes the resulting rectangle into @p out_pos_rect.
 *
 * The rendering area is divided evenly into `layout.rows` ×
 * `layout.columns` cells. Each cell has equal width and height. A vertical
 * offset (`padding_top`) is applied to all rows.
 *
 * Index mapping:
 * - Row    = index / layout.columns
 * - Column = index % layout.columns
 *
 * @param[out] out_pos_rect     Output SDL_Rect to store computed position/size.
 * @param[in]  index            Linear index in row-major order.
 * @param[in]  layout           Grid layout (must have rows > 0 and columns >
 * 0).
 * @param[in]  renderer_height  Total height of the rendering area.
 * @param[in]  renderer_width   Total width of the rendering area.
 * @param[in]  padding_top      Vertical offset applied before grid rendering.
 *
 * @return 0   Success.
 * @return -1  Index exceeds grid capacity (rows * columns).
 * @return -2  Invalid layout (rows < 1 or columns < 1).
 * @return -3  Negative index.
 * @return -4  NULL pointer passed for @p out_pos_rect.
 *
 * @note Uses floating-point division and rounding for positioning.
 * @note If rows or columns equals 1, the full dimension is used on that axis.
 */
int set_pos(SDL_Rect *out_pos_rect, int index, Layout layout,
            int renderer_height, int renderer_width, int padding_top) {

    if (!out_pos_rect) {
        return -4; // NULL pointer
    }

    if ((layout.columns < 1) || (layout.rows < 1)) {
        return -2; // invalid layout values.
    }

    if (index < 0) {
        return -3; // invalid index value.
    }

    if ((index) >= (layout.rows * layout.columns)) {
        return -1; // no space to render
    }

    // 0 indicates first row/column

    // ROW POS = INDEX / N_OF_COLS
    int grid_position_row =
        index / layout.columns; // division by 0 is not possible.

    // COL POS = POS % N_OF_COLS
    int grid_position_column = index % layout.columns;

    float row_weight =
        layout.rows > 1 ? ((float)renderer_height - padding_top) / layout.rows
                        : (float)renderer_height - padding_top;
    float column_weight = layout.columns > 1
                              ? (float)renderer_width / layout.columns
                              : (float)renderer_width;

    out_pos_rect->x = (int)(round)(column_weight * (grid_position_column));
    out_pos_rect->y =
        (int)(round)((row_weight * (grid_position_row)) + padding_top);
    out_pos_rect->w = column_weight;
    out_pos_rect->h = row_weight;

    return 0;
}
