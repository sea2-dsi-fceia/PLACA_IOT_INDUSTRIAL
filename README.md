## Placa de desarrollo IoT Industrial basada en ESP32
📝 ## Descripción
Este repositorio contiene el diseño y la documentación de una placa de desarrollo robusta diseñada para aplicaciones de Internet de las Cosas Industrial (IIoT), domótica y enseñanza de sistemas embebidos. El corazón del sistema es un microcontrolador ESP32 de Espressif, integrado en un hardware capaz de soportar entornos industriales exigentes.  La placa permite gestionar lógica programada para actuar sobre cargas de potencia (electroválvulas, motores, actuadores) bajo condiciones específicas de trabajo.

🚀 ## Características Técnicas
Alimentación y EntradasVoltaje de alimentación: Soporta niveles industriales entre 12V y 24V CC.  Entradas Digitales (4): Optoaisladas para protección del microcontrolador, con soporte de niveles entre 5V y 24V y LEDs testigos.  Entradas Analógicas (2): Rango de 0V a 10V con alta impedancia. 

Salidas de PotenciaSalidas a Relé (3): Relés de simple inversor optoaislados, con capacidad de manejo en 220V CA o 24V CC.  Salidas de Estado Sólido (5): Implementadas con TRIACs optoaislados (MOC3021 y BT136) para cargas de 220V CA.  Protección: Diodos de freewheeling (1N4007) en antiparalelo con las bobinas de los relés para proteger los transistores driver.

Periféricos e InterfazPantalla OLED: Comandada por protocolo $I^{2}C$ para interfaz HMI local.  Almacenamiento: Memoria EEPROM para datos no volátiles.  Tiempo Real: Reloj calendario (RTC) con batería de backup para registro de eventos.  Indicadores: LED RGB y alarma sonora (buzzer) para monitoreo de estados.

🛠️ Diseño de HardwareEl PCB fue diseñado en KiCad priorizando la robustez y la facilidad de mantenimiento:  Aislamiento Galvánico: Uso de optoacopladores PC817 para separar la lógica de las etapas de potencia.  Seguridad Eléctrica: Muescas de aislación en la placa alrededor de los conectores comunes de los relés para evitar arcos de voltaje en 220V CA.  Dimensionamiento: Pistas trazadas en ambas capas (TOP y BOTTOM) para mejorar el manejo de corriente y evitar sobrecalentamiento.

💻 Desarrollo de SoftwareLa placa es versátil en cuanto a paradigmas de programación:Sistemas Operativos en Tiempo Real (RTOS): Soporte nativo para FreeRTOS y Zephyr, garantizando tiempos de respuesta determinísticos ante eventos críticos.  Estándar Industrial: Compatible con el proyecto OpenPLC, permitiendo programación en lenguaje Ladder bajo la norma IEC 61131-3.  Conectividad IIoT: Implementación de protocolos como Modbus TCP, MQTT para telemetría, WebSockets y Webservers embebidos con interfaces HTML.  

📂 Ejemplo de Aplicación: Control de Caldera
El proyecto incluye un ejemplo integrador de un sistema de control de temperatura con seguridad por presión:  

Sensor: Lectura de temperatura vía LM35 en entrada analógica.  

Seguridad: Detección de alta presión mediante sensor digital que desactiva el calefactor y activa un ventilador de emergencia.  

Telemetría: Monitoreo remoto mediante una webapp alojada en el propio microcontrolador.

📈 Roadmap y Mejoras Futuras
Integración de puerto Ethernet nativo.  

Implementación de buses de campo: RS-485 y CAN BUS.  

Fuente de alimentación conmutada tipo Buck DC-DC para mayor eficiencia
