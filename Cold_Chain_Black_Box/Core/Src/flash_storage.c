#include "flash_storage.h"
#include <string.h>
#include <stddef.h>

void Flash_SaveBlackbox(uint8_t state, float mkt, float min_temp, float max_temp,
                        uint8_t b_mkt, uint8_t b_donma, uint8_t b_darbe,
                        uint32_t darbe_sayisi, DS3231_DateTime *t_start, DS3231_DateTime *t_end)
{
    BlackboxFlashData_t header_only;
    memset(&header_only, 0, offsetof(BlackboxFlashData_t, kayitlar));

    header_only.magic           = FLASH_MAGIC_KEY;
    header_only.locked_state    = state;
    header_only.bayrak_mkt      = b_mkt;
    header_only.bayrak_donma    = b_donma;
    header_only.bayrak_darbe    = b_darbe;
    header_only.nihai_mkt       = mkt;
    header_only.min_temp        = min_temp;   // EKLENDİ
    header_only.max_temp        = max_temp;   // EKLENDİ
    header_only.toplam_darbe    = darbe_sayisi;
    header_only.baslatma_zamani = *t_start;
    header_only.bitirme_zamani  = *t_end;
    header_only.log_sayisi      = Logger_GetCount();

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector       = FLASH_SECTOR_5;
    EraseInitStruct.NbSectors    = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK)
    {
        uint32_t current_addr = FLASH_STORAGE_ADDR;
        uint32_t word_buf = 0;

        // 1. Önce Header kısmını güvenli word hizalamasıyla Flash'a yaz
        const uint8_t *hdr_bytes = (const uint8_t*)&header_only;
        uint32_t hdr_bytes_len = offsetof(BlackboxFlashData_t, kayitlar);

        for (uint32_t offset = 0; offset < hdr_bytes_len; offset += 4) {
            memcpy(&word_buf, &hdr_bytes[offset], 4);
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, word_buf);
            current_addr += 4;
        }

        // 2. Log kayıtlarını packed struct hizalama hatasına düşmeden Flash'a yaz
        for (uint32_t i = 0; i < header_only.log_sayisi; i++) {
            const LogRecord_t *rec = Logger_GetRecord(i);
            if (rec != NULL) {
                const uint8_t *rec_bytes = (const uint8_t*)rec;
                for (uint32_t offset = 0; offset < sizeof(LogRecord_t); offset += 4) {
                    memcpy(&word_buf, &rec_bytes[offset], 4);
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, word_buf);
                    current_addr += 4;
                }
            }
        }
    }

    HAL_FLASH_Lock();
}

uint8_t Flash_LoadBlackbox(BlackboxFlashData_t *out_data)
{
    const BlackboxFlashData_t *flash_ptr = (const BlackboxFlashData_t*)FLASH_STORAGE_ADDR;
    if (flash_ptr->magic != FLASH_MAGIC_KEY) {
        return 0;
    }
    // Dikkat: out_data çağrılırken stack taşmaması için sadece header kopyalanır,
    // kayıtlar okunurken doğrudan FLASH_STORAGE_ADDR üzerinden okunacaktır.
    memcpy(out_data, flash_ptr, offsetof(BlackboxFlashData_t, kayitlar));
    return 1;
}

void Flash_FactoryResetForNewTrip(void)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector       = FLASH_SECTOR_5;
    EraseInitStruct.NbSectors    = 1;

    HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    HAL_FLASH_Lock();
}

const LogRecord_t* Flash_GetRecordPtr(uint32_t idx)
    {
        const BlackboxFlashData_t *flash_ptr = (const BlackboxFlashData_t*)FLASH_STORAGE_ADDR;
        if (flash_ptr->magic != FLASH_MAGIC_KEY || idx >= flash_ptr->log_sayisi) {
            return NULL;
        }
        return &flash_ptr->kayitlar[idx];
    }
