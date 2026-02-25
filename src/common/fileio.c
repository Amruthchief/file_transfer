#include "fileio.h"
#include "platform.h"
#include "logger.h"
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef FT_PLATFORM_WINDOWS
#include <windows.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#define mkdir(path, mode) _mkdir(path)
typedef struct __stat64 stat_t;
#define stat_func _stat64
/* S_ISREG and S_ISDIR are already defined in sys/stat.h on MinGW */
#else
typedef struct stat stat_t;
#define stat_func stat
#include <sys/statvfs.h>
#endif

/* Open file for reading */
FILE* file_open_read(const char *filepath, FTErrorCode *error) {
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        LOG_ERROR("Failed to open file for reading: %s - %s", filepath, strerror(errno));
        if (error) {
            *error = (errno == ENOENT) ? FT_ERR_FILE_NOT_FOUND :
                     (errno == EACCES) ? FT_ERR_PERMISSION : FT_ERR_FILE_OPEN;
        }
        return NULL;
    }
    if (error) *error = FT_SUCCESS;
    return file;
}

/* Open file for writing (creates temp file) */
FILE* file_open_write(const char *output_dir, const char *filename,
                      char *temp_path, size_t temp_path_size, FTErrorCode *error) {
    /* Build temp file path */
    snprintf(temp_path, temp_path_size, "%s%c.%s.tmp",
             output_dir, PATH_SEPARATOR, filename);

    FILE *file = fopen(temp_path, "wb");
    if (file == NULL) {
        LOG_ERROR("Failed to open file for writing: %s - %s", temp_path, strerror(errno));
        if (error) {
            *error = (errno == ENOSPC) ? FT_ERR_DISK_FULL :
                     (errno == EACCES) ? FT_ERR_PERMISSION : FT_ERR_FILE_OPEN;
        }
        return NULL;
    }

    if (error) *error = FT_SUCCESS;
    return file;
}

/* Finalize write (atomic rename) */
int file_finalize_write(const char *temp_path, const char *final_path) {
    /* Close any open handles first (caller should close the FILE*) */

#ifdef FT_PLATFORM_WINDOWS
    /* On Windows, need to delete target if it exists */
    if (file_exists(final_path)) {
        if (remove(final_path) != 0) {
            LOG_WARN("Failed to remove existing file: %s", final_path);
        }
    }
#endif

    if (rename(temp_path, final_path) != 0) {
        LOG_ERROR("Failed to rename %s to %s: %s", temp_path, final_path, strerror(errno));
        return FT_ERR_FILE_WRITE;
    }

    LOG_INFO("File successfully written: %s", final_path);
    return FT_SUCCESS;
}

