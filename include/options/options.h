#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

typedef struct {
    int columns; // Parsed grid columns (Default: 0, means not provided)
    int rows;    // Parsed grid rows (Default 0, means not provided)

    int url_start_index;
    int url_count;

    bool help_requested;
    bool parse_error;
} App_Options_T;

/**
 * @brief Parses command-line arguments using getopt.
 */
App_Options_T options_parse(int argc, char *argv[]);

#endif // OPTIONS_H
