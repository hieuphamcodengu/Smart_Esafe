#ifndef MQTT_MODULE_H
#define MQTT_MODULE_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "config.h"

// Khai báo hàm từ relay_controller.h
void setSpeedLimit(float newLimit);

// MQTT Client objects
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// Biến trạng thái
bool mqttConnected = false;
unsigned long lastMQTTAttempt = 0;
const unsigned long MQTT_RETRY_INTERVAL = 5000;  // 5 giây

/**
 * Callback khi nhận message từ MQTT
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message nhận được [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Xử lý message: format "id,speed"
  int commaIndex = message.indexOf(',');
  if (commaIndex > 0) {
    String receivedId = message.substring(0, commaIndex);
    String speedStr = message.substring(commaIndex + 1);
    
    // Kiểm tra ID có trùng với thiết bị không
    if (receivedId.equals(MQTT_CLIENT_ID)) {
      float newSpeedLimit = speedStr.toFloat();
      if (newSpeedLimit > 0) {
        setSpeedLimit(newSpeedLimit);
        Serial.printf("✓ Speed limit updated via MQTT: %.1f km/h\n", newSpeedLimit);
      }
    } else {
      Serial.println("ID không khớp, bỏ qua.");
    }
  }
}

/**
 * Kết nối WiFi
 */
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println();
  Serial.print("Đang kết nối WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi đã kết nối!");
    Serial.print("Địa chỉ IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println();
    Serial.println("Kết nối WiFi thất bại!");
    return false;
  }
}

/**
 * Kết nối MQTT broker
 */
bool connectMQTT() {
  if (mqttClient.connected()) {
    mqttConnected = true;
    return true;
  }

  // Kiểm tra thời gian retry
  if (millis() - lastMQTTAttempt < MQTT_RETRY_INTERVAL) {
    return false;
  }
  
  lastMQTTAttempt = millis();
  
  Serial.print("Đang kết nối MQTT broker...");
  
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("Đã kết nối!");
    
    // Subscribe topic
    mqttClient.subscribe(MQTT_TOPIC_SUB);
    Serial.print("Đã subscribe: ");
    Serial.println(MQTT_TOPIC_SUB);
    
    // Gửi thông báo kết nối
    mqttClient.publish(MQTT_TOPIC_PUB, "ESP32 connected");
    
    mqttConnected = true;
    return true;
  } else {
    Serial.print("Lỗi, rc=");
    Serial.println(mqttClient.state());
    mqttConnected = false;
    return false;
  }
}

/**
 * Khởi tạo MQTT module
 */
bool initMQTT() {
  Serial.println("Initializing MQTT module...");
  
  // Kết nối WiFi trước
  if (!connectWiFi()) {
    return false;
  }
  
  // Cấu hình SSL/TLS
  espClient.setInsecure();
  
  // Cấu hình MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  
  // Thử kết nối MQTT
  return connectMQTT();
}

/**
 * Publish data lên MQTT
 * Format: id,speed,lat,long
 */
bool publishData(const char* clientId, float speed, double lat, double lon) {
  if (!mqttClient.connected()) {
    // Thử kết nối lại
    if (!connectMQTT()) {
      return false;
    }
  }
  
  // Tạo payload theo format: id,speed,lat,long
  char payload[150];
  snprintf(payload, sizeof(payload), "%s,%.2f,%.6f,%.6f", 
           clientId, speed, lat, lon);
  
  // Publish
  bool success = mqttClient.publish(MQTT_TOPIC_PUB, payload);
  
  if (success) {
    Serial.print("MQTT Published: ");
    Serial.println(payload);
  } else {
    Serial.println("MQTT Publish failed!");
  }
  
  return success;
}

/**
 * Wrapper function để gọi từ mesh_network.h
 * Publish dữ liệu nhờ từ node khác
 */
void publishDataViaMQTT(const char* clientId, float speed, double lat, double lon) {
  publishData(clientId, speed, lat, lon);
}

/**
 * Loop MQTT - gọi trong loop() chính
 */
void mqttLoop() {
  // Kiểm tra WiFi
  if (WiFi.status() != WL_CONNECTED) {
    mqttConnected = false;
    connectWiFi();
    return;
  }
  
  // Kiểm tra kết nối MQTT
  if (!mqttClient.connected()) {
    mqttConnected = false;
    connectMQTT();
  } else {
    mqttClient.loop();
  }
}

/**
 * Kiểm tra MQTT có kết nối không
 */
bool isMQTTConnected() {
  return mqttConnected && mqttClient.connected();
}

#endif
