// ----- Solo declaración e inicialización de entradas y salidas -----
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define ANCHO  128
#define ALTO   64
#define DIR_OLED  0x3C
#define OLED_SDA  21
#define OLED_SCL  22

Adafruit_SH1106G oled(ANCHO, ALTO, &Wire, -1);

#define I0_0  5
#define I0_1  15
#define I0_2  16
#define I0_3  34

#define Q0_0  4
#define Q0_1  17
#define Q0_2  18
#define Q0_3  19
#define Q0_4  23
#define Q0_5  27
#define Q0_6  32
#define Q0_7  33

#define A0_0  36   // AN0.0 (VP)
#define A0_1  35   // AN0.1 (VN)
#define DAC0_0  25
#define DAC0_1  26

// LED RGB (D2 LED_RGBC): común a GND, R=IO14, G=IO13, B=IO12
#define RGB_R  14
#define RGB_G  13
#define RGB_B  12

#define TIEMPO_MS  700
#define SERIAL_ANALOG_MS  500
#define RGB_MS  800

const uint8_t salidas[] = { Q0_0, Q0_1, Q0_2, Q0_3, Q0_4, Q0_5, Q0_6, Q0_7 };
const int N = 8;

int indice = 0;
unsigned long lastStep = 0;
unsigned long lastAnalog = 0;
unsigned long lastRgb = 0;
int rgbEstado = 0;  // 0=R, 1=G, 2=B, 3=Amarillo, 4=Cian, 5=Magenta, 6=Blanco, 7=Apagado

void setup() {
  pinMode(I0_0, INPUT_PULLDOWN);
  pinMode(I0_1, INPUT_PULLDOWN);
  pinMode(I0_2, INPUT_PULLDOWN);
  pinMode(I0_3, INPUT);  // R externa a masa

  pinMode(Q0_0, OUTPUT);
  pinMode(Q0_1, OUTPUT);
  pinMode(Q0_2, OUTPUT);
  pinMode(Q0_3, OUTPUT);
  pinMode(Q0_4, OUTPUT);
  pinMode(Q0_5, OUTPUT);
  pinMode(Q0_6, OUTPUT);
  pinMode(Q0_7, OUTPUT);

  digitalWrite(Q0_0, LOW);
  digitalWrite(Q0_1, LOW);
  digitalWrite(Q0_2, LOW);
  digitalWrite(Q0_3, LOW);
  digitalWrite(Q0_4, LOW);
  digitalWrite(Q0_5, LOW);
  digitalWrite(Q0_6, LOW);
  digitalWrite(Q0_7, LOW);

  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  digitalWrite(RGB_R, LOW);
  digitalWrite(RGB_G, LOW);
  digitalWrite(RGB_B, LOW);

  Serial.begin(115200);
  // DAC: 1.2V -> 1.2/3.3*255 ≈ 93,  3V -> 3.0/3.3*255 ≈ 232
  dacWrite(DAC0_0, (uint8_t)(255.0 * 1.2 / 3.3));
  dacWrite(DAC0_1, (uint8_t)(255.0 * 3.0 / 3.3));

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);
  delay(200);
  if (oled.begin(DIR_OLED, true) || oled.begin(0x3D, true)) {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SH110X_WHITE);
    oled.setCursor(20, 24);
    oled.print("hola man");
    oled.display();
  }
}

void loop() {
  // Con cualquier I0.x en tensión (pin lee HIGH) se hace toggle Q0.0 -> Q0.7 cada 700 ms
  bool activo = (digitalRead(I0_0) == HIGH || digitalRead(I0_1) == HIGH ||
                digitalRead(I0_2) == HIGH || digitalRead(I0_3) == HIGH);

  if (activo) {
    if (lastStep == 0) {
      for (int i = 0; i < N; i++)
        digitalWrite(salidas[i], LOW);
      indice = 0;
      digitalWrite(salidas[0], HIGH);
      lastStep = millis();
    }
    if (millis() - lastStep >= TIEMPO_MS) {
      digitalWrite(salidas[indice], LOW);
      indice = (indice + 1) % N;
      digitalWrite(salidas[indice], HIGH);
      lastStep = millis();
    }
  } else {
    for (int i = 0; i < N; i++)
      digitalWrite(salidas[i], LOW);
    indice = 0;
    lastStep = 0;
  }

  if (millis() - lastAnalog >= SERIAL_ANALOG_MS) {
    lastAnalog = millis();
    int v0 = analogRead(A0_0);
    int v1 = analogRead(A0_1);
    Serial.print("AN0.0=");
    Serial.print(v0);
    Serial.print("  AN0.1=");
    Serial.println(v1);
  }

  // LED RGB: ciclo R -> G -> B -> Amarillo -> Cian -> Magenta -> Blanco -> Apagado
  if (millis() - lastRgb >= RGB_MS) {
    lastRgb = millis();
    digitalWrite(RGB_R, (rgbEstado == 0 || rgbEstado == 3 || rgbEstado == 5 || rgbEstado == 6) ? HIGH : LOW);
    digitalWrite(RGB_G, (rgbEstado == 1 || rgbEstado == 3 || rgbEstado == 4 || rgbEstado == 6) ? HIGH : LOW);
    digitalWrite(RGB_B, (rgbEstado == 2 || rgbEstado == 4 || rgbEstado == 5 || rgbEstado == 6) ? HIGH : LOW);
    rgbEstado = (rgbEstado + 1) % 8;
  }
}

