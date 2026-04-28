/**
 * @file qr_writer.h
 * @brief Generate QR code and write to /run/sleepcare/qr.bin.
 */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Build the pairing QR JSON payload and write qr.bin.
 * @param spki_sha256  Base64-encoded SPKI SHA-256 (from sc_tls_spki_sha256)
 * @param port         WSS port number
 * @param out_path     destination file path
 * Returns true on success.
 */
bool sc_qr_write(const char* spki_sha256, int port, const char* out_path);

#ifdef __cplusplus
}
#endif
