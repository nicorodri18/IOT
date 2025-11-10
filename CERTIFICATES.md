# CERTIFICATES.md — TLS & MQTT en ESP32 (Proyecto Car + MQTT)
**Autores:** Camilo Otálora, Juan Diego Martínez, Nicolas Rodríguez  

**Fecha:** 2025-11-10

---


## 1) ¿Qué es TLS? ¿Por qué es importante? ¿Qué es un “certificado”?
**TLS (Transport Layer Security)** es el protocolo estándar para cifrar y autenticar comunicaciones en red. Proporciona: (1) **confidencialidad** (nadie más puede leer los datos), (2) **integridad** (evita modificaciones en tránsito) y (3) **autenticación** (saber con quién hablamos).  
Un **certificado X.509** vincula una **clave pública** con una identidad (p. ej., un dominio) y está **firmado por una Autoridad de Certificación (CA)**. Los clientes validan esa firma para confiar en el servidor.

## 2) ¿A qué riesgos se expone si no se usa TLS?
- **Espionaje** (sniffing): robo de credenciales, tokens y datos.  
- **Manipulación** (MITM): inyección de comandos o payloads falsos.  
- **Suplantación**: conectar contra un broker/servidor impostor.  
- **No-repudio / evidencia**: sin autenticación no hay trazabilidad confiable.

## 3) ¿Qué es una CA (Certificate Authority)?
Una **CA** es una entidad de confianza que verifica identidades y **firma certificados**. Los sistemas/clientes incluyen una “lista de CAs raíz” confiables. Si una CA firma el certificado de un servidor, los clientes pueden validarlo con esa raíz.

## 4) ¿Qué es una *cadena de certificados* y cuál es su vigencia promedio?
Una **cadena** va desde el **certificado del servidor (hoja/leaf)** → **intermedios** → **raíz**.  
**Vigencias típicas (2025):**
- **Leaf (servidor)**: hasta **398 días** por norma CAB Forum; algunos emisores como Let’s Encrypt usan **90 días** (renovación automática).  
- **Intermedios**: ~1–5 años.  
- **Raíces**: 10–25+ años.  
*(La industria aprobó reducir gradualmente las vigencias máximas en los próximos años.)*

## 5) ¿Qué es un *keystore* y qué es un *certificate bundle*?
- **Keystore**: almacén seguro de **claves privadas** y **certificados** (p. ej., JKS/PKCS#12 en Java). En microcontroladores solemos guardar claves/certificados en **flash** (PROGMEM) o en **FS** (SPIFFS/LittleFS), o en hardware seguro cuando existe.  
- **Certificate bundle**: paquete de **CAs raíz** (varias). En ESP32 podemos usar el **bundle precompilado** del SDK (Arduino-ESP32/IDF) para validar muchos dominios públicos sin copiar un PEM por cada dominio.

## 6) ¿Qué es la **autenticación mutua** (mTLS) en TLS?
Además de que el **cliente verifica** al **servidor**, el **servidor exige** que **el cliente también presente un certificado** propio firmado por una CA que el servidor confía. Así, **ambas partes** se autentican. En Mosquitto de pruebas, el puerto **8884** exige mTLS.

## 7) ¿Cómo se habilita la **validación de certificados** en ESP32?
Con **Arduino-ESP32**:  
**A)** Cargar una **CA** (o cadena) en PEM (o DER) y llamar a `setCACert()` en `WiFiClientSecure`.  
**B)** Usar el **bundle de CAs** del SDK con `setCACertBundle(...)`.  
**C)** Para mTLS, además: `setCertificate()` (cliente) y `setPrivateKey()`.

## 8) Si el sketch conecta a **múltiples dominios** con CAs distintas, ¿qué opciones hay?
1) **Bundle de CAs** (recomendado): una sola línea y sirve para muchos dominios públicos.  
2) **Cargar varias CAs** concatenadas en **un único PEM** y pasarlo a `setCACert()`.  
3) **Seleccionar CA por dominio** (leer desde FS y llamar `setCACert()` antes de cada conexión).  
4) **Pinning** (huella SHA-256 del **certificado del servidor**): muy seguro pero frágil (rompe al renovar el certificado).

## 9) ¿Cómo obtener el certificado para un dominio?
- **Del broker**: si usa una CA pública, bastaría el **bundle**.  
- **Descarga directa** (ejemplo Mosquitto 8883): `https://test.mosquitto.org/ssl/mosquitto.org.crt` (guardarlo como `mosquitto_org_ca.pem`).  
- **OpenSSL** (obtener la cadena):  
  ```bash
  # Ver/extraer cadena del puerto 8886 (Let’s Encrypt) o 8883 (mosquitto.org CA)
  echo | openssl s_client -servername test.mosquitto.org -connect test.mosquitto.org:8886 -showcerts
  ```
  Copie el/los certificados `-----BEGIN CERTIFICATE----- ...` en un `.pem`.

