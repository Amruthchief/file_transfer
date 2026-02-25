#include "logger.h"
#include "platform.h"
#include <string.h>
#include <time.h>

/* Global logger instance */
Logger g_logger = {
    .level = LOG_INFO,
    .log_file = NULL,
    .use_timestamp = 1,
    .use_colors = 1
};

/* ANSI color codes */
#define COLOR_RESET   "\x1b[0m"
#define COLOR_DEBUG   "\x1b[36m"  /* Cyan */
#define COLOR_INFO    "\x1b[32m"  /* Green */
#define COLOR_WARN    "\x1b[33m"  /* Yellow */
#define COLOR_ERROR   "\x1b[31m"  /* Red */

/* Get log level string */
static const char* get_level_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "UNKN ";
    }
}

/* Get log level color */
static const char* get_level_color(LogLevel level) {
    if (!g_logger.use_colors) {
        return "";
    }
    switch (level) {
        case LOG_DEBUG: return COLOR_DEBUG;
        case LOG_INFO:  return COLOR_INFO;
        case LOG_WARN:  return COLOR_WARN;
        case LOG_ERROR: return COLOR_ERROR;
        default:        return COLOR_RESET;
    }
}

/* Initialize logger */
void logger_init(LogLevel level, const char *log_file_path) {
    g_logger.level = level;
    g_logger.use_timestamp = 1;

    /* Disable colors on Windows by default (unless using Windows Terminal with ANSI support) */
#ifdef FT_PLATFORM_WINDOWS
    g_logger.use_colors = 0;
#else
    g_logger.use_colors = 1;
#endif

    if (log_file_path != NULL) {
        g_logger.log_file = fopen(log_file_path, "a");
        if (g_logger.log_file == NULL) {
            fprintf(stderr, "Warning: Could not open log file %s\n", log_file_path);
        }
    }
}

/* Close logger */
void logger_close(void) {
    if (g_logger.log_file != NULL) {
        fclose(g_logger.log_file);
        g_logger.log_file = NULL;
    }
}

/* Set log level */
void logger_set_level(LogLevel level) {
    g_logger.level = level;
}

/* Enable/disable timestamps */
void logger_set_timestamp(int enable) {
    g_logger.use_timestamp = enable;
}

/* Enable/disable colors */
void logger_set_colors(int enable) {
    g_logger.use_colors = enable;
}

/* Format timestamp */
static void format_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* Extract filename from full path */
static const char* extract_filename(const char *path) {
    const char *filename = strrchr(path, '/');
    if (filename == NULL) {
        filename = strrchr(path, '\\');
    }
    return (filename != NULL) ? (filename + 1) : path;
}

/* Log function */
void logger_log(LogLevel level, const char *file, int line, const char *format, ...) {
    if (level < g_logger.level) {
        return;
    }

    char timestamp[64] = "";
    if (g_logger.use_timestamp) {
        format_timestamp(timestamp, sizeof(timestamp));
    }

    const char *level_str = get_level_string(level);
    const char *level_color = get_level_color(level);
    const char *color_reset = g_logger.use_colors ? COLOR_RESET : "";
    const char *filename = extract_filename(file);

    /* Format the user message */
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    /* Log to console */
    if (g_logger.use_timestamp) {
        fprintf(stderr, "[%s] %s%s%s [%s:%d] %s\n",
                timestamp, level_color, level_str, color_reset, filename, line, message);
    } else {
        fprintf(stderr, "%s%s%s [%s:%d] %s\n",
                level_color, level_str, color_reset, filename, line, message);
    }

    /* Log to file (no colors) */
    if (g_logger.log_file != NULL) {
        if (g_logger.use_timestamp) {
            fprintf(g_logger.log_file, "[%s] %s [%s:%d] %s\n",
                    timestamp, level_str, filename, line, message);
        } else {
            fprintf(g_logger.log_file, "%s [%s:%d] %s\n",
                    level_str, filename, line, message);
        }
        fflush(g_logger.log_file);
    }
}
