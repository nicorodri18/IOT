// Camilo Otalora, Juan Diego Martinez, Nicolas Rodriguez
// ===============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// =================== CONFIG WIFI ====================
#define WIFI_SSID "motog72"
#define WIFI_PASSWORD "otalora27"

// =================== CONFIG MQTT ====================
#define MQTT_SERVER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_TOPIC_MOVEMENT "esp32/car/instructions"
#define MQTT_TOPIC_SENSOR "esp32/car/distance"
#define MQTT_CLIENT_ID "ESP32_CarClient"

// =================== PINES MOTORES (PUENTE H) ========
#define ENA 32   // Enable motor A (izquierdo)
#define IN1 25   // Input 1 motor A
#define IN2 26   // Input 2 motor A
#define ENB 33   // Enable motor B (derecho)
#define IN3 14   // Input 3 motor B
#define IN4 27   // Input 4 motor B

// =================== PINES SENSOR HC-SR04 ===========
#define TRIG_PIN 5
#define ECHO_PIN 18

// =================== CONFIG SENSOR ==================
#define SENSOR_READ_INTERVAL 500  // Intervalo de lectura en ms (0.5 segundos)
#define MAX_DISTANCE 400.0        // Distancia maxima del sensor en cm
#define SENSOR_SIMULADO false     // true = simulado, false = sensor fisico

// =================== CONFIG SERVIDOR ================
#define WEB_SERVER_PORT 80
#define SERIAL_BAUD_RATE 115200

// =================== LIMITES DE SEGURIDAD ===========
#define MAX_MOTOR_DURATION 5000   // Duracion maxima de movimiento en ms

// =================== OBJETOS GLOBALES ================
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WebServer server(WEB_SERVER_PORT);

// =================== VARIABLES GLOBALES ==============
unsigned long requestCount = 0;

// Variables para manejo no bloqueante de motores
unsigned long motorStartTime = 0;
int motorDuration = 0;
bool motorsRunning = false;

// Variables para lectura periodica del sensor
unsigned long lastSensorRead = 0;

// =================== FUNCIONES SENSOR HC-SR04 ========

float readUltrasonicSensor() {
  if (SENSOR_SIMULADO) {
    // Genera un numero aleatorio entre 10 y 200 cm
    float simulated = random(10, 200) + random(0, 99) / 100.0;
    return simulated;
  } else {
    // Sensor fisico HC-SR04
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout de 30ms
    
    // Calcular distancia en centimetros
    float distance = (duration * 0.0343) / 2.0;
    
    // Validar rango
    if (distance == 0 || distance > MAX_DISTANCE) {
      return -1.0; // Indica lectura invalida
    }
    
    return distance;
  }
}

void publishSensorData() {
  if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = millis();
    
    float distance = readUltrasonicSensor();
    
    if (distance > 0) {
      // Publicar a MQTT
      if (mqttClient.connected()) {
        String msg = "{\"distance\":" + String(distance, 2) + 
                     ",\"unit\":\"cm\",\"timestamp\":" + String(millis()) + "}";
        mqttClient.publish(MQTT_TOPIC_SENSOR, msg.c_str());
        
        Serial.printf("Sensor: %.2f cm\n", distance);
      }
    } else {
      Serial.println("Sensor: Lectura fuera de rango");
    }
  }
}

// =================== FUNCIONES MOTORES ===============

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  motorsRunning = false;
  Serial.println("MOTORES DETENIDOS");
}

