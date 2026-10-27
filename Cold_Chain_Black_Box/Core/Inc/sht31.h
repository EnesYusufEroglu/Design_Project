#ifndef INC_SHT31_H_
#define INC_SHT31_H_

#include "main.h"

#define SHT31_I2C_ADDR (0x44 << 1) // 0x88

// Tek ölçüm, yüksek tekrarlanabilirlik (Clock stretching kapalı)
#define SHT31_CMD_MEAS_HIGHREP 0x2400

uint8_t SHT31_Read(I2C_HandleTypeDef *hi2c, float *temperature, float *humidity);

#endif /* INC_SHT31_H_ */
