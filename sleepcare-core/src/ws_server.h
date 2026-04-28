/**
 * @file ws_server.h
 * @brief libwebsockets-based WSS server for the SleepCare protocol.
 */
#pragma once
#include <stdbool.h>
#include "session.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScWsServer ScWsServer;

/**
 * Create and start the WSS server.
 * @param port         TCP port (e.g. 8443)
 * @param cert_path    TLS certificate path
 * @param key_path     TLS private key path
 * @param eye_fd       Bound @sleepcare/eye socket fd
 * @param risk_fd      Unbound @sleepcare/risk send socket fd
 * Returns NULL on failure.
 */
ScWsServer* sc_ws_create(int port, const char* cert_path, const char* key_path,
                         int eye_fd, int risk_fd);

/**
 * Service one event-loop iteration.
 * @param timeout_ms  Maximum wait (pass 50 for ~50ms latency).
 */
void sc_ws_service(ScWsServer* srv, int timeout_ms);

/** Destroy the server and free all resources. */
void sc_ws_destroy(ScWsServer* srv);

#ifdef __cplusplus
}
#endif
