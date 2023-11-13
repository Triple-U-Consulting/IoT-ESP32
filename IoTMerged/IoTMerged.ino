#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>


const char* softAPSSID = "ESP32_SoftAP";
const char* softAPPassword = "password";
const char* deviceID = "inhaler_1";
const char* dbserver = "https://back-end-macro-production.up.railway.app";

const int LED_PIN = 14;

int old_value = 0;
int puffID = 0;
unsigned long lastTapTime = 0; 
int kambuhID = 0; 
unsigned long lastReconnectAttempt = 0;


String storedSSID = "";
String storedPassword = "";

Preferences preferences;

WebServer server(80);


void setup() {
  pinMode(12, INPUT);
  pinMode(13, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);

  preferences.begin("WiFiCred", true);
  storedSSID = preferences.getString("ssid", "");
  storedPassword = preferences.getString("password", "");
  preferences.end();
  if (storedSSID == "" || storedPassword == "") {
    Serial.println("No stored WiFi credentials found. Starting SoftAP for configuration.");
    WiFi.softAP(softAPSSID, softAPPassword);
    Serial.println("WiFi SoftAP terpasang");
    Serial.print("Alamat IP Access Point: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Connecting to WiFi with stored credentials.");
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to WiFi.");
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.println("\nFailed to connect to WiFi. Starting SoftAP for configuration.");
      WiFi.softAP(softAPSSID, softAPPassword);
      Serial.print("Alamat IP Access Point: ");
      Serial.println(WiFi.softAPIP());
    }
  }

  server.on("/config-wifi", HTTP_POST, handleConfigWiFi);
  server.on("/device-id", HTTP_GET, handleDeviceID);
  server.begin();

  updateLEDStatus();
}


void loop() {
  server.handleClient();
  updateLEDStatus();
  int Value = digitalRead(12);
  digitalWrite(13, !Value);
  unsigned long currentTime = millis();

   if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentTime = millis();
    if (currentTime - lastReconnectAttempt >= 5000) {
      lastReconnectAttempt = currentTime;
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  
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
  JsonObject jsonObject = jsonDoc.to<JsonObject>();
  
  jsonObject["deviceid"] = deviceID;

  String jsonResponse;
  serializeJson(jsonObject, jsonResponse);

  server.send(200, "application/json", jsonResponse);
}


void handleConfigWiFi() {
  String jsonBody = server.arg("plain");
  StaticJsonDocument<256> doc;
  deserializeJson(doc, jsonBody);

  const char* receivedSSID = doc["ssid"];
  const char* receivedPassword = doc["password"];
  
  if (!receivedSSID || !receivedPassword) {
    sendJSONResponse(400, "Both SSID and password are required.");
    return;
  }
  
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
    storedSSID = String(receivedSSID);
    storedPassword = String(receivedPassword);
    digitalWrite(LED_PIN, HIGH);
    sendJSONResponse(200, "Wifi configuration updated successfully.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
     preferences.begin("WiFiCred", false);
    preferences.putString("ssid", receivedSSID);
    preferences.putString("password", receivedPassword);
    preferences.end();
    delay(1000);
    WiFi.softAPdisconnect(true);
    
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("WiFi Failed to Connect");
    sendJSONResponse(200, "WiFi Failed to Connect.");
  }
}

void sendJSONResponse(int statusCode, const char* message) {
  StaticJsonDocument<256> jsonDoc;
  JsonObject jsonObject = jsonDoc.to<JsonObject>();
  jsonObject["message"] = message;
  String jsonResponse;
  serializeJson(jsonObject, jsonResponse);
  server.send(statusCode, "application/json", jsonResponse);
}


void postData(int puffID, unsigned long dateTime, int kambuhID) {
    HTTPClient http;

    http.begin(String(dbserver) + "/data/puff"); 
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> jsonDoc;
    JsonObject jsonObject = jsonDoc.to<JsonObject>();
    jsonObject["inhaler_id"] = deviceID;

    String jsonData;
    serializeJson(jsonObject, jsonData);

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

void updateLEDStatus() {
  if (WiFi.softAPgetStationNum() > 0) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}


bool reconnect() {
  if (storedSSID != "" && storedPassword != "") {
    Serial.println("Wifi disconnected. Trying to reconnect...");
    WiFi.disconnect();
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("Reconnected to WiFi successfully.");
      return true;
    }
  }
  Serial.println("Failed to reconnect to WiFi");
  return false;
}
