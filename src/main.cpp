#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ThingSpeak.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

// === OLED ===
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === WiFi ===
const char* ssid = "motoedge50fusion_8901";
const char* password = "Delta1234";

// === OpenWeather API ===
String apiKey = "db02e7906a11cb026b58487edb538758";

// === ThingSpeak ===
WiFiClient client;
unsigned long channelNumber = 3125092;
const char* writeApiKey = "QFWCU0T7VP0WQ921";

// === Timers ===
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000; // 5 segundos

// === Variables de clima ===
String ciudadActual = "Bogota";
String climaGlobal = "";
float tempGlobal = 0;

// === Control Manual ===
bool modoManual = false;            // TRUE = Override temporal
unsigned long manualTimeout = 0;    // Control de duración manual
const unsigned long manualDuration = 5000; // 5 segundos

bool detenido = false; // TRUE = Stop total (sin consultar API)

// === Web Server y WebSocket ===
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// === OLED ===
void mostrarOLED(String ciudad, String clima, float temp) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.print(ciudad);
  display.setCursor(0, 20);
  display.println(clima);
  display.setTextSize(2);
  display.setCursor(0, 40);
  display.print(temp);
  display.println(" C");
  display.display();
}

// === API OpenWeather ===
void obtenerClima() {
  if (WiFi.status() != WL_CONNECTED || detenido || modoManual) return;

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + ciudadActual +
               "&appid=" + apiKey + "&units=metric&lang=es";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String respuesta = http.getString();
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, respuesta);

    climaGlobal = doc["weather"][0]["description"].as<String>();
    tempGlobal = doc["main"]["temp"].as<float>();

    Serial.println("🌎 Ciudad: " + ciudadActual);
    Serial.println("☁️ Clima: " + climaGlobal);
    Serial.println("🌡️ Temp: " + String(tempGlobal));

    mostrarOLED(ciudadActual, climaGlobal, tempGlobal);

    // === Enviar datos completos a ThingSpeak ===
    // Field 1: Ciudad (string) - ThingSpeak almacena numérico principalmente; si deseas priorizar la gráfica de temperatura,
    // asegúrate de que el widget use Field 3 (Temperatura). Aquí se envían los tres campos.
    ThingSpeak.setField(1, ciudadActual);
    ThingSpeak.setField(2, climaGlobal);
    ThingSpeak.setField(3, tempGlobal);

    int response = ThingSpeak.writeFields(channelNumber, writeApiKey);
    if (response == 200) {
      Serial.println("✅ Datos enviados a ThingSpeak correctamente");
    } else {
      Serial.println("❌ Error al enviar a ThingSpeak: " + String(response));
    }

    // === Enviar por WebSocket ===
    DynamicJsonDocument ws(256);
    ws["city"] = ciudadActual;
    ws["weather"] = climaGlobal;
    ws["temp"] = tempGlobal;
    String mensaje;
    serializeJson(ws, mensaje);
    webSocket.broadcastTXT(mensaje);
  } else {
    Serial.println("❌ Error HTTP: " + String(httpCode));
  }
  http.end();
}

// === Control WebSocket ===
// Firma completa y correcta: número de cliente, tipo, payload y longitud
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String mensaje = String((char*)payload);
    Serial.println("📩 Ciudad recibida por WS: " + mensaje);
    ciudadActual = mensaje;
    obtenerClima();
  }
}

// === Control desde /update ===
void handleUpdate() {
  if (!server.hasArg("city") || !server.hasArg("weather") || !server.hasArg("temp")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  String city = server.arg("city");
  String weather = server.arg("weather");
  float temp = server.arg("temp").toFloat();

  if (weather == "STOP") {
    detenido = true;
    modoManual = false;
    mostrarOLED(city, "DETENIDO", 0);
    server.send(200, "text/plain", "Ejecución detenida");
    Serial.println("🛑 Ejecución detenida por frontend");
    return;
  }

  if (weather == "RESUME") {
    detenido = false;
    modoManual = false;
    mostrarOLED(city, "Reanudando...", tempGlobal);
    server.send(200, "text/plain", "Ejecución reanudada");
    Serial.println("▶️ Ejecución reanudada");
    return;
  }

  if (weather == "MANUAL") {
    modoManual = true;
    manualTimeout = millis() + manualDuration;

    mostrarOLED(city, "OVERRIDE", temp);
    Serial.println("⚙️ Override manual: " + String(temp));

    // Enviar broadcast a WebSocket
    DynamicJsonDocument ws(256);
    ws["city"] = city;
    ws["weather"] = "MANUAL";
    ws["temp"] = temp;
    String mensaje;
    serializeJson(ws, mensaje);
    webSocket.broadcastTXT(mensaje);

    server.send(200, "text/plain", "Override manual recibido");
    return;
  }

  ciudadActual = city;
  detenido = false;
  modoManual = false;
  Serial.println("🌍 Cambio de ciudad: " + ciudadActual + " (modo automático activado)");
  obtenerClima();
  server.send(200, "text/plain", "Ciudad actualizada y modo automático reanudado");
}

// === Servir archivos desde SPIFFS ===
void handleFile(String path) {
  if (path.endsWith("/")) path += "index.html";
  String contentType = "text/plain";
  if (path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";

  File file = SPIFFS.open(path, "r");
  if (!file) {
    server.send(404, "text/plain", "Not Found");
    return;
  }
  server.streamFile(file, contentType);
  file.close();
}

void handleNotFound() {
  handleFile(server.uri());
}

// === SETUP ===
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }
  Serial.println("\n✅ Conectado!");
  Serial.println("IP del ESP32: " + WiFi.localIP().toString());

  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Error al montar SPIFFS");
  }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  mostrarOLED("Iniciando...", "", 0);

  ThingSpeak.begin(client);

  server.on("/update", HTTP_POST, handleUpdate);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// === LOOP ===
void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long now = millis();

  if (modoManual && now > manualTimeout) {
    modoManual = false;
    Serial.println("✅ Fin del override manual, retomando API");
  }

  if (!detenido && !modoManual && (now - lastUpdate >= updateInterval)) {
    lastUpdate = now;
    static int indiceCiudad = 0;
    String ciudades[] = {"Bogota", "Medellin", "Cali", "Barranquilla", "Cartagena"};
    int totalCiudades = sizeof(ciudades) / sizeof(ciudades[0]);

    ciudadActual = ciudades[indiceCiudad];
    indiceCiudad = (indiceCiudad + 1) % totalCiudades;

    Serial.println("🌆 Cambiando a ciudad: " + ciudadActual);
    obtenerClima();
  }
}
