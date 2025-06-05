



#include <WiFi.h>
#include <DHTesp.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


// Cấu hình chân GPIO
#define DHTpin 15
#define BUTTON_PIN 4
#define LED_FEEDBACK_PIN 16
#define LED_REMOTE_PIN 17
#define NEW_BUZZER_PIN 26
#define MQ2_PIN 27
#define FSR_PIN 32


// Cấu hình thời gian (ms)
const unsigned long UPDATE_INTERVAL = 5000;
const unsigned long DEBOUNCE_DELAY = 1000;
const unsigned long HELMET_DEBOUNCE = 500;
const unsigned long BUTTONR_BLINK_DURATION = 5000;
const unsigned long BUTTONR_PRESS_WINDOW = 1500;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
const unsigned long WIFI_STATUS_INTERVAL = 10000; // 10s
const int BLINK_INTERVAL = 150;


// Ngưỡng cảm biến
const int fsrWearThreshold = 110;
const int reconnectThresholdRSSI = -75;


DHTesp dht;


struct WiFiCredentials {
 const char* ssid;
 const char* password;
};


WiFiCredentials knownNetworks[] = {
 {"Khu 1", "88888888"},
 {"Khu 2", "88888888"},
 {"A1306", "thanha1306"}
};
const int knownCount = sizeof(knownNetworks) / sizeof(WiFiCredentials);


const char* mqtt_server = "d7ca8bc2882f48479e8f50e7636d7da9.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "Hoang_Long";
const char* mqtt_password = "Long1234";


WiFiClientSecure espClient;
PubSubClient client(espClient);


// Biến trạng thái
unsigned long lastUpdate = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastMQTTReconnect = 0;
unsigned long lastWiFiStatusUpdate = 0;
bool lastButtonState = HIGH;
int pressCount = 0;
unsigned long lastPressTime = 0;
bool buttonSBuzzerOn = false;
unsigned long lastButtonSBlinkTime = 0;
bool buttonRBuzzerOn = false;
int buttonRPressCount = 0;
unsigned long lastButtonRPressTime = 0;
unsigned long lastButtonRBlinkTime = 0;
bool buttonRBuzzerContinuous = false;
unsigned long buttonRBuzzerStartTime = 0;
bool isHelmetWorn = false;
unsigned long lastHelmetWornChangeTime = 0;
String currentSSID = ""; // Lưu SSID hiện tại


void connectBestWiFi() {
 Serial.println("🔍 [WiFi] Scanning...");
 WiFi.mode(WIFI_STA);
 WiFi.disconnect(true);
 delay(100);


 int n = WiFi.scanNetworks();
 if (n == 0) {
   Serial.println("❌ [WiFi] No networks found!");
   return;
 }


 const char* bestSSID = nullptr;
 const char* bestPassword = nullptr;
 int bestRSSI = -100;


 for (int i = 0; i < n; ++i) {
   String scannedSSID = WiFi.SSID(i);
   int rssi = WiFi.RSSI(i);
   for (int j = 0; j < knownCount; ++j) {
     if (scannedSSID == knownNetworks[j].ssid && rssi > bestRSSI) {
       bestSSID = knownNetworks[j].ssid;
       bestPassword = knownNetworks[j].password;
       bestRSSI = rssi;
     }
   }
 }


 if (bestSSID) {
   if (WiFi.status() == WL_CONNECTED) {
     if (WiFi.SSID() != bestSSID && WiFi.RSSI() < reconnectThresholdRSSI) {
       Serial.println("🔁 [WiFi] Signal weak, switching to stronger network...");
       WiFi.disconnect(true);
       delay(100);
     } else {
       Serial.println("✅ [WiFi] Already connected to: " + String(WiFi.SSID()));
       return;
     }
   }


   Serial.println("📶 [WiFi] Connecting to: " + String(bestSSID));
   WiFi.begin(bestSSID, bestPassword);


   unsigned long start = millis();
   while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
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
     Serial.println("\n❌ [WiFi] Connection failed after 10 seconds.");
   }
 } else {
   Serial.println("⚠️ [WiFi] No known networks found.");
 }
}


void reconnectMQTT() {
 if (WiFi.status() != WL_CONNECTED) {
   Serial.println("❌ [MQTT] WiFi not connected, attempting to reconnect...");
   connectBestWiFi();
   return;
 }


 unsigned long now = millis();
 if (now - lastMQTTReconnect < MQTT_RECONNECT_INTERVAL) return;


 String clientId = "ESP32_SENDER_" + String(random(0xffff), HEX);
 Serial.println("📡 [MQTT] Attempting connection...");
 if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
   client.subscribe("helmet/sender/ledcontrol");
   client.subscribe("helmet/receiver/request");
   Serial.println("✅ [MQTT] Connected");
 } else {
   Serial.println("❌ [MQTT] Failed, retrying later...");
   lastMQTTReconnect = now;
 }
}


