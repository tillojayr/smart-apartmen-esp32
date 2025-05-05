#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <map>
#include <HTTPClient.h>
#include <EEPROM.h>

#define BUTTON_PIN 21 // GPIO for push button
#define EEPROM_SIZE 1

struct WiFiConfig {
  const char* ssid;
  const char* password;
};

WiFiConfig wifiConfigs[] = {
  {"SM-S901W7054", "tobz9999"},
  {"Benjie", "benjiegwapo"},
  {"Fucute", "ljay12345"},
  {"Hotspot1", "password1"},
  {"Hotspot2", "password2"}
};

int wifiIndex = 0;

std::map<String, int> relayMap = {
    {"1a", 12}, {"1b", 14}, {"2a", 27}, {"2b", 26},
    {"3a", 22}, {"3b", 33}, {"4a", 32}, {"4b", 23}
};

const char* websocket_server = "jayrtillo.online";
const int websocket_port = 443;
const char* websocket_path = "/app/jf9neyn7tw8z7zvariyv";
WebSocketsClient webSocket;

const char* serverUrl = "https://jayrtillo.online/api/v1/room/save";
const char* getServerUrl = "https://jayrtillo.online/api/v1/room/all-state";
const char* token = "MQX730gLTIsgGEE8eTut7vjGKCFlLYwCSsUqAHqS5Cst7yUD0F2YhGKumfFDC84w";


String relays[] = {"1a", "1b", "2a", "2b", "3a", "3b", "4a", "4b"};
const int NUM_RELAYS = 8;
int relayStates[NUM_RELAYS];

const int red = 16, green = 17, blue = 5;
unsigned long previousMillis = 0;
const long interval = 5000;

unsigned long lastButtonPress = 0;

void setup() {
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(red, OUTPUT); pinMode(green, OUTPUT); pinMode(blue, OUTPUT);

  for (auto const& pair : relayMap) pinMode(pair.second, OUTPUT);

  digitalWrite(red, HIGH); digitalWrite(blue, LOW); digitalWrite(green, LOW);
  delay(5000);

  wifiIndex = EEPROM.read(0);
  if (wifiIndex < 0 || wifiIndex > 3) wifiIndex = 0;

  connectToWiFi();
  connectWebSocket();
  getInitialStates();
}

void loop() {
  webSocket.loop();

  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > 1000) {
    lastButtonPress = millis();
    wifiIndex = (wifiIndex + 1) % 5;
    EEPROM.write(0, wifiIndex);
    EEPROM.commit();
    Serial.print("Switching to WiFi #"); Serial.println(wifiIndex);
    connectToWiFi();
    connectWebSocket();
  }

  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    if (Serial.available()) {
      String data = Serial.readStringUntil('\n');
      sendHttpPostRequest(data);
    }
  }
}

void connectToWiFi() {
  WiFi.disconnect(true);
  delay(1000);
  WiFi.begin(wifiConfigs[wifiIndex].ssid, wifiConfigs[wifiIndex].password);

  Serial.print("Connecting to "); Serial.println(wifiConfigs[wifiIndex].ssid);
  digitalWrite(red, HIGH); digitalWrite(blue, LOW); digitalWrite(green, LOW);
  while (WiFi.status() != WL_CONNECTED) {
    if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > 1000) {
      lastButtonPress = millis();
      wifiIndex = (wifiIndex + 1) % 5;
      EEPROM.write(0, wifiIndex);
      EEPROM.commit();
      Serial.print("Switching to WiFi #"); Serial.println(wifiIndex);
      connectToWiFi();
      connectWebSocket();
    }
    delay(50); Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  digitalWrite(blue, HIGH); digitalWrite(red, LOW); digitalWrite(green, LOW);
}

void connectWebSocket() {
  webSocket.beginSSL(websocket_server, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      digitalWrite(red, HIGH); digitalWrite(blue, LOW); digitalWrite(green, LOW);
      break;
    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      digitalWrite(green, HIGH); digitalWrite(red, LOW); digitalWrite(blue, LOW);
      subscribeToChannel();
      break;
    case WStype_TEXT:
      Serial.print("Received message: "); Serial.println((char*)payload);
      parseJsonPayload((char*)payload);
      break;
    case WStype_ERROR:
      Serial.println("WebSocket error");
      digitalWrite(red, HIGH); digitalWrite(blue, LOW); digitalWrite(green, LOW);
      break;
  }
}

void subscribeToChannel() {
  String msg = "{\"event\":\"pusher:subscribe\",\"data\":{\"channel\":\"ApartmentChannel\"}}";
  webSocket.sendTXT(msg);
  Serial.println("Subscribed to ApartmentChannel");
}

void parseJsonPayload(char* payload) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload)) return;
  const char* dataString = doc["data"];
  StaticJsonDocument<256> nestedDoc;
  if (deserializeJson(nestedDoc, dataString)) return;

  const char* relay = nestedDoc["message"]["relay"];
  int state = nestedDoc["message"]["state"];
  controlRelay(relay, state);
}

void controlRelay(String relay, int state) {
  int pin = relayMap[relay];
  digitalWrite(pin, state == 1 ? HIGH : LOW);
}

void sendHttpPostRequest(String jsonPayload) {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(token));

  int code = http.POST(jsonPayload);
  Serial.println(jsonPayload);
  Serial.println("Response code: " + String(code));
  Serial.println(http.getString());
  http.end();
}

void getInitialStates() {
  HTTPClient http;
  http.begin(getServerUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(token));
  int code = http.GET();
  if (code > 0) {
    String resp = http.getString();
    StaticJsonDocument<200> doc;
    if (!deserializeJson(doc, resp)) {
      String stateData = doc["data"].as<String>();
      splitFixedStates(stateData, '+', relayStates);
      for (int i = 0; i < NUM_RELAYS; i++) {
        int pin = relayMap[relays[i]];
        digitalWrite(pin, relayStates[i]);
      }
    }
  }
  http.end();
}

void splitFixedStates(String data, char delimiter, int states[NUM_RELAYS]) {
  int start = 0, end;
  for (int i = 0; i < NUM_RELAYS - 1; i++) {
    end = data.indexOf(delimiter, start);
    states[i] = data.substring(start, end).toInt();
    start = end + 1;
  }
  states[NUM_RELAYS - 1] = data.substring(start).toInt();
}
