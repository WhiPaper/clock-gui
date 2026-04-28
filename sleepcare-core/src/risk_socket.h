/**
 * @file risk_socket.h
 * @brief Send RiskFrames to clock-gui via @sleepcare/risk.
 */
#pragma once
#include "sleepcare_proto.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Open an unbound SOCK_DGRAM socket. Returns fd >= 0, or -1. */
int sc_risk_open(void);

/** Send a RiskFrame to @sleepcare/risk (drops silently if clock-gui absent). */
bool sc_risk_send(int fd, const RiskFrame* frame);

void sc_risk_close(int fd);

#ifdef __cplusplus
}
#endif
