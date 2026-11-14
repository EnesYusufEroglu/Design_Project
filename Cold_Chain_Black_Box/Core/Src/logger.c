#include "logger.h"
#include <string.h>
#include <stdio.h>

static LogRecord_t log_buffer[MAX_LOG_RECORDS];
static uint32_t log_count = 0;
static uint8_t current_chain_hash[32];

// Genesis (Başlangıç) Tohumu
static const char *GENESIS_SEED = "COLDCHAIN_BLACKBOX_STM32F401_GENESIS_2026";

void Logger_Init(void) {
    log_count = 0;
    memset(log_buffer, 0, sizeof(log_buffer));
    // Zincirin ilk halkası için başlangıç hash'ini üret
    sha256_compute((const uint8_t*)GENESIS_SEED, strlen(GENESIS_SEED), current_chain_hash);
}

uint8_t Logger_AddRecord(DS3231_DateTime *dt, float temp, float hum, uint32_t shocks) {
    if (log_count >= MAX_LOG_RECORDS) {
        return 0; // Bellek dolu
    }

    LogRecord_t *rec = &log_buffer[log_count];
    rec->index       = log_count;
    rec->year        = (uint8_t)(dt->year - 2000);
    rec->month       = dt->month;
    rec->day         = dt->day;
    rec->hours       = dt->hours;
    rec->minutes     = dt->minutes;
    rec->seconds     = dt->seconds;
    rec->temp_x100   = (int16_t)(temp * 100.0f);
    rec->hum_x100    = (uint16_t)(hum * 100.0f);
    rec->shock_count = (shocks > 65535) ? 65535 : (uint16_t)shocks;

    // SHA-256 Zincirleme: Önceki Hash (32B) + Yeni Kaydın Veri Kısmı (16B)
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, current_chain_hash, 32);
    sha256_update(&ctx, (const uint8_t*)rec, 16); // İlk 16 bayt (hash hariç veri gövdesi)
    sha256_final(&ctx, rec->hash);

    // Yeni zincir halkasını güncelle
    memcpy(current_chain_hash, rec->hash, 32);
    log_count++;

    return 1;
}

uint32_t Logger_GetCount(void) {
    return log_count;
}

const LogRecord_t* Logger_GetRecord(uint32_t idx) {
    if (idx >= log_count) return NULL;
    return &log_buffer[idx];
}

void Logger_GetLastHashHex(char out_hex[65]) {
    for (int i = 0; i < 32; i++) {
        sprintf(&out_hex[i * 2], "%02x", current_chain_hash[i]);
    }
    out_hex[64] = '\0';
}
