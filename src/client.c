/* usage: ./build/client <host> <port> <command...>   e.g. ./build/client 127.0.0.1 9000 INFO */
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s host port command...\n", argv[0]); return 1; }
    signal(SIGPIPE, SIG_IGN);                 /* a closed server gives an error, not a kill */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }
    struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };   /* do not wait forever for a reply */
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &a.sin_addr) != 1) { fprintf(stderr, "bad address\n"); return 1; }
    if (connect(fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("connect"); return 1; }

    char req[256] = "";
    for (int i = 3; i < argc; i++) {
        strncat(req, argv[i], sizeof req - strlen(req) - 2);
        if (i + 1 < argc) strncat(req, " ", sizeof req - strlen(req) - 1);
    }
    strcat(req, "\n");
    LOG(LOG_DEBUG, "sending: %s", req);
    send_all(fd, req, strlen(req));

    char resp[512];
    ssize_t n = recv_line(fd, resp, sizeof resp);
    if (n > 0) printf("%s\n", resp);
    else LOG(LOG_ERROR, "no response");
    send_all(fd, "QUIT\n", 5);
    recv_line(fd, resp, sizeof resp);   /* wait for "OK bye": server closes first */
    close(fd);
    return n > 0 ? 0 : 1;
}
