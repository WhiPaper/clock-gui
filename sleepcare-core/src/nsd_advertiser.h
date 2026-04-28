/**
 * @file nsd_advertiser.h
 * @brief Avahi mDNS/DNS-SD advertisement for _sleepcare._tcp.
 */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScNsd ScNsd;

/**
 * Create and start mDNS advertisement.
 * @param device_id  Value for TXT record device_id field.
 * @param port       WSS port number.
 * Returns NULL on failure.
 */
ScNsd* sc_nsd_start(const char* device_id, int port);

/** Poll Avahi event loop (call from main loop, timeout_ms max wait). */
void sc_nsd_poll(ScNsd* nsd, int timeout_ms);

/** Stop advertising and free resources. */
void sc_nsd_stop(ScNsd* nsd);

#ifdef __cplusplus
}
#endif
