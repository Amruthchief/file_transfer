#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>

/* Log levels */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
} LogLevel;

/* Logger configuration */
typedef struct {
    LogLevel level;        /* Minimum log level to display */
    FILE *log_file;        /* Log file handle (NULL for console only) */
    int use_timestamp;     /* Include timestamp in log messages */
    int use_colors;        /* Use ANSI colors (for terminal output) */
} Logger;

/* Global logger instance */
extern Logger g_logger;

/* Initialize logger */
void logger_init(LogLevel level, const char *log_file_path);

/* Close logger */
void logger_close(void);

/* Set log level */
void logger_set_level(LogLevel level);

/* Enable/disable timestamps */
void logger_set_timestamp(int enable);

/* Enable/disable colors */
void logger_set_colors(int enable);

/* Log functions */
void logger_log(LogLevel level, const char *file, int line, const char *format, ...);

/* Convenience macros */
#define LOG_DEBUG(...) logger_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  logger_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  logger_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif /* LOGGER_H */
