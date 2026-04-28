/**
 * @file eye_socket.h
 * @brief Bind @sleepcare/eye and receive EyeFrames from EARSYS.
 */
#pragma once
#include "sleepcare_proto.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Open and bind to @sleepcare/eye. Returns fd >= 0 on success, -1 on error. */
int sc_eye_open(void);

/** Non-blocking receive. Returns true if a valid EyeFrame was received. */
bool sc_eye_recv(int fd, EyeFrame* out);

/** Close the socket. */
void sc_eye_close(int fd);

#ifdef __cplusplus
}
#endif
