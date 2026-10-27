#include "adxl345.h"

// Register'a tek bayt yazma
static void ADXL345_WriteRegister(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val) {
    uint8_t data[2] = { reg, val };
    HAL_I2C_Master_Transmit(hi2c, ADXL345_I2C_ADDR, data, 2, 100);
}

// Register'dan tek bayt okuma
static uint8_t ADXL345_ReadRegister(I2C_HandleTypeDef *hi2c, uint8_t reg) {
    uint8_t val = 0;
    HAL_I2C_Master_Transmit(hi2c, ADXL345_I2C_ADDR, &reg, 1, 100);
    HAL_I2C_Master_Receive(hi2c, ADXL345_I2C_ADDR, &val, 1, 100);
    return val;
}

uint8_t ADXL345_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t dev_id = ADXL345_ReadRegister(hi2c, ADXL345_REG_DEVID);
    if (dev_id != 0xE5) {
        return 0; // Sensör tanınmadı
    }

    // 1. +-16g aralık, Tam çözünürlük modu (Full Res)
    ADXL345_WriteRegister(hi2c, ADXL345_REG_DATA_FORMAT, 0x0B);

    // 2. Çıkış Veri Hızı: 100 Hz
    ADXL345_WriteRegister(hi2c, ADXL345_REG_BW_RATE, 0x0A);

    // 3. DARBE EŞİĞİ (GÜNCELLENDİ):
    // 80 * 0.0625g = 5.0g eşiği (Masaya dokunmaları eler, sadece sert darbeleri yakalar)
    // Hâlâ hassas gelirse bu 80 değerini 96 (6g) veya 112 (7g) yapabilirsiniz.
    ADXL345_WriteRegister(hi2c, ADXL345_REG_THRESH_ACT, 160);

    // 4. AC-Coupled Mod: Statik yerçekimini filtrele, X, Y, Z eksenlerini aç (0x70)
    ADXL345_WriteRegister(hi2c, ADXL345_REG_ACT_INACT_CTL, 0x70);

    // 5. Kesmeyi INT1 (PA2) bacağına yönlendir
    ADXL345_WriteRegister(hi2c, ADXL345_REG_INT_MAP, 0x00);

    // 6. Activity kesmesini aktif et
    ADXL345_WriteRegister(hi2c, ADXL345_REG_INT_ENABLE, 0x10);

    // 7. Ölçüm modunu başlat
    ADXL345_WriteRegister(hi2c, ADXL345_REG_POWER_CTL, 0x08);

    // Varsa önceki kesmeyi temizle
    ADXL345_ClearInterrupt(hi2c);

    return 1;
}

uint8_t ADXL345_ReadAccel(I2C_HandleTypeDef *hi2c, ADXL345_Data *data) {
    uint8_t reg = ADXL345_REG_DATAX0;
    uint8_t raw_data[6];

    if (HAL_I2C_Master_Transmit(hi2c, ADXL345_I2C_ADDR, &reg, 1, 100) != HAL_OK)
        return 0;

    if (HAL_I2C_Master_Receive(hi2c, ADXL345_I2C_ADDR, raw_data, 6, 100) != HAL_OK)
        return 0;

    data->x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
    data->y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
    data->z = (int16_t)((raw_data[5] << 8) | raw_data[4]);

    // Full-res modunda 1 LSB = 3.9 mg (0.0039g)
    data->ax_g = data->x * 0.0039f;
    data->ay_g = data->y * 0.0039f;
    data->az_g = data->z * 0.0039f;

    return 1;
}

// Kesme bayrağını sıfırlamak için INT_SOURCE register'ı okunmalıdır
void ADXL345_ClearInterrupt(I2C_HandleTypeDef *hi2c) {
    ADXL345_ReadRegister(hi2c, ADXL345_REG_INT_SOURCE);
}
