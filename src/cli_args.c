#include "cli_args.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HELP_FLAG_SHORT "-h"
#define HELP_FLAG_LONG "--help"
#define PROG_NAME "rtsp-peek"

static bool get_args(Cli_Args *args, int argc, char *argv[]);
static bool starts_with(const char *str, const char *prefix);
static bool check_help_flag(int argc, char *argv[]);
void printHelpMenu();

ParseStatus parse_args(int argc, char *argv[], Cli_Args *args) {
  if (!(argc >= 2)) {
    printHelpMenu();
    return PARSE_ERROR;
  }

  if (check_help_flag(argc, argv)) {
    printHelpMenu();
    return PARSE_HELP;
  } else if (!get_args(args, argc, argv)) {
    return PARSE_ERROR;
  }
  return PARSE_OK;
}

void Cli_args_clean_up(Cli_Args *args) {
  for (int i = 0; i < args->n_of_rtsp_urls; i++) {
    free(args->rtsp_urls[i]);
    args->rtsp_urls[i] = NULL;
  }
  free(args->rtsp_urls);
  args->rtsp_urls = NULL;
  args->n_of_rtsp_urls = 0;
}

static bool get_args(Cli_Args *args, int argc, char *argv[]) {
  bool ret = true;

  int valid_urls_index = 0;
  args->rtsp_urls = malloc((argc - 1) * sizeof(char *));

  if (!args->rtsp_urls) {
    fprintf(stderr, "Error: failed to allocate memory for URLs\n");
    return false;
  }

  for (int i = 1; i < argc; i++) {
    if (!starts_with(argv[i], "rtsp://")) {
      fprintf(stderr,
              "Error: argument '%s' is not a valid RTSP URL (must start with "
              "'rtsp://')\n",
              argv[i]);
      ret = false;

    } else if (ret == true) { // do not collect args if any invalid arg.
      args->rtsp_urls[valid_urls_index] = strdup(argv[i]);
      if (!args->rtsp_urls[valid_urls_index]) {
        fprintf(stderr, "Error: failed to allocate memory for URL '%s'\n",
                argv[i]);
        // cleanup any previously allocated URLs
        Cli_args_clean_up(args);
        return false;
      }
      valid_urls_index += 1;
      args->n_of_rtsp_urls += 1;
    }
  }
  if (!ret) {
    Cli_args_clean_up(args);
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

static bool check_help_flag(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(HELP_FLAG_LONG, argv[i]) == 0 ||
        strcmp(HELP_FLAG_SHORT, argv[i]) == 0) {
      return true;
    }
  }
  return false;
}

void printHelpMenu() {

  static const char *HELP_MESSAGE =
      "Usage: " PROG_NAME " [OPTIONS] rtsp_url1 [rtsp_url2 ...]\n"
      "\n"
      "Options:\n"
      "  -h, --help        Show this help message and exit\n"
      "\n"
      "Arguments:\n"
      "  rtsp_url          One or more RTSP URLs to stream (must start with "
      "'rtsp://')\n"
      "\n"
      "Example:\n"
      "  ./my_program rtsp://192.168.0.10:554/stream1 "
      "rtsp://192.168.0.11:554/stream2\n";

  printf("%s", HELP_MESSAGE);
}
