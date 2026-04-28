/**
 * @file hr_buffer.h
 * @brief Receive and buffer hr.ingest samples from the Android app.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HR_BUF_MAX 30

typedef enum {
    HR_QUALITY_OK           = 0,
    HR_QUALITY_MOTION_WEAK  = 1,
    HR_QUALITY_DETACHED     = 2,
    HR_QUALITY_BUSY_INITIAL = 3,
} HrQuality;

typedef struct {
    int32_t   bpm;
    HrQuality quality;
    int64_t   rx_ms;
} HrSample;

typedef struct {
    HrSample  buf[HR_BUF_MAX];
    int       head;
    int       count;
} HrBuffer;

/** Parse hr.ingest body JSON and push to buffer. Returns false on parse error. */
bool hr_buffer_ingest(HrBuffer* b, const char* body_json);

/** Return weight for HR quality. */
float hr_quality_weight(HrQuality q);

/**
 * Latest valid sample (not older than max_age_ms).
 * Returns false if buffer empty or sample too old.
 */
bool hr_buffer_latest(const HrBuffer* b, int64_t now_ms, int64_t max_age_ms,
                      int32_t* bpm_out, float* weight_out);

void hr_buffer_clear(HrBuffer* b);

#ifdef __cplusplus
}
#endif
