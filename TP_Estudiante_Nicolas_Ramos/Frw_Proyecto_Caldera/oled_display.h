/**
 * @file oled_display.h
 * @brief Plantilla OLED (SH1106 I2C): mostrar texto sin configurar nada.
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

/* Pines y dirección I2C del OLED (SH1106). Si no muestra nada, probar OLED_ADDR 0x3D */
#define OLED_SDA    21
#define OLED_SCL    22
#define OLED_ADDR   0x3C
#define OLED_ANCHO  128
#define OLED_ALTO   64

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa I2C y el display SH1106. Llamar una vez al inicio (o se llama
 * automáticamente en la primera oled_show).
 */
void oled_display_init(void);

/**
 * Muestra la cadena 'texto' en el display (centrada en la página central).
 * Solo caracteres imprimibles ASCII (32..126). No hace falta configurar nada.
 * @param texto  Cadena terminada en '\0' (máx. ~21 caracteres visibles por línea).
 */
void oled_show(const char *texto);

/**
 * Limpia la pantalla (todo a negro).
 */
void oled_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* OLED_DISPLAY_H */
