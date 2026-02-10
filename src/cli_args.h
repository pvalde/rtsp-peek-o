#ifndef CLI_ARGS_H
#define CLI_ARGS_H
#include <stdbool.h>

typedef enum { PARSE_OK = 0, PARSE_HELP = 1, PARSE_ERROR = -1 } ParseStatus;

typedef struct {
  int n_of_rtsp_urls;
  char **rtsp_urls;
} Cli_Args;

ParseStatus parse_args(int argc, char *argv[], Cli_Args *args);
void Cli_args_clean_up(Cli_Args *args);

#endif // CLI_ARGS_H
