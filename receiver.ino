#define BLYNK_TEMPLATE_ID "TMPL6KB0upDKf"
#define BLYNK_TEMPLATE_NAME "Safety helmet"
#define BLYNK_AUTH_TOKEN "EEy0_MJSocEiUMnOm5Wvrhm9S1VbBtGo"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp8266.h>

#define LED_TEMP_HUM D1
#define LED_BUTTON D2
#define BUTTON_MANUAL D3
#define LED_BLINK D7
#define BUZZER D0

char auth[] = BLYNK_AUTH_TOKEN;

struct KnownNetwork {
  const char* ssid;
  const char* password;
};

KnownNetwork knownNetworks[] = {
  {"Goldenland P402", "golden1234"},
  {"WIFI_B", "password123"},
  {"A1306", "thanha1306"}
};
const int knownCount = sizeof(knownNetworks) / sizeof(KnownNetwork);

const char* mqtt_server = "d7ca8bc2882f48479e8f50e7636d7da9.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "Hoang_Long";
const char* mqtt_password = "Long1234";

#define TOPIC_DHT "helmet/sender/data"
#define TOPIC_BUTTON_STATUS "helmet/sender/status"
#define TOPIC_MANUAL_TRIGGER "helmet/sender/ledcontrol"
#define TOPIC_BUTTON_R "helmet/receiver/request"
#define TOPIC_ALERT "helmet/sender/alert"

// Google Apps Script Web App URL
const char* googleScriptURL = "script.google.com";
const char* googleScriptPath = "/macros/s/AKfycbwfE1L7zq6TZSROIQA3xy-Zg2lt1P_4e-4EBtvRxmHpbxYtcDSv0pIpt70Ien5cKRVh/exec";

WiFiClientSecure espClient;
PubSubClient client(espClient);

bool blinking = false;
unsigned long lastBlinkTime = 0;
const int blinkInterval = 150;
bool lastHelmetWorn = true;
unsigned long lastHelmetWornAlertTime = 0;
const unsigned long helmetWornAlertDebounce = 1000;
bool gasDetected = false; // Track gas detection status

// Function to URL encode a string
String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// Function to send data to Google Sheet
void sendToGoogleSheet(String dataType, String data) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ [Google Sheet] WiFi not connected, cannot send data");
    return;
  }

  // URL encode the data to handle special characters
  String encodedData = urlEncode(data);
  String encodedDataType = urlEncode(dataType);

  if (espClient.connect(googleScriptURL, 443)) {
    String url = String(googleScriptPath) + "?dataType=" + encodedDataType + "&data=" + encodedData;
    Serial.println("[Google Sheet] Sending request: " + url);
    espClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: " + googleScriptURL + "\r\n" +
                   "Connection: close\r\n\r\n");
    while (espClient.connected()) {
      String line = espClient.readStringUntil('\n');
      if (line == "\r") break;
    }
    String response = espClient.readString();
    Serial.println("[Google Sheet] Response: " + response);
  } else {
    Serial.println("❌ [Google Sheet] Connection to Google Script failed");
  }
  espClient.stop();
}

