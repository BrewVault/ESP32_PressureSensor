#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <EEPROM.h>
#include <HTTPClient.h>

const char* ssid = "ESP32-AP";
const char* password = "12345678";

WebServer server(80);

String apiPath = "http://example.com/api";
const int apiPathAddress = 0;

const int I2C_ADDRESS = 0x78; // Default I2C address for the sensor
const int SENSOR_REG_PRESSURE = 0x06;
const int SENSOR_REG_TEMP = 0x09;

unsigned long previousMillis = 0;
const long interval = 900000; // 15 minutes in milliseconds

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  // Load saved API path from EEPROM
  apiPath = loadString(apiPathAddress);

  // Initialize I2C
  Wire.begin();

  // Start WiFi in AP mode
  WiFi.softAP(ssid, password);
  Serial.println("AP mode started");

  // Define routes
  server.on("/", handleRoot);
  server.on("/update", handleUpdate);
  server.begin();

  // Print IP address
  Serial.print("Connect to AP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendDataToApi();
  }
}

void handleRoot() {
  String html = "<html><body><h1>ESP32 Configuration</h1>";
  html += "<form action=\"/update\" method=\"POST\">";
  html += "WiFi SSID: <input type=\"text\" name=\"ssid\"><br>";
  html += "WiFi Password: <input type=\"text\" name=\"password\"><br>";
  html += "API Path: <input type=\"text\" name=\"apiPath\" value=\"" + apiPath + "\"><br>";
  html += "<input type=\"submit\" value=\"Update\">";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleUpdate() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("apiPath")) {
    String newSsid = server.arg("ssid");
    String newPassword = server.arg("password");
    apiPath = server.arg("apiPath");

    // Save API path to EEPROM
    saveString(apiPathAddress, apiPath);

    WiFi.softAPdisconnect(true);
    WiFi.begin(newSsid.c_str(), newPassword.c_str());

    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      server.send(200, "text/html", "<html><body><h1>Configuration updated. Connected to WiFi.</h1></body></html>");
    } else {
      server.send(200, "text/html", "<html><body><h1>Configuration failed. Please try again.</h1></body></html>");
      WiFi.softAP(ssid, password);
    }
  } else {
    server.send(200, "text/html", "<html><body><h1>Invalid input.</h1></body></html>");
  }
}

String loadString(int address) {
  String value = "";
  for (int i = address; i < address + 100; i++) {
    char c = EEPROM.read(i);
    if (c == '\0') break;
    value += c;
  }
  return value;
}

void saveString(int address, String value) {
  for (int i = 0; i < value.length(); i++) {
    EEPROM.write(address + i, value[i]);
  }
  EEPROM.write(address + value.length(), '\0');
  EEPROM.commit();
}

void sendDataToApi() {
  float pressure = readPressure();
  float temperature = readTemperature();

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(apiPath);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"name\":\"Pressure Sensor\",\"pressure\":\"" + String(pressure) + "\",\"temp\":\"" + String(temperature) + "\"}";
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

float readPressure() {
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(SENSOR_REG_PRESSURE);
  Wire.endTransmission();
  delay(50);

  Wire.requestFrom(I2C_ADDRESS, 3);
  if (Wire.available() == 3) {
    byte highByte = Wire.read();
    byte midByte = Wire.read();
    byte lowByte = Wire.read();

    long adcValue = ((long)highByte << 16) | ((long)midByte << 8) | (long)lowByte;
    if (adcValue > 8388608) {
      return (adcValue - 16777216) / 8388608.0 * 1000;
    } else {
      return adcValue / 8388608.0 * 1000;
    }
  }
  return -1;
}

float readTemperature() {
  Wire.beginTransmission(I2C_ADDRESS);
  Wire.write(SENSOR_REG_TEMP);
  Wire.endTransmission();
  delay(50);

  Wire.requestFrom(I2C_ADDRESS, 2);
  if (Wire.available() == 2) {
    byte highByte = Wire.read();
    byte lowByte = Wire.read();

    int adcValue = ((int)highByte << 8) | (int)lowByte;
    if (adcValue > 32768) {
      return (adcValue - 65536) / 256.0;
    } else {
      return adcValue / 256.0;
    }
  }
  return -1;
}
