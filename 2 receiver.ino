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
#define TOPIC_WIFI_STATUS "helmet/sender/wifi_status"

WiFiClientSecure espClient;
PubSubClient client(espClient);

bool blinking = false;
unsigned long lastBlinkTime = 0;
const int blinkInterval = 150;
bool lastHelmetWorn = true;
unsigned long lastHelmetWornAlertTime = 0;
const unsigned long helmetWornAlertDebounce = 1000;
bool gasDetected = false;
String senderSSID = ""; // Lưu SSID của sender
unsigned long lastWiFiStatusPrint = 0;
const unsigned long WIFI_STATUS_INTERVAL = 10000; // 10s
String currentSSID = ""; // Lưu SSID hiện tại của receiver

void connectBestWiFi() {
  Serial.println("🔍 [WiFi] Scanning...");
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
    Serial.println("📶 [WiFi] Attempting to connect to: " + String(knownNetworks[bestIndex].ssid));
    WiFi.begin(knownNetworks[bestIndex].ssid, knownNetworks[bestIndex].password);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      if (currentSSID != WiFi.SSID()) {
        Serial.println("\n🟢 [WiFi] Connected to: " + String(WiFi.SSID()) + " | IP: " + WiFi.localIP().toString());
        currentSSID = WiFi.SSID();
      } else {
        Serial.println("\n🟢 [WiFi] Connected to: " + String(WiFi.SSID()));
      }
    } else {
      Serial.println("\n❌ [WiFi] Failed to connect to WiFi after timeout");
    }
  } else {
    Serial.println("❌ [WiFi] No known WiFi found");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  DynamicJsonDocument doc(128);
  if (deserializeJson(doc, message)) {
    Serial.println("❌ [MQTT] Failed to parse JSON");
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
    Blynk.virtualWrite(V2, gas ? 1 : 0);
    Blynk.virtualWrite(V3, ledHelmet);
    Blynk.virtualWrite(V4, force);
    digitalWrite(LED_TEMP_HUM, !helmetWorn ? HIGH : LOW);

    Serial.println("📊 [Sensor] T=" + String(temp) + "C, H=" + String(hum) + "%, Gas=" + String(gas ? "YES" : "NO") + ", Force=" + String(force) + ", LED Helmet=" + String(ledHelmet ? "ON" : "OFF") + ", Helmet=" + String(helmetWorn ? "Worn" : "Not Worn"));

    if (gas) {
      if (!gasDetected) {
        gasDetected = true;
        Blynk.virtualWrite(V2, 1);
        Blynk.virtualWrite(V5, 1);
        Serial.println("🚨 Gas detected! LED D2 turned ON, event signaled on V5");
      }
    } else {
      if (gasDetected) {
        gasDetected = false;
        Blynk.virtualWrite(V2, 0);
        Blynk.virtualWrite(V5, 0);
        Serial.println("✅ Gas cleared. LED D2 turned OFF, event cleared on V5");
      }
    }
  }
  else if (String(topic) == TOPIC_ALERT) {
    if (doc.containsKey("alert") && doc["alert"]) {
      bool triggerBuzzer = false;
      String alertMsg = "⚠️ [Alert]: ";
      if (doc.containsKey("temp_hum") && doc["temp_hum"]) {
        alertMsg += "Temp/Hum, ";
        triggerBuzzer = true;
      }
      if (doc.containsKey("gas") && doc["gas"]) {
        alertMsg += "Gas, ";
        if (!gasDetected) {
          gasDetected = true;
          Blynk.virtualWrite(V2, 1);
          Blynk.virtualWrite(V5, 1);
          Serial.println("🚨 Gas alert from TOPIC_ALERT! LED D2 turned ON, event signaled on V5");
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
            Blynk.virtualWrite(V5, 1);
            Serial.println("🚨 Helmet not worn signaled on V5");
          } else {
            Blynk.virtualWrite(V5, 0);
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
    } else {
      if (gasDetected) {
        gasDetected = false;
        Blynk.virtualWrite(V2, 0);
        Blynk.virtualWrite(V5, 0);
        Serial.println("✅ Gas alert cleared from TOPIC_ALERT. LED D2 turned OFF, event cleared on V5");
      }
    }
  }
  else if (String(topic) == TOPIC_BUTTON_STATUS) {
    if (doc["button"] == "pressed") {
      int pressCount = doc["pressCount"];
      // In thông báo khi nhận được tín hiệu bấm nút từ sender
      Serial.println("[ButtonS] Received press, Count: " + String(pressCount));
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
    }
    if (doc.containsKey("led_d2") && doc["led_d2"] == true) {
      gasDetected = true;
      digitalWrite(LED_BUTTON, HIGH);
      Blynk.virtualWrite(V2, 1);
      Blynk.virtualWrite(V5, 1);
      Serial.println("🚨 Gas detected via TOPIC_MANUAL_TRIGGER! LED D2 turned ON, event signaled on V5");
    }
  }
  else if (String(topic) == TOPIC_WIFI_STATUS) {
    if (doc.containsKey("ssid")) {
      senderSSID = doc["ssid"].as<String>();
      Serial.println("[WiFi] Sender connected to: " + senderSSID);
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
      client.subscribe(TOPIC_WIFI_STATUS);
      Serial.println("📡 [MQTT] Connected");
    } else {
      Serial.println("❌ [MQTT] Failed to connect, retrying in 5 seconds...");
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
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V5, 0);
  Serial.println("[SETUP] Setup completed!");
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  Blynk.run();
  checkManualButton();

  // In SSID của sender mỗi 10s
  unsigned long now = millis();
  if (now - lastWiFiStatusPrint >= WIFI_STATUS_INTERVAL && senderSSID != "") {
    Serial.println("[WiFi] Sender connected to: " + senderSSID);
    lastWiFiStatusPrint = now;
  }

  if (blinking) {
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




