#include "network.h"
#include "logger.h"
#include "checksum.h"
#include <string.h>
#include <stdio.h>

/* Create socket */
socket_t socket_create(FTErrorCode *error) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to create socket: %s", platform_get_socket_error(socket_errno));
        if (error) *error = FT_ERR_SOCKET;
        return INVALID_SOCKET_VALUE;
    }
    if (error) *error = FT_SUCCESS;
    return sock;
}

/* Set socket timeout */
int socket_set_timeout(socket_t sock, int timeout_seconds, FTErrorCode *error) {
#ifdef FT_PLATFORM_WINDOWS
    DWORD timeout_ms = timeout_seconds * 1000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) != 0 ||
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms)) != 0) {
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
#endif
        LOG_ERROR("Failed to set socket timeout: %s", platform_get_socket_error(socket_errno));
        if (error) *error = FT_ERR_SOCKET;
        return -1;
    }
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Set TCP_NODELAY (disable Nagle's algorithm) */
int socket_set_nodelay(socket_t sock, int enable, FTErrorCode *error) {
    int flag = enable ? 1 : 0;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag)) != 0) {
        LOG_WARN("Failed to set TCP_NODELAY: %s", platform_get_socket_error(socket_errno));
        /* Not fatal */
    }
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Set SO_REUSEADDR */
int socket_set_reuseaddr(socket_t sock, int enable, FTErrorCode *error) {
    int flag = enable ? 1 : 0;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&flag, sizeof(flag)) != 0) {
        LOG_WARN("Failed to set SO_REUSEADDR: %s", platform_get_socket_error(socket_errno));
        /* Not fatal */
    }
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Bind and listen */
int socket_bind_and_listen(socket_t sock, uint16_t port, int backlog, FTErrorCode *error) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        LOG_ERROR("Failed to bind to port %u: %s", port, platform_get_socket_error(socket_errno));
        if (error) *error = FT_ERR_BIND;
        return -1;
    }

    if (listen(sock, backlog) != 0) {
        LOG_ERROR("Failed to listen: %s", platform_get_socket_error(socket_errno));
        if (error) *error = FT_ERR_LISTEN;
        return -1;
    }

    LOG_INFO("Listening on port %u", port);
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Accept connection */
socket_t socket_accept_connection(socket_t listen_sock, char *client_ip, size_t ip_size, FTErrorCode *error) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    socket_t client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &addr_len);
    if (client_sock == INVALID_SOCKET_VALUE) {
        LOG_ERROR("Failed to accept connection: %s", platform_get_socket_error(socket_errno));
        if (error) *error = FT_ERR_ACCEPT;
        return INVALID_SOCKET_VALUE;
    }

    if (client_ip != NULL && ip_size > 0) {
        const char *ip = inet_ntoa(client_addr.sin_addr);
        strncpy(client_ip, ip, ip_size - 1);
        client_ip[ip_size - 1] = '\0';
        LOG_INFO("Accepted connection from %s", client_ip);
    }

    if (error) *error = FT_SUCCESS;
    return client_sock;
}

/* Connect with retry */
int socket_connect_with_retry(socket_t sock, const char *host, uint16_t port,
                               int max_retries, FTErrorCode *error) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    /* Try to parse as IP address first */
    addr.sin_addr.s_addr = inet_addr(host);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        /* Not a valid IP, try to resolve as hostname */
        struct hostent *he = gethostbyname(host);
        if (he == NULL || he->h_addr_list[0] == NULL) {
            LOG_ERROR("Failed to resolve hostname: %s", host);
            if (error) *error = FT_ERR_CONNECT;
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    }

    /* Try to connect with exponential backoff */
    int delay_ms = 1000;
    for (int attempt = 1; attempt <= max_retries; attempt++) {
        LOG_INFO("Connecting to %s:%u (attempt %d/%d)", host, port, attempt, max_retries);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            LOG_INFO("Connected successfully");
            if (error) *error = FT_SUCCESS;
            return 0;
        }

        int err = socket_errno;
        LOG_WARN("Connection attempt %d failed: %s", attempt, platform_get_socket_error(err));

        if (attempt < max_retries) {
            LOG_INFO("Retrying in %d ms...", delay_ms);
            platform_sleep_ms(delay_ms);
            delay_ms = (delay_ms * 2 < FT_BACKOFF_MAX_MS) ? delay_ms * 2 : FT_BACKOFF_MAX_MS;
        }
    }

    LOG_ERROR("Failed to connect after %d attempts", max_retries);
    if (error) *error = FT_ERR_CONNECT;
    return -1;
}

