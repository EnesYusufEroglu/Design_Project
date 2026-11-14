#include "flash_storage.h"
#include <string.h>
#include <stddef.h>

void Flash_SaveBlackbox(uint8_t state, float mkt, uint8_t b_mkt, uint8_t b_donma, uint8_t b_darbe,
                        uint32_t darbe_sayisi, DS3231_DateTime *t_start, DS3231_DateTime *t_end)
{
    // Sadece başlık bilgilerini (Header - 36 bayt) RAM'de hazırla, 48 KB dizi AÇMA!
    BlackboxFlashData_t header_only;
    memset(&header_only, 0, offsetof(BlackboxFlashData_t, kayitlar));

    header_only.magic           = FLASH_MAGIC_KEY;
    header_only.locked_state    = state;
    header_only.bayrak_mkt      = b_mkt;
    header_only.bayrak_donma    = b_donma;
    header_only.bayrak_darbe    = b_darbe;
    header_only.nihai_mkt       = mkt;
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

        // 1. Önce Header kısmını (kayitlar dizisine kadar olan baytları) Flash'a yaz
        uint32_t *hdr_ptr = (uint32_t*)&header_only;
        uint32_t hdr_words = offsetof(BlackboxFlashData_t, kayitlar) / 4;

        for (uint32_t i = 0; i < hdr_words; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, hdr_ptr[i]);
            current_addr += 4;
        }

        // 2. Ardından log kayıtlarını doğrudan logger.c RAM'inden okuyup Flash'a yaz
        for (uint32_t i = 0; i < header_only.log_sayisi; i++) {
            const LogRecord_t *rec = Logger_GetRecord(i);
            if (rec != NULL) {
                const uint32_t *rec_words = (const uint32_t*)rec;
                uint32_t words_per_rec = sizeof(LogRecord_t) / 4; // 48 / 4 = 12 word
                for (uint32_t w = 0; w < words_per_rec; w++) {
                    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, rec_words[w]);
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
