#include "risk_socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>

int sc_risk_open(void) {
    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) perror("[risk_tx] socket");
    return fd;
}

bool sc_risk_send(int fd, const RiskFrame* frame) {
    if (fd < 0) return false;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, SC_SOCK_RISK, SC_SOCK_RISK_LEN);
    socklen_t len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + SC_SOCK_RISK_LEN);
    ssize_t n = sendto(fd, frame, sizeof(*frame), 0, (struct sockaddr*)&addr, len);
    if (n < 0 && errno != ECONNREFUSED && errno != ENOENT) {
        perror("[risk_tx] sendto");
    }
    return n == (ssize_t)sizeof(*frame);
}

void sc_risk_close(int fd) {
    if (fd >= 0) close(fd);
}
