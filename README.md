# IOT

# ESP32 Car Control – API REST + MQTT + Sensor Ultrasónico

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
- Control de motores a través de puente H (L298N o similar).
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
