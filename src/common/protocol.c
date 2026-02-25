#include "protocol.h"
#include "platform.h"
#include "checksum.h"
#include <string.h>
#include <stdio.h>

/* Initialize message header */
void protocol_init_header(MessageHeader *header, MessageType msg_type,
                         uint64_t sequence_num, uint64_t payload_size) {
    memset(header, 0, sizeof(MessageHeader));
    header->magic = FT_MAGIC_NUMBER;
    header->version = FT_PROTOCOL_VERSION;
    header->msg_type = (uint8_t)msg_type;
    header->flags = 0;
    header->sequence_num = sequence_num;
    header->payload_size = payload_size;
    header->reserved = 0;
    /* Checksum will be computed during serialization */
}

/* Serialize header to network byte order */
void protocol_serialize_header(const MessageHeader *header, uint8_t *buffer) {
    uint32_t *buf32 = (uint32_t*)buffer;
    uint16_t *buf16 = (uint16_t*)buffer;
    uint64_t *buf64 = (uint64_t*)buffer;

    /* Layout: magic(4) version(1) msg_type(1) flags(2) seq(8) payload(8) checksum(4) reserved(4) */
    buf32[0] = htonl(header->magic);                    /* offset 0 */
    buffer[4] = header->version;                         /* offset 4 */
    buffer[5] = header->msg_type;                        /* offset 5 */
    buf16[3] = htons(header->flags);                    /* offset 6 */
    buf64[1] = htonll(header->sequence_num);            /* offset 8 */
    buf64[2] = htonll(header->payload_size);            /* offset 16 */

    /* Compute checksum of first 24 bytes */
    uint32_t checksum = crc32_compute(buffer, 24);
    buf32[6] = htonl(checksum);                         /* offset 24 */
    buf32[7] = htonl(header->reserved);                 /* offset 28 */
}

/* Deserialize header from network byte order */
int protocol_deserialize_header(const uint8_t *buffer, MessageHeader *header) {
    const uint32_t *buf32 = (const uint32_t*)buffer;
    const uint16_t *buf16 = (const uint16_t*)buffer;
    const uint64_t *buf64 = (const uint64_t*)buffer;

    header->magic = ntohl(buf32[0]);
    header->version = buffer[4];
    header->msg_type = buffer[5];
    header->flags = ntohs(buf16[3]);
    header->sequence_num = ntohll(buf64[1]);
    header->payload_size = ntohll(buf64[2]);
    header->checksum = ntohl(buf32[6]);
    header->reserved = ntohl(buf32[7]);

    return 0;
}

/* Validate header */
int protocol_validate_header(const MessageHeader *header) {
    /* Check magic number */
    if (header->magic != FT_MAGIC_NUMBER) {
        return FT_ERR_PROTOCOL;
    }

    /* Check version */
    if (header->version != FT_PROTOCOL_VERSION) {
        return FT_ERR_VERSION;
    }

    /* Check message type */
    if (header->msg_type < MSG_HANDSHAKE_REQ ||
        (header->msg_type > MSG_VERIFY_RESPONSE && header->msg_type != MSG_ERROR)) {
        return FT_ERR_INVALID_MSG;
    }

    return FT_SUCCESS;
}

/* Compute header checksum */
uint32_t protocol_compute_header_checksum(const MessageHeader *header) {
    uint8_t buffer[FT_HEADER_SIZE];
    protocol_serialize_header(header, buffer);
    return crc32_compute(buffer, 24);
}

/* Serialize file info */
void protocol_serialize_file_info(const FileInfo *file_info, uint8_t *buffer) {
    uint16_t *buf16 = (uint16_t*)buffer;
    uint32_t *buf32;
    uint64_t *buf64;
    size_t offset = 0;

    /* filename_len (2 bytes) */
    buf16[0] = htons(file_info->filename_len);
    offset += 2;

    /* filename (256 bytes) */
    memcpy(buffer + offset, file_info->filename, FT_MAX_FILENAME_LEN);
    offset += FT_MAX_FILENAME_LEN;

    /* file_size (8 bytes) */
    buf64 = (uint64_t*)(buffer + offset);
    *buf64 = htonll(file_info->file_size);
    offset += 8;

    /* total_chunks (8 bytes) */
    buf64 = (uint64_t*)(buffer + offset);
    *buf64 = htonll(file_info->total_chunks);
    offset += 8;

    /* chunk_size (4 bytes) */
    buf32 = (uint32_t*)(buffer + offset);
    *buf32 = htonl(file_info->chunk_size);
    offset += 4;

    /* checksum_type (1 byte) */
    buffer[offset++] = file_info->checksum_type;

    /* file_checksum (32 bytes) */
    memcpy(buffer + offset, file_info->file_checksum, FT_SHA256_SIZE);
    offset += FT_SHA256_SIZE;

    /* file_mode (4 bytes) */
    buf32 = (uint32_t*)(buffer + offset);
    *buf32 = htonl(file_info->file_mode);
    offset += 4;

    /* timestamp (8 bytes) */
    buf64 = (uint64_t*)(buffer + offset);
    *buf64 = htonll(file_info->timestamp);
    offset += 8;

    /* reserved (669 bytes) */
    memset(buffer + offset, 0, 669);
}

