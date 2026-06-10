#include "stream_manager2.h"
#include "rtsp_stream.h"
#include "stream_manager_internal.h"
#include <pthread.h>

void managed_stream_destroy(struct ManagedStream **managed_stream) {
    if (!managed_stream || !*managed_stream) {
        return;
    }

    struct ManagedStream *ms = *managed_stream;

    if (ms->thread_created) {
        pthread_join(ms->thread, NULL);
        ms->thread_created = false;
    }

    if (ms->decoding_state) {
        rtsp_stream_shared_state_destroy(ms->decoding_state);
        ms->decoding_state = NULL;
    }

    if (ms->stream_url) {
        free((*managed_stream)->stream_url);
        ms->stream_url = NULL;
    }

    free(ms);
    *managed_stream = NULL;

    return;
}

struct ManagedStream *managed_stream_create() {
    struct ManagedStream *managed_stream =
        calloc(1, sizeof(struct ManagedStream));
    if (!managed_stream) {
        return NULL; // allocation error.
    }
    return managed_stream;
}

/* takes ownership of `stream`, copies `stream_url`. ms->decoding_thread is
 * NULL. On error returns negative integer and destroys `ms` */
int managed_stream_fill(struct ManagedStream *ms, Rtsp_Stream *stream,
                        const char *stream_url) {
    if (!ms || !stream || !stream_url) {
        return -1;
    }

    /* create internal copy of stream_url */
    ms->stream_url = strdup(stream_url);
    if (!ms->stream_url) {
        goto fail;
    }

    if (!(ms->decoding_state = rtsp_stream_shared_state_create(stream))) {
        goto fail;
    }

    ms->thread_created = false;

    return 0;

fail:
    if (ms->stream_url) {
        free(ms->stream_url);
        ms->stream_url = NULL;
    }
    return -1;
}

struct StreamManager {
    struct ManagedStream **streams;
    int stream_count;
};

void stream_manager_destroy(struct StreamManager **stream_manager) {
    if (!stream_manager || !*stream_manager) {
        return;
    }

    struct StreamManager *sm = *stream_manager;

    if (sm->streams) {
        for (int i = 0; i < sm->stream_count; i++) {
            managed_stream_destroy(&sm->streams[i]);
        }
        free(sm->streams);
        sm->streams = NULL;
    }

    free(sm);
    *stream_manager = NULL;
}

/* Creates an empty `StreamManager` object */
struct StreamManager *stream_manager_create() {
    struct StreamManager *sm = calloc(1, sizeof(struct StreamManager));

    if (!sm) {
        return NULL;
    }

    return sm;
}

/* stream_manager_init */
/* create streams */
/* urls are borrowed only, internal copies will be created */
int stream_manager_init(struct StreamManager *sm, const char **urls,
                        int urls_count, enum Transport_Protocol protocol) {

    /* TODO: CLEAN/SIMPLIFY this function */
    // use the creator and destructor for the intercommunication structs!

    bool error = false;

    if (!sm || !urls || urls_count <= 0) {
        return -1;
    }

    /* Allocate managed streams */
    sm->stream_count = urls_count;

    bool ms_stream_alloc_error = false;
    sm->streams = calloc(urls_count, sizeof(struct ManagedStream *));
    for (int i = 0; i < urls_count; i++) {
        sm->streams[i] = managed_stream_create();
        if (!sm->streams[i]) {
            ms_stream_alloc_error = true;
        }
    }

    if (ms_stream_alloc_error) {
        for (int i = 0; i < urls_count; i++) {
            if (sm->streams[i]) {
                managed_stream_destroy(&sm->streams[i]);
            }
        }
        free(sm->streams);
        sm->streams = NULL;
        return -1;
    }

    /* Initialize Streams */
    pthread_t *init_threads = calloc(sm->stream_count, sizeof(pthread_t));
    if (!init_threads) {
        stream_manager_destroy(&sm);
        return -1;
    }

    struct RtspStreamSharedInitContext *stream_init_context =
        calloc(sm->stream_count, sizeof(struct RtspStreamSharedInitContext));
    if (!stream_init_context) {
        free(init_threads);
        stream_manager_destroy(&sm);
        return -1;
    }

    /* run init threads */
    bool failed_thread_init = false;
    int threads_started = 0;
    for (int i = 0; i < sm->stream_count; i++) {
        stream_init_context[i].out_return_val = 1;
        stream_init_context[i].in_protocol = protocol;
        stream_init_context[i].in_url = urls[i];
        stream_init_context[i].out_stream = NULL;

        if (pthread_create(&init_threads[i], NULL, rtsp_stream_threaded_create,
                           &stream_init_context[i]) != 0) {
            failed_thread_init = true;
            break;
        } else {
            threads_started++;
        }
    }

    /* wait for threads to finish */
    for (int i = 0; i < threads_started; i++) {
        pthread_join(init_threads[i], NULL);
    }

    if (failed_thread_init) {
        error = true;
        goto cleanup;
    }

    bool failed_ms_fill = false;
    for (int i = 0; i < sm->stream_count; i++) {
        if (stream_init_context[i].out_return_val < 0) {
            fprintf(stderr, "Warning: stream failed to initialize:\n%s\n",
                    urls[i]);
        }
        if (managed_stream_fill(sm->streams[i],
                                stream_init_context[i].out_stream,
                                urls[i]) < 0) {
            failed_ms_fill = true;
        }
    }
    if (failed_ms_fill) {
        error = true;
        goto cleanup;
    }

    goto cleanup;

cleanup:
    if (init_threads) {
        free(init_threads);
    }
    if (stream_init_context) {
        free(stream_init_context);
    }

    if (error) {
        return -1;
    }
    return 0;
}

int stream_manager_start_decoding_workers(struct StreamManager *sm) {
    // TODO
    return 0;
}

int stream_manager_stop_decoding_workers(struct StreamManager *sm) {
    // TODO
    return 0;
}

int stream_manager_get_length(struct StreamManager *sm) {
    return sm->stream_count;
}

int stream_manager_check_status(struct StreamManager *sm) {
    // TODO
    return 0;
}

// int stream_manager_get_frame ?? TODO: function signature
