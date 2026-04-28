#include "alert_local.h"
#include "../../include/buzzer_drv.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

struct ScAlert {
    int buzzer_fd;
};

static void buzzer_beep(int fd, unsigned int duration_ms) {
    if (fd >= 0) {
        ioctl(fd, BUZZER_BEEP, &duration_ms);
    }
}

static void buzzer_set_freq(int fd, unsigned int freq_hz) {
    if (fd >= 0) {
        ioctl(fd, BUZZER_SET_FREQ, &freq_hz);
    }
}

ScAlert* sc_alert_open(void) {
    ScAlert* a = (ScAlert*)calloc(1, sizeof(ScAlert));
    
    a->buzzer_fd = open("/dev/buzzer", O_RDWR);
    if (a->buzzer_fd < 0) {
        fprintf(stderr, "[alert] failed to open /dev/buzzer: %s\n", strerror(errno));
    }

    return a;
}

void sc_alert_fire(ScAlert* a, int level) {
    if (!a) { printf("[alert] LOCAL ALERT level=%d\n", level); return; }

    switch (level) {
    case 1:
        // Level 1: 짧고 경쾌한 단음 (2000Hz)
        buzzer_set_freq(a->buzzer_fd, 2000);
        buzzer_beep(a->buzzer_fd, 150);
        usleep(150000);
        break;

    case 2:
        // Level 2: 띠-동- (고음 2200Hz -> 저음 1700Hz) 2회 반복
        for (int i = 0; i < 2; i++) {
            buzzer_set_freq(a->buzzer_fd, 2200);
            buzzer_beep(a->buzzer_fd, 200);
            usleep(200000);
            
            buzzer_set_freq(a->buzzer_fd, 1700);
            buzzer_beep(a->buzzer_fd, 200);
            usleep(200000);
            
            usleep(100000); // 100ms 휴식 간격
        }
        break;

    default: /* level 3 */
        // Level 3: 구급차 사이렌 (고음 2500Hz <-> 저음 1500Hz 교차) 4회 반복
        for (int i = 0; i < 4; i++) {
            buzzer_set_freq(a->buzzer_fd, 2500);
            buzzer_beep(a->buzzer_fd, 300);
            usleep(300000);
            
            buzzer_set_freq(a->buzzer_fd, 1500);
            buzzer_beep(a->buzzer_fd, 300);
            usleep(300000);
        }
    }
}

void sc_alert_stop(ScAlert* a) {
    if (!a) return;
    if (a->buzzer_fd >= 0) ioctl(a->buzzer_fd, BUZZER_STOP);
}

void sc_alert_close(ScAlert* a) {
    if (!a) return;
    sc_alert_stop(a);
    if (a->buzzer_fd >= 0) close(a->buzzer_fd);
    free(a);
}
