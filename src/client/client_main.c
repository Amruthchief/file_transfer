#include "../common/platform.h"
#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/logger.h"
#include "../common/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Client configuration */
typedef struct {
    char host[256];
    uint16_t port;
    char filepath[1024];
    int verbose;
    char *log_file;
} ClientConfig;

/* Parse command-line arguments */
static int parse_args(int argc, char *argv[], ClientConfig *config) {
    /* Set defaults */
    config->host[0] = '\0';
    config->port = FT_DEFAULT_PORT;
    config->filepath[0] = '\0';
    config->verbose = 0;
    config->log_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            strncpy(config->host, argv[++i], sizeof(config->host) - 1);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config->port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            strncpy(config->filepath, argv[++i], sizeof(config->filepath) - 1);
        } else if (strcmp(argv[i], "-v") == 0) {
            config->verbose = 1;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            config->log_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("File Transfer Client\n");
            printf("Usage: %s -h <host> -f <file> [options]\n", argv[0]);
            printf("\nRequired:\n");
            printf("  -h <host>      Server hostname or IP address\n");
            printf("  -f <file>      File to transfer\n");
            printf("\nOptions:\n");
            printf("  -p <port>      Server port (default: %d)\n", FT_DEFAULT_PORT);
            printf("  -v             Verbose logging\n");
            printf("  -l <file>      Log to file\n");
            printf("  --help         Show this help message\n");
            return -1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    /* Validate required arguments */
    if (config->host[0] == '\0' || config->filepath[0] == '\0') {
        fprintf(stderr, "Error: Host (-h) and file (-f) are required\n");
        fprintf(stderr, "Use --help for usage information\n");
        return -1;
    }

    return 0;
}

