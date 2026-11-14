/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sht31.h"
#include "ds3231.h"
#include "adxl345.h"
#include "mkt.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
float sicaklik = 0.0f;
float nem = 0.0f;
uint8_t okuma_durumu = 0;
DS3231_DateTime ayar_zaman;
ADXL345_Data ivme;
volatile uint8_t darbe_tespit_edildi = 0;
uint32_t toplam_darbe_sayisi = 0;
MKT_Context soguk_zincir_mkt;

// --- GÜVENLİK VE DENETİM BAYRAKLARI ---

// İhlal Nedenleri (Bitmask veya Bayrak olarak)
uint8_t bayrak_mkt_ihlali    = 0; // MKT 2-8°C dışında mı?
uint8_t bayrak_donma_riski   = 0; // Sıcaklık 0°C altına düştü mü?
uint8_t bayrak_mekanik_darbe = 0; // Kritik şok algılandı mı?
// --- SİSTEM DURUM MAKİNESİ (TEK YÖNLÜ) ---
typedef enum {
    STATE_IDLE = 0,   // Hazır / Bekleme (Soğutucuya konup iklimlenmesi bekleniyor)
    STATE_LOGGING,    // Yolculuk / Aktif Kayıt (LED'ler kapalı, sadece kümülatif toplam)
    STATE_FINISHED    // Teslimat / Kilitli (MKT hesaplandı, nihai LED sabit yanıyor)
} SystemState_t;

SystemState_t cihaz_durumu = STATE_IDLE;

// --- SENSÖR VE MKT DEĞİŞKENLERİ ---
DS3231_DateTime suanki_zaman;
ADXL345_Data ivme;

MKT_Context soguk_zincir_mkt;
float nihai_mkt_degeri = 0.0f;

// --- ZAMANLAYICI VE BUTON YÖNETİMİ ---
uint32_t son_ornek_zamani   = 0;
uint32_t son_idle_blink     = 0;
uint32_t buton_basilma_ani  = 0;
uint8_t  buton_basili_tutuldu = 0;
uint8_t  komut_tetiklendi   = 0; // Tek basışta birden fazla geçişi engeller

DS3231_DateTime baslatma_zamani;
DS3231_DateTime bitirme_zamani;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Yolculugu_Baslat(void)
{
	HAL_Delay(10000);
	DS3231_GetTime(&hi2c1, &baslatma_zamani);
    // Sayaçları ve bayrakları sıfırla
    MKT_Init(&soguk_zincir_mkt);
    toplam_darbe_sayisi  = 0;
    darbe_tespit_edildi  = 0;
    bayrak_mkt_ihlali    = 0;
    bayrak_donma_riski   = 0;
    bayrak_mekanik_darbe = 0;
    ADXL345_ClearInterrupt(&hi2c1);

    // Kullanıcıya "Kayıt Başladı" onayı: 1 saniye sabit Yeşil LED, sonra karanlık
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);
    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);

    son_ornek_zamani = HAL_GetTick();
    cihaz_durumu = STATE_LOGGING;
}

void Yolculugu_Bitir_Ve_Degerlendir(void)
{
	DS3231_GetTime(&hi2c1, &bitirme_zamani);
    cihaz_durumu = STATE_FINISHED;

    // MKT hesaplaması yolculuk boyunca HİÇ yapılmadı, SADECE ŞİMDİ 1 KEZ yapılıyor
    nihai_mkt_degeri = MKT_Calculate(&soguk_zincir_mkt);

    // 1. Donma kontrolü (0°C ve altı)
    if (soguk_zincir_mkt.min_temp <= 0.0f) {
        bayrak_donma_riski = 1;
    }

    // 2. MKT eşik kontrolü (2°C - 8°C dışı veya hiç örnek alınmadıysa)
    if (soguk_zincir_mkt.sample_count == 0 ||
        nihai_mkt_degeri < 2.0f || nihai_mkt_degeri > 8.0f) {
        bayrak_mkt_ihlali = 1;
    }

    // 3. Mekanik şok kontrolü
    if (toplam_darbe_sayisi > 0) {
        bayrak_mekanik_darbe = 1;
    }

    // NİHAİ LED KARARI
    if (bayrak_donma_riski || bayrak_mkt_ihlali || bayrak_mekanik_darbe) {
        // ÜRÜN BOZULMUŞ: Kırmızı LED Sabit
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    } else {
        // ÜRÜN SAĞLAM: Yeşil LED Sabit
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    }
}

