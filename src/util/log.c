#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "util/log.h"
#include "util/list.h"

#define RESET "\x1b[0m"
#define BOLD "\x1b[1m"
#define GRAY "\x1b[90m"

static const char* severity_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char* severity_colors[] = {
    "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"
};

static const char* severity_text_colors[] = {
    GRAY, RESET, BOLD "\x1b[33m", BOLD "\x1b[31m", BOLD "\x1b[35m"
};



// Log sinks are stored as a doubly-linked list
struct log_sink_node {
    log_sink sink;
    struct list_head list;
};
static struct list_head sinks = LIST_HEAD_INIT(sinks);

// Shared log configuration
static struct log_config config = {
    .quiet = false,
    .colored = false,
    .min_severity = LOG_DEBUG,
    .destination = NULL
};



// Forward declaration of the default destination sink
static void destination_sink(struct log_config* config, const char* file, int line, log_severity severity, const char* format, va_list args);

void log_init() {
    config.destination = stderr;
    log_register_sink(destination_sink);

    // If we failed to register the default sink for any reason (out of memory), just abort
    if (list_empty(&sinks)) {
        fprintf(config.destination, "Cannot initialize logging framework.\n");
        abort();
    }
}

void log_teardown() {
    
    // Free all the sinks
    list_for_each_member(sink, &sinks, struct log_sink_node, list) {
        log_unregister_sink((void*) sink);
    }

    // Close log destination
    fclose(config.destination);

}

void log_quiet(bool q) {
    config.quiet = q;
}

void log_colored(bool c) {
    config.colored = c;
}

void log_min_severity(log_severity severity) {
    assert(severity >= LOG_DEBUG && severity <= LOG_FATAL);
    config.min_severity = severity;
}

void log_destination(FILE* destination) {
    config.destination = destination;
}

void* log_register_sink(log_sink sink) {

    if (sink == NULL) {
        log_error(NULL, "Cannot register NULL log sink.");
        return NULL;
    }

    // Create a new sink node
    struct log_sink_node* node = malloc(sizeof(struct log_sink_node));
    if (node == NULL) {
        log_error(NULL, "Cannot register new log sink: out of memory.");
        return NULL;
    }
    node->sink = sink;
    list_init(&node->list);

    // Append the sink at the end of the list
    list_add_tail(&sinks, &node->list);

    return (void*) node;

}

void log_unregister_sink(void* token) {

    // The opaque tokes is a pointer to the node to remove
    struct log_sink_node* node = token;

    if (node == NULL) {
        log_error(NULL, "Cannot unregister NULL log sink.");
        return;
    }

    // Remove the node from the list
    list_del(&node->list);
    free(node);

}

void __hedit_log(const char* file, int line, log_severity severity, const char* format, ...) {

    // Assert the validity of the severity
    assert(severity >= LOG_DEBUG && severity <= LOG_FATAL);

    // As an exception to the rules, if we receive a fatal message, the program is likely to terminate
    // due to an unrecoverable error, so, even if logging is disabled, print the message to stderr.
    if (config.quiet && severity == LOG_FATAL) {
        
        bool old_colored = config.colored;
        FILE* old_destination = config.destination;
        config.colored = isatty(STDERR_FILENO);
        config.destination = stderr;

        va_list args;
        va_start(args, format);
        destination_sink(&config, file, line, severity, format, args);
        va_end(args);

        config.colored = old_colored;
        config.destination = old_destination;

    }

    // Exit immediately if we have to stay quiet or the event is ignored by the minimum severity
    if (config.quiet || severity < config.min_severity) {
        return;
    }

    // Iterate all the sinks and pass all the parameters
    list_for_each_member(sink, &sinks, struct log_sink_node, list) {
        va_list args;
        va_start(args, format);
        sink->sink(&config, file, line, severity, format, args);
        va_end(args);
    }

}

static void destination_sink(struct log_config* config, const char* file, int line, log_severity severity, const char* format, va_list args) {

    // Exit if the destination is null
    if (config->destination == NULL) {
        return;
    }

    // Format current time
    time_t t;
    struct tm* tm_info;
    char formatted_time[32];
    time(&t);
    tm_info = localtime(&t);
    if (strftime(formatted_time, 32, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
        // Error formatting the time: do not output the time
        formatted_time[0] = '\0'; 
    }

    // Output time and severity to destination
    if (config->colored) {
        fprintf(config->destination, "%s %s" BOLD "%-5s" RESET " " GRAY "%s:%d:" RESET "%s ",
                formatted_time, severity_colors[severity], severity_names[severity], file, line, severity_text_colors[severity]);

    } else {
        fprintf(config->destination, "%s %-5s %s:%d: ", formatted_time, severity_names[severity], file, line);
    }

    // Now write the actual message
    vfprintf(config->destination, format, args);

    // Restore the normal colors if we wrote the whole message colored
    if (config->colored) {
        fprintf(config->destination, RESET);
    }

    fprintf(config->destination, "\n");
    fflush(config->destination);

}