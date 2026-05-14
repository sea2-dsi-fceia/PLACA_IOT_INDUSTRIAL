/**
 * @file control_state.h
 * @brief Estado del control de temperatura compartido entre tareas PLC y el servidor web.
 */

#ifndef CONTROL_STATE_H
#define CONTROL_STATE_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    float      temp_c;
    int        setpoint;
    bool       alarm_active;
    TickType_t alarm_low_since_ticks;
    bool       calefactor_on;       /* calefacción tras histéresis (mismo comando que Q0.0) */
    bool       sensor_sobrepresion; /* misma lectura lógica de I0.0 que usa la alarma (para la web) */
} control_estado_t;

extern control_estado_t g_control_estado;
extern SemaphoreHandle_t g_control_mutex;

#endif
