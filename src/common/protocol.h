#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* Protocol constants */
#define FT_PROTOCOL_VERSION    0x01
#define FT_MAGIC_NUMBER        0x46544350  /* "FTCP" in hex */
#define FT_DEFAULT_PORT        8080
#define FT_DEFAULT_CHUNK_SIZE  524288      /* 512 KB */
#define FT_MAX_FILENAME_LEN    256
#define FT_MAX_RETRIES         3
#define FT_TIMEOUT_SECONDS     60
#define FT_KEEPALIVE_INTERVAL  30
#define FT_BACKOFF_MAX_MS      16000
#define FT_HEADER_SIZE         32
#define FT_FILE_INFO_SIZE      1024
#define FT_CHUNK_HEADER_SIZE   24
#define FT_SHA256_SIZE         32

/* Message types */
typedef enum {
    MSG_HANDSHAKE_REQ = 0x01,      /* Client initiates connection */
    MSG_HANDSHAKE_ACK = 0x02,      /* Server acknowledges */
    MSG_FILE_INFO = 0x03,          /* File metadata */
    MSG_FILE_ACK = 0x04,           /* Server ready to receive */
    MSG_CHUNK_DATA = 0x05,         /* File chunk payload */
    MSG_CHUNK_ACK = 0x06,          /* Chunk received confirmation */
    MSG_TRANSFER_COMPLETE = 0x07,  /* All chunks sent */
    MSG_VERIFY_REQUEST = 0x08,     /* Request final verification */
    MSG_VERIFY_RESPONSE = 0x09,    /* Verification result */
    MSG_ERROR = 0xFF               /* Error condition */
} MessageType;

/* Error codes */
typedef enum {
    FT_SUCCESS = 0,
    FT_ERR_SOCKET = -1,
    FT_ERR_CONNECT = -2,
    FT_ERR_BIND = -3,
    FT_ERR_LISTEN = -4,
    FT_ERR_ACCEPT = -5,
    FT_ERR_SEND = -6,
    FT_ERR_RECV = -7,
    FT_ERR_TIMEOUT = -8,
    FT_ERR_FILE_OPEN = -10,
    FT_ERR_FILE_READ = -11,
    FT_ERR_FILE_WRITE = -12,
    FT_ERR_FILE_SEEK = -13,
    FT_ERR_DISK_FULL = -14,
    FT_ERR_PERMISSION = -15,
    FT_ERR_CHECKSUM = -20,
    FT_ERR_PROTOCOL = -21,
    FT_ERR_VERSION = -22,
    FT_ERR_INVALID_MSG = -23,
    FT_ERR_OUT_OF_MEMORY = -30,
    FT_ERR_INVALID_ARG = -31,
    FT_ERR_FILE_NOT_FOUND = -32,
    FT_ERR_FILENAME_TOO_LONG = -33
} FTErrorCode;

/* Checksum types */
typedef enum {
    CHECKSUM_CRC32 = 0,
    CHECKSUM_MD5 = 1,
    CHECKSUM_SHA256 = 2
} ChecksumType;

/* Message header (32 bytes, fixed size) */
typedef struct {
    uint32_t magic;           /* Protocol magic number (0x46544350) */
    uint8_t  version;         /* Protocol version */
    uint8_t  msg_type;        /* Message type (MessageType enum) */
    uint16_t flags;           /* Reserved flags */
    uint64_t sequence_num;    /* Packet sequence number */
    uint64_t payload_size;    /* Size of payload following header */
    uint32_t checksum;        /* CRC32 of header (bytes 0-23) */
    uint32_t reserved;        /* Reserved for future use */
} __attribute__((packed)) MessageHeader;

/* Handshake request/ack payload */
typedef struct {
    uint8_t protocol_version;
    uint8_t capabilities;      /* Reserved for future capabilities */
    uint16_t reserved;
} __attribute__((packed)) HandshakePayload;

/* File info payload */
typedef struct {
    uint16_t filename_len;                /* Length of filename */
    char     filename[FT_MAX_FILENAME_LEN]; /* Filename (UTF-8) */
    uint64_t file_size;                   /* Total file size in bytes */
    uint64_t total_chunks;                /* Total number of chunks */
    uint32_t chunk_size;                  /* Size of each chunk (except last) */
    uint8_t  checksum_type;               /* Checksum type (ChecksumType enum) */
    uint8_t  file_checksum[FT_SHA256_SIZE]; /* File checksum (zero-padded) */
    uint32_t file_mode;                   /* File permissions (Unix-style) */
    uint64_t timestamp;                   /* File modification time (Unix epoch) */
    uint8_t  reserved[669];               /* Reserved for future use */
} __attribute__((packed)) FileInfo;

/* File acknowledgment payload */
typedef struct {
    uint8_t status;           /* 0 = ready, 1 = error */
    uint8_t error_code;       /* Error code if status != 0 */
    uint8_t reserved[2];
} __attribute__((packed)) FileAck;

/* Chunk header (follows message header in CHUNK_DATA messages) */
typedef struct {
    uint64_t chunk_id;        /* Chunk sequence number (0-based) */
    uint64_t chunk_offset;    /* Byte offset in file */
    uint32_t chunk_size;      /* Actual size of this chunk */
    uint32_t chunk_crc32;     /* CRC32 of chunk data */
} __attribute__((packed)) ChunkHeader;

/* Chunk acknowledgment payload */
typedef struct {
    uint64_t chunk_id;        /* Chunk ID being acknowledged */
    uint8_t  status;          /* 0 = OK, 1 = retry requested */
    uint8_t  reserved[3];
} __attribute__((packed)) ChunkAck;

/* Verification response payload */
typedef struct {
    uint8_t checksum_match;   /* 0 = mismatch, 1 = match */
    uint8_t error_code;       /* Error code if match failed */
    uint8_t reserved[2];
} __attribute__((packed)) VerifyResponse;

/* Error message payload */
typedef struct {
    uint8_t  error_code;      /* FTErrorCode */
    uint64_t chunk_id;        /* Relevant chunk ID (if applicable) */
    uint8_t  message[247];    /* Error message string (null-terminated) */
} __attribute__((packed)) ErrorMessage;

/* Protocol functions */

/* Initialize message header */
void protocol_init_header(MessageHeader *header, MessageType msg_type,
                         uint64_t sequence_num, uint64_t payload_size);

/* Serialize header to network byte order */
void protocol_serialize_header(const MessageHeader *header, uint8_t *buffer);

/* Deserialize header from network byte order */
int protocol_deserialize_header(const uint8_t *buffer, MessageHeader *header);

/* Validate header (magic, version, checksum) */
int protocol_validate_header(const MessageHeader *header);

/* Compute header checksum (CRC32 of first 24 bytes) */
uint32_t protocol_compute_header_checksum(const MessageHeader *header);

/* Serialize file info */
void protocol_serialize_file_info(const FileInfo *file_info, uint8_t *buffer);

/* Deserialize file info */
int protocol_deserialize_file_info(const uint8_t *buffer, FileInfo *file_info);

/* Serialize chunk header */
void protocol_serialize_chunk_header(const ChunkHeader *chunk_hdr, uint8_t *buffer);

/* Deserialize chunk header */
int protocol_deserialize_chunk_header(const uint8_t *buffer, ChunkHeader *chunk_hdr);

/* Get error message string from error code */
const char* protocol_get_error_string(FTErrorCode error_code);

#endif /* PROTOCOL_H */
