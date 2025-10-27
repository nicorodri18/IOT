# IOT

# ESP32 Car Control – API REST + MQTT + Sensor Ultrasónico

## Nicolas Rodriguez - Camilo Otalora - Juan Diego Martinez 

## Descripción General
Este proyecto implementa el control remoto de un vehículo con un ESP32, usando una API REST y comunicación MQTT.  
El sistema permite:
- Recibir instrucciones de movimiento (adelante, atrás, izquierda, derecha, detener).
- Publicar lecturas periódicas del sensor ultrasónico HC-SR04.
- Operar en modo físico o simulado.
- Exponer endpoints HTTP para pruebas o integración.

## Características Principales
- Conexión WiFi configurable.
- Publicación y suscripción MQTT.
- Control de motores a través de puente H .
- Lectura física o simulada del sensor ultrasónico.
- Estructura modular con variables definidas mediante preprocesador.
- API REST sencilla con tres endpoints (`/ping`, `/move`, `/distance`).

## Estructura del Código
- **Variables de preprocesador:** Configuración de WiFi, MQTT, pines de motores, pines del sensor, parámetros del servidor y constantes de seguridad.
- **Funciones de motor:** Controlan el movimiento del vehículo (adelante, atrás, izquierda, derecha, detener).
- **Funciones del sensor:** Lectura o simulación del HC-SR04 y publicación periódica en un tema MQTT.
- **Endpoints REST:**
  - `/ping`: Verifica disponibilidad del servidor.
  - `/move`: Recibe instrucciones de movimiento (`dir`, `speed`, `time`).
  - `/distance`: Retorna la lectura actual del sensor.
- **MQTT:** Conexión y publicación de mensajes en temas distintos:
  - `esp32/car/instructions`: Recibe comandos de movimiento.
  - `esp32/car/distance`: Publica las lecturas del sensor ultrasónico.
  <img width="1280" height="666" alt="image" src="https://github.com/user-attachments/assets/e89a1530-28d3-4aff-b447-f88b4c4121d4" />
  <img width="1280" height="481" alt="image" src="https://github.com/user-attachments/assets/997ef80e-b072-415d-8385-c560b1692425" />
  <img width="1280" height="460" alt="image" src="https://github.com/user-attachments/assets/16acec0e-1659-427d-b624-9a2729b1d427" />

  ## Relación con las instrucciones del laboratorio
- Se implementó una función `readUltrasonicSensor()` que simula o lee el sensor HC-SR04.
- La lectura se ejecuta periódicamente y publica en el tema MQTT `esp32/car/distance`.
- Se definieron variables de preprocesador para pines, WiFi, MQTT y servidor.
- Se podrían trasladar las definiciones a un archivo `.h` para mejor organización.
- El sensor físico debe protegerse con un divisor de voltaje en el pin ECHO.


## Temas MQTT
- **Movimiento:** `esp32/car/instructions`
- **Sensor:** `esp32/car/distance`

## Ejemplo de Publicación del Sensor
```json
{
  "distance": 134.25,
  "unit": "cm",
  "timestamp": 485312
}