void publish(const char* topic, const String& payload, bool retained = false) {
 client.publish(topic, payload.c_str(), retained);
}


void callback(char* topic, byte* payload, unsigned int length) {
 String message;
 message.reserve(length);
 for (unsigned int i = 0; i < length; i++) {
   message += (char)payload[i];
 }


 StaticJsonDocument<64> doc;
 if (deserializeJson(doc, message)) {
   Serial.println("❌ [MQTT] JSON deserialize error");
   return;
 }
 if (String(topic) == "helmet/sender/ledcontrol") {
   digitalWrite(LED_FEEDBACK_PIN, HIGH);
   delay(1000);
   digitalWrite(LED_FEEDBACK_PIN, LOW);
 } else if (String(topic) == "helmet/receiver/request") {
   if (doc.containsKey("buttonR") && doc["buttonR"] == "pressed") {
     unsigned long now = millis();
     if (now - lastButtonRPressTime > BUTTONR_PRESS_WINDOW) {
       buttonRPressCount = 0;
     }
     buttonRPressCount++;
     lastButtonRPressTime = now;


     if (buttonRPressCount == 1) {
       buttonRBuzzerOn = true;
       lastButtonRBlinkTime = now;
       buttonRBuzzerStartTime = now;
       buttonRBuzzerContinuous = false;
     } else if (buttonRPressCount == 2) {
       buttonRBuzzerContinuous = true;
       buttonRBuzzerStartTime = now;
       buttonRBuzzerOn = false;
     } else if (buttonRPressCount >= 3) {
       buttonRBuzzerOn = false;
       buttonRBuzzerContinuous = false;
       buttonRPressCount = 0;
       digitalWrite(NEW_BUZZER_PIN, HIGH);
     }
   }
 }
}


bool isGasDetected() {
 return digitalRead(MQ2_PIN) == LOW;
}


int readFSRAverage() {
 const int samples = 10;
 long sum = 0;
 for (int i = 0; i < samples; i++) {
   sum += analogRead(FSR_PIN);
   delay(10);
 }
 return sum / samples;
}


void setup() {
 Serial.begin(115200);
 dht.setup(DHTpin, DHTesp::DHT11);
 pinMode(BUTTON_PIN, INPUT_PULLUP);
 pinMode(LED_FEEDBACK_PIN, OUTPUT);
 pinMode(LED_REMOTE_PIN, OUTPUT);
 pinMode(NEW_BUZZER_PIN, OUTPUT);
 pinMode(MQ2_PIN, INPUT);
 pinMode(FSR_PIN, INPUT);
 digitalWrite(LED_FEEDBACK_PIN, LOW);
 digitalWrite(LED_REMOTE_PIN, LOW);
 digitalWrite(NEW_BUZZER_PIN, HIGH);
 analogReadResolution(12);
 analogSetAttenuation(ADC_11db);
 Serial.println("[SETUP] Connecting to WiFi...");
 connectBestWiFi();
 Serial.println("[SETUP] Setting up MQTT...");
 espClient.setInsecure();
 client.setServer(mqtt_server, mqtt_port);
 client.setCallback(callback);
}


