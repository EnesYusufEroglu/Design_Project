#include "usb_protocol.h"
#include "usbd_cdc_if.h"
#include "main.h"
#include "mkt.h"
#include "logger.h"
#include "flash_storage.h"
#include "ds3231.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// main.c içindeki dış (extern) değişken ve fonksiyonlar
extern I2C_HandleTypeDef hi2c1;
extern uint8_t cihaz_durumu; // 0: IDLE, 1: LOGGING, 2: FINISHED
extern MKT_Context soguk_zincir_mkt;
extern float nihai_mkt_degeri;
extern uint8_t bayrak_mkt_ihlali;
extern uint8_t bayrak_donma_riski;
extern uint8_t bayrak_mekanik_darbe;
extern uint32_t toplam_darbe_sayisi;
extern DS3231_DateTime baslatma_zamani;
extern DS3231_DateTime bitirme_zamani;

extern void Cihazi_Uygulamadan_Sifirla(void);

#define RX_CMD_BUF_SIZE 64
static char rx_cmd_buffer[RX_CMD_BUF_SIZE];
static volatile uint8_t cmd_ready = 0;

// USB güvenli gönderim yardımcı fonksiyonu (Meşgul ise bekler)
static void USB_SendString(const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    uint32_t timeout = HAL_GetTick();
    while (CDC_Transmit_FS((uint8_t*)str, len) == USBD_BUSY) {
        if (HAL_GetTick() - timeout > 200) break;
    }
}

// USB kesmesi içinden çağrılır (Sadece komutu tampona alır)
void USB_Proto_OnReceive(uint8_t *buf, uint32_t len)
{
    if (cmd_ready) return; // Önceki komut henüz işlenmediyse yoksay

    uint32_t copy_len = (len < RX_CMD_BUF_SIZE - 1) ? len : (RX_CMD_BUF_SIZE - 1);
    memcpy(rx_cmd_buffer, buf, copy_len);
    rx_cmd_buffer[copy_len] = '\0';

    // \r ve \n karakterlerini temizle
    for (uint32_t i = 0; i < copy_len; i++) {
        if (rx_cmd_buffer[i] == '\r' || rx_cmd_buffer[i] == '\n') {
            rx_cmd_buffer[i] = '\0';
            break;
        }
    }
    cmd_ready = 1;
}

