/**
 * @file session.h
 * @brief Per-WebSocket-connection session context and state machine.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sleepcare_proto.h"
#include "hr_buffer.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SS_IDLE      = 0,
    SS_WARMUP    = 1,
    SS_BASELINE  = 2,
    SS_SUSPECT   = 3,
    SS_ALERTING  = 4,
    SS_COOLDOWN  = 5,
    SS_CLOSING   = 6,
} SessionState;

typedef struct {
    /* WebSocket handshake */
    bool      hello_done;
    bool      session_open;
    bool      watch_available;
    bool      eye_only_mode;
    char      study_mode[32];

    /* Session id */
    char      sid[64];

    /* State machine */
    SessionState state;
    double    suspect_acc_sec;     /* accumulated time in SUSPECT */
    double    alerting_acc_sec;    /* fast-path: time above T2 */
    double    cooldown_acc_sec;    /* time in COOLDOWN */

    /* Statistics */
    int       total_alerts;
    float     peak_fused;
    char      final_reason[64];

    /* HR data */
    HrBuffer  hr_buf;

    /* Last eye frame */
    EyeFrame  last_eye;
    int64_t   last_eye_ms;       /* when we last received a valid eye frame */

    /* Outgoing sequence */
    uint32_t  seq_out;

    /* Ping watchdog */
    int64_t   last_ping_ms;
} SessionCtx;

void session_init(SessionCtx* s);
void session_reset(SessionCtx* s);

/**
 * Tick the state machine.
 * @param s         session context
 * @param fused     fused_score (0.0–1.0)
 * @param hr_weight HR quality weight (0.0 = eye-only)
 * @param dt_sec    elapsed seconds since last tick
 * @return new state
 */
SessionState session_tick(SessionCtx* s, float fused, float hr_weight, double dt_sec);

const char* session_state_name(SessionState st);

#ifdef __cplusplus
}
#endif
