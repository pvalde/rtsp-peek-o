/* #include "stream_manager.h" */
/* #include <pthread.h> */

/* struct SM_Data { */
/*     Rtsp_Stream **streams_array; */
/*     unsigned int n_of_streams; */
/*     unsigned int streams_array_capacity; */
/* }; */

/* enum Transport_Protocol transport_protocol = TCP; */

/* struct SM_Data *stream_manager_init_old(char **streams_url, */
/*                                         int number_of_streams) { */
/*     struct SM_Data *data = calloc(1, sizeof(struct SM_Data)); */
/*     if (!data) { */
/*         return NULL; */
/*     } */

/*     data->streams_array = calloc(number_of_streams, sizeof(Rtsp_Stream *));
 */
/*     if (!data->streams_array) { */
/*         free(data); */
/*         return NULL; */
/*     } */

/*     data->streams_array_capacity = number_of_streams; */

/*     for (int i = 0; i < number_of_streams; i++) { */

/*         Rtsp_Stream *stream = */
/*             rtsp_stream_create(streams_url[i], transport_protocol); */
/*         if (!stream) { */
/*             goto error_cleanup; */
/*         } */
/*         data->streams_array[i] = stream; */
/*         data->n_of_streams++; */
/*     } */

/*     return data; */

/* error_cleanup: */

/*     stream_manager_cleanup(&data); */
/*     return NULL; */
/* } */

/* SM_Data *stream_manager_init(int size) { */
/*     struct SM_Data *data = calloc(1, sizeof(struct SM_Data)); */
/*     if (!data) { */
/*         return NULL; */
/*     } */

/*     data->streams_array = calloc(size, sizeof(Rtsp_Stream *)); */
/*     if (!data->streams_array) { */
/*         free(data); */
/*         return NULL; */
/*     } */

/*     data->streams_array_capacity = size; */

/*     return data; */
/* } */

/* int stream_manager_rtsp_streams_init_th(SM_Data *data, char **streams_url, */
/*                                         int number_of_streams, */
/*                                         enum Transport_Protocol protocol) {
 */
/*     if (!data) { */
/*         return -1; // null data */
/*     } */
/*     if ((unsigned)number_of_streams > data->streams_array_capacity) { */
/*         return -2; // not enough capacity... (for now, in the future this
 * will */
/*                    // modify the underlying array's length) */
/*     } */
/*     pthread_t threads[number_of_streams]; */
/*     struct RtspStreamSharedInitContext threads_result[number_of_streams]; */

/*     for (int i = 0; i < number_of_streams; i++) { */
/*         threads_result[i].in_protocol = protocol; */
/*         threads_result[i].out_stream = NULL; */
/*         threads_result[i].in_url = streams_url[i]; */
/*         threads_result[i].out_return_val = 1; */
/*         pthread_create(&threads[i], NULL, rtsp_stream_threaded_create, */
/*                        &threads_result[i]); */
/*     } */

/*     for (int i = 0; i < number_of_streams; i++) { */
/*         pthread_join(threads[i], NULL); */
/*     } */

/*     data->n_of_streams = 0; */

/*     for (int i = 0; i < number_of_streams; i++) { */
/*         data->streams_array[i] = threads_result[i].out_stream; */
/*         data->n_of_streams++; */
/*     } */

/*     for (int i = 0; i < number_of_streams; i++) { */
/*         if (threads_result[i].out_return_val < 0) { */
/*             return -3; // at least one stream didn't initialize correctly */
/*             // in the future we can deal with this we don't exit the whole */
/*             // program. */
/*         } */
/*     } */

/*     return 0; */
/* } */

/* void stream_manager_cleanup(SM_Data **data) { */

/*     if (!data || !*data) { */
/*         return; */
/*     } */

/*     SM_Data *data_internal = *data; */

/*     for (unsigned int i = 0; i < data_internal->n_of_streams; i++) { */
/*         rtsp_stream_destroy(&(data_internal->streams_array[i])); */
/*     } */

/*     free(data_internal->streams_array); */
/*     data_internal->streams_array = NULL; */
/*     free(data_internal); */
/*     *data = NULL; */
/* } */

/* Rtsp_Stream *stream_manager_get_rtsp_stream(SM_Data *data, int index) { */

/*     if (!data) */
/*         return NULL; */
/*     if (index < 0) */
/*         return NULL; */
/*     if ((unsigned int)index >= data->n_of_streams) */
/*         return NULL; */

/*     return data->streams_array[index]; */
/* } */

/* int stream_manager_get_length(SM_Data *data) { */
/*     if (!data) */
/*         return -1; */

/*     return data->n_of_streams; */
/* } */
