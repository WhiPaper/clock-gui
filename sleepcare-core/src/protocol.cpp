#include "protocol.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int64_t sc_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int sc_proto_parse(const char* json, ScEnvelope* out) {
    memset(out, 0, sizeof(*out));
    cJSON* root = cJSON_Parse(json);
    if (!root) return -1;

    cJSON* t = cJSON_GetObjectItemCaseSensitive(root, "t");
    if (cJSON_IsString(t) && t->valuestring)
        snprintf(out->t, sizeof(out->t), "%s", t->valuestring);

    cJSON* sid = cJSON_GetObjectItemCaseSensitive(root, "sid");
    if (cJSON_IsString(sid) && sid->valuestring)
        snprintf(out->sid, sizeof(out->sid), "%s", sid->valuestring);

    cJSON* seq = cJSON_GetObjectItemCaseSensitive(root, "seq");
    if (cJSON_IsNumber(seq))
        out->seq = (int64_t)seq->valuedouble;

    cJSON* ack = cJSON_GetObjectItemCaseSensitive(root, "ack_required");
    out->ack_required = cJSON_IsTrue(ack);

    /* We do NOT keep body_raw (root will be freed) — callers re-parse body */
    out->body_raw = NULL;
    cJSON_Delete(root);
    return (out->t[0] != '\0') ? 0 : -1;
}

char* sc_proto_build(const char* t, const char* sid, uint32_t* seq,
                     const char* body_json, bool ack_required) {
    uint32_t s = seq ? (*seq)++ : 0;
    int64_t  now = sc_now_ms();

    /* body_json must be a valid JSON object string */
    cJSON* body = cJSON_Parse(body_json ? body_json : "{}");
    if (!body) body = cJSON_CreateObject();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "v",    SC_PROTO_VERSION);
    cJSON_AddStringToObject(root, "t",    t);
    if (sid && sid[0])
        cJSON_AddStringToObject(root, "sid", sid);
    else
        cJSON_AddNullToObject(root, "sid");
    cJSON_AddNumberToObject(root, "seq",  (double)s);
    cJSON_AddStringToObject(root, "src",  SC_SRC_PI);
    cJSON_AddNumberToObject(root, "sent_at_ms", (double)now);
    cJSON_AddBoolToObject(root, "ack_required", ack_required);
    cJSON_AddItemToObject(root, "body", body);

    char* out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;  /* caller must free() */
}
