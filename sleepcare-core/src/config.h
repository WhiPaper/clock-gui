/**
 * @file config.h
 * @brief Compile-time configuration for sleepcare-ws.
 *        Runtime overrides via environment variables where noted.
 */
#pragma once

/* ── Identity ── */
#define SC_DEVICE_ID    "deskpi-a1"
#define SC_DISPLAY_NAME "SleepCare Pi Desk"
#define SC_KEY_ID       "deskpi-a1-2026-04"

/* ── Network ── */
#define SC_PORT     8443
#define SC_WS_PATH  "/ws"

/* ── TLS certificates ── */
#define SC_CERT_PATH "/etc/sleepcare/pi_cert.pem"
#define SC_KEY_PATH  "/etc/sleepcare/pi_key.pem"

/* ── mDNS ── */
#define SC_SERVICE_TYPE "_sleepcare._tcp"

/* ── IPC / files ── */
#define SC_QR_BIN_PATH  "/run/sleepcare/qr.bin"
#define SC_QR_DIR       "/run/sleepcare"

/* ── Session timing (ms / sec) ── */
#define SC_HELLO_TIMEOUT_MS   6000
#define SC_SESSION_TIMEOUT_MS 6000
#define SC_SUMMARY_TIMEOUT_MS 8000
#define SC_PING_TIMEOUT_SEC   60
#define SC_RISK_INTERVAL_MS   1000   /* risk.update send period */

/* ── Risk thresholds ── */
#define SC_T1_SUSPECT   0.60f   /* BASELINE → SUSPECT */
#define SC_T2_ALERTING  0.80f   /* SUSPECT  → ALERTING (fast) */
#define SC_H1_HR_SUPP   0.50f   /* HR support weight threshold */
#define SC_COOLDOWN_SEC 20.0    /* ALERTING → COOLDOWN → BASELINE */

/* ── State machine timings (seconds) ── */
#define SC_SUSPECT_HOLD_SEC      3.0  /* eye>=T1 持続 → SUSPECT */
#define SC_ALERT_FAST_HOLD_SEC   2.0  /* eye>=T2 持続 → ALERTING */
#define SC_ALERT_SLOW_HOLD_SEC   5.0  /* eye>=T1 + hr_support → ALERTING */
