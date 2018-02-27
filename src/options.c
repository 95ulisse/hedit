#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "build-config.h"
#include "options.h"
#include "core.h"
#include "util/common.h"
#include "util/log.h"

static struct option long_options[] = {
    
    { "debug-fd",           required_argument, NULL, 'D' },
    { "debug-colors",       no_argument,       NULL,  0  },
    { "debug-min-severity", required_argument, NULL,  0  },

    { "help",               no_argument,       NULL, 'h' },
    { "version",            no_argument,       NULL, 'v' },

    { 0, 0, 0, 0 }

};

static void print_usage(const char* selfpath) {
    fprintf(stderr,
        "Usage: %s [filename] [-hv]\n"
        "\n"
        "Debug options:\n"
        "-D, --debug-fd               Output debug information to the given file descriptor.\n"
        "    --debug-colors           Enable colors in debug output.\n"
        "    --debug-min-severity     Filter debug messages. Available severities:\n"
        "                             debug, info, warn, error, fatal."
        "\n"
        "Other options:\n"
        "-h, --help                   Display this help text.\n"
        "-v, --version                Display version information.\n",
        selfpath
    );
}

static void print_version() {
    printf(
        "HEdit v" HEDIT_VERSION "\n"
        "+ v8 v" V8_VERSION "\n"
    );
}

static bool is_writable_fd(int fd) {
    int flags;
    errno = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0 && errno == EBADF) {
        return false;
    }
    flags &= O_ACCMODE;
    return flags == O_RDWR || flags == O_WRONLY;
}

bool options_parse(Options* options, int argc, char** argv) {

    // Defaults
    log_quiet(true);
    log_colored(false);
    log_min_severity(LOG_DEBUG);
    options->show_help = false;
    options->show_version = false;
    options->file = NULL;

    // Args parsing
    int opt;
    int longopt_index;
    while ((opt = getopt_long(argc, argv, "D:hv", long_options, &longopt_index)) != -1) {
        switch (opt) {
            
            case 'D': {
                // Check that the argument is a valid writable file descriptor
                int fd = -1;
                if (!str2int(optarg, 10, &fd)) {
                    log_fatal("Invalid file descriptor %s.", optarg);
                } else if (!is_writable_fd(fd)) {
                    log_fatal("File descriptor %d is not writable.", fd);
                } else {
                    FILE* stream = fdopen(fd, "w");
                    if (stream == NULL) {
                        log_fatal("Cannot fdopen fd %d: %s.", fd, strerror(errno));
                        goto error;
                    }
                    log_destination(stream);
                    log_quiet(false);
                    break;
                }

                goto error;
            }

            case 'h':
                print_usage(argv[0]);
                options->show_help = true;
                break;
            
            case 'v':
                print_version();
                options->show_version = true;
                break;

            case 0: {
                const char* opt_name = long_options[longopt_index].name;

                if (strcmp("debug-colors", opt_name) == 0) {
                    log_colored(true);
                    break;
                
                } else if (strcmp("debug-min-severity", opt_name) == 0) {
                    if (strcmp("debug", optarg) == 0) {
                        log_min_severity(LOG_DEBUG);
                        break;
                    } else if (strcmp("info", optarg) == 0) {
                        log_min_severity(LOG_INFO);
                        break;
                    } else if (strcmp("warn", optarg) == 0) {
                        log_min_severity(LOG_WARN);
                        break;
                    } else if (strcmp("error", optarg) == 0) {
                        log_min_severity(LOG_ERROR);
                        break;
                    } else if (strcmp("fatal", optarg) == 0) {
                        log_min_severity(LOG_FATAL);
                        break;
                    }
                }

                goto error;
            }

            default:
                goto error;
        }
    }

    // Treat the first argument as the file to open
    if (optind < argc) {
        options->file = argv[optind];
    }

    return true;

error:

    print_usage(argv[0]);
    return false;

}