#ifndef INC_SHA256_H_
#define INC_SHA256_H_

#include <stdint.h>
#include <stddef.h>

#define SHA256_BLOCK_SIZE 32 // 256-bit = 32 bayt

typedef struct {
    uint64_t bitlen;    // 8-byte hizalama için en üstte olmalı
    uint32_t state[8];
    uint32_t datalen;
    uint8_t data[64];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len);
void sha256_final(SHA256_CTX *ctx, uint8_t hash[]);

// Tek seferde veri bloğunun SHA-256 özetini çıkaran yardımcı fonksiyon
void sha256_compute(const uint8_t *data, size_t len, uint8_t hash[32]);

#endif /* INC_SHA256_H_ */
