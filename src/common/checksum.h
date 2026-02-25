#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

/* CRC32 Functions (minimal implementation for protocol) */
uint32_t crc32_compute(const uint8_t *data, size_t length);

/* TODO: Implement SHA-256 for file integrity checking */
/* TODO: Implement incremental checksum contexts */

#endif /* CHECKSUM_H */
