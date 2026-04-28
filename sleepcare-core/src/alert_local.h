/**
 * @file alert_local.h
 * @brief Local alert output: GPIO buzzer/LED via libgpiod.
 *        Gracefully degrades if GPIO unavailable.
 */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScAlert ScAlert;

/** Open Buzzer resources. Returns valid pointer even if it fails. */
ScAlert* sc_alert_open(void);

/**
 * Fire local alert pattern.
 * level 1: single short beep
 * level 2: 200ms ON / 100ms OFF × 2
 * level 3: continuous 400ms beep + rapid LED blink
 */
void sc_alert_fire(ScAlert* a, int level);

/** Stop any ongoing alert. */
void sc_alert_stop(ScAlert* a);

void sc_alert_close(ScAlert* a);

#ifdef __cplusplus
}
#endif
