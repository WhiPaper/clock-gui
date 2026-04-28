#include "eye_socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>

int sc_eye_open(void) {
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) { perror("[eye] socket"); return -1; }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, SC_SOCK_EYE, SC_SOCK_EYE_LEN);
    socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + SC_SOCK_EYE_LEN);

    if (bind(fd, (struct sockaddr*)&addr, len) < 0) {
        fprintf(stderr, "[eye] bind: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

bool sc_eye_recv(int fd, EyeFrame* out) {
    ssize_t n = recv(fd, out, sizeof(*out), MSG_DONTWAIT);
    if (n != (ssize_t)sizeof(EyeFrame)) return false;
    if (out->magic[0]!='S' || out->magic[1]!='E' ||
        out->magic[2]!='Y' || out->magic[3]!='E') return false;
    if (out->version != 1) return false;
    return true;
}

void sc_eye_close(int fd) {
    if (fd >= 0) close(fd);
}