## 10) ¿A qué se refiere **llave pública** y **privada** en TLS?
- **Llave pública**: se distribuye con el **certificado**; otros la usan para **verificar firmas** y cifrar hacia el dueño.  
- **Llave privada**: la custodia el dueño; se usa para **firmar** (p. ej., el *handshake* del servidor) o para autenticación del **cliente** en mTLS. **Nunca** debe exponerse.

## 11) ¿Qué pasará con el código cuando los certificados **expiren**?
- Si el **leaf** caduca (muy frecuente): el **handshake fallará** y la conexión MQTT no se establecerá.  
- Si hizo **pinning del leaf**, deberá **recompilar/actualizar** cada renovación (p. ej., cada 90 días con Let’s Encrypt).  
- Si confía en la **CA raíz** (o bundle), el mantenimiento es mucho menor, pero ocasionalmente cambian **intermedios** o se **revocan** cadenas: conviene probar y actualizar.

## 12) ¿Qué teoría matemática fundamenta la criptografía moderna? ¿Implicaciones cuánticas?
- Fundamentos: **teoría de números**, **álgebra** (grupos, campos finitos), **complejidad computacional**. RSA se basa en **factorización**, Diffie‑Hellman/ECC en **logaritmos discretos**.  
- **Computación cuántica**:
  - **Shor** rompe **RSA/DH/ECC** si existieran computadores cuánticos grandes.  
  - **Grover** acelera el ataque por fuerza bruta (≈√N), por lo que se recomiendan **claves simétricas más largas**.  
  - **PQC** (post‑cuántica): NIST está estandarizando **Kyber (KEM)** y **Dilithium/Falcon/SPHINCS+ (firmas)**, con transición gradual en los próximos años.

---

# Prueba de código (MQTT seguro en ESP32)

A continuación se documentan **tres pasos** y el resultado esperado. Usamos `test.mosquitto.org`:

- **1883**: sin cifrar.
- **8883**: **TLS** con **CA propia de mosquitto.org** (hay que cargar su PEM).
- **8886**: **TLS** con **Let’s Encrypt** (sirve el **bundle** del ESP32).

## Preparación
- Arduino Core ESP32 ≥ 2.x.  
- Librerías: `WiFi.h`, `WiFiClientSecure.h`, `PubSubClient.h`.  
- Elegir una de dos rutas en el paso 3: **(3A)** 8883 + CA de mosquitto **o** **(3B)** 8886 + bundle.

---

## (1) Cambiar solo a puerto seguro
**Cambio:** `#define MQTT_PORT 8883` y (aún) **dejar** `WiFiClient espClient;` (o cambiar a `WiFiClientSecure` **sin** CA).  
**Resultado esperado:** **Falla** la conexión (handshake/TCP). En serie se verá algo como `ERROR (rc=-2)` y reconexiones.

---

## (2) Habilitar validación de certificados, sin cargar CA
**Cambio:** usar `WiFiClientSecure espClient;` **sin** `setCACert(...)` ni `setCACertBundle(...)`.  
**Resultado esperado:** **Falla** la conexión por **falta de anclas de confianza**. Es correcto: la validación está activa y no confiamos en ninguna CA aún.

---

## (3A) Validación OK usando **8883 + CA de mosquitto.org** (recomendado si quiere 8883)
1. Descargue la CA: `mosquitto_org_ca.pem`.  
2. Inclúyala en el sketch como cadena PEM (o súbala a SPIFFS y léala).  
3. Use `WiFiClientSecure` + `setCACert()` y mantenga `MQTT_PORT 8883`.

**Parche mínimo (extracto):**
```cpp
#include <WiFiClientSecure.h>
// PEM pegado aquí (recorte):
static const char MOSQ_ORG_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
... pegue aquí el contenido de https://test.mosquitto.org/ssl/mosquitto.org.crt ...
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

void setup() {
  // ...
  espClient.setCACert(MOSQ_ORG_CA);        // <-- valida contra CA de mosquitto.org
  mqttClient.setServer("test.mosquitto.org", 8883);
  // ...
}
```

**Resultado esperado:** la conexión **funciona** y los `publish()`/`subscribe()` vuelven a operar.

---

