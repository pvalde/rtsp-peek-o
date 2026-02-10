#ifndef CLI_ARGS_H
#define CLI_ARGS_H
#include <stdbool.h>

typedef enum { PARSE_OK = 0, PARSE_HELP = 1, PARSE_ERROR = -1 } ParseStatus;

typedef struct {
  int n_of_rtsp_urls;
  char **rtsp_urls;
} Cli_args;

ParseStatus parse_args(int argc, char *argv[], Cli_args *args);
void clean_cli_args(Cli_args *args);

#endif // CLI_ARGS_H
