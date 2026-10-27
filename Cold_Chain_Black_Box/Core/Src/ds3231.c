#include "ds3231.h"

// BCD -> Decimal dönüşümü
static uint8_t BCD2DEC(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

// Decimal -> BCD dönüşümü
static uint8_t DEC2BCD(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// Saati ilk kez ayarlamak için (Gerektiğinde 1 kez çağrılır)
void DS3231_SetTime(I2C_HandleTypeDef *hi2c, DS3231_DateTime *dt) {
    uint8_t buffer[8];
    buffer[0] = 0x00; // Başlangıç register adresi (Seconds)
    buffer[1] = DEC2BCD(dt->seconds);
    buffer[2] = DEC2BCD(dt->minutes);
    buffer[3] = DEC2BCD(dt->hours);
    buffer[4] = DEC2BCD(dt->day_of_week);
    buffer[5] = DEC2BCD(dt->day);
    buffer[6] = DEC2BCD(dt->month);
    buffer[7] = DEC2BCD((uint8_t)(dt->year - 2000));

    HAL_I2C_Master_Transmit(hi2c, DS3231_I2C_ADDR, buffer, 8, 100);
}

// Anlık saati ve tarihi okur
uint8_t DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_DateTime *dt) {
    uint8_t reg = 0x00;
    uint8_t buffer[7];

    // Okuma adresini 0x00'a ayarla
    if (HAL_I2C_Master_Transmit(hi2c, DS3231_I2C_ADDR, &reg, 1, 100) != HAL_OK) {
        return 0; // İletişim hatası
    }

    // 7 bayt zaman verisini oku
    if (HAL_I2C_Master_Receive(hi2c, DS3231_I2C_ADDR, buffer, 7, 100) != HAL_OK) {
        return 0; // Okuma hatası
    }

    dt->seconds     = BCD2DEC(buffer[0] & 0x7F);
    dt->minutes     = BCD2DEC(buffer[1] & 0x7F);
    dt->hours       = BCD2DEC(buffer[2] & 0x3F); // 24 saat formatı
    dt->day_of_week = BCD2DEC(buffer[3] & 0x07);
    dt->day         = BCD2DEC(buffer[4] & 0x3F);
    dt->month       = BCD2DEC(buffer[5] & 0x1F);
    dt->year        = 2000 + BCD2DEC(buffer[6]);

    return 1; // Başarılı
}
