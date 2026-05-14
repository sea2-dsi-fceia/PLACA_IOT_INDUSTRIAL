# Componente main: fuentes en la raíz del proyecto
idf_component_register(
    SRCS
        "../ESP32_FreeRTOS.c"
        "../control_state.c"
        "../web_ui.c"
        "../board_esp32.c"
        "../oled_display.c"
    INCLUDE_DIRS
        ".."
    REQUIRES esp_adc driver esp_http_server esp_wifi nvs_flash esp_netif esp_event
)
