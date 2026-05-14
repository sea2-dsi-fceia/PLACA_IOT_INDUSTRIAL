/**
 * @file web_ui.c
 * @brief WiFi punto de acceso (red creada por el ESP32) + HTTP en http://192.168.4.1
 */

#include "web_ui.h"
#include "control_state.h"
#include "board_esp32.h"

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

static const char *TAG = "web_ui";

/* Nombre y clave de la red WiFi que crea el ESP32 (conectate con el celular/PC a esta red). */
#define WEB_AP_SSID      "Control_Temperatura"
#define WEB_AP_PASSWORD  "ESP32_point"

static esp_err_t send_index(httpd_req_t *req)
{
    static const char html[] =
        "<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Control de Temperatura</title>"
        "<style>"
        "body{font-family:system-ui,Segoe UI,sans-serif;max-width:520px;margin:1rem auto;padding:0 1rem;background:#1a1d23;color:#e8eaed;}"
        "h1{font-size:1.15rem;font-weight:600;margin:0 0 .4rem 0;text-align:center;line-height:1.3;}"
        ".subtitle{font-size:.72rem;opacity:.78;text-align:center;margin:0 .2rem .35rem;line-height:1.35;}"
        ".student{font-size:.82rem;opacity:.85;text-align:center;margin:0 0 1rem 0;}"
        "h2{font-size:1.2rem;font-weight:600;margin:.25rem 0 .85rem 0;text-align:center;}"
        ".row{display:flex;justify-content:space-between;align-items:center;padding:.65rem .85rem;margin:.4rem 0;"
        "background:#2d323c;border-radius:10px;border:1px solid #3d4450;}"
        ".lbl{opacity:.85;font-size:.9rem;}"
        ".val{font-weight:700;font-size:1.05rem;}"
        ".on{color:#7ee787}.off{color:#8b949e}.si{color:#f0883e}.no{color:#7ee787}"
        "</style></head><body>"
        "<h1>Sistemas Embebidos Avanzados II</h1>"
        "<p class=\"subtitle\">Departamento de Sistemas e Informática - EIE - FCEIA - UNR</p>"
        "<p class=\"student\">Estudiante Nicolás Ramos</p>"
        "<h2>Control de Temperatura</h2>"
        "<div class=\"row\"><span class=\"lbl\">Temperatura</span><span class=\"val\" id=\"temp\">--</span></div>"
        "<div class=\"row\"><span class=\"lbl\">Setpoint</span><span class=\"val\" id=\"sp\">--</span></div>"
        "<div class=\"row\"><span class=\"lbl\">Calefactor (Q0.0)</span><span class=\"val\" id=\"heat\">--</span></div>"
        "<div class=\"row\"><span class=\"lbl\">Sensor sobrepresión (I0.0)</span><span class=\"val\" id=\"pres\">--</span></div>"
        "<div class=\"row\"><span class=\"lbl\">Alarma activa</span><span class=\"val\" id=\"alm\">--</span></div>"
        "<p style=\"opacity:.6;font-size:.8rem;margin-top:1.2rem;\">Actualización cada 500 ms</p>"
        "<script>"
        "async function u(){"
        "try{const r=await fetch('/api/status');const j=await r.json();"
        "document.getElementById('temp').textContent=j.temp_c.toFixed(1)+' °C';"
        "document.getElementById('sp').textContent=j.setpoint+' °C';"
        "const h=document.getElementById('heat');h.textContent=j.calefactor_on?'Encendido':'Apagado';"
        "h.className='val '+(j.calefactor_on?'on':'off');"
        "const p=document.getElementById('pres');p.textContent=j.presion_activa?'Activo (alto)':'No activo';"
        "p.className='val '+(j.presion_activa?'si':'no');"
        "const a=document.getElementById('alm');a.textContent=j.alarma_activa?'Sí':'No';"
        "a.className='val '+(j.alarma_activa?'si':'no');"
        "}catch(e){}}"
        "setInterval(u,500);u();"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_api(httpd_req_t *req)
{
    control_estado_t st;
    if (g_control_mutex == NULL) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }
    if (xSemaphoreTake(g_control_mutex, pdMS_TO_TICKS(80)) != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }
    st = g_control_estado;
    xSemaphoreGive(g_control_mutex);

    bool calef = st.calefactor_on;
    bool presion = st.sensor_sobrepresion;

    char buf[320];
    int n = snprintf(buf, sizeof(buf),
                     "{\"temp_c\":%.2f,\"setpoint\":%d,\"calefactor_on\":%s,\"presion_activa\":%s,\"alarma_activa\":%s}",
                     (double)st.temp_c,
                     st.setpoint,
                     calef ? "true" : "false",
                     presion ? "true" : "false",
                     st.alarm_active ? "true" : "false");
    if (n <= 0 || n >= (int)sizeof(buf)) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, (size_t)n);
}

static httpd_handle_t s_server = NULL;

static httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = send_index,
    .user_ctx = NULL,
};

static httpd_uri_t uri_api = {
    .uri = "/api/status",
    .method = HTTP_GET,
    .handler = send_api,
    .user_ctx = NULL,
};

static void wifi_init_ap(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    esp_err_t e = esp_netif_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(e);
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
        ESP_ERROR_CHECK(e);

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.ap.ssid, WEB_AP_SSID, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = (uint8_t)strlen(WEB_AP_SSID);
    strncpy((char *)wifi_config.ap.password, WEB_AP_PASSWORD, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP: SSID=%s  pass=%s  → http://192.168.4.1", WEB_AP_SSID, WEB_AP_PASSWORD);
}

static void http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar el servidor HTTP");
        return;
    }
    httpd_register_uri_handler(s_server, &uri_root);
    httpd_register_uri_handler(s_server, &uri_api);
    ESP_LOGI(TAG, "HTTP en http://192.168.4.1");
}

static void web_ui_task(void *arg)
{
    wifi_init_ap();
    vTaskDelay(pdMS_TO_TICKS(500));
    http_start();
    vTaskDelete(NULL);
}

void web_ui_start(void)
{
    /* Pila amplia: init WiFi + TLS/stack interno del servidor */
    xTaskCreate(web_ui_task, "web_ui", 8192, NULL, 3, NULL);
}
