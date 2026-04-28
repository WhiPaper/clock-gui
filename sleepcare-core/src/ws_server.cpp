#include "ws_server.h"
#include "protocol.h"
#include "session.h"
#include "hr_buffer.h"
#include "eye_socket.h"
#include "risk_socket.h"
#include "alert_local.h"
#include "config.h"
#include "sleepcare_proto.h"

#include <libwebsockets.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Message queue ──────────────────────────────────────────────────── */
#define MSG_QUEUE_MAX  16
#define MSG_MAX_LEN   2048

typedef struct {
    char   data[MSG_QUEUE_MAX][MSG_MAX_LEN];
    int    head, tail, count;
} MsgQueue;

static void mq_push(MsgQueue* q, const char* msg) {
    if (q->count >= MSG_QUEUE_MAX) {
        fprintf(stderr, "[core] message queue full, dropping\n");
        return;
    }
    snprintf(q->data[q->tail], MSG_MAX_LEN, "%s", msg);
    q->tail  = (q->tail + 1) % MSG_QUEUE_MAX;
    q->count++;
}

static bool mq_pop(MsgQueue* q, char* out, size_t out_len) {
    if (q->count == 0) return false;
    snprintf(out, out_len, "%s", q->data[q->head]);
    q->head  = (q->head + 1) % MSG_QUEUE_MAX;
    q->count--;
    return true;
}

/* ── Per-connection context ─────────────────────────────────────────── */
typedef struct {
    SessionCtx session;
    MsgQueue   send_queue;
    int64_t    last_ping_ms;
} ConnCtx;

/* ── Server context ─────────────────────────────────────────────────── */
struct ScWsServer {
    struct lws_context* lws_ctx;
    int                 eye_fd;
    int                 risk_fd;
    ScAlert*            alert;
    uint32_t            risk_seq;
    /* pointer to active connection (NULL if none) */
    struct lws*         active_wsi;
    ConnCtx*            active_conn;
};

static ScWsServer* g_srv = NULL;  /* single global for LWS callback */

/* ── Helpers ────────────────────────────────────────────────────────── */
static void send_msg(struct lws* wsi, ConnCtx* conn, const char* msg) {
    mq_push(&conn->send_queue, msg);
    lws_callback_on_writable(wsi);
}

static void send_envelope(struct lws* wsi, ConnCtx* conn,
                           const char* t, const char* body, bool ack) {
    char* s = sc_proto_build(t, conn->session.sid,
                              &conn->session.seq_out, body, ack);
    if (s) { send_msg(wsi, conn, s); free(s); }
}

static void handle_hello(struct lws* wsi, ConnCtx* conn, const char* body_raw) {
    /* Parse watch_available */
    cJSON* root = cJSON_Parse(body_raw ? body_raw : "{}");
    if (root) {
        cJSON* wa = cJSON_GetObjectItemCaseSensitive(root, "watch_available");
        conn->session.watch_available = cJSON_IsTrue(wa);
        cJSON* eo = cJSON_GetObjectItemCaseSensitive(root, "supports_eye_only");
        conn->session.eye_only_mode = cJSON_IsTrue(eo);
        cJSON_Delete(root);
    }
    conn->session.hello_done = true;

    const char* mode = conn->session.watch_available ? "eye+hr" : "eye-only";
    char body[256];
    snprintf(body, sizeof(body),
             "{\"device_id\":\"%s\",\"mode\":\"%s\",\"proto\":\"v1\"}",
             SC_DEVICE_ID, mode);
    send_envelope(wsi, conn, T_HELLO_ACK, body, false);
}

static void handle_session_open(struct lws* wsi, ConnCtx* conn, ScEnvelope* env,
                                  const char* body_raw) {
    snprintf(conn->session.sid, sizeof(conn->session.sid), "%s", env->sid);
    conn->session.session_open = true;
    session_reset(&conn->session);
    conn->session.state = SS_BASELINE;  /* EARSYS runs camera independently */
    g_srv->active_wsi  = wsi;
    g_srv->active_conn = conn;

    send_envelope(wsi, conn, T_SESSION_ACK, "{}", false);
    /* Start 1-second risk timer */
    lws_set_timer_usecs(wsi, SC_RISK_INTERVAL_MS * 1000);
    printf("[core] Session %s opened\n", conn->session.sid);
}

