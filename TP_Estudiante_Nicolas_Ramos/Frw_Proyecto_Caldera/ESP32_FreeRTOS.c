/**
 * @file ESP32_FreeRTOS.c
 * @brief Control de temperatura con FreeRTOS.
 *        LM35 0–150 °C → 0–1,5 V en AN0.0; divisor R28/R30 (2/3) → 0–1,0 V en A0.0; escala °C = 150·(V_pin/1V).
 *        I0.0 = sensor presión (5V/0V). Q0.0 = calefacción, Q0.1 = cooler.
 *        Setpoint por UART0 (USB): línea con Enter, 1–2 dígitos (0–99).
 *        Alarma: Q0.0 off, Q0.1 on, buzzer toggle 500 ms; 2 s sin I0.0 activo → normal.
 *        Calefacción con histéresis ±1 °C alrededor del setpoint (menos conmutaciones).
 */

#include "board_esp32.h"
#include "oled_display.h"
#include "control_state.h"
#include "web_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdio.h>

/* Prioridades: mayor número = mayor prioridad en el planificador FreeRTOS */
#define PRIO_CONTROL  5
#define PRIO_ADC      4
#define PRIO_UART     3
#define PRIO_DISPLAY  2

#define SETPOINT_QUEUE_LEN  4
#define SETPOINT_DEFAULT    25
#define ALARMA_TIEMPO_MS    2000
#define BUZZER_TOGGLE_MS    500
#define STACK_TASK          2048

/* Enciende si T < SP - HYST; apaga si T > SP + HYST; entre medias mantiene el estado anterior */
#define HYSTERESIS_TEMP_C  1.0f

/* 1 = sobrepresión con I0.0 en alto (5 V, informe). 0 = activo en bajo (contacto a masa / NPN). */
#define PRESION_I0_ACTIVO_NIVEL_ALTO  1

/* Acondicionamiento: V_pin = V_AN0.0 · R30/(R28+R30) = V_AN0.0 · 2/3.
 * LM35 máx. 1,5 V → V_pin máx. 1,0 V. T(°C) = T_max · (V_pin / 1 V). */
#define LM35_TEMP_MAX_C   150.0f
#define LM35_PIN_MV_MAX   1000.0f   /* 1,5 V LM35 × 2/3 en el pin A0.0 */
#define LM35_PIN_MV_TRIM  1.0f      /* Ganancia fina si la lectura no calza con multímetro */
#define LM35_OFFSET_C     0.0f      /* Corrección de offset en °C */
#define LM35_ADC_SAMPLES  4         /* Promedio de lecturas para reducir ruido ADC */

/* Estado global: control_state.c (g_control_estado + g_control_mutex) */
static QueueHandle_t queue_setpoint;

/* Convierte mV en A0.0 (board_esp32_adc_a0_0_mv, calibrado) a °C según escala 0–1 V → 0–150 °C */
static float leer_temperatura_lm35(void)
{
    int acc = 0, n = 0;
    for (int i = 0; i < LM35_ADC_SAMPLES; i++) {
        int mv = board_esp32_adc_a0_0_mv();
        if (mv >= 0) {
            acc += mv;
            n++;
        }
    }
    if (n == 0) return 0.0f;
    float mv_pin = LM35_PIN_MV_TRIM * (float)acc / (float)n;
    return (mv_pin / LM35_PIN_MV_MAX) * LM35_TEMP_MAX_C + LM35_OFFSET_C;
}

