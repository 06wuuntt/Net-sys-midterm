#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

static const char *names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

void log_write(int level, const char *file, int line, const char *fmt, ...)
{
    static int runtime_level = -2;            /* -2 = not read yet */
    if (runtime_level == -2) {
        const char *e = getenv("LOG_LEVEL");
        runtime_level = e ? atoi(e) : LOG_INFO;
    }
    if (level < runtime_level) return;

    /* build the whole line first and write it with one call, so lines from
     * different processes do not get mixed together */
    char buf[1024], ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof ts, "%H:%M:%S", localtime(&now));
    int n = snprintf(buf, sizeof buf, "[%s][%d][%s] %s:%d: ", ts, (int)getpid(), names[level], file, line);
    va_list ap;
    va_start(ap, fmt);
    n += vsnprintf(buf + n, sizeof buf - (size_t)n - 1, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof buf - 2) n = (int)sizeof buf - 2;   /* message was truncated */
    buf[n++] = '\n';
    if (write(STDERR_FILENO, buf, (size_t)n) < 0) { /* nothing useful to do */ }
}

ssize_t send_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = send(fd, p, left, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return (ssize_t)len;
}

ssize_t recv_line(int fd, char *buf, size_t max)
{
    size_t i = 0;
    while (i + 1 < max) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) return i ? (ssize_t)i : 0;   /* EOF */
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (c == '\n') { buf[i] = '\0'; return (ssize_t)i + 1; }
        buf[i++] = c;
    }
    errno = EMSGSIZE;                             /* line too long */
    return -1;
}

void get_sysinfo(char *out, size_t max)
{
    struct utsname u;
    struct sysinfo si;
    uname(&u);
    sysinfo(&si);
    snprintf(out, max, "host=%s os=%s %s arch=%s uptime=%lds procs=%d",
             u.nodename, u.sysname, u.release, u.machine, si.uptime, si.procs);
}
