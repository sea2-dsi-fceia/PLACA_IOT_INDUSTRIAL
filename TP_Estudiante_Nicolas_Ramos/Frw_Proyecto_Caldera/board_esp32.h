/**
 * @file board_esp32.h
 * @brief Plantilla PLC_ESP32: declaraciones y configuraciones de GPIO,
 *        entradas/salidas digitales, analógicas, buzzer, RGB.
 *        OLED se declara en oled_display.h / oled_display.c.
 */

#ifndef BOARD_ESP32_H
#define BOARD_ESP32_H

#include <stdint.h>

/* ========== Entradas digitales I0.x ========== */
#define I0_0   5
#define I0_1  15
#define I0_2  16
#define I0_3  34

#define NUM_I0  4

/* ========== Salidas digitales Q0.x ========== */
#define Q0_0   4
#define Q0_1  17
#define Q0_2  18
#define Q0_3  19
#define Q0_4  23
#define Q0_5  27
#define Q0_6  32
#define Q0_7  33

#define NUM_Q0  8

/* ========== Entradas analógicas A0.x (ADC) ========== */
#define A0_0  36   /* VP */
#define A0_1  35   /* VN */

#define NUM_A0  2

/* ========== Salidas PWM DAC0.x ========== */
#define DAC0_0  25
#define DAC0_1  26

#define NUM_DAC0  2

/* ========== Buzzer ========== */
#define BUZZER  2

/* ========== LED RGB (cátodo común) ========== */
#define RGB_R  14
#define RGB_G  13
#define RGB_B  12

/* ========== Array de salidas digitales Q0.x (para recorrer) ========== */
extern const uint8_t board_esp32_salidas_dig[NUM_Q0];

/* ========== Inicialización (implementada en board_esp32.c) ========== */
void board_esp32_init(void);

#ifdef ESP_PLATFORM
/** Lectura raw del canal A0.0 (GPIO 36). 12 bits, -1 si error. */
int board_esp32_adc_a0_0_raw(void);
/** Tensión en el pin A0.0 (después del buffer), en mV; calibración line fitting si existe. -1 si error. */
int board_esp32_adc_a0_0_mv(void);
#endif

#endif /* BOARD_ESP32_H */
