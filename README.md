# TCP File Transfer Utility

A robust, cross-platform file transfer utility capable of securely transferring files up to 16 GB over TCP/IP networks. This implementation uses pure C for maximum portability across Windows, Linux, and macOS.

## Features

- ✅ **Large File Support**: Transfer files up to 16 GB
- ✅ **Integrity Checking**: CRC32 per-chunk verification for data integrity
- ✅ **Error Handling**: Automatic retry mechanisms with exponential backoff
- ✅ **Cross-Platform**: Works on Windows, Linux, and macOS
- ✅ **Efficient Transfer**: 512 KB chunk size for optimal bandwidth utilization
- ✅ **Progress Tracking**: Real-time transfer progress and speed reporting
- ✅ **Robust Protocol**: Binary protocol with handshake and acknowledgments

## Architecture

- **Client-Server Model**: Separate sender (client) and receiver (server) programs
- **Binary Protocol**: Custom protocol with 32-byte headers for efficient communication
- **Chunk-Based Transfer**: Files are split into 512 KB chunks for manageable transfer
- **Synchronous I/O**: Simple, reliable implementation with timeout handling
- **Atomic File Operations**: Temporary file writing with atomic rename on success

## Project Structure

```
file_transfer/
├── src/
│   ├── common/          # Shared utilities
│   │   ├── platform.h/c # Cross-platform abstractions (sockets, time)
│   │   ├── protocol.h/c # Protocol definitions and serialization
│   │   ├── checksum.h/c # CRC32 checksum implementation
│   │   ├── network.h/c  # Network I/O and message handling
│   │   ├── fileio.h/c   # Safe file operations
│   │   └── logger.h/c   # Logging system
│   ├── server/
│   │   └── server_main.c # Server program (file receiver)
│   └── client/
│       └── client_main.c # Client program (file sender)
├── build/               # Build output directory
│   ├── ftserver.exe     # Server executable (Windows)
│   └── ftclient.exe     # Client executable (Windows)
├── docs/                # Documentation
├── tests/               # Unit and integration tests
├── Makefile            # Build system
└── README.md           # This file
```

## Prerequisites

### All Platforms
- C compiler (gcc, clang, or MSVC)
- Standard C11 library support

### Windows
- MinGW or MinGW-w64 (for gcc)
- Or Microsoft Visual C++ compiler
- Winsock2 library (ws2_32)

### Linux
- GCC or Clang
- GNU Make (optional, for using Makefile)
- Standard POSIX libraries

### macOS
- Xcode Command Line Tools (for gcc/clang)
- GNU Make (pre-installed)

## Building

### Windows (MinGW/Cygwin)

The project has been successfully compiled on Windows. The executables are in the `build/` directory.

```bash
# Manual compilation (if needed)
gcc -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 -Isrc/common -O2 \
    src/common/*.c src/server/*.c -o build/ftserver.exe -lws2_32

gcc -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 -Isrc/common -O2 \
    src/common/*.c src/client/*.c -o build/ftclient.exe -lws2_32
```

### Linux / macOS

```bash
# Using Makefile
make clean
make

# Or using gcc directly
gcc -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 -Isrc/common -O2 \
    src/common/*.c src/server/*.c -o build/ftserver -lpthread

gcc -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 -Isrc/common -O2 \
    src/common/*.c src/client/*.c -o build/ftclient -lpthread
```

### Debug Build

```bash
make MODE=debug
# Or manually add: -g -O0 -DDEBUG
```

## Usage

### Starting the Server

The server listens for incoming connections and receives files.

```bash
# Windows
build\ftserver.exe -p 8080 -d received

# Linux/macOS
./build/ftserver -p 8080 -d received
```

**Server Options:**
- `-p <port>` - Port to listen on (default: 8080)
- `-d <dir>` - Output directory for received files (default: current directory)
- `-v` - Verbose logging (show DEBUG messages)
- `-l <file>` - Log to file in addition to console
- `-h, --help` - Show help message

### Transferring a File (Client)

The client connects to a server and sends a file.

```bash
# Windows
build\ftclient.exe -h 192.168.1.100 -p 8080 -f myfile.iso

# Linux/macOS
./build/ftclient -h 192.168.1.100 -p 8080 -f myfile.iso
```

**Client Options:**
- `-h <host>` - Server hostname or IP address (required)
- `-f <file>` - File to transfer (required)
- `-p <port>` - Server port (default: 8080)
- `-v` - Verbose logging (show DEBUG messages)
- `-l <file>` - Log to file in addition to console
- `--help` - Show help message

## Example Session

