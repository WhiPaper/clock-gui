#pragma once
/**
 * @file DrowsinessBridge.hpp
 * @brief Receives RiskFrame from sleepcare-ws via UDS (@sleepcare/risk).
 *
 * Replaces the previous POSIX SHM implementation.
 * Binds to the abstract-namespace socket @sleepcare/risk and exposes
 * the latest received frame via simple accessors.
 *
 * Integration into the LVGL main loop:
 *   1. Call open() once at startup.
 *   2. Pass epoll_fd() to epoll_wait() instead of plain usleep().
 *   3. Call recv_nonblock() when epoll signals EPOLLIN.
 */

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "sleepcare_proto.h"

class RiskBridge {
   public:
    RiskBridge()  = default;
    ~RiskBridge() { close(); }

    RiskBridge(const RiskBridge&)            = delete;
    RiskBridge& operator=(const RiskBridge&) = delete;

    /** Bind socket and create epoll fd. Returns false on failure (non-fatal). */
    bool open() {
        sock_fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (sock_fd_ < 0) {
            std::fprintf(stderr, "[risk] socket: %s\n", std::strerror(errno));
            return false;
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, SC_SOCK_RISK, SC_SOCK_RISK_LEN);
        socklen_t addrlen =
            static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + SC_SOCK_RISK_LEN);

        if (::bind(sock_fd_, reinterpret_cast<struct sockaddr*>(&addr), addrlen) < 0) {
            std::fprintf(stderr, "[risk] bind: %s\n", std::strerror(errno));
            ::close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }

        epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ < 0) {
            std::fprintf(stderr, "[risk] epoll_create1: %s\n", std::strerror(errno));
            ::close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }

        struct epoll_event ev{};
        ev.events  = EPOLLIN;
        ev.data.fd = sock_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_fd_, &ev) < 0) {
            std::fprintf(stderr, "[risk] epoll_ctl: %s\n", std::strerror(errno));
            ::close(epoll_fd_);
            ::close(sock_fd_);
            epoll_fd_ = sock_fd_ = -1;
            return false;
        }

        return true;
    }

    void close() {
        if (epoll_fd_ >= 0) { ::close(epoll_fd_); epoll_fd_ = -1; }
        if (sock_fd_  >= 0) { ::close(sock_fd_);  sock_fd_  = -1; }
        std::memset(&last_, 0, sizeof(last_));
    }

    /**
     * Non-blocking receive. Returns true if a valid new frame was received.
     * Call in the EPOLLIN branch of the main loop.
     */
    bool recv_nonblock() {
        if (sock_fd_ < 0) return false;

        RiskFrame frame{};
        ssize_t n = ::recv(sock_fd_, &frame, sizeof(frame), MSG_DONTWAIT);
        if (n != static_cast<ssize_t>(sizeof(frame))) return false;

        if (frame.magic[0] != 'S' || frame.magic[1] != 'R' ||
            frame.magic[2] != 'S' || frame.magic[3] != 'K') return false;
        if (frame.version != 1u) return false;

        last_ = frame;
        return true;
    }

    /** epoll fd for main-loop integration. Returns -1 if not open. */
    int epoll_fd() const { return epoll_fd_; }

    /* Accessors */
    uint8_t  state()       const { return last_.state; }
    uint8_t  alert_level() const { return last_.alert_level; }
    uint8_t  qr_ready()    const { return last_.qr_ready; }
    float    eye_score()   const { return last_.eye_score; }
    float    fused_score() const { return last_.fused_score; }
    uint16_t hr_bpm()      const { return last_.hr_bpm; }
    bool     is_drowsy()   const { return last_.state == SC_STATE_ALERTING; }

   private:
    int       sock_fd_  = -1;
    int       epoll_fd_ = -1;
    RiskFrame last_     = {};
};