void adelante(int speed) {
  analogWrite(ENA, speed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENB, speed);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void atras(int speed) {
  analogWrite(ENA, speed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENB, speed);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void izquierda(int speed) {
  analogWrite(ENA, speed);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENB, speed);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void derecha(int speed) {
  analogWrite(ENA, speed);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENB, speed);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void startMotors(char dir, int speed, int duration) {
  stopMotors();

  String dirName;
  switch (dir) {
    case 'f': 
      adelante(speed);
      dirName = "ADELANTE";
      break;
    case 'b': 
      atras(speed);
      dirName = "ATRAS";
      break;
    case 'l': 
      izquierda(speed);
      dirName = "IZQUIERDA";
      break;
    case 'r': 
      derecha(speed);
      dirName = "DERECHA";
      break;
    case 'x':
      stopMotors();
      return;
    default: 
      stopMotors();
      return;
  }

  motorsRunning = true;
  motorStartTime = millis();
  motorDuration = duration;
  
  Serial.printf("INICIANDO MOVIMIENTO: %s | Velocidad: %d | Duracion: %dms\n", 
                dirName.c_str(), speed, duration);
}

void checkMotors() {
  if (motorsRunning && (millis() - motorStartTime >= motorDuration)) {
    stopMotors();
    Serial.println("Movimiento completado");
  }
}

// =================== ENDPOINTS =======================

void handlePing() {
  requestCount++;
  
  Serial.println("\n========================================");
  Serial.printf("PING - Request #%lu\n", requestCount);
  Serial.printf("IP: %s\n", server.client().remoteIP().toString().c_str());
  Serial.println("========================================\n");
  Serial.flush();
  
  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"PONG\"}");
}

void handleMove() {
  requestCount++;
  
  Serial.println("\n");
  Serial.println("========================================");
  Serial.printf("MOVE REQUEST #%lu\n", requestCount);
  Serial.printf("IP: %s\n", server.client().remoteIP().toString().c_str());
  Serial.printf("Argumentos: %d\n", server.args());
  Serial.flush();
  
  for (int i = 0; i < server.args(); i++) {
    Serial.printf("   -> %s = %s\n", server.argName(i).c_str(), server.arg(i).c_str());
    Serial.flush();
  }
  
  if (!server.hasArg("dir") || !server.hasArg("speed") || !server.hasArg("time")) {
    Serial.println("ERROR: Faltan parametros");
    Serial.println("========================================\n");
    Serial.flush();
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters: dir, speed, time\"}");
    return;
  }

  char dir = server.arg("dir")[0];
  int speed = server.arg("speed").toInt();
  int duration = server.arg("time").toInt();
  if (duration > MAX_MOTOR_DURATION) duration = MAX_MOTOR_DURATION;

  Serial.printf("Dir: %c | Speed: %d | Time: %d ms\n", dir, speed, duration);
  Serial.println("========================================\n");
  Serial.flush();

  String response = "{\"status\":\"ok\",\"dir\":\"" + String(dir) + 
                    "\",\"speed\":" + String(speed) + 
                    ",\"time\":" + String(duration) + 
                    ",\"request_id\":" + String(requestCount) + "}";
  server.send(200, "application/json", response);
  
  startMotors(dir, speed, duration);

  if (mqttClient.connected()) {
    String msg = "{\"dir\":\"" + String(dir) + "\",\"speed\":" + String(speed) +
                 ",\"time\":" + String(duration) + ",\"request_id\":" + String(requestCount) + "}";
    mqttClient.publish(MQTT_TOPIC_MOVEMENT, msg.c_str());
    Serial.println("MQTT publicado (movimiento)");
  }
  
  Serial.flush();
}

void handleGetDistance() {
  requestCount++;
  
  Serial.println("\n========================================");
  Serial.printf("GET DISTANCE - Request #%lu\n", requestCount);
  Serial.println("========================================\n");
  
  float distance = readUltrasonicSensor();
  
  if (distance > 0) {
    String response = "{\"status\":\"ok\",\"distance\":" + String(distance, 2) + 
                      ",\"unit\":\"cm\",\"timestamp\":" + String(millis()) + "}";
    server.send(200, "application/json", response);
    Serial.printf("Distancia medida: %.2f cm\n", distance);
  } else {
    server.send(500, "application/json", 
                "{\"status\":\"error\",\"message\":\"Sensor reading out of range\"}");
    Serial.println("Error en lectura del sensor");
  }
  
  Serial.flush();
}

void handleNotFound() {
  Serial.println("\n404 - Ruta no encontrada: " + server.uri());
  server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Not Found\"}");
}

// =================== MQTT ===========================

void reconnectMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("Reconectando MQTT... ");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println("OK");
    } else {
      Serial.printf("ERROR (rc=%d)\n", mqttClient.state());
    }
  }
}

// =================== SETUP ===========================

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("   ESP32 CAR CONTROL - API REST + SENSOR");
  Serial.println("========================================\n");
  
  // Seed para numeros aleatorios (para modo simulado)
  randomSeed(analogRead(0));
  
  // Configurar pines de motores (Puente H)
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotors();
  
  // Configurar pines del sensor HC-SR04
  if (!SENSOR_SIMULADO) {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    Serial.println("Sensor HC-SR04 FISICO inicializado");
  } else {
    Serial.println("Sensor HC-SR04 SIMULADO inicializado");
  }

  // Conectar WiFi
  Serial.printf("Conectando WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Conectar MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  reconnectMQTT();

  // Configurar endpoints
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/move", HTTP_GET, handleMove);
  server.on("/distance", HTTP_GET, handleGetDistance);
  server.onNotFound(handleNotFound);
  
  server.begin();
  
  Serial.println("\n========================================");
  Serial.println("  API REST LISTA");
  Serial.println("========================================");
  Serial.println("\nENDPOINTS DISPONIBLES:");
  Serial.println("   GET /ping");
  Serial.println("   GET /move?dir=<f|b|l|r|x>&speed=<0-255>&time=<ms>");
  Serial.println("   GET /distance");
  Serial.println("\nTOPICS MQTT:");
  Serial.printf("   Movimiento: %s\n", MQTT_TOPIC_MOVEMENT);
  Serial.printf("   Sensor: %s\n", MQTT_TOPIC_SENSOR);
  Serial.println("\nEJEMPLOS EN POSTMAN:");
  Serial.print("   http://");
  Serial.print(WiFi.localIP());
  Serial.println("/move?dir=f&speed=200&time=2000");
  Serial.print("   http://");
  Serial.print(WiFi.localIP());
  Serial.println("/distance");
  Serial.printf("\nModo sensor: %s\n", SENSOR_SIMULADO ? "SIMULADO" : "FISICO");
  Serial.println("\nEsperando peticiones...\n");
  Serial.flush();
}

// =================== LOOP ============================

void loop() {
  server.handleClient();
  checkMotors();
  publishSensorData();  // Publicacion periodica automatica del sensor
  
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}