## (3B) Validación OK usando **8886 + bundle de CAs** (Let’s Encrypt)
1. Cambie a `MQTT_PORT 8886`.  
2. Active el bundle del ESP32:
```cpp
#include <WiFiClientSecure.h>
#include <esp_crt_bundle.h>

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

void setup() {
  // ...
  espClient.setCACertBundle(esp_crt_bundle_attach);  // <-- usa bundle del SDK
  mqttClient.setServer("test.mosquitto.org", 8886);
}
```
**Resultado esperado:** la conexión **funciona** sin pegar ningún PEM manual.

> **Nota:** en 8884 (mTLS) añadir además `espClient.setCertificate(client_crt_pem);` y `espClient.setPrivateKey(client_key_pem);`.

---

## Evidencia sugerida (para anexar en /evidence y referenciar aquí)
1. **(1) Falla en 8883 sin CA:** captura de Serial Monitor con `rc=-2` o handshake fallido.  
2. **(2) Validación sin CA:** captura de Serial Monitor con error de validación.  
3. **(3A) Éxito 8883 + CA** o **(3B) Éxito 8886 + bundle:** captura con `Reconectando MQTT... OK` y publicaciones de `distance`.  
4. **Prueba externa opcional:** en PC:  
   ```bash
   mosquitto_sub -h test.mosquitto.org -p 8883 --cafile mosquitto_org_ca.pem -t "esp32/car/distance" -v
   # o si usó 8886 (Let's Encrypt) y su sistema ya confía en LE, sólo:
   mosquitto_sub -h test.mosquitto.org -p 8886 -t "esp32/car/distance" -v
   ```

---

# Diff mínimo aplicado a nuestro código

```diff
-#include <WiFi.h>
-#include <WebServer.h>
-#include <PubSubClient.h>
+#include <WiFi.h>
+#include <WebServer.h>
+#include <WiFiClientSecure.h>
+#include <esp_crt_bundle.h> // si se opta por 8886
+#include <PubSubClient.h>

-#define MQTT_PORT 1883
+// Opción A (8883 + PEM mosquitto.org)  -> usar setCACert(MOSQ_ORG_CA)
+// Opción B (8886 + bundle Let’s Encrypt)-> usar setCACertBundle(esp_crt_bundle_attach)
+#define MQTT_PORT 8886

- WiFiClient espClient;
+ WiFiClientSecure espClient;

  PubSubClient mqttClient(espClient);

  void setup() {
     // ...
-    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
+    // Opción A: espClient.setCACert(MOSQ_ORG_CA);
+    // Opción B (recomendada por simplicidad en público): 
+    espClient.setCACertBundle(esp_crt_bundle_attach);
+    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  }
```

> Si prefiere 8883, sustituya el `setCACertBundle(...)` por `setCACert(MOSQ_ORG_CA)` y pegue el PEM.

---

## Apéndice A — mTLS rápido (puerto 8884 en test.mosquitto.org)
1. Genere clave y CSR para el cliente y consiga que una CA la **firme** (o configure su broker propio).  
2. Convierta a PEM si hace falta y pegue:  
```cpp
espClient.setCACert(ca_pem);           // CA que firma al broker
espClient.setCertificate(client_crt);  // Cert del cliente (PEM)
espClient.setPrivateKey(client_key);   // Clave privada (PEM)
```
3. En el broker, habilite `require_certificate true` y confíe en la CA que firma a sus clientes.

## Apéndice B — Buenas prácticas en microcontroladores
- **Evite `setInsecure()`** (sólo para pruebas puntuales).  
- **Revise expiraciones**: anote la fecha de caducidad y planifique actualizaciones OTA.  
- **Minimice pinning** del leaf si el certificado rota a 90 días.  
- **Memoria**: guarde PEMs en **PROGMEM** o en **FS** y lea a RAM sólo al usar.  
- **Crypto‑agilidad**: diseñe para poder **cambiar CA/algoritmos** sin reflashear todo.

---

## Referencias
- Puertos y CAs de `test.mosquitto.org` (8883/8884 usan CA propia; 8886 usa Let’s Encrypt).  
- Límite vigente de **398 días** y calendario de reducción de vigencias (CA/B Forum, 2025).  
- Cert bundle en ESP32 (`setCACertBundle`).  
- NIST **PQC** (Kyber, Dilithium, SPHINCS+, Falcon) y aprobación FIPS 203/204/205.

---

## Cómo subir este archivo
1. Guarde este archivo como `CERTIFICATES.md` en la raíz del repo.  
2. Añada evidencia (capturas) en `evidence/` y referéncielas en este documento.  
3. *Opcional:* agregue `certs/mosquitto_org_ca.pem` o el header `certs.h` si usa 8883.
