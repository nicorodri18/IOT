// Camilo Otalora, Juan Diego Martinez, Nicolas Rodriguez
// VERSIÓN FINAL: MOVIMIENTO CONTINUO + BLOQUEO SOLO ADELANTE + TLS/SSL MQTT
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// =================== CONFIG PANTALLA OLED ===========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =================== CONFIG WIFI ====================
#define WIFI_SSID "motog72"
#define WIFI_PASSWORD "otalora27"

// =================== CONFIG MQTT CON TLS ====================
#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 8883  // Puerto TLS (antes era 1883)
#define MQTT_TOPIC_MOVEMENT "esp32/car/instructions"
#define MQTT_TOPIC_SENSOR "esp32/car/distance"
#define MQTT_TOPIC_ODOMETRY "esp32/car/odometry"
#define MQTT_TOPIC_EMERGENCY "esp32/car/emergency"
#define MQTT_CLIENT_ID "ESP32_CarClient_TLS"

// =================== CERTIFICADO ROOT CA ====================
// Certificado para conexión segura con HiveMQ
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGXPI6T53iH
TfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUsN+gDS63pYaACbvXy8MW
y7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vvo/ufQJVtMVT8QtPHRh8
jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU5MsI+yMRQ+hDKXJioal
dXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpyrqXRfboQnoZsG4q5WTP
468SQvvG5
-----END CERTIFICATE-----)EOF";

// =================== PINES MOTORES =================
#define IN1 25
#define IN2 26
#define IN3 14
#define IN4 27
#define ENA_PIN 32
#define ENB_PIN 33
const int freq = 30000;
const int pwmChannelA = 0;
const int pwmChannelB = 1;
const int resolution = 8;

// =================== SENSOR HC-SR04 ==================
#define TRIG_PIN 5
#define ECHO_PIN 18

// =================== ENCODER ========================
#define ENCODER_PIN 19

// =================== CONFIGURACIÓN ==================
#define SAFETY_DISTANCE 20.0 // cm → bloquea solo ADELANTE
#define SENSOR_READ_INTERVAL 250
#define VELOCITY_CALC_INTERVAL 200
#define DISPLAY_UPDATE_INTERVAL 300
#define WHEEL_DIAMETER 6.5
#define PULSES_PER_REVOLUTION 20

// =================== VARIABLES GLOBALES ==============
WiFiClientSecure secureClient;  // Cliente seguro en lugar de WiFiClient
PubSubClient mqttClient(secureClient);  // MQTT sobre TLS
WebServer server(80);

int currentSpeed = 240;
String currentDirection = "STOP";
bool obstacleAhead = false; // solo bloquea adelante
volatile long encoderCount = 0;
volatile long encoderCountTemp = 0;
unsigned long lastVelocityCalc = 0;
float distancePerPulse = 0.0;
float currentVelocity = 0.0;
float currentVelocityKmh = 0.0;
float lastUltrasonicDistance = 0.0;
unsigned long lastDisplayUpdate = 0;

