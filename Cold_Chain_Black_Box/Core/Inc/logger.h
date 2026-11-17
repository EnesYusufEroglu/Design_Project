#ifndef INC_LOGGER_H_
#define INC_LOGGER_H_

#include <stdint.h>
#include "ds3231.h"
#include "sha256.h"

#define MAX_LOG_RECORDS 500

typedef struct __attribute__((packed)) {
    uint32_t index;          // Kayıt sıra numarası (0, 1, 2...)
    uint8_t  year;           // 2000 + year (örn: 26 -> 2026)
    uint8_t  month;
    uint8_t  day;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
    int16_t  temp_x100;      // Örn: 4.35°C -> 435
    uint16_t hum_x100;       // Örn: %62.10 -> 6210
    uint16_t shock_count;    // O ana kadarki kümülatif darbe sayısı
    uint8_t  hash[32];       // Önceki hash + bu kaydın verilerinin SHA-256 özeti
} LogRecord_t;               // Toplam: 48 Bayt

void Logger_Init(void);
uint8_t Logger_AddRecord(DS3231_DateTime *dt, float temp, float hum, uint32_t shocks);
uint32_t Logger_GetCount(void);
const LogRecord_t* Logger_GetRecord(uint32_t idx);
void Logger_GetLastHashHex(char out_hex[65]);

#endif /* INC_LOGGER_H_ */
