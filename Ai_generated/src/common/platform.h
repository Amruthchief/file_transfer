#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define FT_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define FT_PLATFORM_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
    #define FT_PLATFORM_MACOS
#elif defined(__unix__)
    #define FT_PLATFORM_UNIX
#else
    #error "Unsupported platform"
#endif

/* Platform-specific includes and definitions */
#ifdef FT_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    /* Socket types */
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR

    /* Socket functions */
    #define close_socket closesocket
    #define socket_errno WSAGetLastError()
    #define WOULD_BLOCK WSAEWOULDBLOCK
    #define IN_PROGRESS WSAEINPROGRESS
    #define CONN_REFUSED WSAECONNREFUSED
    #define CONN_RESET WSAECONNRESET
    #define TIMED_OUT WSAETIMEDOUT

    /* File I/O */
    #define fseeko _fseeki64
    #define ftello _ftelli64

    /* Path separator */
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"

#else
    /* Unix-like systems (Linux, macOS, BSD) */
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>

    /* Socket types */
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1

    /* Socket functions */
    #define close_socket close
    #define socket_errno errno
    #define WOULD_BLOCK EWOULDBLOCK
    #define IN_PROGRESS EINPROGRESS
    #define CONN_REFUSED ECONNREFUSED
    #define CONN_RESET ECONNRESET
    #define TIMED_OUT ETIMEDOUT

    /* Path separator */
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"

    /* Large file support is handled by _FILE_OFFSET_BITS=64 */
#endif

/* Cross-platform sleep function (milliseconds) */
void platform_sleep_ms(uint32_t milliseconds);

/* Platform initialization and cleanup */
int platform_init(void);
void platform_cleanup(void);

/* Get current time in milliseconds since epoch */
uint64_t platform_get_time_ms(void);

/* Get monotonic time in milliseconds (for elapsed time calculations) */
uint64_t platform_get_monotonic_ms(void);

/* Network byte order conversion (these are standard but included for completeness) */
#ifndef htonll
#define htonll(x) ((1==htonl(1)) ? (x) : \
    ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#endif

#ifndef ntohll
#define ntohll(x) ((1==ntohl(1)) ? (x) : \
    ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

/* Error code retrieval and formatting */
const char* platform_get_socket_error(int error_code);
const char* platform_get_last_error(void);

/* Check if socket error is fatal */
int platform_is_fatal_socket_error(int error_code);

#endif /* PLATFORM_H */
