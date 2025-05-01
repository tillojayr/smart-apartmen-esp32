#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h> // Include ArduinoJson library
#include <map>
#include <HTTPClient.h>

// Wi-Fi credentials
// const char* ssid = "DESKTOP-7167JBJ 0804";
// const char* password = "12345678";

const char* ssid = "SM-S901W7054";
const char* password = "tobz9999";

// const char* ssid = "LAPTOP-5CEC6IRB 0434";
// const char* password = "123456789";
// 192.168.39.223
// 192.168.137.231

// WebSocket server details
// const char* websocket_server = "192.168.137.1"; // Replace with your server IP or hostname
// const int websocket_port = 8080;
// const char* websocket_path = "/app/jf9neyn7tw8z7zvariyv"; // Replace with your app ID

const char* websocket_server = "jayrtillo.online"; // Replace with your server IP or hostname
const int websocket_port = 443;
const char* websocket_path = "/app/jf9neyn7tw8z7zvariyv"; // Replace with your app ID

// WebSocket client object
WebSocketsClient webSocket;

// const char* serverUrl = "http://192.168.137.1/api/v1/room/save"; // Replace with your server URL
// const char* getServerUrl = "http://192.168.137.1/api/v1/room/all-state"; // Replace with your server URL

const char* serverUrl = "https://jayrtillo.online/api/v1/room/save"; // Replace with your server URL
const char* getServerUrl = "https://jayrtillo.online/api/v1/room/all-state"; // Replace with your server URL

// const char* token = "R8F0zAWi8gflGJHjewDgOr0JHbUCNtHA2ROKSmzo8EyASXJW9w8Y0kJ6DZKBAArt";
const char* token = "MQX730gLTIsgGEE8eTut7vjGKCFlLYwCSsUqAHqS5Cst7yUD0F2YhGKumfFDC84w";

// Define relay pin mappings
std::map<String, int> relayMap = {
    {"1a", 12},
    {"1b", 14},
    {"2a", 27},
    {"2b", 26},
    {"3a", 22},
    {"3b", 33},
    {"4a", 32},
    {"4b", 23}
};

String relays[] = {"1a", "1b", "2a", "2b", "3a", "3b", "4a", "4b"};

const int capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(3) + 100;

unsigned long previousMillis = 0;
const long interval = 5000; // 1 minute

const int NUM_RELAYS = 8;
int relayStates[NUM_RELAYS];

const int red = 16;
const int green = 17;
const int blue = 5;

int state = 0;
void setup() {
  // Start Serial communication
  Serial.begin(9600);
  pinMode(12, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(27, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(22, OUTPUT);
  pinMode(33, OUTPUT);
  pinMode(32, OUTPUT);
  pinMode(23, OUTPUT);

  pinMode(16, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(5, OUTPUT);

  digitalWrite(red, HIGH);
  digitalWrite(blue, LOW);
  digitalWrite(green, LOW);
  // Set all relay pins as OUTPUT
  // for (auto const &pair : relayMap) {
  //   pinMode(pair.second, OUTPUT);
  // }

  delay(5000);
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");

  digitalWrite(blue, HIGH);
  digitalWrite(red, LOW);
  digitalWrite(green, LOW);

  // Connect to WebSocket server
  webSocket.beginSSL(websocket_server, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);

  // Enable debugging and heartbeat
  webSocket.setReconnectInterval(5000); // Reconnect every 5 seconds
  // webSocket.enableHeartbeat(15000, 3000, 2); // Enable heartbeat

  getInitialStates();
}

void loop() {
  // Maintain WebSocket connection
  webSocket.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (Serial.available() > 0) {
      String data = Serial.readStringUntil('\n'); // Read data until newline
      Serial.print("Data received: ");
      Serial.println(data); // Print data to Serial Monitor
      Serial.println("Sending request");
      sendHttpPostRequest(data);
      Serial.println("post request sent!");
    }
  }
}

void sendHttpPostRequest(String jsonPayload) {
  // Serial.println(jsonPayload);
  // Create an HTTPClient object
  HTTPClient http;

  // Start the HTTP POST request
  http.begin(serverUrl);

  // Set headers (if needed)
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(token));

  // Create a JSON payload
  // String jsonPayload = "{\"roomId\":\""++"\",\"voltage\":\""++"\",\"current\":\""++"\",\"consumed\":\""++"\"}";

  // Send the POST request with the JSON payload
  int httpResponseCode = http.POST(jsonPayload);
  Serial.println(jsonPayload);

  // Check the response code
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    // Get the response payload
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);

    String response = http.getString();
    Serial.println("Response: " + response);
  }

  // End the HTTP request
  http.end();
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket disconnected");
      digitalWrite(red, HIGH);
      digitalWrite(blue, LOW);
      digitalWrite(green, LOW);
      break;

    case WStype_CONNECTED:
      Serial.println("WebSocket connected");
      digitalWrite(green, HIGH);
      digitalWrite(red, LOW);
      digitalWrite(blue, LOW);
      // Subscribe to the channel after connection is established
      subscribeToChannel();
      break;

    case WStype_TEXT:
      Serial.print("Received message: ");
      Serial.println((char*)payload);
      // Parse the JSON payload
      parseJsonPayload((char*)payload);
      break;

    case WStype_ERROR:
      Serial.println("WebSocket error");
      digitalWrite(red, HIGH);
      digitalWrite(blue, LOW);
      digitalWrite(green, LOW);
      break;
  }
}

