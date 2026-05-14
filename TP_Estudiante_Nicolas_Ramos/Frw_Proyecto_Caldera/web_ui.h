/**
 * @file web_ui.h
 * @brief Página web "Control de Temperatura" (WiFi AP + HTTP).
 */

#ifndef WEB_UI_H
#define WEB_UI_H

/** WiFi punto de acceso del ESP32 + HTTP (http://192.168.4.1). Tras crear g_control_mutex y board_esp32_init(). */
void web_ui_start(void);

#endif