static void handle_session_close(struct lws* wsi, ConnCtx* conn, const char* body_raw) {
    conn->session.state = SS_CLOSING;

    /* Parse reason */
    const char* reason = "user_stop";
    cJSON* root = cJSON_Parse(body_raw ? body_raw : "{}");
    if (root) {
        cJSON* r = cJSON_GetObjectItemCaseSensitive(root, "reason");
        if (cJSON_IsString(r) && r->valuestring)
            reason = r->valuestring;
    }
    snprintf(conn->session.final_reason, sizeof(conn->session.final_reason),
             "%s", reason);

    char body[512];
    const char* state_name = session_state_name(conn->session.state);
    snprintf(body, sizeof(body),
             "{\"final_state\":\"%s\","
             "\"total_alerts\":%d,"
             "\"peak_fused_score\":%.3f,"
             "\"mode\":\"%s\","
             "\"summary_reason\":\"%s\"}",
             state_name, conn->session.total_alerts,
             conn->session.peak_fused,
             conn->session.watch_available ? "eye+hr" : "eye-only",
             reason);

    if (root) cJSON_Delete(root);
    send_envelope(wsi, conn, T_SESSION_SUMM, body, false);
    g_srv->active_wsi  = NULL;
    g_srv->active_conn = NULL;
    printf("[core] Session %s closed (%s)\n", conn->session.sid, reason);
}

static void do_risk_tick(struct lws* wsi, ConnCtx* conn) {
    if (!conn->session.session_open) return;

    /* Read eye frame(s) */
    EyeFrame eye = {0};
    bool got_eye = false;
    while (sc_eye_recv(g_srv->eye_fd, &eye)) got_eye = true;
    if (got_eye) {
        conn->session.last_eye    = eye;
        conn->session.last_eye_ms = (int64_t)time(NULL) * 1000;
    }

    float eye_score = conn->session.last_eye.eye_score;

    /* HR */
    int64_t now_ms = (int64_t)time(NULL) * 1000;
    int32_t bpm    = 0;
    float   hr_w   = 0.0f;
    hr_buffer_latest(&conn->session.hr_buf, now_ms, 10000, &bpm, &hr_w);

    /* hr_score: rough mapping — lower bpm = higher score */
    float hr_score = 0.0f;
    if (bpm > 0 && hr_w > 0.0f) {
        hr_score = (bpm < 55) ? 0.9f :
                   (bpm < 65) ? 0.6f :
                   (bpm < 75) ? 0.3f : 0.1f;
    }

    float fused = (hr_w > 0.0f)
                  ? (0.7f * eye_score + 0.3f * (hr_score * hr_w))
                  : eye_score;

    SessionState prev  = conn->session.state;
    SessionState state = session_tick(&conn->session, fused, hr_w, 1.0 /* 1s */);

    /* Build risk.update */
    const char* state_str = session_state_name(state);
    int flush_sec = (state == SS_SUSPECT) ? 5 :
                    (state == SS_ALERTING) ? 2 : 0;

    char body[512];
    snprintf(body, sizeof(body),
             "{\"mode\":\"%s\","
             "\"eye_score\":%.3f,"
             "\"hr_score\":%.3f,"
             "\"fused_score\":%.3f,"
             "\"state\":\"%s\","
             "\"recommended_flush_sec\":%d}",
             conn->session.watch_available ? "eye+hr" : "eye-only",
             eye_score, hr_score, fused, state_str, flush_sec);

    send_envelope(wsi, conn, T_RISK_UPDATE, body, false);

    /* Fire risk frame to clock-gui */
    RiskFrame rf = {0};
    rf.magic[0]='S'; rf.magic[1]='R'; rf.magic[2]='S'; rf.magic[3]='K';
    rf.version     = 1;
    rf.state       = (uint8_t)state;
    rf.eye_score   = eye_score;
    rf.fused_score = fused;
    rf.hr_bpm      = (uint16_t)bpm;
    rf.seq         = g_srv->risk_seq++;
    rf.ts_ms       = (uint64_t)now_ms;
    sc_risk_send(g_srv->risk_fd, &rf);

    /* ALERTING transition — fire alert */
    if (state == SS_ALERTING && prev != SS_ALERTING) {
        conn->session.total_alerts++;
        int level = (fused >= 0.90f) ? 3 : (fused >= 0.75f) ? 2 : 1;
        rf.alert_level = (uint8_t)level;
        sc_risk_send(g_srv->risk_fd, &rf);

        char abody[256];
        snprintf(abody, sizeof(abody),
                 "{\"level\":%d,\"reason\":\"eye_closed_persistent\","
                 "\"duration_ms\":5000}", level);
        send_envelope(wsi, conn, T_ALERT_FIRE, abody, true);
        sc_alert_fire(g_srv->alert, level);

        conn->session.state = SS_COOLDOWN;
        conn->session.cooldown_acc_sec = 0.0;
    }
}

