#include "risk_socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

static time_t g_drop_log_ts = 0;
static unsigned int g_drop_suppressed = 0;
static bool g_was_dropping = false;

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
    if (n == (ssize_t)sizeof(*frame)) {
        if (g_was_dropping) {
            fprintf(stderr, "[risk_tx] delivery recovered\n");
            g_was_dropping = false;
            g_drop_suppressed = 0;
        }
        return true;
    }

    if (n < 0 && (errno == ECONNREFUSED || errno == ENOENT)) {
        time_t now = time(NULL);
        g_was_dropping = true;
        if (g_drop_log_ts == 0 || (now - g_drop_log_ts) >= 5) {
            fprintf(stderr,
                    "[risk_tx] receiver unavailable (errno=%d), dropped=%u\n",
                    errno, g_drop_suppressed + 1);
            g_drop_log_ts = now;
            g_drop_suppressed = 0;
        } else {
            g_drop_suppressed++;
        }
        return false;
    }

    if (n < 0) {
        perror("[risk_tx] sendto");
    }
    return false;
}

void sc_risk_close(int fd) {
    if (fd >= 0) close(fd);
}