void subscribeToChannel() {
  // Send a subscription message to the WebSocket server
  String subscribeMessage = "{\"event\":\"pusher:subscribe\",\"data\":{\"channel\":\"ApartmentChannel\"}}";
  webSocket.sendTXT(subscribeMessage);
  Serial.println("Subscribed to ApartmentChannel");
}

void parseJsonPayload(char* payload) {
  // Create a JSON document with sufficient size

    // Create a JSON document
    StaticJsonDocument<512> doc;

    // Deserialize JSON
    DeserializationError error = deserializeJson(doc, payload);

    // Check for errors
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Extract the nested JSON string inside "data"
    const char* dataString = doc["data"];

    // Create another JSON document for the nested data
    StaticJsonDocument<256> nestedDoc;

    // Deserialize the nested JSON
    error = deserializeJson(nestedDoc, dataString);

    if (error) {
        Serial.print("Nested JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Extract values
    const char* apartmentId = nestedDoc["message"]["apartmentId"];
    const char* relay = nestedDoc["message"]["relay"];
    int state = nestedDoc["message"]["state"];

    // Print values
    Serial.print("Apartment ID: ");
    Serial.println(apartmentId);
    
    Serial.print("Relay: ");
    Serial.println(relay);
    
    Serial.print("State: ");
    Serial.println(state);

    // String relayStr = String(relay);
    controlRelay(relay, state);
}

void splitFixedStates(String data, char delimiter, int states[NUM_RELAYS]) {
  int startIndex = 0;
  int endIndex;
  
  for (int i = 0; i < NUM_RELAYS - 1; i++) {
    endIndex = data.indexOf(delimiter, startIndex);
    states[i] = data.substring(startIndex, endIndex).toInt();
    startIndex = endIndex + 1;
  }

  // Last value (no more delimiter after it)
  states[NUM_RELAYS - 1] = data.substring(startIndex).toInt();
}

void getInitialStates() {
  HTTPClient http;

  http.begin(getServerUrl); // Make sure getServerUrl is defined
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(token)); // Make sure token is defined

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    String response = http.getString();
    Serial.println("Response: " + response);

    // Parse the JSON response
    StaticJsonDocument<200> doc;  // Increase size if needed
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      String stateData = doc["data"].as<String>();
      Serial.println("Parsed State Data: " + stateData);

      // Call split function
      splitFixedStates(stateData, '+', relayStates);

      // Debug print states
      for (int i = 0; i < NUM_RELAYS; i++) {
        Serial.print("Relay ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.println(relayStates[i]);

        int pin = relayMap[relays[i]];
        digitalWrite(pin, relayStates[i]);
      }
    } else {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    }

  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Response: " + http.getString());
  }

  http.end();
}


void controlRelay(String relay, int state) {
    int pin = relayMap[relay];
    Serial.println(pin);
    if(state == 1){
      digitalWrite(pin, HIGH);
    }
    else{
      digitalWrite(pin, LOW);
    }
}