/* ── LWS callback ───────────────────────────────────────────────────── */
static int sc_ws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                           void* user, void* in, size_t len) {
    ConnCtx* conn = (ConnCtx*)user;

    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        session_init(&conn->session);
        memset(&conn->send_queue, 0, sizeof(conn->send_queue));
        conn->last_ping_ms = (int64_t)time(NULL) * 1000;
        printf("[core] Client connected\n");
        break;

    case LWS_CALLBACK_RECEIVE: {
        char buf[4096];
        size_t cplen = (len < sizeof(buf)-1) ? len : sizeof(buf)-1;
        memcpy(buf, in, cplen);
        buf[cplen] = '\0';

        ScEnvelope env = {0};
        if (sc_proto_parse(buf, &env) != 0) break;

        /* Re-parse body from original buffer */
        cJSON* root = cJSON_Parse(buf);
        const char* body_raw = "{}";
        char body_buf[2048] = "{}";
        if (root) {
            cJSON* body = cJSON_GetObjectItemCaseSensitive(root, "body");
            char* bs = body ? cJSON_PrintUnformatted(body) : NULL;
            if (bs) { snprintf(body_buf, sizeof(body_buf), "%s", bs); free(bs); }
            cJSON_Delete(root);
            body_raw = body_buf;
        }

        if      (strcmp(env.t, T_HELLO)         == 0) handle_hello(wsi, conn, body_raw);
        else if (strcmp(env.t, T_SESSION_OPEN)  == 0) handle_session_open(wsi, conn, &env, body_raw);
        else if (strcmp(env.t, T_HR_INGEST)     == 0) hr_buffer_ingest(&conn->session.hr_buf, body_raw);
        else if (strcmp(env.t, T_SESSION_CLOSE) == 0) handle_session_close(wsi, conn, body_raw);
        else if (strcmp(env.t, T_PING)          == 0) {
            send_envelope(wsi, conn, T_PONG, "{}", false);
            conn->last_ping_ms = (int64_t)time(NULL) * 1000;
        }
        break;
    }

    case LWS_CALLBACK_SERVER_WRITEABLE: {
        char msg[MSG_MAX_LEN];
        if (!mq_pop(&conn->send_queue, msg, sizeof(msg))) break;

        size_t msg_len = strlen(msg);
        unsigned char* out = (unsigned char*)malloc(LWS_PRE + msg_len);
        if (!out) break;
        memcpy(out + LWS_PRE, msg, msg_len);
        lws_write(wsi, out + LWS_PRE, msg_len, LWS_WRITE_TEXT);
        free(out);

        if (conn->send_queue.count > 0)
            lws_callback_on_writable(wsi);
        break;
    }

    case LWS_CALLBACK_TIMER:
        do_risk_tick(wsi, conn);
        lws_set_timer_usecs(wsi, SC_RISK_INTERVAL_MS * 1000);
        break;

    case LWS_CALLBACK_CLOSED:
        printf("[core] Client disconnected\n");
        if (g_srv->active_wsi == wsi) {
            g_srv->active_wsi  = NULL;
            g_srv->active_conn = NULL;
        }
        break;

    default:
        break;
    }
    return 0;
}

/* ── Public API ─────────────────────────────────────────────────────── */
static struct lws_protocols kProtocols[] = {
    { "sleepcare-v1", sc_ws_callback, sizeof(ConnCtx), 4096, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

static const struct lws_http_mount kMount = {
    .mount_next       = NULL,
    .mountpoint       = SC_WS_PATH,
    .origin           = NULL,
    .def              = NULL,
    .protocol         = "sleepcare-v1",
    .cgienv           = NULL,
    .extra_mimetypes  = NULL,
    .interpret        = NULL,
    .cgi_timeout      = 0,
    .cache_max_age    = 0,
    .auth_mask        = 0,
    .cache_intermediaries = 0,
    .cache_no         = 0,
    .origin_protocol  = LWSMPRO_CALLBACK,
    .mountpoint_len   = sizeof(SC_WS_PATH) - 1,
    .basic_auth_login_file = NULL,
};

ScWsServer* sc_ws_create(int port, const char* cert_path, const char* key_path,
                          int eye_fd, int risk_fd) {
    ScWsServer* srv = (ScWsServer*)calloc(1, sizeof(ScWsServer));
    srv->eye_fd  = eye_fd;
    srv->risk_fd = risk_fd;
    srv->alert   = sc_alert_open();
    g_srv        = srv;

    struct lws_context_creation_info info = {0};
    info.port      = port;
    info.protocols = kProtocols;
    info.mounts    = &kMount;
    info.ssl_cert_filepath        = cert_path;
    info.ssl_private_key_filepath = key_path;
    info.options   = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.gid       = -1;
    info.uid       = -1;

    srv->lws_ctx = lws_create_context(&info);
    if (!srv->lws_ctx) {
        fprintf(stderr, "[core] lws_create_context failed\n");
        free(srv);
        g_srv = NULL;
        return NULL;
    }
    printf("[core] WSS server started on port %d\n", port);
    return srv;
}

void sc_ws_service(ScWsServer* srv, int timeout_ms) {
    lws_service(srv->lws_ctx, timeout_ms);
}

void sc_ws_destroy(ScWsServer* srv) {
    if (!srv) return;
    lws_context_destroy(srv->lws_ctx);
    sc_alert_close(srv->alert);
    g_srv = NULL;
    free(srv);
}
