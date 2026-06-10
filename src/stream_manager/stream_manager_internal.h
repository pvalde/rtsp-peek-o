#ifndef STREAM_MANAGER_INTERNAL_H
#define STREAM_MANAGER_INTERNAL_H
#include "rtsp_stream.h"
#include <pthread.h>
#include <stdbool.h>

struct ManagedStream {
    pthread_t thread;
    bool thread_created;
    struct RtspStreamSharedState *decoding_state;
    char *stream_url;
};

void managed_stream_destroy(struct ManagedStream **managed_stream);

struct ManagedStream *managed_stream_create();

/* takes ownership of `stream`, copies `stream_url`. ms->decoding_thread is
 * NULL. On error returns negative integer and destroys `ms` */
int managed_stream_fill(struct ManagedStream *ms, Rtsp_Stream *stream,
                        const char *stream_url);

#endif // STREAM_MANAGER_INTERNAL_H
