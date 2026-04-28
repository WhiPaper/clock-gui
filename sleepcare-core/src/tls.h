/**
 * @file tls.h
 * @brief OpenSSL self-signed certificate management.
 */
#pragma once
#include <openssl/ssl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Ensure /etc/sleepcare/pi_cert.pem and pi_key.pem exist.
 * Generates RSA-2048 self-signed certificate if not present.
 * Returns true on success.
 */
bool sc_tls_ensure_cert(const char* cert_path, const char* key_path);

/**
 * Compute DER SubjectPublicKeyInfo SHA-256 → Base64 string.
 * Caller must free() the returned string. Returns NULL on error.
 */
char* sc_tls_spki_sha256(const char* cert_pem_path);

/**
 * Create a server SSL_CTX with the given cert and key.
 * Returns NULL on error.
 */
SSL_CTX* sc_tls_make_ctx(const char* cert_path, const char* key_path);

#ifdef __cplusplus
}
#endif
