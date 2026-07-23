#ifndef FB_REPLY_H
#define FB_REPLY_H
/* Tiny HTTP/1.1 response writers, shared between the accept-loop (main.c) and the /bake handler
 * pool (bakepool.c) -- both write directly to a raw socket fd, no keep-alive, no chunking. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

static int fb_send_all(int fd, const void *buf, size_t len) {
    const char *p = buf;
    while (len) {
        ssize_t w = send(fd, p, len, MSG_NOSIGNAL);
        if (w > 0) { p += w; len -= (size_t)w; continue; }
        if (w < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        return -1;
    }
    return 0;
}

static void fb_reply(int fd, const char *status, const char *ctype, const char *body) {
    char hdr[512];
    int n = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        status, ctype, strlen(body));
    fb_send_all(fd, hdr, (size_t)n);
    fb_send_all(fd, body, strlen(body));
}

static void fb_reply_bin(int fd, const char *ctype, const uint8_t *body, size_t n) {
    char hdr[512];
    int h = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nCache-Control: public, max-age=31536000, immutable\r\n"
        "Connection: close\r\n\r\n", ctype, n);
    fb_send_all(fd, hdr, (size_t)h);
    fb_send_all(fd, body, n);
}

#endif
