#include "platform.h"
#include <stdio.h>
#include <string.h>

#ifdef FT_PLATFORM_WINDOWS
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/time.h>
#endif

/* Cross-platform sleep function */
void platform_sleep_ms(uint32_t milliseconds) {
#ifdef FT_PLATFORM_WINDOWS
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

/* Platform initialization */
int platform_init(void) {
#ifdef FT_PLATFORM_WINDOWS
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        fprintf(stderr, "WSAStartup failed with error: %d\n", result);
        return -1;
    }
    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
        fprintf(stderr, "Could not find a usable version of Winsock.dll\n");
        WSACleanup();
        return -1;
    }
#endif
    return 0;
}

/* Platform cleanup */
void platform_cleanup(void) {
#ifdef FT_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

/* Get current time in milliseconds since Unix epoch */
uint64_t platform_get_time_ms(void) {
#ifdef FT_PLATFORM_WINDOWS
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Convert from 100-nanosecond intervals since 1601 to milliseconds since 1970 */
    return (uli.QuadPart / 10000ULL) - 11644473600000ULL;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/* Get monotonic time in milliseconds (for elapsed time calculations) */
uint64_t platform_get_monotonic_ms(void) {
#ifdef FT_PLATFORM_WINDOWS
    static LARGE_INTEGER frequency;
    static int initialized = 0;
    LARGE_INTEGER counter;

    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }

    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000) / frequency.QuadPart);
#else
    struct timespec ts;
    #ifdef CLOCK_MONOTONIC
        clock_gettime(CLOCK_MONOTONIC, &ts);
    #else
        clock_gettime(CLOCK_REALTIME, &ts);
    #endif
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* Get human-readable socket error message */
const char* platform_get_socket_error(int error_code) {
#ifdef FT_PLATFORM_WINDOWS
    static char buffer[256];
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer,
        sizeof(buffer),
        NULL
    );
    return buffer;
#else
    return strerror(error_code);
#endif
}

/* Get last error message */
const char* platform_get_last_error(void) {
#ifdef FT_PLATFORM_WINDOWS
    return platform_get_socket_error(GetLastError());
#else
    return strerror(errno);
#endif
}

/* Check if socket error is fatal (connection should be closed) */
int platform_is_fatal_socket_error(int error_code) {
    switch (error_code) {
        case WOULD_BLOCK:
        case IN_PROGRESS:
            return 0;  /* Not fatal, can retry */
        case CONN_REFUSED:
        case CONN_RESET:
        case TIMED_OUT:
            return 1;  /* Fatal, connection lost */
        default:
            return 1;  /* Assume fatal for unknown errors */
    }
}
