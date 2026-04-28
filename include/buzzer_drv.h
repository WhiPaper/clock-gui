/* SPDX-License-Identifier: GPL-2.0 */
/*
 * buzzer_drv.h – Passive Buzzer Character Device Driver
 * GPIO 13 (BCM) → Hardware PWM1 (RP1 PWM on Raspberry Pi 5)
 *
 * Module: KY-006 Passive Buzzer
 * Oscillation Frequency: 1500 ~ 2500 Hz
 *
 * Include this header in both the kernel module and userspace applications.
 */
#ifndef _BUZZER_DRV_H
#define _BUZZER_DRV_H

#include <linux/ioctl.h>

/* ioctl magic number */
#define BUZZER_IOC_MAGIC    'B'

/*
 * BUZZER_SET_FREQ  – set output frequency (Hz)
 *   arg: unsigned int, range [BUZZER_FREQ_MIN, BUZZER_FREQ_MAX]
 *
 * BUZZER_START     – start PWM output at the current frequency
 *   arg: (none)
 *
 * BUZZER_STOP      – stop PWM output
 *   arg: (none)
 *
 * BUZZER_BEEP      – beep for N milliseconds then auto-stop
 *   arg: unsigned int, duration in ms (1 – 30000)
 */
#define BUZZER_SET_FREQ     _IOW(BUZZER_IOC_MAGIC, 0, unsigned int)
#define BUZZER_START        _IO (BUZZER_IOC_MAGIC, 1)
#define BUZZER_STOP         _IO (BUZZER_IOC_MAGIC, 2)
#define BUZZER_BEEP         _IOW(BUZZER_IOC_MAGIC, 3, unsigned int)

/* Frequency limits – KY-006 oscillation range */
#define BUZZER_FREQ_MIN     1500u   /* Hz – KY-006 lower bound */
#define BUZZER_FREQ_MAX     2500u   /* Hz – KY-006 upper bound */
#define BUZZER_FREQ_DEFAULT 2000u   /* Hz – midpoint of KY-006 range */

#endif /* _BUZZER_DRV_H */
