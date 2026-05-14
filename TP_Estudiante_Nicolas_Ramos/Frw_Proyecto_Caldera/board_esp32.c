/**
 * @file board_esp32.c
 * @brief Plantilla PLC_ESP32: definiciones y configuración de GPIO.
 */

#include "board_esp32.h"

#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/dac.h"
#endif

/* Array de salidas digitales en orden Q0.0 .. Q0.7 */
const uint8_t board_esp32_salidas_dig[NUM_Q0] = {
    Q0_0, Q0_1, Q0_2, Q0_3, Q0_4, Q0_5, Q0_6, Q0_7
};

#ifdef ESP_PLATFORM
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_a0 = NULL;
#endif

void board_esp32_init(void)
{
#ifdef ESP_PLATFORM
    gpio_config_t io = {
        .pin_bit_mask   = 0,
        .mode           = GPIO_MODE_OUTPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };

    /* Entradas digitales I0.0, I0.1, I0.2 (pull-down), I0.3 (sin pull) */
    gpio_config_t in_pulldown = {
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_ENABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };
    in_pulldown.pin_bit_mask = (1ULL << I0_0) | (1ULL << I0_1) | (1ULL << I0_2);
    gpio_config(&in_pulldown);

    gpio_config_t in_float = {
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE,
    };
    in_float.pin_bit_mask = (1ULL << I0_3);
    gpio_config(&in_float);

    /* Salidas digitales Q0.0 .. Q0.7 */
    io.pin_bit_mask = (1ULL << Q0_0) | (1ULL << Q0_1) | (1ULL << Q0_2) | (1ULL << Q0_3)
                    | (1ULL << Q0_4) | (1ULL << Q0_5) | (1ULL << Q0_6) | (1ULL << Q0_7);
    gpio_config(&io);
    for (int i = 0; i < NUM_Q0; i++)
        gpio_set_level(board_esp32_salidas_dig[i], 0);

    /* Buzzer */
    io.pin_bit_mask = (1ULL << BUZZER);
    gpio_config(&io);
    gpio_set_level(BUZZER, 0);

    /* LED RGB */
    io.pin_bit_mask = (1ULL << RGB_R) | (1ULL << RGB_G) | (1ULL << RGB_B);
    gpio_config(&io);
    gpio_set_level(RGB_R, 0);
    gpio_set_level(RGB_G, 0);
    gpio_set_level(RGB_B, 0);

    /* Entradas analógicas A0.0 (GPIO 36), A0.1 (GPIO 35): ADC1 oneshot (ESP-IDF 5.x) */
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&adc_cfg, &adc1_handle);
    if (err != ESP_OK) {
        adc1_handle = NULL;
    } else {
        adc_oneshot_chan_cfg_t ch_cfg = {
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
        };
        adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &ch_cfg);  /* A0_0 */
        adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &ch_cfg);   /* A0_1 */

        adc_cali_line_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_11,
            .bitwidth = ADC_BITWIDTH_12,
        };
        if (adc_cali_create_scheme_line_fitting(&cali_cfg, &adc1_cali_a0) != ESP_OK)
            adc1_cali_a0 = NULL;
    }

    /* Salidas DAC0.0 (GPIO 25), DAC0.1 (GPIO 26) */
    dac_output_enable(DAC_CHANNEL_1);  /* DAC0_0 = GPIO 25 */
    dac_output_enable(DAC_CHANNEL_2);  /* DAC0_1 = GPIO 26 */
    dac_output_voltage(DAC_CHANNEL_1, 0);
    dac_output_voltage(DAC_CHANNEL_2, 0);
#endif
}

#ifdef ESP_PLATFORM
int board_esp32_adc_a0_0_raw(void)
{
    if (adc1_handle == NULL) return -1;
    int raw = 0;
    if (adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw) != ESP_OK)
        return -1;
    return raw;
}

int board_esp32_adc_a0_0_mv(void)
{
    if (adc1_handle == NULL) return -1;
    int raw = 0;
    if (adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &raw) != ESP_OK)
        return -1;
    if (adc1_cali_a0 != NULL) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(adc1_cali_a0, raw, &mv) == ESP_OK)
            return mv;
    }
    return (int)((float)raw * 2450.0f / 4095.0f + 0.5f);
}
#endif
