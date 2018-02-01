#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum log_severity { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL } log_severity;

struct log_config {
    bool quiet;
    bool colored;
    log_severity min_severity;
    FILE* destination;
};

typedef void (*log_sink)(void* user, struct log_config* config, const char* file, int line, log_severity severity, const char* format, va_list args);

#define log_debug(...) __hedit_log(__FILE__, __LINE__, LOG_DEBUG, __VA_ARGS__)
#define log_info(...)  __hedit_log(__FILE__, __LINE__, LOG_INFO,  __VA_ARGS__)
#define log_warn(...)  __hedit_log(__FILE__, __LINE__, LOG_WARN,  __VA_ARGS__)
#define log_error(...) __hedit_log(__FILE__, __LINE__, LOG_ERROR, __VA_ARGS__)
#define log_fatal(...) __hedit_log(__FILE__, __LINE__, LOG_FATAL, __VA_ARGS__)

void log_init();
void log_teardown();

void log_quiet(bool quiet);
void log_colored(bool colored);
void log_min_severity(log_severity severity);
void log_destination(FILE* destination);

void* log_register_sink(log_sink sink, void* user);
void log_unregister_sink(void* token);

void __hedit_log(const char* file, int line, log_severity severity, const char* format, ...);


#ifdef __cplusplus
}
#endif

#endif