#include "../common/platform.h"
#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/logger.h"
#include "../common/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Server configuration */
typedef struct {
    uint16_t port;
    char output_dir[512];
    int verbose;
    char *log_file;
} ServerConfig;

/* Parse command-line arguments */
static int parse_args(int argc, char *argv[], ServerConfig *config) {
    /* Set defaults */
    config->port = FT_DEFAULT_PORT;
    strcpy(config->output_dir, ".");
    config->verbose = 0;
    config->log_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config->port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            strncpy(config->output_dir, argv[++i], sizeof(config->output_dir) - 1);
        } else if (strcmp(argv[i], "-v") == 0) {
            config->verbose = 1;
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            config->log_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("File Transfer Server\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("\nOptions:\n");
            printf("  -p <port>      Port to listen on (default: %d)\n", FT_DEFAULT_PORT);
            printf("  -d <dir>       Output directory for received files (default: current)\n");
            printf("  -v             Verbose logging\n");
            printf("  -l <file>      Log to file\n");
            printf("  -h, --help     Show this help message\n");
            return -1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

/* Receive file from client */
static int receive_file(socket_t client_sock, const char *output_dir) {
    FTErrorCode error;
    FILE *file = NULL;
    uint8_t *chunk_buffer = NULL;
    char temp_path[1024];
    char final_path[1024];
    uint64_t sequence_num = 2;
    int result = -1;

    /* Perform handshake */
    LOG_INFO("Performing handshake...");
    if (perform_handshake_server(client_sock, &error) != 0) {
        LOG_ERROR("Handshake failed: %s", protocol_get_error_string(error));
        return -1;
    }

    /* Receive file info */
    LOG_INFO("Receiving file info...");
    FileInfo file_info;
    if (recv_file_info(client_sock, &file_info, &error) != 0) {
        LOG_ERROR("Failed to receive file info: %s", protocol_get_error_string(error));
        return -1;
    }

    LOG_INFO("File: %s, Size: %llu bytes, Chunks: %llu",
             file_info.filename, (unsigned long long)file_info.file_size,
             (unsigned long long)file_info.total_chunks);

    /* Sanitize filename */
    char sanitized_name[FT_MAX_FILENAME_LEN];
    if (file_sanitize_filename(file_info.filename, sanitized_name, sizeof(sanitized_name)) != 0) {
        LOG_ERROR("Invalid filename: %s", file_info.filename);
        send_error(client_sock, FT_ERR_INVALID_ARG, 0, "Invalid filename", sequence_num++, NULL);
        return -1;
    }

    /* Check disk space */
    if (file_check_disk_space(output_dir, file_info.file_size, &error) != 0) {
        LOG_ERROR("Insufficient disk space");
        send_error(client_sock, FT_ERR_DISK_FULL, 0, "Insufficient disk space", sequence_num++, NULL);
        return -1;
    }

    /* Open file for writing */
    file = file_open_write(output_dir, sanitized_name, temp_path, sizeof(temp_path), &error);
    if (file == NULL) {
        LOG_ERROR("Failed to open output file: %s", protocol_get_error_string(error));
        send_error(client_sock, error, 0, "Cannot create file", sequence_num++, NULL);
        return -1;
    }

    /* Send file ACK */
    FileAck file_ack;
    file_ack.status = 0;  /* Ready */
    file_ack.error_code = 0;
    uint8_t ack_buf[4] = {0};
    if (send_message(client_sock, MSG_FILE_ACK, sequence_num++, ack_buf, sizeof(file_ack), &error) != 0) {
        LOG_ERROR("Failed to send file ACK");
        goto cleanup;
    }

    /* Allocate chunk buffer */
    chunk_buffer = (uint8_t*)malloc(file_info.chunk_size);
    if (chunk_buffer == NULL) {
        LOG_ERROR("Failed to allocate chunk buffer");
        send_error(client_sock, FT_ERR_OUT_OF_MEMORY, 0, "Out of memory", sequence_num++, NULL);
        goto cleanup;
    }

    /* Receive chunks */
    LOG_INFO("Receiving %llu chunks...", (unsigned long long)file_info.total_chunks);
    uint64_t received_chunks = 0;
    uint64_t received_bytes = 0;

    while (received_chunks < file_info.total_chunks) {
        ChunkHeader chunk_hdr;

        /* Receive chunk */
        if (recv_chunk(client_sock, &chunk_hdr, chunk_buffer, file_info.chunk_size, &error) != 0) {
            LOG_ERROR("Failed to receive chunk %llu: %s",
                      (unsigned long long)received_chunks, protocol_get_error_string(error));

            if (error == FT_ERR_CHECKSUM) {
                /* Request retransmit */
                send_chunk_ack(client_sock, chunk_hdr.chunk_id, 1, sequence_num++, NULL);
                continue;  /* Retry */
            } else {
                goto cleanup;
            }
        }

        /* Write chunk to file */
        if (file_write_chunk(file, chunk_hdr.chunk_offset, chunk_buffer, chunk_hdr.chunk_size, &error) != 0) {
            LOG_ERROR("Failed to write chunk %llu: %s",
                      (unsigned long long)chunk_hdr.chunk_id, protocol_get_error_string(error));
            send_error(client_sock, error, chunk_hdr.chunk_id, "Write failed", sequence_num++, NULL);
            goto cleanup;
        }

        /* Send chunk ACK */
        if (send_chunk_ack(client_sock, chunk_hdr.chunk_id, 0, sequence_num++, &error) != 0) {
            LOG_ERROR("Failed to send chunk ACK");
            goto cleanup;
        }

        received_chunks++;
        received_bytes += chunk_hdr.chunk_size;

        /* Log progress every 10% */
        if (received_chunks % (file_info.total_chunks / 10 + 1) == 0) {
            double progress = (double)received_chunks / file_info.total_chunks * 100.0;
            LOG_INFO("Progress: %.1f%% (%llu/%llu chunks)",
                     progress, (unsigned long long)received_chunks,
                     (unsigned long long)file_info.total_chunks);
        }
    }

    LOG_INFO("All chunks received successfully");

    /* TODO: Compute and verify SHA-256 checksum */
    LOG_WARN("Checksum verification not yet implemented");

    /* Close file */
    fclose(file);
    file = NULL;

    /* Finalize file (atomic rename) */
    file_build_path(output_dir, sanitized_name, final_path, sizeof(final_path));
    if (file_finalize_write(temp_path, final_path) != 0) {
        LOG_ERROR("Failed to finalize file");
        goto cleanup;
    }

    LOG_INFO("File received successfully: %s (%llu bytes)",
             final_path, (unsigned long long)received_bytes);

    result = 0;

cleanup:
    if (file != NULL) {
        fclose(file);
        /* Delete temp file on error */
        if (result != 0) {
            file_delete(temp_path);
        }
    }
    if (chunk_buffer != NULL) {
        free(chunk_buffer);
    }

    return result;
}

int main(int argc, char *argv[]) {
    ServerConfig config;
    socket_t listen_sock = INVALID_SOCKET_VALUE;
    socket_t client_sock = INVALID_SOCKET_VALUE;
    int exit_code = 1;

    /* Parse arguments */
    if (parse_args(argc, argv, &config) != 0) {
        return (strcmp(argv[argc-1], "-h") == 0 || strcmp(argv[argc-1], "--help") == 0) ? 0 : 1;
    }

    /* Initialize logger */
    logger_init(config.verbose ? LOG_DEBUG : LOG_INFO, config.log_file);

    /* Initialize platform */
    if (platform_init() != 0) {
        LOG_ERROR("Failed to initialize platform");
        goto cleanup;
    }

    LOG_INFO("File Transfer Server starting...");
    LOG_INFO("Output directory: %s", config.output_dir);

    /* Create output directory if it doesn't exist */
    if (!file_exists(config.output_dir)) {
        if (file_create_directory(config.output_dir) != 0) {
            LOG_ERROR("Failed to create output directory: %s", config.output_dir);
            goto cleanup;
        }
    }

    /* Create server socket */
    FTErrorCode error;
    listen_sock = socket_create(&error);
    if (listen_sock == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to create socket: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    /* Set socket options */
    socket_set_reuseaddr(listen_sock, 1, NULL);
    socket_set_timeout(listen_sock, FT_TIMEOUT_SECONDS, NULL);

    /* Bind and listen */
    if (socket_bind_and_listen(listen_sock, config.port, 5, &error) != 0) {
        LOG_ERROR("Failed to bind and listen: %s", protocol_get_error_string(error));
        goto cleanup;
    }

    LOG_INFO("Server listening on port %u", config.port);
    LOG_INFO("Waiting for connections...");

    /* Accept connections (one at a time for now) */
    while (1) {
        char client_ip[64];
        client_sock = socket_accept_connection(listen_sock, client_ip, sizeof(client_ip), &error);
        if (client_sock == INVALID_SOCKET_VALUE) {
            LOG_ERROR("Failed to accept connection: %s", protocol_get_error_string(error));
            continue;
        }

        LOG_INFO("Client connected: %s", client_ip);

        /* Set timeout for client socket */
        socket_set_timeout(client_sock, FT_TIMEOUT_SECONDS, NULL);

        /* Receive file */
        if (receive_file(client_sock, config.output_dir) == 0) {
            LOG_INFO("Transfer completed successfully");
            exit_code = 0;
        } else {
            LOG_ERROR("Transfer failed");
        }

        /* Close client socket */
        close_socket(client_sock);
        client_sock = INVALID_SOCKET_VALUE;

        LOG_INFO("Client disconnected");

        /* For now, exit after first transfer */
        /* TODO: Support multiple consecutive transfers */
        break;
    }

cleanup:
    if (client_sock != INVALID_SOCKET_VALUE) {
        close_socket(client_sock);
    }
    if (listen_sock != INVALID_SOCKET_VALUE) {
        close_socket(listen_sock);
    }

    platform_cleanup();
    logger_close();

    return exit_code;
}