void loop() {
 unsigned long now = millis();


 // Kiểm tra và kết nối lại MQTT
 if (!client.connected() && now - lastMQTTReconnect >= MQTT_RECONNECT_INTERVAL) {
   reconnectMQTT();
 }
 client.loop();


 // Kiểm tra tín hiệu WiFi định kỳ
 if (WiFi.status() == WL_CONNECTED && now - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
   if (WiFi.RSSI() < reconnectThresholdRSSI) {
     Serial.println("[WiFi] Signal weak, attempting to reconnect...");
     connectBestWiFi();
   }
   lastWiFiCheck = now;
 }


 // In và gửi SSID mỗi 10s
 if (WiFi.status() == WL_CONNECTED && now - lastWiFiStatusUpdate >= WIFI_STATUS_INTERVAL) {
   Serial.println("[WiFi] Sender connected to: " + String(WiFi.SSID()));
   StaticJsonDocument<64> wifiDoc;
   wifiDoc["ssid"] = WiFi.SSID();
   char wifiMsg[64];
   serializeJson(wifiDoc, wifiMsg);
   publish("helmet/sender/wifi_status", wifiMsg);
   lastWiFiStatusUpdate = now;
 }


 // Xử lý buttonS
 bool buttonState = digitalRead(BUTTON_PIN);
 if (buttonState == LOW && lastButtonState == HIGH && now - lastPressTime > DEBOUNCE_DELAY) {
   pressCount++;
   lastPressTime = now;


   StaticJsonDocument<64> doc;
   doc["button"] = "pressed";
   doc["pressCount"] = pressCount;
   char msg[64];
   serializeJson(doc, msg);
   publish("helmet/sender/status", msg);
   if (pressCount == 1) {
     buttonSBuzzerOn = true;
     lastButtonSBlinkTime = now;
   } else if (pressCount >= 3) {
     buttonSBuzzerOn = false;
     pressCount = 0;
   }
   digitalWrite(LED_FEEDBACK_PIN, HIGH);
   delay(200);
   digitalWrite(LED_FEEDBACK_PIN, LOW);
 }
 lastButtonState = buttonState;


 // Xử lý FSR-402
 static bool pendingHelmetWorn = false;
 static unsigned long pendingHelmetWornTime = 0;
 int fsrReading = readFSRAverage();
 bool newHelmetWorn = fsrReading > fsrWearThreshold;
 if (newHelmetWorn != isHelmetWorn) {
   if (!pendingHelmetWorn) {
     pendingHelmetWorn = true;
     pendingHelmetWornTime = now;
   } else if (now - pendingHelmetWornTime >= HELMET_DEBOUNCE) {
     isHelmetWorn = newHelmetWorn;
     pendingHelmetWorn = false;
     String status = isHelmetWorn ? "{\"helmet_worn\":true}" : "{\"helmet_worn\":false}";
     publish("helmet/sender/alert", status, true);
   }
 } else {
   pendingHelmetWorn = false;
 }


 // Đọc dữ liệu cảm biến và gửi cảnh báo
 if (now - lastUpdate >= UPDATE_INTERVAL) {
   float h = dht.getHumidity();
   float t = dht.getTemperature();
   bool gasDetected = isGasDetected();
   if (!isnan(h) && !isnan(t)) {
     StaticJsonDocument<128> doc;
     doc["humidity"] = h;
     doc["temperature"] = t;
     doc["gas_detected"] = gasDetected;
     doc["force"] = fsrReading;
     doc["helmet_worn"] = isHelmetWorn;
     doc["led_helmet"] = !isHelmetWorn ? 1 : 0;
     char msg[128];
     serializeJson(doc, msg);
     publish("helmet/sender/data", msg, true);
     Serial.println("[Sensor] T=" + String(t) + "C, H=" + String(h) + "%, Gas=" + (gasDetected ? "YES" : "NO") + ", Force=" + String(fsrReading) + ", LED Helmet=" + String(!isHelmetWorn ? "ON" : "OFF"));


     // Gửi tín hiệu để bật LED D2 trên receiver nếu phát hiện gas
     if (gasDetected) {
       StaticJsonDocument<64> ledDoc;
       ledDoc["led_d2"] = true;
       char ledMsg[64];
       serializeJson(ledDoc, ledMsg);
       publish("helmet/sender/ledcontrol", ledMsg);
     }


     bool alert = false;
     String alertMsg = "{\"alert\":true";
     if (t > 30 || h > 65) {
       alert = true;
       alertMsg += ",\"temp_hum\":true";
     }
     if (gasDetected) {
       alert = true;
       alertMsg += ",\"gas\":true";
     }
     if (alert) {
       alertMsg += "}";
       publish("helmet/sender/alert", alertMsg);
     }
   } else {
     Serial.println("❌ [DHT] Failed to read sensor");
   }
   lastUpdate = now;
 }


 // Điều khiển LED D1 (LED_REMOTE_PIN)
 digitalWrite(LED_REMOTE_PIN, !isHelmetWorn ? HIGH : LOW);
 digitalWrite(LED_FEEDBACK_PIN, LOW);


 // Điều khiển buzzer
 bool buzzerActivated = false;
 if (buttonRBuzzerContinuous && now - buttonRBuzzerStartTime < BUTTONR_BLINK_DURATION) {
   digitalWrite(NEW_BUZZER_PIN, LOW);
   buzzerActivated = true;
 } else if (buttonRBuzzerOn && now - buttonRBuzzerStartTime < BUTTONR_BLINK_DURATION) {
   if (now - lastButtonRBlinkTime >= BLINK_INTERVAL) {
     digitalWrite(NEW_BUZZER_PIN, !digitalRead(NEW_BUZZER_PIN));
     lastButtonRBlinkTime = now;
   }
   buzzerActivated = true;
 } else if (buttonSBuzzerOn) {
   if (now - lastButtonSBlinkTime >= BLINK_INTERVAL) {
     digitalWrite(NEW_BUZZER_PIN, !digitalRead(NEW_BUZZER_PIN));
     lastButtonSBlinkTime = now;
   }
   buzzerActivated = true;
 } else {
   digitalWrite(NEW_BUZZER_PIN, HIGH);
 }


 // Tắt buzzer liên tục sau thời gian tối đa
 if (buttonRBuzzerContinuous && now - buttonRBuzzerStartTime >= BUTTONR_BLINK_DURATION) {
   buttonRBuzzerContinuous = false;
   digitalWrite(NEW_BUZZER_PIN, HIGH);
 }
}







