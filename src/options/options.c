#include "options.h"
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

App_Options_T options_parse(int argc, char *argv[]) {
    App_Options_T options = {0};

    char extra;
    int matched;

    static struct option long_options[] = {
        {"grid", required_argument, NULL, 'g'}, // --grid mapped to 'g'
        {"help", no_argument, NULL, 'h'},       // --help mapped to 'h'
        {0, 0, 0, 0}                            // Required null-terminator
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "g:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'g':

            matched = sscanf(optarg, "%dx%d%c", &options.columns, &options.rows,
                             &extra);
            if (matched != 2 || options.columns <= 0 || options.rows <= 0) {
                options.parse_error = true;
            }
            break;
        case 'h':
            options.help_requested = true;
            break;
        case '?':
            options.parse_error = true;
            break;
        }
    }

    options.url_start_index = optind;
    options.url_count = argc - optind;

    return options;
}
