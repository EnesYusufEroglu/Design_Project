#ifndef INC_DS3231_H_
#define INC_DS3231_H_

#include "main.h"

#define DS3231_I2C_ADDR (0x68 << 1) // 0xD0

typedef struct {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day_of_week; // 1: Pazartesi, ..., 7: Pazar
    uint8_t day;
    uint8_t month;
    uint16_t year;
} DS3231_DateTime;

// Fonksiyon prototipleri
void DS3231_SetTime(I2C_HandleTypeDef *hi2c, DS3231_DateTime *dt);
uint8_t DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_DateTime *dt);

#endif /* INC_DS3231_H_ */
