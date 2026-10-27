#include "mkt.h"

void MKT_Init(MKT_Context *ctx) {
    ctx->exp_sum = 0.0;
    ctx->sample_count = 0;
    ctx->min_temp = 999.0f;
    ctx->max_temp = -999.0f;
    ctx->violation_sec = 0;
}

void MKT_AddSample(MKT_Context *ctx, float temp_c) {
    // Min - Max sınır takibi
    if (temp_c < ctx->min_temp) ctx->min_temp = temp_c;
    if (temp_c > ctx->max_temp) ctx->max_temp = temp_c;

    // 2°C - 8°C soğuk zincir aralık kontrolü
    if (temp_c < 2.0f || temp_c > 8.0f) {
        ctx->violation_sec++;
    }

    // Kelvin dönüşümü
    double temp_k = (double)temp_c + 273.15;

    // Arrhenius kümülatif toplamı: e^(- (dH/R) / T_K )
    ctx->exp_sum += exp(-MKT_DH_R / temp_k);
    ctx->sample_count++;
}

float MKT_Calculate(MKT_Context *ctx) {
    if (ctx->sample_count == 0 || ctx->exp_sum <= 0.0) {
        return 0.0f;
    }

    double ortalama_exp = ctx->exp_sum / (double)ctx->sample_count;
    double mkt_kelvin = MKT_DH_R / (-log(ortalama_exp));

    return (float)(mkt_kelvin - 273.15); // Celsius olarak döndür
}