/* Cada 200 ms muestrea LM35 y actualiza estado.temp_c bajo mutex */
static void task_adc(void *pvParameters)
{
    for (;;) {
        float t = leer_temperatura_lm35();
        if (xSemaphoreTake(g_control_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            g_control_estado.temp_c = t;
            xSemaphoreGive(g_control_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* UART0 (USB): TX GPIO1, RX GPIO3 — mismo puerto que el monitor; setpoint línea + Enter */
#define UART_SETPOINT_NUM   UART_NUM_0
#define UART_SETPOINT_TX    1
#define UART_SETPOINT_RX    3
#define UART_BAUD           115200
#define UART_BUF            256

/* Lee el setpoint carácter a carácter: el USB envía dígitos sueltos y \r\n; leer 2 bytes de golpe falla a menudo */
static void task_uart(void *pvParameters)
{
    uint8_t line[12];
    size_t len = 0;

    for (;;) {
        uint8_t c;
        int n = uart_read_bytes(UART_SETPOINT_NUM, &c, 1, portMAX_DELAY);
        if (n <= 0)
            continue;

        if (c == '\r' || c == '\n') {
            int setpoint = -1;
            if (len == 1 && line[0] >= '0' && line[0] <= '9')
                setpoint = line[0] - '0';
            else if (len >= 2 && line[0] >= '0' && line[0] <= '9' && line[1] >= '0' && line[1] <= '9')
                setpoint = (line[0] - '0') * 10 + (line[1] - '0');
            len = 0;
            if (setpoint >= 0) {
                if (setpoint > 99)
                    setpoint = 99;
                (void)xQueueSend(queue_setpoint, &setpoint, pdMS_TO_TICKS(50));
            }
            continue;
        }

        if (len < sizeof(line) - 1)
            line[len++] = c;
        else
            len = 0;
    }
}

/* Prioridad máxima: cola setpoint, lógica alarma I0.0, T vs SP → Q0.0/Q0.1, buzzer en alarma */
static void task_control(void *pvParameters)
{
    int setpoint_local = SETPOINT_DEFAULT;
    int nuevo_setpoint;
    TickType_t now;
    static TickType_t buzz_last_toggle = 0;
    static int buzz_level = 0;
    /* Memoria de la histéresis: entre SP−HYST y SP+HYST no cambia el pedido de calefacción */
    static bool retencion_calefactor = false;

    for (;;) {
        now = xTaskGetTickCount();

        if (xQueueReceive(queue_setpoint, &nuevo_setpoint, 0) == pdTRUE)
            setpoint_local = nuevo_setpoint;

        /* portMAX_DELAY: evita saltar el ciclo sin escribir Q0.x si el mutex no se obtiene a tiempo */
        xSemaphoreTake(g_control_mutex, portMAX_DELAY);

        int sp = setpoint_local;
        float temp = g_control_estado.temp_c;
        int i0_pin = gpio_get_level(I0_0);
        bool i0_high = PRESION_I0_ACTIVO_NIVEL_ALTO ? (i0_pin == 1) : (i0_pin == 0);

        if (i0_high) {
            g_control_estado.alarm_active = true;
            g_control_estado.alarm_low_since_ticks = 0;
        } else if (g_control_estado.alarm_active) {
            /* Salir de alarma solo tras ALARMA_TIEMPO_MS con I0.0 en bajo */
            if (g_control_estado.alarm_low_since_ticks == 0)
                g_control_estado.alarm_low_since_ticks = now;
            if ((now - g_control_estado.alarm_low_since_ticks) >= pdMS_TO_TICKS(ALARMA_TIEMPO_MS))
                g_control_estado.alarm_active = false;
        } else {
            g_control_estado.alarm_low_since_ticks = 0;
        }
        bool alarm = g_control_estado.alarm_active;
        g_control_estado.setpoint = setpoint_local;  /* Para el OLED y la web */

        if (alarm) {
            retencion_calefactor = false;
        } else {
            float sp_f = (float)sp;
            if (temp < sp_f - HYSTERESIS_TEMP_C)
                retencion_calefactor = true;
            else if (temp > sp_f + HYSTERESIS_TEMP_C)
                retencion_calefactor = false;
        }
        g_control_estado.calefactor_on = retencion_calefactor;
        g_control_estado.sensor_sobrepresion = i0_high;

        xSemaphoreGive(g_control_mutex);

        if (alarm) {
            gpio_set_level(Q0_0, 0);
            gpio_set_level(Q0_1, 1);
            /* Buzzer en toggle cada BUZZER_TOGGLE_MS mientras dure la alarma */
            if (buzz_last_toggle == 0) {
                buzz_last_toggle = now;
                buzz_level = 1;
                gpio_set_level(BUZZER, 1);
            } else if ((now - buzz_last_toggle) >= pdMS_TO_TICKS(BUZZER_TOGGLE_MS)) {
                buzz_last_toggle = now;
                buzz_level = !buzz_level;
                gpio_set_level(BUZZER, buzz_level ? 1 : 0);
            }
        } else {
            buzz_last_toggle = 0;
            gpio_set_level(BUZZER, 0);
            gpio_set_level(Q0_1, 0);
            gpio_set_level(Q0_0, retencion_calefactor ? 1 : 0);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* OLED cada 500 ms: T y SP o mensaje de sobrepresión */
static void task_display(void *pvParameters)
{
    char line[24];
    float t;
    int sp;
    bool alarm;

    for (;;) {
        if (xSemaphoreTake(g_control_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        t = g_control_estado.temp_c;
        sp = g_control_estado.setpoint;
        alarm = g_control_estado.alarm_active;
        xSemaphoreGive(g_control_mutex);

        if (alarm) {
            oled_show("SOBREPRESION");
        } else {
            snprintf(line, sizeof(line), "T=%.0f SP=%d", t, sp);
            oled_show(line);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    board_esp32_init();
    oled_display_init();

    g_control_mutex = xSemaphoreCreateMutex();
    queue_setpoint = xQueueCreate(SETPOINT_QUEUE_LEN, sizeof(int));
    if (g_control_mutex == NULL || queue_setpoint == NULL)
        return;

    uart_config_t uart_cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_SETPOINT_NUM, &uart_cfg);
    uart_set_pin(UART_SETPOINT_NUM, UART_SETPOINT_TX, UART_SETPOINT_RX, -1, -1);
    /* RX para setpoint desde USB; buffer TX mínimo (IDF puede fallar si TX buffer inválido) */
    esp_err_t uerr = uart_driver_install(UART_SETPOINT_NUM, UART_BUF, 0, 0, NULL, 0);
    if (uerr == ESP_OK)
        uart_flush_input(UART_SETPOINT_NUM);

    /* Tarea UART solo si el driver instaló bien */
    xTaskCreate(task_adc,     "adc",  STACK_TASK, NULL, PRIO_ADC,     NULL);
    if (uerr == ESP_OK)
        xTaskCreate(task_uart, "uart", STACK_TASK, NULL, PRIO_UART, NULL);
    xTaskCreate(task_control, "ctrl", STACK_TASK, NULL, PRIO_CONTROL, NULL);
    xTaskCreate(task_display, "disp", STACK_TASK, NULL, PRIO_DISPLAY, NULL);

    web_ui_start();
}
