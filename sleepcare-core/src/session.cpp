#include "session.h"
#include <string.h>
#include <math.h>

void session_init(SessionCtx* s) {
    memset(s, 0, sizeof(*s));
    s->state = SS_IDLE;
}

void session_reset(SessionCtx* s) {
    char sid_bak[64];
    memcpy(sid_bak, s->sid, sizeof(sid_bak));
    memset(s, 0, sizeof(*s));
    memcpy(s->sid, sid_bak, sizeof(s->sid));
    s->state = SS_BASELINE;
    hr_buffer_clear(&s->hr_buf);
}

SessionState session_tick(SessionCtx* s, float fused, float hr_weight, double dt_sec) {
    switch (s->state) {
    case SS_IDLE:
    case SS_WARMUP:
    case SS_CLOSING:
        break;  /* transitions handled externally */

    case SS_BASELINE:
        if (fused >= SC_T2_ALERTING) {
            s->alerting_acc_sec += dt_sec;
            s->suspect_acc_sec  += dt_sec;
        } else if (fused >= SC_T1_SUSPECT) {
            s->suspect_acc_sec  += dt_sec;
            s->alerting_acc_sec  = 0.0;
        } else {
            s->suspect_acc_sec  = 0.0;
            s->alerting_acc_sec = 0.0;
        }
        if (s->suspect_acc_sec >= SC_SUSPECT_HOLD_SEC)
            s->state = SS_SUSPECT;
        break;

    case SS_SUSPECT:
        if (fused >= SC_T2_ALERTING) {
            s->alerting_acc_sec += dt_sec;
            if (s->alerting_acc_sec >= SC_ALERT_FAST_HOLD_SEC)
                s->state = SS_ALERTING;
        } else if (fused >= SC_T1_SUSPECT) {
            s->alerting_acc_sec = 0.0;
            s->suspect_acc_sec += dt_sec;
            /* slow path: eye >= T1 for 5s with HR support */
            if (s->suspect_acc_sec >= SC_ALERT_SLOW_HOLD_SEC &&
                hr_weight >= SC_H1_HR_SUPP)
                s->state = SS_ALERTING;
        } else {
            /* recovery */
            s->state            = SS_BASELINE;
            s->suspect_acc_sec  = 0.0;
            s->alerting_acc_sec = 0.0;
        }
        break;

    case SS_ALERTING:
        /* transition to COOLDOWN is triggered externally after alert.fire */
        break;

    case SS_COOLDOWN:
        if (fused < SC_T1_SUSPECT) {
            s->cooldown_acc_sec += dt_sec;
            if (s->cooldown_acc_sec >= SC_COOLDOWN_SEC) {
                s->state = SS_BASELINE;
                s->suspect_acc_sec  = 0.0;
                s->alerting_acc_sec = 0.0;
                s->cooldown_acc_sec = 0.0;
            }
        } else {
            /* re-escalate */
            s->cooldown_acc_sec = 0.0;
            if (fused >= SC_T2_ALERTING)
                s->state = SS_ALERTING;
        }
        break;
    }

    /* track peak */
    if (fused > s->peak_fused) s->peak_fused = fused;

    return s->state;
}

const char* session_state_name(SessionState st) {
    switch (st) {
    case SS_IDLE:      return "IDLE";
    case SS_WARMUP:    return "CAMERA_WARMUP";
    case SS_BASELINE:  return "BASELINE";
    case SS_SUSPECT:   return "SUSPECT";
    case SS_ALERTING:  return "ALERTING";
    case SS_COOLDOWN:  return "COOLDOWN";
    case SS_CLOSING:   return "CLOSING";
    default:           return "UNKNOWN";
    }
}