void Buton_Kontrol_Et(void)
{
    // Aktif LOW (Pull-up olduğu için basılınca GPIO_PIN_RESET okunur)
    uint8_t buton_basili = (HAL_GPIO_ReadPin(BTN_USER_GPIO_Port, BTN_USER_Pin) == GPIO_PIN_RESET);

    if (buton_basili)
    {
        if (!buton_basili_tutuldu) {
            buton_basili_tutuldu = 1;
            buton_basilma_ani = HAL_GetTick();
            komut_tetiklendi = 0;
        }

        uint32_t basili_kalma_suresi = HAL_GetTick() - buton_basilma_ani;

        // 3 saniye (3000 ms) ile 10 saniye (10000 ms) arası bilinçli basış
        // 10 saniyeden uzun basışlar bantlama/sıkışma sabotajı sayılır ve yoksayılır
        if (basili_kalma_suresi >= 3000 && basili_kalma_suresi <= 10000 && !komut_tetiklendi)
        {
            komut_tetiklendi = 1; // Elinizi çekene kadar tekrar tetiklenmesini önle

            if (cihaz_durumu == STATE_IDLE) {
                Yolculugu_Baslat();
            }
            else if (cihaz_durumu == STATE_LOGGING) {
                Yolculugu_Bitir_Ve_Degerlendir();
            }
            // STATE_FINISHED durumunda hiçbir şey yapmaz (Tek yönlü kilit)
        }
    }
    else
    {
        // Buton bırakıldı: Eğer LOGGING modundaysa ve kısa basış (< 1 sn) yapıldıysa
        // Kuryeye "Cihaz aktif, pil dolu" pingi (100 ms yeşil göz kırpma) ver
        if (buton_basili_tutuldu && !komut_tetiklendi && cihaz_durumu == STATE_LOGGING)
        {
            uint32_t sure = HAL_GetTick() - buton_basilma_ani;
            if (sure >= 50 && sure < 1000) {
                HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
                HAL_Delay(100);
                HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
            }
        }

        buton_basili_tutuldu = 0;
        komut_tetiklendi = 0;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ADXL_INT1_Pin && cihaz_durumu == STATE_LOGGING)
    {
        darbe_tespit_edildi = 1;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  char compile_time[] = __TIME__; // "HH:MM:SS"


  ayar_zaman.hours   = (compile_time[0] - '0') * 10 + (compile_time[1] - '0');
  ayar_zaman.minutes = (compile_time[3] - '0') * 10 + (compile_time[4] - '0');
  ayar_zaman.seconds = (compile_time[6] - '0') * 10 + (compile_time[7] - '0') + 5; // +5 sn yükleme payı

  ayar_zaman.day     = 26;
  ayar_zaman.month   = 10;
  ayar_zaman.year    = 2026;
  ayar_zaman.day_of_week = 1;

  // Başlangıçta tüm LED'leri kapat
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);

  // İvmeölçeri başlat
  if (ADXL345_Init(&hi2c1) != 1) {
      // Donanım hatası varsa kırmızı LED ile uyar
      HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  }

  MKT_Init(&soguk_zincir_mkt);
  cihaz_durumu = STATE_IDLE;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  uint32_t simdi = HAL_GetTick();

	        // 1. BUTON KONTROLÜ (Her döngüde anlık taranır)
	        Buton_Kontrol_Et();

	        // 2. DURUMA GÖRE SİSTEM DAVRANIŞI
	        switch (cihaz_durumu)
	        {
	            case STATE_IDLE:
	                // Hazır bekleme modu: Saniyede bir 50 ms yeşil göz kırpma
	                if (simdi - son_idle_blink >= 1000) {
	                    son_idle_blink = simdi;
	                    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
	                    HAL_Delay(100);
	                    HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
	                }
	                break;

	            case STATE_LOGGING:
	                // A) Darbe / Şok Kesmesi Kontrolü (Sessiz)
	                if (darbe_tespit_edildi) {
	                    darbe_tespit_edildi = 0;
	                    toplam_darbe_sayisi++;
	                    ADXL345_ClearInterrupt(&hi2c1);
	                }

	                // B) Periyodik Veri Toplama (1000 ms'de bir - Sadece kümülatif toplam)
	                if (simdi - son_ornek_zamani >= 1000) {
	                    son_ornek_zamani = simdi;

	                    okuma_durumu = SHT31_Read(&hi2c1, &sicaklik, &nem);
	                    DS3231_GetTime(&hi2c1, &suanki_zaman);
	                    ADXL345_ReadAccel(&hi2c1, &ivme);

	                    if (okuma_durumu == 1) {
	                        // Ağır logf() yok, sadece hızlı expf() kümülatif toplama
	                        MKT_AddSample(&soguk_zincir_mkt, sicaklik);
	                    }
	                }
	                break;

	            case STATE_FINISHED:
	                // Kayıt kilitlendi. LED'ler Yolculugu_Bitir_Ve_Degerlendir() içinde sabitlendi.
	                // Burada USB CDC üzerinden gelecek masaüstü komutları dinlenecek (FAZ 3).
	                break;
	        }

	        HAL_Delay(20); // Buton debounce ve CPU rahatlatma
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED_GREEN_Pin|LED_RED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED_GREEN_Pin LED_RED_Pin */
  GPIO_InitStruct.Pin = LED_GREEN_Pin|LED_RED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ADXL_INT1_Pin */
  GPIO_InitStruct.Pin = ADXL_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ADXL_INT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_USER_Pin */
  GPIO_InitStruct.Pin = BTN_USER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_USER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