/* Send file to server */
static int send_file(socket_t server_sock, const char *filepath) {
    FTErrorCode error;
    FILE *file = NULL;
    uint8_t *chunk_buffer = NULL;
    uint64_t sequence_num = 2;
    int result = -1;

    /* Get file metadata */
    FileMetadata metadata;
    if (file_get_metadata(filepath, &metadata, &error) != 0) {
        LOG_ERROR("Failed to get file metadata: %s", protocol_get_error_string(error));
        return -1;
    }

    LOG_INFO("File: %s, Size: %llu bytes", metadata.filename, (unsigned long long)metadata.file_size);

    /* Open file */
    file = file_open_read(filepath, &error);
    if (file == NULL) {
        LOG_ERROR("Failed to open file: %s", protocol_get_error_string(error));
        return -1;
    }

    /* Prepare file info */
    FileInfo file_info;
    memset(&file_info, 0, sizeof(file_info));
    file_info.filename_len = (uint16_t)strlen(metadata.filename);
    strncpy(file_info.filename, metadata.filename, FT_MAX_FILENAME_LEN - 1);
    file_info.file_size = metadata.file_size;
    file_info.chunk_size = FT_DEFAULT_CHUNK_SIZE;
    file_info.total_chunks = (metadata.file_size + file_info.chunk_size - 1) / file_info.chunk_size;
    file_info.checksum_type = CHECKSUM_SHA256;
    memset(file_info.file_checksum, 0, FT_SHA256_SIZE);  /* TODO: Compute SHA-256 */
    file_info.file_mode = metadata.file_mode;
    file_info.timestamp = metadata.timestamp;

    LOG_INFO("Total chunks: %llu (chunk size: %u bytes)",
             (unsigned long long)file_info.total_chunks, file_info.chunk_size);

    /* Perform handshake */
    LOG_INFO("Performing handshake...");
    if (perform_handshake_client(server_sock, &error) != 0) {
        LOG_ERROR("Handshake failed: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    /* Send file info */
    LOG_INFO("Sending file info...");
    if (send_file_info(server_sock, &file_info, sequence_num++, &error) != 0) {
        LOG_ERROR("Failed to send file info: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    /* Receive file ACK */
    MessageHeader header;
    uint8_t ack_buf[16];
    if (recv_message(server_sock, &header, ack_buf, sizeof(ack_buf), &error) != 0) {
        LOG_ERROR("Failed to receive file ACK: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    if (header.msg_type == MSG_ERROR) {
        ErrorMessage err_msg;
        /* Parse error message from ack_buf */
        err_msg.error_code = ack_buf[0];
        LOG_ERROR("Server rejected file: %s", protocol_get_error_string((FTErrorCode)err_msg.error_code));
        goto cleanup;
    }

    if (header.msg_type != MSG_FILE_ACK) {
        LOG_ERROR("Expected FILE_ACK, got message type %d", header.msg_type);
        goto cleanup;
    }

    /* Allocate chunk buffer */
    chunk_buffer = (uint8_t*)malloc(file_info.chunk_size);
    if (chunk_buffer == NULL) {
        LOG_ERROR("Failed to allocate chunk buffer");
        goto cleanup;
    }

    /* Send chunks */
    LOG_INFO("Sending file...");
    uint64_t sent_chunks = 0;
    uint64_t sent_bytes = 0;
    uint64_t start_time = platform_get_monotonic_ms();

    for (uint64_t chunk_id = 0; chunk_id < file_info.total_chunks; chunk_id++) {
        uint64_t chunk_offset = chunk_id * file_info.chunk_size;
        size_t bytes_to_read = file_info.chunk_size;

        /* Last chunk may be smaller */
        if (chunk_offset + bytes_to_read > file_info.file_size) {
            bytes_to_read = (size_t)(file_info.file_size - chunk_offset);
        }

        /* Read chunk from file */
        size_t bytes_read;
        if (file_read_chunk(file, chunk_offset, chunk_buffer, bytes_to_read, &bytes_read, &error) != 0) {
            LOG_ERROR("Failed to read chunk %llu: %s",
                      (unsigned long long)chunk_id, protocol_get_error_string(error));
            goto cleanup;
        }

        /* Send chunk (with retry) */
        int retry_count = 0;
        while (retry_count < FT_MAX_RETRIES) {
            if (send_chunk(server_sock, chunk_id, chunk_offset, chunk_buffer, bytes_read, sequence_num++, &error) != 0) {
                LOG_ERROR("Failed to send chunk %llu: %s",
                          (unsigned long long)chunk_id, protocol_get_error_string(error));
                retry_count++;
                if (retry_count >= FT_MAX_RETRIES) {
                    goto cleanup;
                }
                LOG_WARN("Retrying chunk %llu (%d/%d)", (unsigned long long)chunk_id, retry_count, FT_MAX_RETRIES);
                continue;
            }

            /* Receive chunk ACK */
            ChunkAck ack;
            if (recv_chunk_ack(server_sock, &ack, &error) != 0) {
                LOG_ERROR("Failed to receive chunk ACK: %s", protocol_get_error_string(error));
                retry_count++;
                if (retry_count >= FT_MAX_RETRIES) {
                    goto cleanup;
                }
                continue;
            }

            if (ack.chunk_id != chunk_id) {
                LOG_WARN("Received ACK for chunk %llu, expected %llu",
                         (unsigned long long)ack.chunk_id, (unsigned long long)chunk_id);
            }

            if (ack.status != 0) {
                LOG_WARN("Server requested retransmit of chunk %llu", (unsigned long long)chunk_id);
                retry_count++;
                if (retry_count >= FT_MAX_RETRIES) {
                    LOG_ERROR("Max retries exceeded for chunk %llu", (unsigned long long)chunk_id);
                    goto cleanup;
                }
                continue;
            }

            /* Chunk sent successfully */
            break;
        }

        sent_chunks++;
        sent_bytes += bytes_read;

        /* Display progress every 5% or every 100 chunks */
        if (sent_chunks % ((file_info.total_chunks / 20) + 1) == 0 || sent_chunks % 100 == 0) {
            uint64_t elapsed_ms = platform_get_monotonic_ms() - start_time;
            double progress = (double)sent_chunks / file_info.total_chunks * 100.0;
            double speed_mbps = 0.0;

            if (elapsed_ms > 0) {
                speed_mbps = (double)sent_bytes / elapsed_ms / 1000.0;  /* MB/s */
            }

            LOG_INFO("Progress: %.1f%% (%llu/%llu chunks) - %.2f MB/s",
                     progress, (unsigned long long)sent_chunks,
                     (unsigned long long)file_info.total_chunks, speed_mbps);
        }
    }

    /* Calculate transfer statistics */
    uint64_t elapsed_ms = platform_get_monotonic_ms() - start_time;
    double elapsed_sec = elapsed_ms / 1000.0;
    double speed_mbps = (elapsed_sec > 0) ? (double)sent_bytes / elapsed_ms / 1000.0 : 0.0;

    LOG_INFO("All chunks sent successfully");
    LOG_INFO("Transfer complete: %llu bytes in %.2f seconds (%.2f MB/s)",
             (unsigned long long)sent_bytes, elapsed_sec, speed_mbps);

    /* TODO: Send TRANSFER_COMPLETE and verify checksums */
    LOG_WARN("Checksum verification not yet implemented");

    result = 0;

cleanup:
    if (file != NULL) {
        fclose(file);
    }
    if (chunk_buffer != NULL) {
        free(chunk_buffer);
    }

    return result;
}

int main(int argc, char *argv[]) {
    ClientConfig config;
    socket_t server_sock = INVALID_SOCKET_VALUE;
    int exit_code = 1;

    /* Parse arguments */
    if (parse_args(argc, argv, &config) != 0) {
        return (argc > 1 && strcmp(argv[argc-1], "--help") == 0) ? 0 : 1;
    }

    /* Initialize logger */
    logger_init(config.verbose ? LOG_DEBUG : LOG_INFO, config.log_file);

    /* Initialize platform */
    if (platform_init() != 0) {
        LOG_ERROR("Failed to initialize platform");
        goto cleanup;
    }

    LOG_INFO("File Transfer Client starting...");

    /* Validate file exists */
    if (!file_exists(config.filepath)) {
        LOG_ERROR("File not found: %s", config.filepath);
        goto cleanup;
    }

    /* Create client socket */
    FTErrorCode error;
    server_sock = socket_create(&error);
    if (server_sock == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to create socket: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    /* Set socket options */
    socket_set_timeout(server_sock, FT_TIMEOUT_SECONDS, NULL);
    socket_set_nodelay(server_sock, 1, NULL);

    /* Connect to server */
    LOG_INFO("Connecting to %s:%u...", config.host, config.port);
    if (socket_connect_with_retry(server_sock, config.host, config.port, 5, &error) != 0) {
        LOG_ERROR("Failed to connect: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    LOG_INFO("Connected to server");

    /* Send file */
    if (send_file(server_sock, config.filepath) == 0) {
        LOG_INFO("File transfer completed successfully");
        exit_code = 0;
    } else {
        LOG_ERROR("File transfer failed");
    }

cleanup:
    if (server_sock != INVALID_SOCKET_VALUE) {
        close_socket(server_sock);
    }

    platform_cleanup();
    logger_close();

    return exit_code;
}
