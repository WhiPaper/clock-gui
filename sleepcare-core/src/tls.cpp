#include "tls.h"
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool sc_tls_ensure_cert(const char* cert_path, const char* key_path) {
    if (file_exists(cert_path) && file_exists(key_path)) {
        printf("[tls] Using existing cert: %s\n", cert_path);
        return true;
    }
    printf("[tls] Generating self-signed RSA-2048 certificate...\n");

    EVP_PKEY* pkey = EVP_RSA_gen(2048);
    if (!pkey) { ERR_print_errors_fp(stderr); return false; }

    X509* x509 = X509_new();
    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509),  (long)3650 * 24 * 3600);
    X509_set_pubkey(x509, pkey);

    X509_NAME* name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                               (unsigned char*)"sleepcare-pi", -1, -1, 0);
    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha256());

    /* Write key */
    FILE* kf = fopen(key_path, "wb");
    if (!kf) { perror(key_path); goto fail; }
    PEM_write_PrivateKey(kf, pkey, NULL, NULL, 0, NULL, NULL);
    fclose(kf);
    chmod(key_path, 0600);

    /* Write cert */
    FILE* cf = fopen(cert_path, "wb");
    if (!cf) { perror(cert_path); goto fail; }
    PEM_write_X509(cf, x509);
    fclose(cf);

    X509_free(x509);
    EVP_PKEY_free(pkey);
    printf("[tls] Certificate written to %s\n", cert_path);
    return true;

fail:
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return false;
}

char* sc_tls_spki_sha256(const char* cert_pem_path) {
    FILE* f = fopen(cert_pem_path, "rb");
    if (!f) return NULL;

    X509* cert = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    if (!cert) return NULL;

    /* DER-encode SubjectPublicKeyInfo */
    unsigned char* spki_der = NULL;
    int spki_len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &spki_der);
    X509_free(cert);
    if (spki_len <= 0) return NULL;

    /* SHA-256 */
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(spki_der, (size_t)spki_len, digest);
    OPENSSL_free(spki_der);

    /* Base64 */
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, digest, SHA256_DIGEST_LENGTH);
    BIO_flush(b64);

    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    char* out = (char*)malloc(bptr->length + 1);
    memcpy(out, bptr->data, bptr->length);
    out[bptr->length] = '\0';
    BIO_free_all(b64);
    return out;
}

SSL_CTX* sc_tls_make_ctx(const char* cert_path, const char* key_path) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return NULL;
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
    return ctx;
}
