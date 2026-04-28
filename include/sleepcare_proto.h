/**
 * @file sleepcare_proto.h
 * @brief Shared IPC protocol between EARSYS, sleepcare-ws, and clock-gui.
 *
 * Transport: AF_UNIX / SOCK_DGRAM (abstract namespace)
 *
 *   @sleepcare/eye   — EARSYS  → sleepcare-ws  (EyeFrame,  20 bytes)
 *   @sleepcare/risk  — sleepcare-ws → clock-gui  (RiskFrame, 32 bytes)
 *
 * Python (EARSYS) equivalent:
 *   EyeFrame  = struct.Struct("<4sBBHfIQ")   # 20 bytes
 *   RiskFrame = struct.Struct("<4sBBBBffHHIQ") # 32 bytes
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Socket addresses (abstract namespace, first byte = '\0') ── */
#define SC_SOCK_EYE      "\x00sleepcare/eye"
#define SC_SOCK_EYE_LEN  14   /* strlen("sleepcare/eye") + 1 */
#define SC_SOCK_RISK     "\x00sleepcare/risk"
#define SC_SOCK_RISK_LEN 15

/* ── State codes (RiskFrame.state) ── */
#define SC_STATE_IDLE      0u
#define SC_STATE_WARMUP    1u
#define SC_STATE_BASELINE  2u
#define SC_STATE_SUSPECT   3u
#define SC_STATE_ALERTING  4u
#define SC_STATE_COOLDOWN  5u

/* ── Status codes (EyeFrame.status) ── */
#define SC_STATUS_AWAKE   0u
#define SC_STATUS_DROWSY  1u
#define SC_STATUS_NOFACE  2u

/* ── EyeFrame — EARSYS → sleepcare-ws (24 bytes) ──
 *   [0 :4 ] magic[4]    "SEYE"
 *   [4 :5 ] version     1
 *   [5 :6 ] status      SC_STATUS_*
 *   [6 :8 ] reserved    0
 *   [8 :12] eye_score   float32 LE
 *   [12:16] seq         uint32 LE
 *   [16:24] ts_ms       uint64 LE   ← Q = 8 bytes
 * Python: struct.Struct("<4sBBHfIQ").size == 24
 */
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];   /* "SEYE" */
    uint8_t  version;    /* 1 */
    uint8_t  status;     /* SC_STATUS_* */
    uint16_t reserved;
    float    eye_score;  /* 0.0 – 1.0 */
    uint32_t seq;
    uint64_t ts_ms;      /* Unix epoch ms */
} EyeFrame;

/* ── RiskFrame — sleepcare-ws → clock-gui (32 bytes) ── */
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];    /* "SRSK" */
    uint8_t  version;     /* 1 */
    uint8_t  state;       /* SC_STATE_* */
    uint8_t  alert_level; /* 0 = none, 1–3 */
    uint8_t  qr_ready;    /* 0 or 1 */
    float    eye_score;   /* 0.0 – 1.0 */
    float    fused_score; /* 0.0 – 1.0 */
    uint16_t hr_bpm;
    uint16_t reserved;
    uint32_t seq;
    uint64_t ts_ms;
} RiskFrame;

#ifdef __cplusplus
} /* extern "C" */
#endif

/* Compile-time size checks (C11 / C++11) */
#ifdef __cplusplus
static_assert(sizeof(EyeFrame)  == 24, "EyeFrame must be 24 bytes");
static_assert(sizeof(RiskFrame) == 32, "RiskFrame must be 32 bytes");
#endif