**Terminal 1 (Server):**
```
$ ./build/ftserver -p 8080 -d ./received -v
[2026-02-26 00:30:15] INFO  [server_main.c:243] File Transfer Server starting...
[2026-02-26 00:30:15] INFO  [server_main.c:244] Output directory: ./received
[2026-02-26 00:30:15] INFO  [network.c:73] Listening on port 8080
[2026-02-26 00:30:23] INFO  [network.c:101] Accepted connection from 192.168.1.50
[2026-02-26 00:30:23] INFO  [network.c:361] Handshake successful
[2026-02-26 00:30:23] INFO  [server_main.c:78] File: largefile.iso, Size: 1073741824 bytes
[2026-02-26 00:30:24] INFO  [server_main.c:141] Receiving 2048 chunks...
[2026-02-26 00:30:30] INFO  [server_main.c:172] Progress: 10.0% (205/2048 chunks)
...
[2026-02-26 00:31:45] INFO  [server_main.c:199] File received successfully
```

**Terminal 2 (Client):**
```
$ ./build/ftclient -h 192.168.1.100 -p 8080 -f largefile.iso -v
[2026-02-26 00:30:22] INFO  [client_main.c:238] File Transfer Client starting...
[2026-02-26 00:30:23] INFO  [network.c:138] Connected successfully
[2026-02-26 00:30:24] INFO  [client.c:131] Sending file...
[2026-02-26 00:30:36] INFO  [client.c:182] Progress: 10.0% (205/2048) - 85.23 MB/s
...
[2026-02-26 00:31:45] INFO  [client.c:194] Transfer complete: 1073741824 bytes in 82.34s (98.47 MB/s)
```

## Protocol Specification

The utility uses a custom binary protocol with the following structure:

### Message Header (32 bytes)
- `magic` (4 bytes): Protocol identifier (0x46544350 = "FTCP")
- `version` (1 byte): Protocol version (0x01)
- `msg_type` (1 byte): Message type
- `flags` (2 bytes): Reserved
- `sequence_num` (8 bytes): Packet sequence number
- `payload_size` (8 bytes): Size of following payload
- `checksum` (4 bytes): CRC32 of header bytes 0-23
- `reserved` (4 bytes): Reserved for future use

### Message Types
- `0x01` HANDSHAKE_REQ - Client initiates connection
- `0x02` HANDSHAKE_ACK - Server acknowledges
- `0x03` FILE_INFO - File metadata
- `0x04` FILE_ACK - Server ready to receive
- `0x05` CHUNK_DATA - File chunk with data
- `0x06` CHUNK_ACK - Chunk received confirmation
- `0xFF` ERROR - Error condition

### Transfer Flow
1. Client connects to server
2. HANDSHAKE_REQ → HANDSHAKE_ACK (version negotiation)
3. FILE_INFO → FILE_ACK (file metadata exchange)
4. Loop: CHUNK_DATA → CHUNK_ACK (data transfer with retry on error)

## Performance

### Expected Performance
- **Throughput**: 80-120 MB/s on gigabit LAN
- **Memory Usage**: ~2-3 MB per transfer
- **CPU Usage**: <20% on modern processors

### Performance Tips
- Use a wired network connection for best performance
- Ensure server disk is not the bottleneck (use SSD)
- Disable firewalls or add exceptions for faster throughput

## Error Handling

The utility handles various error conditions:

- **Network Errors**: Automatic retry with exponential backoff
- **Checksum Failures**: Chunk retransmission (up to 3 attempts)
- **Connection Loss**: Timeout detection and graceful cleanup
- **Disk Full**: Early detection and proper error reporting
- **Permission Denied**: Clear error messages

## Limitations & Future Work

### Current Limitations
- Single file transfer per session
- No file compression
- No encryption (data sent in clear text)
- No authentication
- SHA-256 file integrity checking not yet implemented

### Future Enhancements (v2)
- [ ] Implement SHA-256 full-file verification
- [ ] Add TLS/SSL encryption support
- [ ] Authentication mechanism
- [ ] Resume interrupted transfers
- [ ] Multi-file batch transfers
- [ ] Compression support (zlib)

## Troubleshooting

### "Failed to bind to port"
- Port is already in use by another application
- Try a different port with `-p <port>`
- On Linux, ports below 1024 require root privileges

### "Connection refused"
- Server is not running
- Firewall blocking the connection
- Wrong IP address or port

### "Checksum mismatch"
- Network corruption (utility will automatically retry)

### "Disk full"
- Insufficient space on receiving disk
- Server detects this before transfer

## Testing

### Quick Test (Local)
```bash
# Terminal 1: Start server
./build/ftserver -p 9000 -d /tmp/received -v

# Terminal 2: Create test file and send
dd if=/dev/urandom of=testfile.bin bs=1M count=100
./build/ftclient -h localhost -p 9000 -f testfile.bin -v

# Verify received file
diff testfile.bin /tmp/received/testfile.bin
```

## License

This project is provided as-is for educational and assignment purposes.

## Documentation

For more detailed information, see:
- [docs/PROTOCOL.md](docs/PROTOCOL.md) - Detailed protocol specification
- [docs/PROJECT.md](docs/PROJECT.md) - Project architecture and design decisions

## Author

Created as a TCP file transfer utility assignment demonstrating:
- Cross-platform C programming
- Network protocol design
- Reliable data transfer mechanisms
- Error handling and recovery
- Performance optimization
