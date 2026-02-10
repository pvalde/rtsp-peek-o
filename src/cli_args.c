#include "cli_args.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool starts_with(const char *str, const char *prefix);

bool verify_cli_args(int cli_args_count, char *cli_args[]) {
  int ret = true;
  for (int i = 1; i < cli_args_count; i++) {
    if (!starts_with(cli_args[i], "rtsp://")) {
      fprintf(stderr,
              "Error: argument '%s' is not a valid RTSP URL (must start with "
              "'rtsp://')\n",
              cli_args[i]);
      ret = false;
    }
  }
  if (!ret) {
    printf("\n");
  }
  return ret;
}

static bool starts_with(const char *str, const char *prefix) {
  if (!str || !prefix) {
    fprintf(stderr, "starts_with: NULL char* received\n");
    return false;
  }
  size_t str_len = strlen(str);
  size_t prefix_len = strlen(prefix);
  if (prefix_len > str_len) {
    return false;
  }
  return (strncmp(str, prefix, prefix_len) == 0);
}
