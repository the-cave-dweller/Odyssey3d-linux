/* SHA3-256 + HMAC-SHA3-256 (vía OpenSSL libcrypto).
   Se usa para el challenge/response SR+AUTH del tablero de lente Dimenco. */
#ifndef ODYSSEY_SHA3_H
#define ODYSSEY_SHA3_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* out: 32 bytes */
void sha3_256(const void *data, size_t len, uint8_t out[32]);

/* Clave arbitraria (se hashea internamente si es >136 bytes). out: 32 bytes */
void hmac_sha3_256(const void *key, size_t klen, const void *msg, size_t mlen, uint8_t out[32]);

#ifdef __cplusplus
}
#endif
#endif
