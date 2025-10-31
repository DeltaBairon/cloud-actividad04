![ESP32](https://img.shields.io/badge/ESP32-Ready-blue?logo=espressif)
![WebSockets](https://img.shields.io/badge/WebSockets-Active-green?logo=websocket)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Build-orange?logo=platformio)
![WiFi](https://img.shields.io/badge/WiFi-Connected-success?logo=wifi)
![HTML5](https://img.shields.io/badge/Frontend-HTML%2FCSS%2FJS-yellow?logo=html5)

# 🌦 Proyecto ESP32 + WebApp Interactiva de Clima

Este proyecto combina el uso de un **ESP32** y una **interfaz web interactiva** para simular y controlar condiciones climáticas en tiempo real.  
Utiliza **WebSockets** para la comunicación bidireccional entre el microcontrolador y el cliente, permitiendo el envío y recepción de datos sin recargar la página.  

---

## 📋 Descripción General

El ESP32 se conecta a una red WiFi local y crea un servidor que aloja una interfaz web.  
Desde la página web, el usuario puede:
- Visualizar temperatura y descripción del clima.
- Cambiar manualmente la temperatura.
- Pausar o reanudar la actualización automática.
- Cambiar la ciudad actual.

Mientras tanto, el ESP32 genera datos automáticos cada 5 segundos (modo “auto”) o recibe comandos en tiempo real (modo “manual”).

---

## 🧩 Estructura del Proyecto

```
proyecto-colab/
│
├── src/
│   └── main.cpp                # Código principal del ESP32 (lógica principal y WebSocket)
│
├── data/
│   ├── index.html              # Interfaz web principal
│   ├── style.css               # Estilos visuales del frontend
│   └── script.js               # Lógica de interacción cliente-servidor
│
├── platformio.ini              # Configuración del entorno PlatformIO
└── README.md                   # Documentación del proyecto
```

---

## ⚙️ Archivo de Configuración `platformio.ini`

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SSD1306
    mathworks/ThingSpeak @ ^2.1.1
    ArduinoJson @ ^7.0.0
    links2004/WebSockets @ ^2.3.6
    ESP Async WebServer @ ^3.0.6
    me-no-dev/AsyncTCP @ ^1.1.1
```

---

## 💻 Código Principal `src/main.cpp`

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>

const char *ssid = "TU_WIFI";
const char *password = "TU_PASSWORD";

WebSocketsServer webSocket = WebSocketsServer(81);

String currentCity = "Medellin";
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000;
bool autoMode = true;

void webSocketEvent(uint8_t client, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char *)payload);
    if (msg.startsWith("city:")) {
      currentCity = msg.substring(5);
      Serial.println("📩 Ciudad recibida: " + currentCity);
    } else if (msg.startsWith("temp:")) {
      autoMode = false;
      String tempManual = msg.substring(5);
      Serial.println("🌡 Temperatura manual: " + tempManual);
      String json = "{\"city\":\"" + currentCity + "\",\"temp\":" + tempManual + ",\"desc\":\"Modo manual\"}";
      webSocket.broadcastTXT(json);
      delay(5000);
      autoMode = true;
    } else if (msg == "pause") {
      autoMode = false;
      Serial.println("⏸ Ejecución detenida");
    } else if (msg == "resume") {
      autoMode = true;
      Serial.println("▶️ Ejecución reanudada");
    }
  }
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\n✅ Conectado!");
  Serial.print("🌍 IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  connectWiFi();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("✅ WebSocket iniciado en puerto 81");
}

void loop() {
  webSocket.loop();
  unsigned long now = millis();
  if (autoMode && (now - lastUpdate >= updateInterval)) {
    lastUpdate = now;
    Serial.println("🌡 Solicitando clima de: " + currentCity);
    float temp = random(15, 35);
    String json = "{\"city\":\"" + currentCity + "\",\"temp\":" + String(temp, 1) + ",\"desc\":\"Clima dinámico\"}";
    webSocket.broadcastTXT(json);
    Serial.println("📤 Enviado a WebApp: " + json);
  }
}
```

---

## 🌐 Archivos del Cliente Web (`/data`)

### `index.html`
```html
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>🌦 Control de Clima ESP32</title>
  <link rel="stylesheet" href="style.css">
</head>
<body>
  <h1>🌍 Control de Clima ESP32</h1>
  <div class="card">
    <h2 id="city">Ciudad: Medellín</h2>
    <h3 id="temp">Temperatura: -- °C</h3>
    <p id="desc">Esperando datos...</p>
  </div>

  <input type="text" id="cityInput" placeholder="Nueva ciudad">
  <button onclick="sendCity()">Cambiar Ciudad</button>
  <input type="number" id="tempInput" placeholder="Temperatura manual">
  <button onclick="sendTemp()">Enviar Temperatura</button>
  <button id="pauseBtn" onclick="pauseAuto()">⏸ Pausar</button>
  <button id="resumeBtn" onclick="resumeAuto()">▶️ Reanudar</button>

  <script src="script.js"></script>
</body>
</html>
```

### `style.css`
```css
body {
  font-family: Arial, sans-serif;
  text-align: center;
  background-color: #1e1e2f;
  color: white;
}

.card {
  background: #2d2d44;
  padding: 20px;
  margin: 20px auto;
  width: 280px;
  border-radius: 12px;
  box-shadow: 0 4px 10px rgba(0,0,0,0.3);
}

button {
  background: #0078ff;
  color: white;
  border: none;
  padding: 10px;
  margin: 5px;
  border-radius: 8px;
  cursor: pointer;
  transition: 0.3s;
}

button:hover {
  background: #005fcc;
}
```

### `script.js`
```js
let ws = new WebSocket(`ws://${window.location.hostname}:81`);
ws.onopen = () => console.log("✅ WebSocket conectado");
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  document.getElementById("city").textContent = "Ciudad: " + data.city;
  document.getElementById("temp").textContent = "Temperatura: " + data.temp + " °C";
  document.getElementById("desc").textContent = data.desc;
};

function sendCity() {
  const city = document.getElementById("cityInput").value;
  if (city) ws.send("city:" + city);
}

function sendTemp() {
  const temp = document.getElementById("tempInput").value;
  if (temp) ws.send("temp:" + temp);
}

function pauseAuto() {
  ws.send("pause");
}

function resumeAuto() {
  ws.send("resume");
}
```

---

## 🧱 Errores Presentados y Soluciones

### 🧩 Error 1:  
```
error: expected ',' or '...' at end of input
void webSocketEvent(uint8_t num, WStype_t t
```

**Causa:**  
El error se debía a que la función `webSocketEvent` no estaba completamente definida (faltaba cerrar los parámetros o las llaves).  

**Solución:**  
Corregir la firma completa de la función:
```cpp
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
```

---

### 🧩 Error 2: Problemas de conexión WebSocket no iniciando
**Síntoma:**  
No se recibían mensajes en la interfaz web.

**Causa:**  
El puerto `81` no estaba inicializado correctamente o el cliente estaba intentando conectarse antes de establecer WiFi.  

**Solución:**  
Asegurar la secuencia correcta:
```cpp
connectWiFi();
webSocket.begin();
webSocket.onEvent(webSocketEvent);
```
Y verificar en el cliente:
```js
let ws = new WebSocket(`ws://${window.location.hostname}:81`);
```

---

### 🧩 Error 3: Códigos HTML o JS no actualizaban datos
**Causa:**  
No se parseaban correctamente los mensajes JSON.  

**Solución:**  
Agregar `JSON.parse(event.data)` en el cliente para leer correctamente la información enviada por el ESP32.

---

## 🧠 Conclusión

Este proyecto demuestra cómo integrar:
- **ESP32 + WebSockets + Frontend Web**
en una comunicación fluida en tiempo real.

Se puede extender para aplicaciones IoT reales como:
- Monitoreo ambiental  
- Sensores de temperatura o humedad reales  
- Control remoto de dispositivos  

---

## 🧰 Créditos

Proyecto desarrollado y depurado paso a paso con integración de:
- **PlatformIO**  
- **Arduino Framework**  
- **Servidor WebSocket nativo en ESP32**  
- **Frontend HTML + JS + CSS**  





