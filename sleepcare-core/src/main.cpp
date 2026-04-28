/**
 * @file main.cpp
 * @brief sleepcare-ws entry point.
 *
 * Startup sequence:
 *   1. Ensure /etc/sleepcare/ TLS certificate
 *   2. Compute SPKI SHA-256 → write /run/sleepcare/qr.bin → set qr_ready RiskFrame
 *   3. Bind @sleepcare/eye UDS socket
 *   4. Open @sleepcare/risk send socket
 *   5. Start Avahi mDNS advertisement
 *   6. Create libwebsockets WSS server
 *   7. Main service loop
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>

#include "config.h"
#include "tls.h"
#include "nsd_advertiser.h"
#include "eye_socket.h"
#include "risk_socket.h"
#include "ws_server.h"
#include "qr_writer.h"
#include "risk_socket.h"
#include "sleepcare_proto.h"
#include "protocol.h"

static volatile int g_running = 1;

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void send_qr_ready_frame(int risk_fd, uint8_t ready) {
    RiskFrame rf = {0};
    rf.magic[0]='S'; rf.magic[1]='R'; rf.magic[2]='S'; rf.magic[3]='K';
    rf.version   = 1;
    rf.state     = SC_STATE_IDLE;
    rf.qr_ready  = ready;
    rf.ts_ms     = (uint64_t)sc_now_ms();
    sc_risk_send(risk_fd, &rf);
}

int main(void) {
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    /* --- 1. TLS certificate ----------------------------------------- */
    mkdir("/etc/sleepcare", 0700);
    if (!sc_tls_ensure_cert(SC_CERT_PATH, SC_KEY_PATH)) {
        fprintf(stderr, "[core] TLS certificate setup failed (%s, %s)\n",
                SC_CERT_PATH, SC_KEY_PATH);
        return 1;
    }

    /* --- 2. QR code -------------------------------------------------- */
    char* spki = sc_tls_spki_sha256(SC_CERT_PATH);
    if (!spki) {
        fprintf(stderr, "[core] SPKI computation failed\n");
        return 1;
    }
    printf("[core] SPKI SHA-256: %s\n", spki);

    mkdir(SC_QR_DIR, 0755);
    if (!sc_qr_write(spki, SC_PORT, SC_QR_BIN_PATH)) {
        fprintf(stderr, "[core] QR write failed (continuing anyway)\n");
    }
    free(spki);

    /* --- 3. UDS sockets --------------------------------------------- */
    int eye_fd  = sc_eye_open();
    if (eye_fd < 0) {
        fprintf(stderr, "[core] Cannot bind @sleepcare/eye\n");
        return 1;
    }
    int risk_fd = sc_risk_open();
    if (risk_fd < 0) {
        fprintf(stderr, "[core] Cannot open risk send socket\n");
        sc_eye_close(eye_fd);
        return 1;
    }

    /* Notify clock-gui that QR is ready */
    send_qr_ready_frame(risk_fd, 1);

    /* --- 4. mDNS advertisement -------------------------------------- */
    ScNsd* nsd = sc_nsd_start(SC_DEVICE_ID, SC_PORT);
    if (!nsd) {
        fprintf(stderr, "[core] WARNING: mDNS advertisement unavailable on port %d\n", SC_PORT);
    }

    /* --- 5. WebSocket server ---------------------------------------- */
    ScWsServer* srv = sc_ws_create(SC_PORT, SC_CERT_PATH, SC_KEY_PATH,
                                   eye_fd, risk_fd);
    if (!srv) {
        fprintf(stderr, "[core] WebSocket server creation failed\n");
        sc_nsd_stop(nsd);
        sc_eye_close(eye_fd);
        sc_risk_close(risk_fd);
        return 1;
    }

    printf("[core] Starting service loop...\n");

    /* --- 6. Main service loop --------------------------------------- */
    while (g_running) {
        sc_ws_service(srv, 50);   /* ≤50ms polling */
        sc_nsd_poll(nsd, 0);      /* non-blocking Avahi poll */
    }

    /* --- Cleanup ---------------------------------------------------- */
    printf("[core] Shutting down...\n");
    send_qr_ready_frame(risk_fd, 0);  /* tell clock-gui to hide QR */
    sc_ws_destroy(srv);
    sc_nsd_stop(nsd);
    sc_eye_close(eye_fd);
    sc_risk_close(risk_fd);
    return 0;
}
