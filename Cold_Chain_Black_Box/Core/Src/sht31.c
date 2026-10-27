#include "sht31.h"

// Sensörün donanımsal CRC-8 algoritması
static uint8_t SHT31_CheckCRC(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 8; bit > 0; --bit) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

uint8_t SHT31_Read(I2C_HandleTypeDef *hi2c, float *temperature, float *humidity) {
    uint8_t cmd[2] = { (SHT31_CMD_MEAS_HIGHREP >> 8) & 0xFF, SHT31_CMD_MEAS_HIGHREP & 0xFF };
    uint8_t raw_data[6];

    // 1. Ölçüm başlatma komutunu gönder
    if (HAL_I2C_Master_Transmit(hi2c, SHT31_I2C_ADDR, cmd, 2, 100) != HAL_OK) {
        return 0; // İletişim hatası
    }

    // 2. Sensörün dahili ADC ölçümünü tamamlaması için bekleme (Yüksek hassasiyet için max 15ms)
    HAL_Delay(20);

    // 3. 6 bayt veriyi oku
    if (HAL_I2C_Master_Receive(hi2c, SHT31_I2C_ADDR, raw_data, 6, 100) != HAL_OK) {
        return 0; // Okuma hatası
    }

    // 4. CRC kontrolleri
    if (SHT31_CheckCRC(&raw_data[0], 2) != raw_data[2] ||
        SHT31_CheckCRC(&raw_data[3], 2) != raw_data[5]) {
        return 0; // Bozuk veri
    }

    // 5. Ham veriyi sıcaklık ve neme dönüştür
    uint16_t raw_temp = (raw_data[0] << 8) | raw_data[1];
    uint16_t raw_hum  = (raw_data[3] << 8) | raw_data[4];

    // Formüller sensörün orijinal datasheet değerleridir
    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity    = 100.0f * ((float)raw_hum / 65535.0f);

    return 1; // Başarılı
}
