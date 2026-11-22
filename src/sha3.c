/* SHA3-256 + HMAC-SHA3-256 sobre OpenSSL libcrypto (EVP_MAC, sin APIs deprecados).
   Se usa para el challenge/response SR+AUTH del tablero de control de lente Dimenco. */
#include "sha3.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <string.h>

void sha3_256(const void *data, size_t len, uint8_t out[32]) {
  unsigned int olen = 32;
  EVP_Digest(data, len, out, &olen, EVP_sha3_256(), NULL);
}

void hmac_sha3_256(const void *key, size_t klen, const void *msg, size_t mlen, uint8_t out[32]) {
  EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
  EVP_MAC_CTX *ctx = mac ? EVP_MAC_CTX_new(mac) : NULL;
  char digest[] = "SHA3-256";
  OSSL_PARAM params[2] = {OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest, 0),
                          OSSL_PARAM_construct_end()};
  size_t olen = 32;
  if (ctx && EVP_MAC_init(ctx, (const unsigned char *)key, klen, params) &&
      EVP_MAC_update(ctx, (const unsigned char *)msg, mlen) &&
      EVP_MAC_final(ctx, out, &olen, 32)) {
    /* ok */
  } else {
    memset(out, 0, 32); /* nunca debe pasar; el llamador lo trata como fallo de autenticación */
  }
  if (ctx) EVP_MAC_CTX_free(ctx);
  if (mac) EVP_MAC_free(mac);
}
