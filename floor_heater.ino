#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "FS.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define t_sensor  4
#define lineIO1  13
#define lineIO2  12
#define lineIO3  14
#define lineIO4  16
#define CONNECTION_TIMEOUT 10

String PRIVATE_KEY = "RGVueXMgS2FycG92LCBvbmNlMmdvQGdtYWlsLmNvbQ==";
String pinCode = "";

char* ssid;
char* password;
char t_ssid[64];
char t_psk[64];

ESP8266WebServer server(80);

OneWire oneWire(t_sensor);
DallasTemperature DS18B20(&oneWire);

int firstLine = 0;
int secondLine = 0;
int thirdLine = 0;
int fourthLine = 0;
int miliss_counter = 0;
float temperature = 0;
String deviceIpAddr = "";

void handleRoot() {
  File index = SPIFFS.open("/index.html", "r");
  server.streamFile(index, "text/html");
  index.close();
}

void handleSetupConfig() {
  File wifisetup = SPIFFS.open("/wifisetup.html", "r");
  server.streamFile(wifisetup, "text/html");
  wifisetup.close();
}


void setupClient() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int c_counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("-");
    if (CONNECTION_TIMEOUT < c_counter) {
      setupAP();
      return;
    }
    c_counter++;
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  deviceIpAddr = WiFi.localIP().toString();
  Serial.println(deviceIpAddr);
  server.on("/", handleRoot);
  server.on("/login", login);
  server.on("/getstate", getState);
  server.on("/setstate", setState);
}

void setupAP() {
  WiFi.disconnect();
  Serial.println("Setup AP");
  WiFi.softAP("Floor Heater Setup");
  server.on("/", handleSetupConfig);
  server.on("/config", setConfig);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {
  DS18B20.begin();
  pinMode(lineIO1, OUTPUT);
  pinMode(lineIO2, OUTPUT);
  pinMode(lineIO3, OUTPUT);
  pinMode(lineIO4, OUTPUT);
  digitalWrite(lineIO1, 0);
  digitalWrite(lineIO2, 0);
  digitalWrite(lineIO3, 0);
  digitalWrite(lineIO4, 0);
  DS18B20.requestTemperatures();
  temperature = DS18B20.getTempCByIndex(0);
  Serial.begin(115200);
  delay(1000);
  Serial.println("Init");
  SPIFFS.begin();
  File conf = SPIFFS.open("/conf", "r");

  int line_counter = 0;
  while (conf.available()) {
    String line = conf.readStringUntil('\n');
    Serial.println(line);
    switch (line_counter) {
      case 0:
        line.toCharArray(t_ssid, line.length());
        ssid = t_ssid;
        break;
      case 1:
        line.toCharArray(t_psk, line.length());
        password = t_psk;
        break;
      case 2:
        pinCode = line;
        pinCode.trim();
        break;
    }
    line_counter++;
  }
  if (String(ssid).length() == 0 && pinCode.length() == 0 && String(password).length() == 0) {
    setupAP();
  } else {
    setupClient();
  }

  if (MDNS.begin("floor-heater")) {
    Serial.println("MDNS responder started");
  }

  server.onNotFound(handleNotFound);
  server.begin();
}

void loop(void) {
  server.handleClient();
  if (miliss_counter >= 6000000) {
    DS18B20.requestTemperatures();
    temperature = DS18B20.getTempCByIndex(0) - 6;
    miliss_counter = 0;
  }
  miliss_counter++;
}

void login() {
  String response = "";
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "pin") {
      Serial.println("received:" + server.arg(i) + " existing:" + pinCode);
      if (pinCode.equals(String(server.arg(i)))) {
        response = PRIVATE_KEY;
      }
    }
  }
  Serial.println("response:" + response);
  server.send(response.length() > 0 ? 200 : 401, "text/plain", response);
}

void setConfig() {
  String error = "";
  String l_ssid = "";
  String l_pswd = "";
  String l_pin = "";
  if (server.args() != 3) {
    error += "AP name, password & pin cant be empty \n";
  }
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "ssid") {
      l_ssid = server.arg(i);
      if (l_ssid.length() == 0) {
        error += "Can't connect to hidden network \n";
      }
    }
    if (server.argName(i) == "password") {
      l_pswd = server.arg(i);
      if (l_pswd.length() < 8) {
        error += "Network should be WPA/WPA2 secured.Password should be 8 symbols at least \n";
      }
    }
    if (server.argName(i) == "pin") {
      l_pin = server.arg(i);
      if (l_pin.length() != 4) {
        error += "Pincode should be 4 digits \n";
      }
    }
  }
  if (error.length() == 0) {
    Serial.println("Save credentials");
    File conf = SPIFFS.open("/conf", "w");
    if (!conf) {
      Serial.println("file creation failed");
    }
    conf.println(l_ssid);
    conf.println(l_pswd);
    conf.println(l_pin);
    conf.close();
    server.send(200, "text/plain", "ok");
    delay(2000);
    ESP.restart();
  } else {
    server.send(404, "text/plain", error);
  }
}

void setState() {
  bool secure = false;
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "key") {
      if (PRIVATE_KEY.equals(String(server.arg(i)))) {
        secure = true;
      }
    }
  }
  if (secure) {
    if (server.args() > 0) {
      for (uint8_t i = 0; i < server.args(); i++) {
        if (server.argName(i) == "line1") {
          firstLine = server.arg(i).toInt();
        }
        if (server.argName(i) == "line2") {
          secondLine = server.arg(i).toInt();
        }
        if (server.argName(i) == "line3") {
          thirdLine = server.arg(i).toInt();
        }
        if (server.argName(i) == "line4") {
          fourthLine = server.arg(i).toInt();
        }
      }
    }

    digitalWrite(lineIO1, firstLine);
    digitalWrite(lineIO2, secondLine);
    digitalWrite(lineIO3, thirdLine);
    digitalWrite(lineIO4, fourthLine);
    server.send(200, "text/plain", "ok");
  } else {
    server.send(401, "text/plain", "Not authorized");
  }
}

void getState() {
  String response = "";
  char temp[100];
  sprintf(temp, "{\"name\":\"line1\", \"state\": %d},", firstLine);
  response += temp;
  sprintf(temp, "{\"name\":\"line2\", \"state\": %d},", secondLine);
  response += temp;
  sprintf(temp, "{\"name\":\"line3\", \"state\": %d},", thirdLine);
  response += temp;
  sprintf(temp, "{\"name\":\"line4\", \"state\": %d},", fourthLine);
  response += temp;
  sprintf(temp, "{\"name\":\"temperature\", \"state\": \"%f\"},", temperature);
  response += temp;
  response += "{\"name\":\"ip_addr\", \"state\": \"" + deviceIpAddr + "\"},";
  response += "{\"name\":\"rssi\", \"state\": " + String(WiFi.RSSI()) + "}";
  response = String("{\"state\": [" + response + "]}");

  server.send(200, "text/plain", response);
}

