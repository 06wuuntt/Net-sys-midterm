#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>
#include <sys/types.h>

/* ---- logging: two-level control ----
 * 1) compile time: make CFLAGS+=-DLOG_COMPILE_LEVEL=1  (messages below it are removed)
 * 2) run time:     LOG_LEVEL=0..3 ./build/server             (0=DEBUG 1=INFO 2=WARN 3=ERROR)
 */
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3

#ifndef LOG_COMPILE_LEVEL
#define LOG_COMPILE_LEVEL LOG_DEBUG
#endif

void log_write(int level, const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#define LOG(level, ...) \
    do { if ((level) >= LOG_COMPILE_LEVEL) \
        log_write((level), __FILE__, __LINE__, __VA_ARGS__); } while (0)

/* ---- socket helpers ---- */
ssize_t send_all(int fd, const void *buf, size_t len);
/* read one '\n'-terminated line; returns length, 0 on EOF, -1 on error (errno set;
 * EMSGSIZE = line too long, EAGAIN = receive timeout) */
ssize_t recv_line(int fd, char *buf, size_t max);

/* ---- system info (what the client asks the server for) ---- */
void get_sysinfo(char *out, size_t max);
#endif
