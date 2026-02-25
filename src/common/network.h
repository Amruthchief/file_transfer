#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include "platform.h"
#include "protocol.h"

/* Socket creation and configuration */
socket_t socket_create(FTErrorCode *error);
int socket_set_timeout(socket_t sock, int timeout_seconds, FTErrorCode *error);
int socket_set_nodelay(socket_t sock, int enable, FTErrorCode *error);
int socket_set_reuseaddr(socket_t sock, int enable, FTErrorCode *error);

/* Server-side functions */
int socket_bind_and_listen(socket_t sock, uint16_t port, int backlog, FTErrorCode *error);
socket_t socket_accept_connection(socket_t listen_sock, char *client_ip, size_t ip_size, FTErrorCode *error);

/* Client-side functions */
int socket_connect_with_retry(socket_t sock, const char *host, uint16_t port,
                               int max_retries, FTErrorCode *error);

/* Raw send/receive with retry logic */
int socket_send_all(socket_t sock, const uint8_t *buffer, size_t length, FTErrorCode *error);
int socket_recv_all(socket_t sock, uint8_t *buffer, size_t length, FTErrorCode *error);

/* Protocol message functions */
int send_message(socket_t sock, MessageType msg_type, uint64_t sequence_num,
                 const uint8_t *payload, size_t payload_size, FTErrorCode *error);

int recv_message(socket_t sock, MessageHeader *header, uint8_t *payload,
                 size_t max_payload_size, FTErrorCode *error);

/* Handshake functions */
int perform_handshake_client(socket_t sock, FTErrorCode *error);
int perform_handshake_server(socket_t sock, FTErrorCode *error);

/* File info exchange */
int send_file_info(socket_t sock, const FileInfo *file_info, uint64_t sequence_num, FTErrorCode *error);
int recv_file_info(socket_t sock, FileInfo *file_info, FTErrorCode *error);

/* Chunk transfer */
int send_chunk(socket_t sock, uint64_t chunk_id, uint64_t chunk_offset,
               const uint8_t *data, size_t data_size, uint64_t sequence_num, FTErrorCode *error);

int recv_chunk(socket_t sock, ChunkHeader *chunk_hdr, uint8_t *data,
               size_t max_data_size, FTErrorCode *error);

/* Acknowledgments */
int send_chunk_ack(socket_t sock, uint64_t chunk_id, uint8_t status,
                   uint64_t sequence_num, FTErrorCode *error);

int recv_chunk_ack(socket_t sock, ChunkAck *ack, FTErrorCode *error);

/* Error messages */
int send_error(socket_t sock, FTErrorCode error_code, uint64_t chunk_id,
               const char *message, uint64_t sequence_num, FTErrorCode *send_error);

int recv_error(socket_t sock, ErrorMessage *error_msg, FTErrorCode *error);

/* Utility functions */
int resolve_hostname(const char *hostname, char *ip_address, size_t ip_size);

#endif /* NETWORK_H */
