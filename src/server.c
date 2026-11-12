/* fork-per-connection TCP server.
 * usage: ./build/server [port] [--no-robust] [--timeout SEC] [--max-clients N]
 *   --no-robust     turn every protection off (used by the comparison experiments)
 *   --timeout SEC   close a connection that sends nothing for SEC seconds (default 30)
 *   --max-clients N refuse new connections while N children are running (default 64)
 */
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static volatile sig_atomic_t nchildren = 0;      /* children forked and not yet reaped */

static void on_sigchld(int sig)
{
    (void)sig;
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)       /* reap every finished child */
        nchildren--;
    errno = saved;
}

/* SIGINT / SIGTERM: stop the parent at once.  Only write() and _exit() are used
 * because they are safe inside a signal handler.  Exiting closes the listening
 * socket; children are separate processes and finish serving their clients. */
static void on_stop(int sig)
{
    (void)sig;
    static const char msg[] = "server: stop signal received, no longer accepting connections\n";
    if (write(STDERR_FILENO, msg, sizeof msg - 1) < 0) { /* nothing useful to do */ }
    _exit(0);
}

static void handle_client(int fd, int timeout)
{
    char line[256], reply[512];

    if (timeout > 0) {                           /* recv() fails with EAGAIN after this long */
        struct timeval tv = { .tv_sec = timeout, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    }
    while (1) {
        ssize_t n = recv_line(fd, line, sizeof line);
        if (n == 0) { LOG(LOG_INFO, "client closed the connection"); break; }
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                LOG(LOG_WARN, "client idle for %d s, closing", timeout);
            else
                LOG(LOG_WARN, "bad input: %s", strerror(errno));
            break;
        }
        LOG(LOG_DEBUG, "request: %s", line);
        if (strcmp(line, "INFO") == 0) {
            char info[400];
            get_sysinfo(info, sizeof info);
            snprintf(reply, sizeof reply, "OK %s\n", info);
        } else if (strncmp(line, "ECHO ", 5) == 0) {
            snprintf(reply, sizeof reply, "OK %s\n", line + 5);
        } else if (strcmp(line, "SLEEP") == 0) {  /* keeps a child alive, for concurrency proof */
            sleep(5);
            snprintf(reply, sizeof reply, "OK slept\n");
        } else if (strcmp(line, "FLOOD") == 0) {  /* many writes: exposes SIGPIPE when client vanished */
            memset(reply, 'x', 400);
            reply[400] = '\n';
            usleep(200000);                       /* let the client's close arrive first */
            for (int k = 0; k < 200; k++) {
                if (send_all(fd, reply, 401) < 0) {
                    LOG(LOG_WARN, "FLOOD: send failed at #%d: %s", k, strerror(errno));
                    return;
                }
            }
            LOG(LOG_INFO, "FLOOD: all 200 sent");
            continue;
        } else if (strcmp(line, "QUIT") == 0) {
            send_all(fd, "OK bye\n", 7);
            break;
        } else {
            snprintf(reply, sizeof reply, "ERR unknown command\n");
        }
        if (send_all(fd, reply, strlen(reply)) < 0) {
            LOG(LOG_WARN, "send failed: %s", strerror(errno));
            break;
        }
    }
}

static int parse_number(const char *s, long min, long max, const char *what)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v < min || v > max) {
        fprintf(stderr, "invalid %s: %s (expected %ld..%ld)\n", what, s, min, max);
        exit(2);
    }
    return (int)v;
}

int main(int argc, char **argv)
{
    int port = 9000, robust = 1, timeout = 30, max_clients = 64;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-robust") == 0) robust = 0;
        else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
            timeout = parse_number(argv[++i], 1, 86400, "timeout");
        else if (strcmp(argv[i], "--max-clients") == 0 && i + 1 < argc)
            max_clients = parse_number(argv[++i], 1, 10000, "max-clients");
        else port = parse_number(argv[i], 1, 65535, "port");
    }
    if (!robust) { timeout = 0; max_clients = 0; }   /* 0 = disabled */
    LOG(LOG_INFO, "starting on port %d, robust=%d, timeout=%d, max_clients=%d",
        port, robust, timeout, max_clients);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_stop;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    if (robust) {
        signal(SIGPIPE, SIG_IGN);                /* writing to a dead client must not kill us */
        sa.sa_handler = on_sigchld;              /* no zombies */
        sa.sa_flags = SA_RESTART;
        sigaction(SIGCHLD, &sa, NULL);
    }

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    if (robust) {                                /* allow quick restart */
        int on = 1;
        setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("bind"); return 1; }
    if (listen(srv, 64) < 0) { perror("listen"); return 1; }

    sigset_t chld, old;
    sigemptyset(&chld);
    sigaddset(&chld, SIGCHLD);

    while (1) {
        struct sockaddr_in cli;
        socklen_t len = sizeof cli;
        int fd = accept(srv, (struct sockaddr *)&cli, &len);
        if (fd < 0) {
            if (errno != EINTR) perror("accept");
            continue;
        }
        LOG(LOG_INFO, "connection from %s:%d", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        if (max_clients > 0 && nchildren >= max_clients) {
            LOG(LOG_WARN, "refused: %d clients already being served", (int)nchildren);
            send_all(fd, "ERR server busy\n", 16);
            close(fd);
            continue;
        }

        /* SIGCHLD is blocked while we fork and count, so the handler's
         * nchildren-- cannot run in the middle of our nchildren++ */
        sigprocmask(SIG_BLOCK, &chld, &old);
        pid_t pid = fork();
        if (pid == 0) {                          /* child */
            sigprocmask(SIG_SETMASK, &old, NULL);
            signal(SIGINT, SIG_IGN);             /* Ctrl+C stops the parent; children finish */
            signal(SIGTERM, SIG_DFL);
            close(srv);
            handle_client(fd, timeout);
            close(fd);
            LOG(LOG_INFO, "child finished normally");
            _exit(0);
        }
        if (pid > 0) nchildren++;
        else LOG(LOG_ERROR, "fork failed: %s", strerror(errno));
        sigprocmask(SIG_SETMASK, &old, NULL);
        close(fd);                               /* parent */
    }
}