// Ana while(1) döngüsünden çağrılır
void USB_Proto_Process(void)
{
    if (!cmd_ready) return;

    char tx_buf[256];

    // 1. ÖZET RAPOR KOMUTU
    if (strcmp(rx_cmd_buffer, "GET_SUMMARY") == 0)
    {
        BlackboxFlashData_t flash_hdr;
        uint8_t has_flash = Flash_LoadBlackbox(&flash_hdr);

        uint32_t total_logs = (has_flash && cihaz_durumu == 2) ? flash_hdr.log_sayisi : Logger_GetCount();
        float mkt_val = (cihaz_durumu == 2) ? nihai_mkt_degeri : MKT_Calculate(&soguk_zincir_mkt);

        // Son kaydın SHA-256 kök özetini al
        char root_hash[65] = "0000000000000000000000000000000000000000000000000000000000000000";
        if (total_logs > 0) {
            const LogRecord_t *last_rec = (has_flash && cihaz_durumu == 2)
                                          ? Flash_GetRecordPtr(total_logs - 1)
                                          : Logger_GetRecord(total_logs - 1);
            if (last_rec != NULL) {
                for (int i = 0; i < 32; i++) {
                    sprintf(&root_hash[i * 2], "%02x", last_rec->hash[i]);
                }
                root_hash[64] = '\0';
            }
        }

        // Format: SUMMARY|STATE|MKT|MIN_T|MAX_T|SHOCKS|F_MKT|F_FREEZE|F_SHOCK|START_DT|END_DT|LOG_CNT|ROOT_HASH
        snprintf(tx_buf, sizeof(tx_buf),
                 "SUMMARY|%u|%.2f|%.2f|%.2f|%lu|%u|%u|%u|%04u-%02u-%02u %02u:%02u:%02u|%04u-%02u-%02u %02u:%02u:%02u|%lu|%s\r\n",
                 cihaz_durumu,
                 mkt_val,
                 soguk_zincir_mkt.min_temp,
                 soguk_zincir_mkt.max_temp,
                 toplam_darbe_sayisi,
                 bayrak_mkt_ihlali,
                 bayrak_donma_riski,
                 bayrak_mekanik_darbe,
                 baslatma_zamani.year, baslatma_zamani.month, baslatma_zamani.day,
                 baslatma_zamani.hours, baslatma_zamani.minutes, baslatma_zamani.seconds,
                 bitirme_zamani.year, bitirme_zamani.month, bitirme_zamani.day,
                 bitirme_zamani.hours, bitirme_zamani.minutes, bitirme_zamani.seconds,
                 total_logs,
                 root_hash);

        USB_SendString(tx_buf);
    }
    // 2. TÜM MÜHÜRLÜ KAYITLARI DÖKME KOMUTU
    else if (strcmp(rx_cmd_buffer, "GET_LOGS") == 0)
    {
        BlackboxFlashData_t flash_hdr;
        uint8_t use_flash = (Flash_LoadBlackbox(&flash_hdr) == 1 && cihaz_durumu == 2);
        uint32_t total_logs = use_flash ? flash_hdr.log_sayisi : Logger_GetCount();

        snprintf(tx_buf, sizeof(tx_buf), "LOGS_BEGIN|%lu\r\n", total_logs);
        USB_SendString(tx_buf);

        for (uint32_t i = 0; i < total_logs; i++)
        {
            const LogRecord_t *rec = use_flash ? Flash_GetRecordPtr(i) : Logger_GetRecord(i);
            if (rec != NULL)
            {
                char hash_hex[65];
                for (int b = 0; b < 32; b++) {
                    sprintf(&hash_hex[b * 2], "%02x", rec->hash[b]);
                }
                hash_hex[64] = '\0';

                // Format: LOG|IDX|20YY-MM-DD HH:MM:SS|TEMP|HUM|SHOCKS|HASH
                snprintf(tx_buf, sizeof(tx_buf),
                         "LOG|%lu|20%02u-%02u-%02u %02u:%02u:%02u|%.2f|%.2f|%u|%s\r\n",
                         rec->index,
                         rec->year, rec->month, rec->day,
                         rec->hours, rec->minutes, rec->seconds,
                         rec->temp_x100 / 100.0f,
                         rec->hum_x100 / 100.0f,
                         rec->shock_count,
                         hash_hex);

                USB_SendString(tx_buf);
                HAL_Delay(2); // PC seri port tamponunun taşmaması için 2 ms nefes payı
            }
        }

        USB_SendString("LOGS_END\r\n");
    }
    // 3. YETKİLİ CİHAZ SIFIRLAMA KOMUTU (Sadece masaüstünden kilit açma)
    else if (strcmp(rx_cmd_buffer, "RESET_DEVICE") == 0)
    {
        if (cihaz_durumu == 1) { // STATE_LOGGING
            USB_SendString("ERR|TRIP_IN_PROGRESS\r\n");
        } else {
            Cihazi_Uygulamadan_Sifirla();
            USB_SendString("OK|DEVICE_RESET\r\n");
        }
    }
    // 4. RTC SAAT SENKRONİZASYONU (Format: SYNC_TIME|26|11|14|23|15|00)
    else if (strncmp(rx_cmd_buffer, "SYNC_TIME|", 10) == 0)
    {
        int yy, mm, dd, hh, min, ss;
        if (sscanf(rx_cmd_buffer + 10, "%d|%d|%d|%d|%d|%d", &yy, &mm, &dd, &hh, &min, &ss) == 6) {
            DS3231_DateTime yeni_zaman = {
                .year = (uint16_t)(2000 + yy),
                .month = (uint8_t)mm,
                .day = (uint8_t)dd,
                .hours = (uint8_t)hh,
                .minutes = (uint8_t)min,
                .seconds = (uint8_t)ss,
                .day_of_week = 1
            };
            DS3231_SetTime(&hi2c1, &yeni_zaman);
            USB_SendString("OK|TIME_SYNCED\r\n");
        } else {
            USB_SendString("ERR|INVALID_TIME_FORMAT\r\n");
        }
    }
    else
    {
        USB_SendString("ERR|UNKNOWN_CMD\r\n");
    }

    cmd_ready = 0;
}