void connectBestWiFi() {
  Serial.println("🔍 Scanning WiFi...");
  int n = WiFi.scanNetworks();
  int bestIndex = -1, bestRSSI = -100;

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < knownCount; ++j) {
      if (WiFi.SSID(i) == knownNetworks[j].ssid && WiFi.RSSI(i) > bestRSSI) {
        bestIndex = j;
        bestRSSI = WiFi.RSSI(i);
      }
    }
  }

  if (bestIndex >= 0) {
    Serial.println("Attempting to connect to: " + String(knownNetworks[bestIndex].ssid));
    WiFi.begin(knownNetworks[bestIndex].ssid, knownNetworks[bestIndex].password);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n🚀 WiFi Connected: " + String(knownNetworks[bestIndex].ssid));
    } else {
      Serial.println("\n❌ Failed to connect to WiFi after timeout");
    }
  } else {
    Serial.println("❌ No known WiFi found");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  DynamicJsonDocument doc(128);
  if (deserializeJson(doc, message)) {
    Serial.println("Failed to parse JSON");
    return;
  }

  unsigned long now = millis();

  if (String(topic) == TOPIC_DHT) {
    float temp = doc["temperature"];
    float hum = doc["humidity"];
    bool gas = doc["gas_detected"];
    int force = doc["force"];
    bool helmetWorn = doc["helmet_worn"];
    int ledHelmet = doc["led_helmet"];

    Blynk.virtualWrite(V0, temp);      
    Blynk.virtualWrite(V1, hum);      
    Blynk.virtualWrite(V2, gas ? 1 : 0); // Update Gas Level LED (V2)
    Blynk.virtualWrite(V3, ledHelmet);
    Blynk.virtualWrite(V4, force);    
    digitalWrite(LED_TEMP_HUM, !helmetWorn ? HIGH : LOW);

    String sensorData = "T=" + String(temp) + "C, H=" + String(hum) + "%, Gas=" + String(gas ? "YES" : "NO") + ", Force=" + String(force) + ", LED Helmet=" + String(ledHelmet ? "ON" : "OFF") + ", Helmet=" + String(helmetWorn ? "Worn" : "Not Worn");
    Serial.println("📊 Sensor Data: " + sensorData);
    // Send sensor data to Google Sheet
    sendToGoogleSheet("sensor", sensorData);

    // Handle gas detection for LED and event
    if (gas) {
      if (!gasDetected) {
        gasDetected = true;
        Blynk.virtualWrite(V2, 1); // Ensure Gas Level LED is on
        Blynk.virtualWrite(V5, 1); // Signal gas alert event
        Serial.println("🚨 Gas detected! Gas Level LED turned ON, event signaled on V5");
        // Send gas detection alert to Google Sheet
        sendToGoogleSheet("alert", "Gas detected");
      }
    } else {
      if (gasDetected) {
        gasDetected = false;
        Blynk.virtualWrite(V2, 0); // Turn off Gas Level LED
        Blynk.virtualWrite(V5, 0); // Clear gas alert event
        Serial.println("Gas cleared. Gas Level LED turned OFF, event cleared on V5");
        // Send gas cleared alert to Google Sheet
        sendToGoogleSheet("alert", "Gas cleared");
      }
    }
  }
  else if (String(topic) == TOPIC_ALERT) {
    if (doc.containsKey("alert") && doc["alert"]) {
      bool triggerBuzzer = false;
      String alertMsg = "⚠️ Alert: ";
      if (doc.containsKey("temp_hum") && doc["temp_hum"]) {
        alertMsg += "Temp/Hum, ";
        triggerBuzzer = true;
      }
      if (doc.containsKey("gas") && doc["gas"]) {
        alertMsg += "Gas, ";
        if (!gasDetected) {
          gasDetected = true;
          Blynk.virtualWrite(V2, 1); // Ensure Gas Level LED is on
          Blynk.virtualWrite(V5, 1); // Signal gas alert event
          Serial.println("🚨 Gas alert from TOPIC_ALERT! Gas Level LED turned ON, event signaled on V5");
          // Send gas alert to Google Sheet
          sendToGoogleSheet("alert", "Gas alert from TOPIC_ALERT");
        }
      }
      if (doc.containsKey("impact") && doc["impact"]) {
        alertMsg += "Impact, ";
        triggerBuzzer = true;
      }
      if (doc.containsKey("helmet_worn")) {
        bool helmetWorn = doc["helmet_worn"];
        if (helmetWorn != lastHelmetWorn && now - lastHelmetWornAlertTime >= helmetWornAlertDebounce) {
          lastHelmetWorn = helmetWorn;
          lastHelmetWornAlertTime = now;
          digitalWrite(LED_TEMP_HUM, !helmetWorn ? HIGH : LOW);
          if (!helmetWorn) {
            alertMsg += "Helmet Not Worn, ";
            triggerBuzzer = true;
            Blynk.virtualWrite(V5, 1); // Signal helmet not worn alert
            Serial.println("Helmet not worn signaled on V5");
            // Send helmet not worn alert to Google Sheet
            sendToGoogleSheet("alert", "Helmet not worn");
          } else {
            Blynk.virtualWrite(V5, 0); // Clear alert if helmet is worn
          }
        }
      }
      if (triggerBuzzer) {
        digitalWrite(LED_TEMP_HUM, HIGH);
        digitalWrite(BUZZER, LOW);
        delay(1500);
        digitalWrite(BUZZER, HIGH);
        digitalWrite(LED_TEMP_HUM, !lastHelmetWorn ? HIGH : LOW);
      }
      Serial.println(alertMsg);
      // Send alert message to Google Sheet
      sendToGoogleSheet("alert", alertMsg);
    } else {
      if (gasDetected) {
        gasDetected = false;
        Blynk.virtualWrite(V2, 0); // Turn off Gas Level LED
        Blynk.virtualWrite(V5, 0); // Clear alert
        Serial.println("Gas alert cleared from TOPIC_ALERT. Gas Level LED turned OFF, event cleared on V5");
        // Send gas alert cleared to Google Sheet
        sendToGoogleSheet("alert", "Gas alert cleared from TOPIC_ALERT");
      }
    }
  }
  else if (String(topic) == TOPIC_BUTTON_STATUS) {
    if (doc["button"] == "pressed") {
      int pressCount = doc["pressCount"];
      digitalWrite(LED_BUTTON, HIGH);
      delay(200);
      digitalWrite(LED_BUTTON, LOW);
      if (pressCount == 1) {
        blinking = true;
      } else if (pressCount >= 3) {
        blinking = false;
        digitalWrite(LED_BLINK, LOW);
        digitalWrite(BUZZER, HIGH);
      }
    }
  }
  else if (String(topic) == TOPIC_MANUAL_TRIGGER) {
    if (doc["gpio16"] == "on") {
      digitalWrite(LED_BUTTON, HIGH);
      delay(200);
      digitalWrite(LED_BUTTON, LOW);
      Serial.println("🚨 Manual trigger activated");
      // Send manual trigger to Google Sheet
      sendToGoogleSheet("alert", "Manual trigger activated");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP_Receiver_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      client.subscribe(TOPIC_DHT);
      client.subscribe(TOPIC_BUTTON_STATUS);
      client.subscribe(TOPIC_MANUAL_TRIGGER);
      client.subscribe(TOPIC_ALERT);
      Serial.println("📡 MQTT Connected");
    } else {
      Serial.println("Failed to connect to MQTT, retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void checkManualButton() {
  static bool lastState = HIGH;
  bool current = digitalRead(BUTTON_MANUAL);
  if (lastState == HIGH && current == LOW) {
    digitalWrite(LED_BUTTON, HIGH);
    delay(100);
    digitalWrite(LED_BUTTON, LOW);
    client.publish(TOPIC_MANUAL_TRIGGER, "{\"gpio16\":\"on\"}");
    client.publish(TOPIC_BUTTON_R, "{\"buttonR\":\"pressed\"}");
    delay(50);
  }
  lastState = current;
}

BLYNK_WRITE(V6) {
  DynamicJsonDocument doc(64);
  doc["gpio16"] = "on";
  char msg[64];
  serializeJson(doc, msg);
  client.publish(TOPIC_MANUAL_TRIGGER, msg);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_TEMP_HUM, OUTPUT);
  pinMode(LED_BUTTON, OUTPUT);
  pinMode(BUTTON_MANUAL, INPUT_PULLUP);
  pinMode(LED_BLINK, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED_TEMP_HUM, LOW);
  digitalWrite(LED_BUTTON, LOW);
  digitalWrite(LED_BLINK, LOW);
  digitalWrite(BUZZER, HIGH);
  connectBestWiFi();
  Blynk.config(auth);
  Blynk.connect();
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  // Initialize Blynk virtual pins to ensure correct initial state
  Blynk.virtualWrite(V2, 0); // Gas Level LED off
  Blynk.virtualWrite(V5, 0); // Clear any initial alert
  Serial.println("Setup completed!");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  Blynk.run();
  checkManualButton();
  if (blinking) {
    unsigned long now = millis();
    if (now - lastBlinkTime >= blinkInterval) {
      digitalWrite(LED_BLINK, !digitalRead(LED_BLINK));
      lastBlinkTime = now;
    }
    digitalWrite(BUZZER, digitalRead(LED_BLINK) == HIGH ? LOW : HIGH);
  } else {
    digitalWrite(LED_BLINK, LOW);
    digitalWrite(BUZZER, HIGH);
  }
  digitalWrite(LED_BUTTON, gasDetected ? HIGH : LOW);
}


