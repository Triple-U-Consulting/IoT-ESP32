#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>


const char* softAPSSID = "ESP32_SoftAP";
const char* softAPPassword = "password";
const char* deviceID = "puffer_322614";
const char* dbserver = "https://back-end-macro-production.up.railway.app";

int old_value = 0;
int puffID = 0;
unsigned long lastTapTime = 0; 
int kambuhID = 0; 

WebServer server(80);


void setup() {

  pinMode(12, INPUT);
  pinMode(13, OUTPUT);

  Serial.begin(115200);
  
  // Langkah 1: Membangun WiFi SoftAP
  WiFi.softAP(softAPSSID, softAPPassword);
  Serial.println("WiFi SoftAP terpasang");
  Serial.print("Alamat IP Access Point: ");
  Serial.println(WiFi.softAPIP());

  // Langkah 2: Menunggu hingga perangkat lain terhubung dan mengirimkan SSID dan password
  while (WiFi.softAPgetStationNum() == 0) {
    delay(1000);
  }
  Serial.println("Perangkat terhubung ke WiFi SoftAP");

  // Langkah 3: Mengatur rute HTTP untuk menerima data SSID dan password
  server.on("/config-wifi", HTTP_POST, handleConfigWiFi);
  server.on("/device-id", HTTP_GET, handleDeviceID);

  server.begin();
}

void loop() {
  server.handleClient();
  int Value = digitalRead(12);
  digitalWrite(13, !Value);
  unsigned long currentTime = millis();
if (old_value < Value) {
  old_value = Value;
  puffID++;

  if (currentTime - lastTapTime > 10000) {
    kambuhID++;
  }
  
  lastTapTime = currentTime;
  postData(puffID, currentTime, kambuhID);
  Serial.print("PuffID: "); Serial.println(puffID);
  Serial.print("DateTime: "); Serial.println(currentTime);
  Serial.print("KambuhID: "); Serial.println(kambuhID);
  }
  else if (old_value > Value) {
    old_value = Value;
  }
}

void handleDeviceID() {
  StaticJsonDocument<256> jsonDoc;
  JsonArray jsonArray = jsonDoc.to<JsonArray>();
  
  JsonObject jsonObject = jsonArray.createNestedObject();
  jsonObject["deviceid"] = deviceID;

  String jsonResponse;
  serializeJson(jsonArray, jsonResponse);

  server.send(200, "application/json", jsonResponse);
}

void handleConfigWiFi() {
  String jsonBody = server.arg("plain");
  StaticJsonDocument<256> doc;
  deserializeJson(doc, jsonBody);

  const char* receivedSSID = doc["ssid"];
  const char* receivedPassword = doc["password"];
  Serial.println(receivedSSID);
  Serial.println(receivedPassword);

  WiFi.disconnect(true);
  delay(1000);
  WiFi.begin(receivedSSID, receivedPassword);

  unsigned long startTime = millis();
  bool connected = false;

  while (millis() - startTime <= 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(1000);
    Serial.println("Mencoba menghubungkan ke WiFi...");
  }

  if (connected) {
    Serial.println("Terhubung ke WiFi dengan sukses.");
    WiFi.softAPdisconnect(true);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.send(200, "text/plain", "Konfigurasi WiFi berhasil diterima.");
  } else {
    Serial.println("WiFi Failed to Connect");
    server.send(404, "text/plain", "WiFi Failed to Connect. Please provide valid SSID and password.");
  }
}


void postData(int puffID, unsigned long dateTime, int kambuhID) {
    HTTPClient http;

    http.begin(String(dbserver) + "/data/puff"); 
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"puff_id\":" + String(puffID) + ",\"date_time\":" + String(dateTime) + ",\"kambuhh_id\":" + String(kambuhID) + "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
        String response = http.getString(); 
        Serial.println(httpResponseCode);  
        Serial.println(response);          
    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }

    http.end();
}