// =================== HTML (IGUAL, SOLO AÑADE INDICADOR TLS) ==================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Carro ESP32</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@500;700&display=swap');
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Orbitron', 'Segoe UI', sans-serif;
            background: linear-gradient(135deg, #0f0f23 0%, #1a1a3e 50%, #2d1b69 100%);
            color: white;
            min-height: 100vh;
            padding: 15px;
            touch-action: manipulation;
        }
        .container {
            background: rgba(20, 20, 40, 0.85);
            backdrop-filter: blur(10px);
            border-radius: 25px;
            padding: 25px;
            box-shadow: 0 15px 60px rgba(0,0,0,0.8);
            max-width: 500px;
            margin: 0 auto;
            border: 1px solid #5e43f3;
            box-shadow: 0 0 30px rgba(90, 67, 243, 0.5);
        }
        h1 {
            text-align: center;
            font-size: 28px;
            margin-bottom: 10px;
            color: #a78bff;
            text-shadow: 0 0 15px #5e43f3;
        }
        .status {
            text-align: center;
            padding: 15px;
            border-radius: 15px;
            font-weight: bold;
            font-size: 18px;
            margin-bottom: 20px;
            transition: all 0.4s;
        }
        .connected { background: #00ff9d; color: #000; }
        .warning { background: #ff2d55; color: white; animation: blink 1s infinite; }
        @keyframes blink { 0%,100% { opacity: 1; } 50% { opacity: 0.6; } }

        .speed-control { margin: 25px 0; text-align: center; }
        .speed-control label { font-size: 18px; display: block; margin-bottom: 12px; color: #a0d8ff; }
        input[type="range"] { width: 100%; height: 15px; border-radius: 10px; background: #33335c; outline: none; -webkit-appearance: none; }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none; width: 32px; height: 32px; border-radius: 50%;
            background: #5e43f3; cursor: pointer; box-shadow: 0 0 20px #5e43f3;
        }
        .speed-value { font-size: 32px; margin-top: 12px; color: #5effff; text-shadow: 0 0 10px #5effff; }

        .controls {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            grid-template-rows: 1fr 1fr 1fr;
            gap: 15px;
            margin: 30px 0;
        }
        .btn {
            border: none; border-radius: 20px; font-size: 36px; font-weight: bold;
            color: white; padding: 20px; min-height: 90px;
            display: flex; align-items: center; justify-content: center;
            box-shadow: 0 8px 20px rgba(0,0,0,0.4); transition: all 0.2s;
        }
        .btn:active { transform: scale(0.92); box-shadow: 0 4px 10px rgba(0,0,0,0.6); }
        #btnForward  { background: linear-gradient(135deg, #00dbde, #0066ff); grid-column: 2; grid-row: 1; }
        #btnLeft     { background: linear-gradient(135deg, #ff7e5f, #feb47b); grid-column: 1; grid-row: 2; }
        #btnStop     { background: linear-gradient(135deg, #ff4757, #c44569); grid-column: 2; grid-row: 2; font-size: 28px; }
        #btnRight    { background: linear-gradient(135deg, #11998e, #38ef7d); grid-column: 3; grid-row: 2; }
        #btnBack     { background: linear-gradient(135deg, #8360c3, #2ebf91); grid-column: 2; grid-row: 3; }

        .info-panel { background: rgba(30,30,60,0.7); padding: 18px; border-radius: 15px; margin-top: 25px; border: 1px solid #44447a; }
        .info-item { display: flex; justify-content: space-between; padding: 10px 0; font-size: 18px; border-bottom: 1px solid #55557a; }
        .info-item:last-child { border: none; }
        .info-label { color: #a0a0ff; }
        .info-value { color: #fffa; font-weight: bold; }

        .log { margin-top: 20px; background: #0a0a1a; padding: 15px; border-radius: 12px; height: 100px; overflow-y: auto;
               font-family: 'Courier New', monospace; font-size: 12px; color: #00ff9d; border: 1px solid #333360; }
        
        .security-badge {
            text-align: center;
            margin-top: 10px;
            font-size: 11px;
            color: #00ff9d;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚗 Carro ESP32</h1>
        <div id="status" class="status connected">PARADO</div>

        <div class="speed-control">
            <label>Velocidad PWM:</label>
            <input type="range" id="speedSlider" min="220" max="255" value="240" step="5">
            <div class="speed-value" id="speedValue">240 / 255</div>
        </div>

        <div class="controls">
            <button class="btn" id="btnForward"  onclick="move('f')">↑</button>
            <button class="btn" id="btnLeft"     onclick="move('l')">←</button>
            <button class="btn" id="btnStop"     onclick="move('x')">STOP</button>
            <button class="btn" id="btnRight"    onclick="move('r')">→</button>
            <button class="btn" id="btnBack"     onclick="move('b')">↓</button>
        </div>

        <div class="info-panel">
            <div class="info-item"><span class="info-label">Distancia:</span> <span class="info-value" id="distance">-- cm</span></div>
            <div class="info-item"><span class="info-label">Velocidad:</span> <span class="info-value" id="velocity">-- km/h</span></div>
            <div class="info-item"><span class="info-label">Odometría:</span> <span class="info-value" id="odometry">-- m</span></div>
        </div>

        <div class="log" id="log"><div>> Sistema iniciado con TLS</div></div>
        <div class="security-badge">🔒 Conexión MQTT cifrada (TLS/SSL)</div>
    </div>

    <script>
        let currentSpeed = 240;
        document.getElementById('speedSlider').addEventListener('input', e => {
            currentSpeed = parseInt(e.target.value);
            document.getElementById('speedValue').innerText = currentSpeed + ' / 255';
        });

        function addLog(msg) {
            const log = document.getElementById('log');
            const div = document.createElement('div');
            div.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
            log.appendChild(div);
            log.scrollTop = log.scrollHeight;
        }

        const nombres = {f:'ADELANTE', b:'ATRÁS', l:'IZQUIERDA', r:'DERECHA', x:'PARADO'};
        async function move(dir) {
            await fetch(`/move?dir=${dir}&speed=${currentSpeed}`);
            const texto = nombres[dir] || 'Moviendo';
            document.getElementById('status').textContent = texto;
            document.getElementById('status').className = 'status connected';
            addLog(`Comando: ${texto}`);
        }

        setInterval(async () => {
            try {
                const r = await fetch('/distance');
                const d = await r.json();
                document.getElementById('distance').textContent = d.distance.toFixed(1) + ' cm';
                if (d.obstacle) {
                    document.getElementById('status').textContent = 'OBSTÁCULO!';
                    document.getElementById('status').className = 'status warning';
                }
            } catch(e) {}

            try {
                const o = await fetch('/odometry');
                const od = await o.json();
                document.getElementById('velocity').textContent = od.velocity_kmh.toFixed(2) + ' km/h';
                document.getElementById('odometry').textContent = od.distance_meters.toFixed(2) + ' m';
            } catch(e) {}
        }, 400);
    </script>
</body>
</html>
)rawliteral";

// =================== PWM & MOTORES (100% IGUAL) ===================
void setupPWM() {
  pinMode(ENA_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  ledcAttach(ENA_PIN, freq, resolution);
  ledcAttach(ENB_PIN, freq, resolution);
  ledcWrite(ENA_PIN, 0);
  ledcWrite(ENB_PIN, 0);
}

void setMotorSpeed(int speed) {
  speed = constrain(speed, 0, 255);
  currentSpeed = speed;
  ledcWrite(ENA_PIN, speed);
  ledcWrite(ENB_PIN, speed);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  setMotorSpeed(0);
  currentDirection = "STOP";
}

void adelante() {
  if (obstacleAhead) {
    Serial.println("[!] AVANCE BLOQUEADO - Obstáculo adelante");
    stopMotors();
    currentDirection = "BLOQUEADO";
    return;
  }
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  setMotorSpeed(currentSpeed);
  currentDirection = "ADELANTE";
}

void atras() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  setMotorSpeed(currentSpeed);
  currentDirection = "ATRAS";
}

void izquierda() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  setMotorSpeed(currentSpeed);
  currentDirection = "IZQUIERDA";
}

void derecha() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  setMotorSpeed(currentSpeed);
  currentDirection = "DERECHA";
}

void ejecutarMovimiento(char dir, int speed) {
  setMotorSpeed(speed);
  switch(dir) {
    case 'f': adelante(); break;
    case 'b': atras(); break;
    case 'l': izquierda(); break;
    case 'r': derecha(); break;
    case 'x': stopMotors(); break;
    default: stopMotors(); break;
  }
}

// =================== SENSOR (100% IGUAL) ===================
float readUltrasonicSensor() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return -1;
  return dur * 0.0343 / 2.0;
}

void checkProximitySafety() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < SENSOR_READ_INTERVAL) return;
  lastRead = millis();
  
  float dist = readUltrasonicSensor();
  if (dist > 0 && dist < 400) lastUltrasonicDistance = dist;
  
  bool obstaculo = (lastUltrasonicDistance > 0 && lastUltrasonicDistance < SAFETY_DISTANCE);
  if (obstaculo != obstacleAhead) {
    obstacleAhead = obstaculo;
    if (obstacleAhead && currentDirection == "ADELANTE") {
      stopMotors();
      currentDirection = "BLOQUEADO";
    }
  }
  
  if (mqttClient.connected()) {
    String msg = "{\"distance\":" + String(lastUltrasonicDistance,1) +
                 ",\"obstacle\":" + (obstacleAhead?"true":"false") + "}";
    mqttClient.publish(MQTT_TOPIC_SENSOR, msg.c_str());
  }
}

// =================== ENCODER & ODOMETRÍA (100% IGUAL) ===================
void IRAM_ATTR encoderISR() { 
  encoderCount++; 
  encoderCountTemp++; 
}

float calculateDistanceMeters() {
  return (encoderCount * distancePerPulse) / 100.0;
}

void calculateVelocity() {
  static unsigned long last = 0;
  if (millis() - last < VELOCITY_CALC_INTERVAL) return;
  last = millis();
  
  float distCm = encoderCountTemp * distancePerPulse;
  currentVelocity = distCm / (VELOCITY_CALC_INTERVAL/1000.0);
  currentVelocityKmh = currentVelocity * 0.036;
  encoderCountTemp = 0;
}

// =================== DISPLAY OLED (100% IGUAL) ===================
void updateDisplay() {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  lastDisplayUpdate = millis();

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  if (obstacleAhead) {
    display.setTextSize(2);
    display.setCursor(15, 8);
    display.println("OBSTACULO");

    display.setTextSize(2);
    display.setCursor(20, 32);
    display.print(lastUltrasonicDistance, 1);
    display.println(" cm");

    if ((millis() / 500) % 2 == 0) {
      display.fillRoundRect(5, 50, 118, 12, 4, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK);
      display.setTextSize(1);
      display.setCursor(38, 52);
      display.println("DETENER!");
      display.setTextColor(SH110X_WHITE);
    }
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("== CARRO ESP32 ==");

    display.setCursor(0, 12);
    display.print("Dir: ");
    display.println(currentDirection);

    display.setCursor(0, 24);
    display.print("Vel: ");
    display.print(currentVelocityKmh, 1);
    display.println(" km/h");

    display.setCursor(0, 36);
    display.print("Dist: ");
    if (lastUltrasonicDistance > 0) display.print(lastUltrasonicDistance, 1);
    else display.print("---");
    display.println(" cm");

    display.setCursor(0, 48);
    display.print("Odom: ");
    display.print(calculateDistanceMeters(), 2);
    display.println(" m");

    display.setCursor(0, 58);
    display.println(WiFi.localIP().toString());
  }

  display.display();
}

// =================== WEB SERVER (CON API REST v1) ===================
void handleRoot() { 
  server.send_P(200, "text/html", HTML_PAGE); 
}

// *** API REST v1 - ENDPOINTS REQUERIDOS ***

// GET /api/v1/healthcheck
void handleHealthcheck() {
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  bool mqttOk = mqttClient.connected();
  
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"wifi_connected\":" + String(wifiOk ? "true" : "false") + ",";
  json += "\"mqtt_connected\":" + String(mqttOk ? "true" : "false") + ",";
  json += "\"mqtt_tls\":true,";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"current_direction\":\"" + currentDirection + "\",";
  json += "\"obstacle_ahead\":" + String(obstacleAhead ? "true" : "false") + ",";
  json += "\"distance_cm\":" + String(lastUltrasonicDistance, 2);
  json += "}";
  
  server.send(200, "application/json", json);
  Serial.println("API: Healthcheck solicitado");
}

// POST /api/v1/move (también acepta GET para compatibilidad)
void handleMoveAPI() {
  String direction = "";
  int speed = 240;
  
  // Soportar tanto POST (con body) como GET (con query params)
  if (server.method() == HTTP_POST) {
    // Leer parámetros del body (application/x-www-form-urlencoded)
    if (server.hasArg("direction")) {
      direction = server.arg("direction");
    }
    if (server.hasArg("speed")) {
      speed = server.arg("speed").toInt();
    }
  } else {
    // GET con query params
    if (server.hasArg("direction")) {
      direction = server.arg("direction");
    }
    if (server.hasArg("speed")) {
      speed = server.arg("speed").toInt();
    }
  }
  
  // Validar que se envió dirección
  if (direction.length() == 0) {
    String errorJson = "{";
    errorJson += "\"error\":\"Missing required parameter: direction\",";
    errorJson += "\"valid_directions\":[\"forward\",\"backward\",\"left\",\"right\",\"stop\"],";
    errorJson += "\"example\":\"POST /api/v1/move with body: direction=forward&speed=240\"";
    errorJson += "}";
    server.send(400, "application/json", errorJson);
    return;
  }
  
  // Mapear nombres legibles a comandos de motor
  char dir = 'x';
  if (direction == "forward" || direction == "f") dir = 'f';
  else if (direction == "backward" || direction == "b") dir = 'b';
  else if (direction == "left" || direction == "l") dir = 'l';
  else if (direction == "right" || direction == "r") dir = 'r';
  else if (direction == "stop" || direction == "x") dir = 'x';
  else {
    String errorJson = "{";
    errorJson += "\"error\":\"Invalid direction: " + direction + "\",";
    errorJson += "\"valid_directions\":[\"forward\",\"backward\",\"left\",\"right\",\"stop\"]";
    errorJson += "}";
    server.send(400, "application/json", errorJson);
    return;
  }
  
  // Validar velocidad
  speed = constrain(speed, 220, 255);
  
  // Ejecutar movimiento
  ejecutarMovimiento(dir, speed);
  
  // Respuesta exitosa
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"direction\":\"" + direction + "\",";
  json += "\"speed\":" + String(speed) + ",";
  json += "\"current_state\":\"" + currentDirection + "\",";
  json += "\"obstacle_ahead\":" + String(obstacleAhead ? "true" : "false") + ",";
  json += "\"timestamp\":" + String(millis());
  json += "}";
  
  server.send(200, "application/json", json);
  Serial.println("API: Movimiento ejecutado - " + direction + " @ " + String(speed));
}

// *** ENDPOINTS LEGACY (para compatibilidad con interfaz web) ***

void handleMove() {
  if (!server.hasArg("dir")) {
    server.send(400, "text/plain", "Falta dir");
    return;
  }
  char dir = server.arg("dir")[0];
  int speed = 240;
  if (server.hasArg("speed")) speed = server.arg("speed").toInt();
  speed = constrain(speed, 220, 255);
  ejecutarMovimiento(dir, speed);
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleGetDistance() {
  String json = "{\"distance\":" + String(lastUltrasonicDistance,1) +
                ",\"obstacle\":" + String(obstacleAhead?"true":"false") + "}";
  server.send(200, "application/json", json);
}

void handleGetOdometry() {
  float distM = calculateDistanceMeters();
  String json = "{\"velocity_kmh\":" + String(currentVelocityKmh,2) +
                ",\"distance_meters\":" + String(distM,2) + "}";
  server.send(200, "application/json", json);
}

// =================== MQTT CON TLS (NUEVA FUNCIÓN) ===================
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando MQTT TLS...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println(" ✓ Conectado!");
      Serial.println("🔒 Conexión MQTT cifrada activa");
    } else {
      Serial.print(" ✗ Error rc=");
      Serial.print(mqttClient.state());
      Serial.println(" reintentando en 2s...");
      delay(2000);
    }
  }
}

// =================== SETUP (CON TLS) ===================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("   ESP32 CAR - CON SEGURIDAD TLS");
  Serial.println("========================================\n");
  
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA_PIN, OUTPUT); pinMode(ENB_PIN, OUTPUT);
  setupPWM();
  stopMotors();
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(ENCODER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN), encoderISR, RISING);
  
  float circ = PI * WHEEL_DIAMETER;
  distancePerPulse = circ / PULSES_PER_REVOLUTION;
  
  Wire.begin(21,22);
  display.begin(SCREEN_ADDRESS, true);
  display.clearDisplay(); 
  display.setTextSize(2); 
  display.setTextColor(SH110X_WHITE);
  display.setCursor(20,20); 
  display.println("CARRO"); 
  display.display();
  delay(1500);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\n✓ WiFi conectado");
  Serial.println("IP: " + WiFi.localIP().toString());
  
  // *** CONFIGURACIÓN TLS ***
  // Opción 1: Sin validar certificado (más fácil, menos seguro)
  secureClient.setInsecure();
  
  // Opción 2: Con validación de certificado (más seguro)
  // secureClient.setCACert(ca_cert);
  
  Serial.println("\n🔒 Configurando MQTT con TLS...");
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  reconnectMQTT();
  
  // *** API REST v1 - ENDPOINTS REQUERIDOS ***
  server.on("/api/v1/healthcheck", HTTP_GET, handleHealthcheck);
  server.on("/api/v1/move", HTTP_POST, handleMoveAPI);
  server.on("/api/v1/move", HTTP_GET, handleMoveAPI);  // GET también para flexibilidad
  
  // *** ENDPOINTS LEGACY (para interfaz web) ***
  server.on("/", handleRoot);
  server.on("/move", handleMove);
  server.on("/distance", handleGetDistance);
  server.on("/odometry", handleGetOdometry);
  
  server.begin();
  
  Serial.println("\n========================================");
  Serial.println("  SISTEMA LISTO CON TLS + API REST");
  Serial.println("========================================");
  Serial.println("Web: http://" + WiFi.localIP().toString());
  Serial.println("MQTT: TLS activo en puerto 8883");
  Serial.println("\nAPI REST v1 Endpoints:");
  Serial.println("  GET  /api/v1/healthcheck");
  Serial.println("  POST /api/v1/move");
  Serial.println("  GET  /api/v1/move (también soportado)");
  Serial.println("\nLegacy Endpoints:");
  Serial.println("  GET /move?dir=<f|b|l|r|x>&speed=<220-255>");
  Serial.println("  GET /distance");
  Serial.println("  GET /odometry");
  Serial.println("========================================\n");
}

// =================== LOOP (100% IGUAL) ===================
void loop() {
  server.handleClient();
  checkProximitySafety();
  calculateVelocity();
  updateDisplay();
  
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}