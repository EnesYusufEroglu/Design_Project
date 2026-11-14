#ifndef INC_FLASH_STORAGE_H_
#define INC_FLASH_STORAGE_H_

#include "main.h"
#include "logger.h"
#include "mkt.h"
#include "ds3231.h"

// STM32F401RCT6 Sector 5 Başlangıç Adresi (128 KB alan)
#define FLASH_STORAGE_ADDR   0x08020000U
#define FLASH_MAGIC_KEY      0xDEADBEEFU

typedef struct __attribute__((packed)) {
    uint32_t magic;              // Geçerli kayıt imzası (0xDEADBEEF)
    uint8_t  locked_state;       // 0: IDLE, 2: FINISHED (Kilitli)
    uint8_t  bayrak_mkt;
    uint8_t  bayrak_donma;
    uint8_t  bayrak_darbe;
    float    nihai_mkt;
    uint32_t toplam_darbe;
    DS3231_DateTime baslatma_zamani;
    DS3231_DateTime bitirme_zamani;
    uint32_t log_sayisi;
    LogRecord_t kayitlar[MAX_LOG_RECORDS]; // 1000 * 48B = 48 KB
} BlackboxFlashData_t;

void Flash_SaveBlackbox(uint8_t state, float mkt, uint8_t b_mkt, uint8_t b_donma, uint8_t b_darbe,
                        uint32_t darbe_sayisi, DS3231_DateTime *t_start, DS3231_DateTime *t_end);
uint8_t Flash_LoadBlackbox(BlackboxFlashData_t *out_data);
void Flash_FactoryResetForNewTrip(void);

#endif /* INC_FLASH_STORAGE_H_ */
