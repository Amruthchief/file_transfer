#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/* File information structure */
typedef struct {
    char     filename[FT_MAX_FILENAME_LEN];
    uint64_t file_size;
    uint32_t file_mode;
    uint64_t timestamp;
} FileMetadata;

/* Safe file operations */

/* Open file for reading with error handling */
FILE* file_open_read(const char *filepath, FTErrorCode *error);

/* Open file for writing (creates temp file for atomic write) */
FILE* file_open_write(const char *output_dir, const char *filename,
                      char *temp_path, size_t temp_path_size, FTErrorCode *error);

/* Finalize file write (atomic rename from temp to final) */
int file_finalize_write(const char *temp_path, const char *final_path);

/* Read chunk from file at specified offset */
int file_read_chunk(FILE *file, uint64_t offset, uint8_t *buffer,
                    size_t chunk_size, size_t *bytes_read, FTErrorCode *error);

/* Write chunk to file at specified offset */
int file_write_chunk(FILE *file, uint64_t offset, const uint8_t *buffer,
                     size_t chunk_size, FTErrorCode *error);

/* Get file metadata */
int file_get_metadata(const char *filepath, FileMetadata *metadata, FTErrorCode *error);

/* Get file size */
int file_get_size(const char *filepath, uint64_t *size, FTErrorCode *error);

/* Check if file exists */
int file_exists(const char *filepath);

/* Check available disk space (in bytes) */
int file_check_disk_space(const char *path, uint64_t required_bytes, FTErrorCode *error);

/* Sanitize filename (remove dangerous characters and paths) */
int file_sanitize_filename(const char *filename, char *sanitized, size_t size);

/* Build file path (handles path separators correctly) */
int file_build_path(const char *dir, const char *filename, char *path, size_t size);

/* Delete file */
int file_delete(const char *filepath);

/* Create directory if it doesn't exist */
int file_create_directory(const char *dirpath);

#endif /* FILEIO_H */
