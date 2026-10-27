#ifndef INC_MKT_H_
#define INC_MKT_H_

#include <math.h>
#include <stdint.h>

#define MKT_DH_R  10000.0f  // Standart aktivasyon enerjisi sabiti (delta H / R)

typedef struct {
    double exp_sum;         // e^(-10000/T) değerlerinin kümülatif toplamı
    uint32_t sample_count;  // Alınan toplam geçerli örnek sayısı
    float min_temp;         // Yolculuk boyunca görülen en düşük sıcaklık
    float max_temp;         // Yolculuk boyunca görülen en yüksek sıcaklık
    uint32_t violation_sec; // 2°C - 8°C dışına çıkılan toplam süre (saniye)
} MKT_Context;

void MKT_Init(MKT_Context *ctx);
void MKT_AddSample(MKT_Context *ctx, float temp_c);
float MKT_Calculate(MKT_Context *ctx);

#endif /* INC_MKT_H_ */
