#include "control_state.h"

control_estado_t g_control_estado = {
    .temp_c = 0.0f,
    .setpoint = 25,
    .alarm_active = false,
    .alarm_low_since_ticks = 0,
    .calefactor_on = false,
    .sensor_sobrepresion = false,
};

SemaphoreHandle_t g_control_mutex = NULL;