/* Deserialize file info */
int protocol_deserialize_file_info(const uint8_t *buffer, FileInfo *file_info) {
    const uint16_t *buf16 = (const uint16_t*)buffer;
    const uint32_t *buf32;
    const uint64_t *buf64;
    size_t offset = 0;

    /* filename_len */
    file_info->filename_len = ntohs(buf16[0]);
    offset += 2;

    /* filename */
    memcpy(file_info->filename, buffer + offset, FT_MAX_FILENAME_LEN);
    file_info->filename[FT_MAX_FILENAME_LEN - 1] = '\0';  /* Ensure null-terminated */
    offset += FT_MAX_FILENAME_LEN;

    /* file_size */
    buf64 = (const uint64_t*)(buffer + offset);
    file_info->file_size = ntohll(*buf64);
    offset += 8;

    /* total_chunks */
    buf64 = (const uint64_t*)(buffer + offset);
    file_info->total_chunks = ntohll(*buf64);
    offset += 8;

    /* chunk_size */
    buf32 = (const uint32_t*)(buffer + offset);
    file_info->chunk_size = ntohl(*buf32);
    offset += 4;

    /* checksum_type */
    file_info->checksum_type = buffer[offset++];

    /* file_checksum */
    memcpy(file_info->file_checksum, buffer + offset, FT_SHA256_SIZE);
    offset += FT_SHA256_SIZE;

    /* file_mode */
    buf32 = (const uint32_t*)(buffer + offset);
    file_info->file_mode = ntohl(*buf32);
    offset += 4;

    /* timestamp */
    buf64 = (const uint64_t*)(buffer + offset);
    file_info->timestamp = ntohll(*buf64);
    offset += 8;

    /* reserved bytes ignored */

    return 0;
}

/* Serialize chunk header */
void protocol_serialize_chunk_header(const ChunkHeader *chunk_hdr, uint8_t *buffer) {
    uint64_t *buf64 = (uint64_t*)buffer;
    uint32_t *buf32;

    /* chunk_id (8 bytes) */
    buf64[0] = htonll(chunk_hdr->chunk_id);

    /* chunk_offset (8 bytes) */
    buf64[1] = htonll(chunk_hdr->chunk_offset);

    /* chunk_size (4 bytes) */
    buf32 = (uint32_t*)(buffer + 16);
    buf32[0] = htonl(chunk_hdr->chunk_size);

    /* chunk_crc32 (4 bytes) */
    buf32[1] = htonl(chunk_hdr->chunk_crc32);
}

/* Deserialize chunk header */
int protocol_deserialize_chunk_header(const uint8_t *buffer, ChunkHeader *chunk_hdr) {
    const uint64_t *buf64 = (const uint64_t*)buffer;
    const uint32_t *buf32;

    /* chunk_id */
    chunk_hdr->chunk_id = ntohll(buf64[0]);

    /* chunk_offset */
    chunk_hdr->chunk_offset = ntohll(buf64[1]);

    /* chunk_size */
    buf32 = (const uint32_t*)(buffer + 16);
    chunk_hdr->chunk_size = ntohl(buf32[0]);

    /* chunk_crc32 */
    chunk_hdr->chunk_crc32 = ntohl(buf32[1]);

    return 0;
}

/* Get error message string */
const char* protocol_get_error_string(FTErrorCode error_code) {
    switch (error_code) {
        case FT_SUCCESS: return "Success";
        case FT_ERR_SOCKET: return "Socket error";
        case FT_ERR_CONNECT: return "Connection failed";
        case FT_ERR_BIND: return "Bind failed";
        case FT_ERR_LISTEN: return "Listen failed";
        case FT_ERR_ACCEPT: return "Accept failed";
        case FT_ERR_SEND: return "Send failed";
        case FT_ERR_RECV: return "Receive failed";
        case FT_ERR_TIMEOUT: return "Operation timed out";
        case FT_ERR_FILE_OPEN: return "File open failed";
        case FT_ERR_FILE_READ: return "File read failed";
        case FT_ERR_FILE_WRITE: return "File write failed";
        case FT_ERR_FILE_SEEK: return "File seek failed";
        case FT_ERR_DISK_FULL: return "Disk full";
        case FT_ERR_PERMISSION: return "Permission denied";
        case FT_ERR_CHECKSUM: return "Checksum mismatch";
        case FT_ERR_PROTOCOL: return "Protocol error";
        case FT_ERR_VERSION: return "Version mismatch";
        case FT_ERR_INVALID_MSG: return "Invalid message";
        case FT_ERR_OUT_OF_MEMORY: return "Out of memory";
        case FT_ERR_INVALID_ARG: return "Invalid argument";
        case FT_ERR_FILE_NOT_FOUND: return "File not found";
        case FT_ERR_FILENAME_TOO_LONG: return "Filename too long";
        default: return "Unknown error";
    }
}
