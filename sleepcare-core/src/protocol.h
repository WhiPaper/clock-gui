/**
 * @file protocol.h
 * @brief JSON Envelope builder / parser for the SleepCare WebSocket protocol.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SC_PROTO_VERSION 1
#define SC_SRC_PI        "pi"
#define SC_SRC_PHONE     "phone"

/* Message type strings */
#define T_HELLO          "hello"
#define T_HELLO_ACK      "hello_ack"
#define T_SESSION_OPEN   "session.open"
#define T_SESSION_ACK    "session.ack"
#define T_SESSION_CLOSE  "session.close"
#define T_SESSION_SUMM   "session.summary"
#define T_HR_INGEST      "hr.ingest"
#define T_RISK_UPDATE    "risk.update"
#define T_ALERT_FIRE     "alert.fire"
#define T_PING           "ping"
#define T_PONG           "pong"

/** Parsed incoming envelope (minimal fields needed by the server). */
typedef struct {
    char     t[64];
    char     sid[64];
    int64_t  seq;
    bool     ack_required;
    /* raw body JSON pointer into the original buffer (do NOT free) */
    const char* body_raw;
} ScEnvelope;

/** Parse a raw JSON string into ScEnvelope. Returns 0 on success. */
int sc_proto_parse(const char* json, ScEnvelope* out);

/**
 * Build an outgoing JSON envelope (allocated string — caller must free).
 * @param t            message type
 * @param sid          session id (may be NULL)
 * @param seq          outgoing sequence number (auto-increment)
 * @param body_json    body JSON string, e.g. "{}" or "{\"key\":1}"
 * @param ack_required whether ACK is expected
 */
char* sc_proto_build(const char* t, const char* sid, uint32_t* seq,
                     const char* body_json, bool ack_required);

/** Returns current Unix epoch in milliseconds. */
int64_t sc_now_ms(void);

#ifdef __cplusplus
}
#endif