/* Send all bytes */
int socket_send_all(socket_t sock, const uint8_t *buffer, size_t length, FTErrorCode *error) {
    size_t total_sent = 0;
    while (total_sent < length) {
        int sent = send(sock, (const char*)(buffer + total_sent), (int)(length - total_sent), 0);
        if (sent <= 0) {
            int err = socket_errno;
            LOG_ERROR("Send failed: %s", platform_get_socket_error(err));
            if (error) *error = platform_is_fatal_socket_error(err) ? FT_ERR_SEND : FT_ERR_TIMEOUT;
            return -1;
        }
        total_sent += sent;
    }
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Receive all bytes */
int socket_recv_all(socket_t sock, uint8_t *buffer, size_t length, FTErrorCode *error) {
    size_t total_received = 0;
    while (total_received < length) {
        int received = recv(sock, (char*)(buffer + total_received), (int)(length - total_received), 0);
        if (received == 0) {
            LOG_ERROR("Connection closed by peer");
            if (error) *error = FT_ERR_RECV;
            return -1;
        }
        if (received < 0) {
            int err = socket_errno;
            LOG_ERROR("Receive failed: %s", platform_get_socket_error(err));
            if (error) *error = platform_is_fatal_socket_error(err) ? FT_ERR_RECV : FT_ERR_TIMEOUT;
            return -1;
        }
        total_received += received;
    }
    if (error) *error = FT_SUCCESS;
    return 0;
}

/* Send message */
int send_message(socket_t sock, MessageType msg_type, uint64_t sequence_num,
                 const uint8_t *payload, size_t payload_size, FTErrorCode *error) {
    /* Initialize header */
    MessageHeader header;
    protocol_init_header(&header, msg_type, sequence_num, payload_size);

    /* Serialize header */
    uint8_t header_buf[FT_HEADER_SIZE];
    protocol_serialize_header(&header, header_buf);

    /* Send header */
    if (socket_send_all(sock, header_buf, FT_HEADER_SIZE, error) != 0) {
        return -1;
    }

    /* Send payload if present */
    if (payload_size > 0 && payload != NULL) {
        if (socket_send_all(sock, payload, payload_size, error) != 0) {
            return -1;
        }
    }

    LOG_DEBUG("Sent message type %d, seq %llu, payload %zu bytes",
              msg_type, (unsigned long long)sequence_num, payload_size);
    return 0;
}

/* Receive message */
int recv_message(socket_t sock, MessageHeader *header, uint8_t *payload,
                 size_t max_payload_size, FTErrorCode *error) {
    /* Receive header */
    uint8_t header_buf[FT_HEADER_SIZE];
    if (socket_recv_all(sock, header_buf, FT_HEADER_SIZE, error) != 0) {
        return -1;
    }

    /* Deserialize header */
    protocol_deserialize_header(header_buf, header);

    /* Validate header */
    int validation_result = protocol_validate_header(header);
    if (validation_result != FT_SUCCESS) {
        LOG_ERROR("Invalid message header: %s", protocol_get_error_string(validation_result));
        if (error) *error = validation_result;
        return -1;
    }

    /* Receive payload if present */
    if (header->payload_size > 0) {
        if (header->payload_size > max_payload_size) {
            LOG_ERROR("Payload size %llu exceeds maximum %zu",
                      (unsigned long long)header->payload_size, max_payload_size);
            if (error) *error = FT_ERR_PROTOCOL;
            return -1;
        }

        if (socket_recv_all(sock, payload, (size_t)header->payload_size, error) != 0) {
            return -1;
        }
    }

    LOG_DEBUG("Received message type %d, seq %llu, payload %llu bytes",
              header->msg_type, (unsigned long long)header->sequence_num,
              (unsigned long long)header->payload_size);
    return 0;
}

/* Perform handshake - client side */
int perform_handshake_client(socket_t sock, FTErrorCode *error) {
    HandshakePayload payload;
    payload.protocol_version = FT_PROTOCOL_VERSION;
    payload.capabilities = 0;
    payload.reserved = 0;

    /* Send handshake request */
    if (send_message(sock, MSG_HANDSHAKE_REQ, 0, (uint8_t*)&payload, sizeof(payload), error) != 0) {
        return -1;
    }

    /* Receive handshake acknowledgment */
    MessageHeader header;
    HandshakePayload ack_payload;
    if (recv_message(sock, &header, (uint8_t*)&ack_payload, sizeof(ack_payload), error) != 0) {
        return -1;
    }

    if (header.msg_type != MSG_HANDSHAKE_ACK) {
        LOG_ERROR("Expected HANDSHAKE_ACK, got message type %d", header.msg_type);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    if (ack_payload.protocol_version != FT_PROTOCOL_VERSION) {
        LOG_ERROR("Protocol version mismatch: expected %d, got %d",
                  FT_PROTOCOL_VERSION, ack_payload.protocol_version);
        if (error) *error = FT_ERR_VERSION;
        return -1;
    }

    LOG_INFO("Handshake successful");
    return 0;
}

/* Perform handshake - server side */
int perform_handshake_server(socket_t sock, FTErrorCode *error) {
    /* Receive handshake request */
    MessageHeader header;
    HandshakePayload payload;
    if (recv_message(sock, &header, (uint8_t*)&payload, sizeof(payload), error) != 0) {
        return -1;
    }

    if (header.msg_type != MSG_HANDSHAKE_REQ) {
        LOG_ERROR("Expected HANDSHAKE_REQ, got message type %d", header.msg_type);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    if (payload.protocol_version != FT_PROTOCOL_VERSION) {
        LOG_ERROR("Protocol version mismatch: expected %d, got %d",
                  FT_PROTOCOL_VERSION, payload.protocol_version);
        if (error) *error = FT_ERR_VERSION;
        return -1;
    }

    /* Send handshake acknowledgment */
    HandshakePayload ack_payload;
    ack_payload.protocol_version = FT_PROTOCOL_VERSION;
    ack_payload.capabilities = 0;
    ack_payload.reserved = 0;

    if (send_message(sock, MSG_HANDSHAKE_ACK, header.sequence_num + 1,
                     (uint8_t*)&ack_payload, sizeof(ack_payload), error) != 0) {
        return -1;
    }

    LOG_INFO("Handshake successful");
    return 0;
}

/* Send file info */
int send_file_info(socket_t sock, const FileInfo *file_info, uint64_t sequence_num, FTErrorCode *error) {
    uint8_t buffer[FT_FILE_INFO_SIZE];
    protocol_serialize_file_info(file_info, buffer);
    return send_message(sock, MSG_FILE_INFO, sequence_num, buffer, FT_FILE_INFO_SIZE, error);
}

/* Receive file info */
int recv_file_info(socket_t sock, FileInfo *file_info, FTErrorCode *error) {
    MessageHeader header;
    uint8_t buffer[FT_FILE_INFO_SIZE];
    if (recv_message(sock, &header, buffer, FT_FILE_INFO_SIZE, error) != 0) {
        return -1;
    }

    if (header.msg_type != MSG_FILE_INFO) {
        LOG_ERROR("Expected FILE_INFO, got message type %d", header.msg_type);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    protocol_deserialize_file_info(buffer, file_info);
    return 0;
}

/* Send chunk */
int send_chunk(socket_t sock, uint64_t chunk_id, uint64_t chunk_offset,
               const uint8_t *data, size_t data_size, uint64_t sequence_num, FTErrorCode *error) {
    /* Compute chunk CRC32 */
    uint32_t chunk_crc = crc32_compute(data, data_size);

    /* Create chunk header */
    ChunkHeader chunk_hdr;
    chunk_hdr.chunk_id = chunk_id;
    chunk_hdr.chunk_offset = chunk_offset;
    chunk_hdr.chunk_size = (uint32_t)data_size;
    chunk_hdr.chunk_crc32 = chunk_crc;

    /* Serialize chunk header */
    uint8_t chunk_hdr_buf[FT_CHUNK_HEADER_SIZE];
    protocol_serialize_chunk_header(&chunk_hdr, chunk_hdr_buf);

    /* Send message header */
    size_t total_payload = FT_CHUNK_HEADER_SIZE + data_size;
    MessageHeader msg_hdr;
    protocol_init_header(&msg_hdr, MSG_CHUNK_DATA, sequence_num, total_payload);

    uint8_t msg_hdr_buf[FT_HEADER_SIZE];
    protocol_serialize_header(&msg_hdr, msg_hdr_buf);

    if (socket_send_all(sock, msg_hdr_buf, FT_HEADER_SIZE, error) != 0) {
        return -1;
    }

    /* Send chunk header */
    if (socket_send_all(sock, chunk_hdr_buf, FT_CHUNK_HEADER_SIZE, error) != 0) {
        return -1;
    }

    /* Send chunk data */
    if (socket_send_all(sock, data, data_size, error) != 0) {
        return -1;
    }

    LOG_DEBUG("Sent chunk %llu, %zu bytes, CRC32: 0x%08X",
              (unsigned long long)chunk_id, data_size, chunk_crc);
    return 0;
}

/* Receive chunk */
int recv_chunk(socket_t sock, ChunkHeader *chunk_hdr, uint8_t *data,
               size_t max_data_size, FTErrorCode *error) {
    /* Receive message header */
    MessageHeader msg_hdr;
    uint8_t chunk_hdr_buf[FT_CHUNK_HEADER_SIZE];

    /* First receive the message header */
    uint8_t msg_hdr_buf[FT_HEADER_SIZE];
    if (socket_recv_all(sock, msg_hdr_buf, FT_HEADER_SIZE, error) != 0) {
        return -1;
    }

    protocol_deserialize_header(msg_hdr_buf, &msg_hdr);

    if (protocol_validate_header(&msg_hdr) != FT_SUCCESS) {
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    if (msg_hdr.msg_type != MSG_CHUNK_DATA) {
        LOG_ERROR("Expected CHUNK_DATA, got message type %d", msg_hdr.msg_type);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    /* Receive chunk header */
    if (socket_recv_all(sock, chunk_hdr_buf, FT_CHUNK_HEADER_SIZE, error) != 0) {
        return -1;
    }

    protocol_deserialize_chunk_header(chunk_hdr_buf, chunk_hdr);

    if (chunk_hdr->chunk_size > max_data_size) {
        LOG_ERROR("Chunk size %u exceeds maximum %zu", chunk_hdr->chunk_size, max_data_size);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    /* Receive chunk data */
    if (socket_recv_all(sock, data, chunk_hdr->chunk_size, error) != 0) {
        return -1;
    }

    /* Verify CRC32 */
    uint32_t computed_crc = crc32_compute(data, chunk_hdr->chunk_size);
    if (computed_crc != chunk_hdr->chunk_crc32) {
        LOG_ERROR("Chunk %llu CRC32 mismatch: expected 0x%08X, got 0x%08X",
                  (unsigned long long)chunk_hdr->chunk_id, chunk_hdr->chunk_crc32, computed_crc);
        if (error) *error = FT_ERR_CHECKSUM;
        return -1;
    }

    LOG_DEBUG("Received chunk %llu, %u bytes, CRC32 OK",
              (unsigned long long)chunk_hdr->chunk_id, chunk_hdr->chunk_size);
    return 0;
}

/* Send chunk acknowledgment */
int send_chunk_ack(socket_t sock, uint64_t chunk_id, uint8_t status,
                   uint64_t sequence_num, FTErrorCode *error) {
    ChunkAck ack;
    ack.chunk_id = chunk_id;
    ack.status = status;
    memset(ack.reserved, 0, sizeof(ack.reserved));

    /* Serialize (need to convert to network byte order) */
    uint8_t buffer[16];
    uint64_t *buf64 = (uint64_t*)buffer;
    buf64[0] = htonll(ack.chunk_id);
    buffer[8] = ack.status;
    memset(buffer + 9, 0, 3);

    return send_message(sock, MSG_CHUNK_ACK, sequence_num, buffer, sizeof(ack), error);
}

/* Receive chunk acknowledgment */
int recv_chunk_ack(socket_t sock, ChunkAck *ack, FTErrorCode *error) {
    MessageHeader header;
    uint8_t buffer[16];

    if (recv_message(sock, &header, buffer, sizeof(buffer), error) != 0) {
        return -1;
    }

    if (header.msg_type != MSG_CHUNK_ACK) {
        LOG_ERROR("Expected CHUNK_ACK, got message type %d", header.msg_type);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    /* Deserialize */
    const uint64_t *buf64 = (const uint64_t*)buffer;
    ack->chunk_id = ntohll(buf64[0]);
    ack->status = buffer[8];

    return 0;
}

/* Send error */
int send_error(socket_t sock, FTErrorCode error_code, uint64_t chunk_id,
               const char *message, uint64_t sequence_num, FTErrorCode *send_error) {
    ErrorMessage err_msg;
    err_msg.error_code = (uint8_t)error_code;
    err_msg.chunk_id = chunk_id;
    strncpy((char*)err_msg.message, message ? message : "", sizeof(err_msg.message) - 1);
    err_msg.message[sizeof(err_msg.message) - 1] = '\0';

    /* Serialize */
    uint8_t buffer[256];
    buffer[0] = err_msg.error_code;
    uint64_t *buf64 = (uint64_t*)(buffer + 1);
    *buf64 = htonll(err_msg.chunk_id);
    memcpy(buffer + 9, err_msg.message, sizeof(err_msg.message));

    return send_message(sock, MSG_ERROR, sequence_num, buffer, sizeof(err_msg), send_error);
}

/* Receive error */
int recv_error(socket_t sock, ErrorMessage *error_msg, FTErrorCode *error) {
    MessageHeader header;
    uint8_t buffer[256];

    if (recv_message(sock, &header, buffer, sizeof(buffer), error) != 0) {
        return -1;
    }

    if (header.msg_type != MSG_ERROR) {
        LOG_ERROR("Expected ERROR, got message type %d", header.msg_type);
        if (error) *error = FT_ERR_PROTOCOL;
        return -1;
    }

    /* Deserialize */
    error_msg->error_code = buffer[0];
    const uint64_t *buf64 = (const uint64_t*)(buffer + 1);
    error_msg->chunk_id = ntohll(*buf64);
    memcpy(error_msg->message, buffer + 9, sizeof(error_msg->message));
    error_msg->message[sizeof(error_msg->message) - 1] = '\0';

    return 0;
}

/* Resolve hostname to IP address */
int resolve_hostname(const char *hostname, char *ip_address, size_t ip_size) {
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL || he->h_addr_list[0] == NULL) {
        return -1;
    }

    struct in_addr addr;
    memcpy(&addr, he->h_addr_list[0], sizeof(addr));
    const char *ip = inet_ntoa(addr);
    strncpy(ip_address, ip, ip_size - 1);
    ip_address[ip_size - 1] = '\0';

    return 0;
}
