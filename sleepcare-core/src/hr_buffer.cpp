#include "hr_buffer.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>

static const float kWeights[] = {
    [HR_QUALITY_OK]           = 1.0f,
    [HR_QUALITY_MOTION_WEAK]  = 0.2f,
    [HR_QUALITY_DETACHED]     = 0.0f,
    [HR_QUALITY_BUSY_INITIAL] = 0.0f,
};

float hr_quality_weight(HrQuality q) {
    if ((unsigned)q >= 4) return 0.0f;
    return kWeights[q];
}

static HrQuality parse_quality(const char* s) {
    if (!s) return HR_QUALITY_BUSY_INITIAL;
    if (strcmp(s, "ok")              == 0) return HR_QUALITY_OK;
    if (strcmp(s, "motion_or_weak")  == 0) return HR_QUALITY_MOTION_WEAK;
    if (strcmp(s, "detached")        == 0) return HR_QUALITY_DETACHED;
    return HR_QUALITY_BUSY_INITIAL;
}

bool hr_buffer_ingest(HrBuffer* b, const char* body_json) {
    cJSON* root = cJSON_Parse(body_json);
    if (!root) return false;

    cJSON* bpm_j = cJSON_GetObjectItemCaseSensitive(root, "bpm");
    cJSON* qual_j = cJSON_GetObjectItemCaseSensitive(root, "hr_quality");
    cJSON* rx_j  = cJSON_GetObjectItemCaseSensitive(root, "phone_rx_ms");

    if (!cJSON_IsNumber(bpm_j)) { cJSON_Delete(root); return false; }

    HrSample s = {0};
    s.bpm     = (int32_t)bpm_j->valuedouble;
    s.quality = parse_quality(cJSON_IsString(qual_j) ? qual_j->valuestring : NULL);
    s.rx_ms   = cJSON_IsNumber(rx_j) ? (int64_t)rx_j->valuedouble : 0;

    int idx = (b->head + b->count) % HR_BUF_MAX;
    if (b->count < HR_BUF_MAX) {
        b->count++;
    } else {
        b->head = (b->head + 1) % HR_BUF_MAX;
    }
    b->buf[idx] = s;

    cJSON_Delete(root);
    return true;
}

bool hr_buffer_latest(const HrBuffer* b, int64_t now_ms, int64_t max_age_ms,
                      int32_t* bpm_out, float* weight_out) {
    if (b->count == 0) return false;
    int last_idx = (b->head + b->count - 1) % HR_BUF_MAX;
    const HrSample* s = &b->buf[last_idx];
    if (s->rx_ms > 0 && (now_ms - s->rx_ms) > max_age_ms) return false;
    *bpm_out    = s->bpm;
    *weight_out = hr_quality_weight(s->quality);
    return true;
}

void hr_buffer_clear(HrBuffer* b) {
    memset(b, 0, sizeof(*b));
}