/* Read chunk from file */
int file_read_chunk(FILE *file, uint64_t offset, uint8_t *buffer,
                    size_t chunk_size, size_t *bytes_read, FTErrorCode *error) {
    /* Seek to offset */
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        LOG_ERROR("Failed to seek to offset %llu: %s", (unsigned long long)offset, strerror(errno));
        if (error) *error = FT_ERR_FILE_SEEK;
        return -1;
    }

    /* Read data */
    size_t read = fread(buffer, 1, chunk_size, file);
    if (read == 0 && ferror(file)) {
        LOG_ERROR("Failed to read from file: %s", strerror(errno));
        if (error) *error = FT_ERR_FILE_READ;
        return -1;
    }

    *bytes_read = read;
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Write chunk to file */
int file_write_chunk(FILE *file, uint64_t offset, const uint8_t *buffer,
                     size_t chunk_size, FTErrorCode *error) {
    /* Seek to offset */
    if (fseeko(file, (off_t)offset, SEEK_SET) != 0) {
        LOG_ERROR("Failed to seek to offset %llu: %s", (unsigned long long)offset, strerror(errno));
        if (error) *error = FT_ERR_FILE_SEEK;
        return -1;
    }

    /* Write data */
    size_t written = fwrite(buffer, 1, chunk_size, file);
    if (written != chunk_size) {
        LOG_ERROR("Failed to write chunk (wrote %zu of %zu bytes): %s",
                  written, chunk_size, strerror(errno));
        if (error) {
            *error = (errno == ENOSPC) ? FT_ERR_DISK_FULL : FT_ERR_FILE_WRITE;
        }
        return -1;
    }

    /* Flush to ensure data is written */
    if (fflush(file) != 0) {
        LOG_ERROR("Failed to flush file: %s", strerror(errno));
        if (error) *error = FT_ERR_FILE_WRITE;
        return -1;
    }

    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Get file metadata */
int file_get_metadata(const char *filepath, FileMetadata *metadata, FTErrorCode *error) {
    stat_t st;
    if (stat_func(filepath, &st) != 0) {
        LOG_ERROR("Failed to stat file %s: %s", filepath, strerror(errno));
        if (error) *error = FT_ERR_FILE_NOT_FOUND;
        return -1;
    }

    if (!S_ISREG(st.st_mode)) {
        LOG_ERROR("Path is not a regular file: %s", filepath);
        if (error) *error = FT_ERR_INVALID_ARG;
        return -1;
    }

    /* Extract filename from path */
    const char *filename = strrchr(filepath, PATH_SEPARATOR);
    filename = (filename != NULL) ? (filename + 1) : filepath;

    strncpy(metadata->filename, filename, FT_MAX_FILENAME_LEN - 1);
    metadata->filename[FT_MAX_FILENAME_LEN - 1] = '\0';
    metadata->file_size = (uint64_t)st.st_size;
    metadata->file_mode = (uint32_t)st.st_mode;
    metadata->timestamp = (uint64_t)st.st_mtime;

    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Get file size */
int file_get_size(const char *filepath, uint64_t *size, FTErrorCode *error) {
    stat_t st;
    if (stat_func(filepath, &st) != 0) {
        LOG_ERROR("Failed to stat file %s: %s", filepath, strerror(errno));
        if (error) *error = FT_ERR_FILE_NOT_FOUND;
        return -1;
    }

    *size = (uint64_t)st.st_size;
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Check if file exists */
int file_exists(const char *filepath) {
    stat_t st;
    return (stat_func(filepath, &st) == 0);
}

/* Check disk space */
int file_check_disk_space(const char *path, uint64_t required_bytes, FTErrorCode *error) {
#ifdef FT_PLATFORM_WINDOWS
    ULARGE_INTEGER free_bytes;
    if (!GetDiskFreeSpaceExA(path, &free_bytes, NULL, NULL)) {
        LOG_ERROR("Failed to get disk space for %s", path);
        if (error) *error = FT_ERR_DISK_FULL;
        return -1;
    }
    if (free_bytes.QuadPart < required_bytes) {
        LOG_ERROR("Insufficient disk space: need %llu bytes, have %llu bytes",
                  (unsigned long long)required_bytes, (unsigned long long)free_bytes.QuadPart);
        if (error) *error = FT_ERR_DISK_FULL;
        return -1;
    }
#else
    struct statvfs st;
    if (statvfs(path, &st) != 0) {
        LOG_ERROR("Failed to get disk space for %s: %s", path, strerror(errno));
        if (error) *error = FT_ERR_DISK_FULL;
        return -1;
    }
    uint64_t available = (uint64_t)st.f_bavail * (uint64_t)st.f_frsize;
    if (available < required_bytes) {
        LOG_ERROR("Insufficient disk space: need %llu bytes, have %llu bytes",
                  (unsigned long long)required_bytes, (unsigned long long)available);
        if (error) *error = FT_ERR_DISK_FULL;
        return -1;
    }
#endif

    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Sanitize filename */
int file_sanitize_filename(const char *filename, char *sanitized, size_t size) {
    if (filename == NULL || sanitized == NULL || size == 0) {
        return FT_ERR_INVALID_ARG;
    }

    /* Check for path traversal attempts */
    if (strstr(filename, "..") != NULL) {
        LOG_ERROR("Filename contains '..' - potential path traversal: %s", filename);
        return FT_ERR_INVALID_ARG;
    }

    /* Check for absolute paths */
    if (filename[0] == '/' || filename[0] == '\\' ||
        (filename[1] == ':' && (filename[0] >= 'A' && filename[0] <= 'Z'))) {
        LOG_ERROR("Absolute path not allowed: %s", filename);
        return FT_ERR_INVALID_ARG;
    }

    /* Copy filename, replacing dangerous characters */
    size_t i = 0, j = 0;
    while (filename[i] != '\0' && j < size - 1) {
        char c = filename[i];
        /* Allow alphanumeric, dash, underscore, dot */
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            sanitized[j++] = c;
        } else if (c == '/' || c == '\\') {
            sanitized[j++] = '_';  /* Replace path separators */
        }
        i++;
    }
    sanitized[j] = '\0';

    if (j == 0) {
        LOG_ERROR("Filename sanitization resulted in empty string");
        return FT_ERR_INVALID_ARG;
    }

    return FT_SUCCESS;
}

/* Build file path */
int file_build_path(const char *dir, const char *filename, char *path, size_t size) {
    if (dir == NULL || filename == NULL || path == NULL || size == 0) {
        return FT_ERR_INVALID_ARG;
    }

    /* Check if dir ends with separator */
    size_t dir_len = strlen(dir);
    int has_separator = (dir_len > 0 &&
                        (dir[dir_len - 1] == '/' || dir[dir_len - 1] == '\\'));

    if (has_separator) {
        snprintf(path, size, "%s%s", dir, filename);
    } else {
        snprintf(path, size, "%s%c%s", dir, PATH_SEPARATOR, filename);
    }

    return FT_SUCCESS;
}

/* Delete file */
int file_delete(const char *filepath) {
    if (remove(filepath) != 0) {
        LOG_WARN("Failed to delete file %s: %s", filepath, strerror(errno));
        return -1;
    }
    LOG_DEBUG("Deleted file: %s", filepath);
    return 0;
}

/* Create directory */
int file_create_directory(const char *dirpath) {
    if (file_exists(dirpath)) {
        return 0;  /* Already exists */
    }

    if (mkdir(dirpath, 0755) != 0) {
        LOG_ERROR("Failed to create directory %s: %s", dirpath, strerror(errno));
        return FT_ERR_PERMISSION;
    }

    LOG_INFO("Created directory: %s", dirpath);
    return 0;
}
