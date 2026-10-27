#ifndef INC_ADXL345_H_
#define INC_ADXL345_H_

#include "main.h"

#define ADXL345_I2C_ADDR         (0x53 << 1) // 0xA6

// ADXL345 Register Adresleri
#define ADXL345_REG_DEVID        0x00
#define ADXL345_REG_THRESH_ACT   0x24
#define ADXL345_REG_ACT_INACT_CTL 0x27
#define ADXL345_REG_BW_RATE      0x2C
#define ADXL345_REG_POWER_CTL    0x2D
#define ADXL345_REG_INT_ENABLE   0x2E
#define ADXL345_REG_INT_MAP      0x2F
#define ADXL345_REG_INT_SOURCE   0x30
#define ADXL345_REG_DATA_FORMAT  0x31
#define ADXL345_REG_DATAX0       0x32

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    float ax_g;
    float ay_g;
    float az_g;
} ADXL345_Data;

// Fonksiyon prototipleri
uint8_t ADXL345_Init(I2C_HandleTypeDef *hi2c);
uint8_t ADXL345_ReadAccel(I2C_HandleTypeDef *hi2c, ADXL345_Data *data);
void ADXL345_ClearInterrupt(I2C_HandleTypeDef *hi2c);

#endif /* INC_ADXL345_H_ */
