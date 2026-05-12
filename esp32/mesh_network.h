#ifndef MESH_NETWORK_H
#define MESH_NETWORK_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "config.h"

// Cấu trúc dữ liệu gửi qua ESP-NOW
typedef struct struct_message {
  char clientId[20];      // ID của xe
  float speed;            // Tốc độ
  double latitude;        // Vĩ độ
  double longitude;       // Kinh độ
  bool needRelay;         // Cần relay gửi tiếp không
  uint8_t hopCount;       // Số lần nhảy (tránh loop vô hạn)
} struct_message;

// Biến toàn cục
bool isWiFiAvailable = false;
bool isMeshNode = false;  // true = chỉ dùng mesh, false = có WiFi
struct_message incomingData;
struct_message outgoingData;

// Danh sách MAC của các node láng giềng (broadcast nếu để FF:FF:FF:FF:FF:FF)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Forward declaration
void publishDataViaMQTT(const char* clientId, float speed, double lat, double lon);

/**
 * Callback khi nhận dữ liệu ESP-NOW
 */
void onDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int len) {
  Serial.println("\n========================================");
  Serial.println("📡 ESP-NOW PACKET RECEIVED!");
  Serial.println("========================================");
  
  memcpy(&incomingData, data, sizeof(incomingData));
  
  Serial.printf("  From MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  Serial.printf("  Data Size: %d bytes\n", len);
  Serial.printf("  Client ID: %s\n", incomingData.clientId);
  Serial.printf("  Speed: %.1f km/h\n", incomingData.speed);
  Serial.printf("  Location: %.6f, %.6f\n", incomingData.latitude, incomingData.longitude);
  
  // Nếu node này có WiFi → relay lên MQTT
  if (isWiFiAvailable) {
    Serial.println("  ➡️ Relaying to MQTT...");
    // Gửi dữ liệu nhờ lên MQTT
    publishDataViaMQTT(incomingData.clientId, 
                      incomingData.speed, 
                      lat, 
                      lon);
  } else {
    Serial.println("  ⚠️ No WiFi - Data dropped");
  }
  
  Serial.println("========================================\n");
}

/**
 * Callback khi gửi dữ liệu ESP-NOW
 */
void onDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("📤 ESP-NOW Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

/**
 * Khởi tạo ESP-NOW
 */
bool initESPNOW() {
  Serial.println("\n--- Initializing ESP-NOW ---");
  
  // In ra MAC address
  Serial.print("🔧 ESP32 MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // In channel hiện tại
  Serial.print("📡 WiFi Channel: ");
  Serial.println(WiFi.channel());
  
  // Khởi tạo ESP-NOW
  Serial.print("Initializing ESP-NOW... ");
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ FAILED");
    return false;
  }
  Serial.println("✓ OK");
  
  // Đăng ký callback
  Serial.print("Registering callbacks... ");
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("✓ OK");
  
  // Thêm peer (broadcast)
  Serial.print("Adding broadcast peer... ");
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  // Channel hiện tại
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ FAILED");
    return false;
  }
  Serial.println("✓ OK");
  
  Serial.println("✅ ESP-NOW Ready to receive!");
  Serial.println("--- ESP-NOW Init Complete ---\n");
  return true;
}

/**
 * Kiểm tra WiFi có khả dụng không
 * @return true nếu WiFi kết nối thành công
 */
bool checkWiFiAvailability() {
  Serial.println("\n🕐 Waiting 15 seconds for USB WiFi to boot...");
  for (int i = 15; i > 0; i--) {
    Serial.printf("   %d...\n", i);
    delay(1000);
  }
  Serial.println("✓ Starting WiFi connection\n");
  
  // Thử kết nối WiFi chính
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("🔍 Trying primary WiFi (");
  Serial.print(WIFI_SSID);
  Serial.print(")...");
  
  int timeout = 10;  // 10 giây
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    Serial.print(".");
    timeout--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ Primary WiFi connected");
    Serial.print("   SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
    isWiFiAvailable = true;
    isMeshNode = false;
    return true;
  }
  
  // WiFi chính không được, thử WiFi phụ
  Serial.println("\n⚠️ Primary WiFi failed");
  WiFi.disconnect();
  delay(1000);
  
  WiFi.begin(WIFI_SSID_BACKUP, WIFI_PASSWORD_BACKUP);
  Serial.print("🔍 Trying backup WiFi (");
  Serial.print(WIFI_SSID_BACKUP);
  Serial.print(")...");
  
  timeout = 10;  // 10 giây
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(1000);
    Serial.print(".");
    timeout--;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ Backup WiFi connected");
    Serial.print("   SSID: ");
    Serial.println(WIFI_SSID_BACKUP);
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
    isWiFiAvailable = true;
    isMeshNode = false;
    return true;
  }
  
  // Cả 2 WiFi đều không được
  Serial.println("\n⚠️ Both WiFi networks failed - switching to MESH mode");
  isWiFiAvailable = false;
  isMeshNode = true;
  
  // Ngắt kết nối WiFi để tiết kiệm pin
  WiFi.disconnect();
  
  return false;
}

/**
 * Gửi dữ liệu qua ESP-NOW
 * @param clientId ID của xe
 * @param speed Tốc độ hiện tại
 * @param lat Vĩ độ
 * @param lon Kinh độ
 */
void sendDataViaESPNOW(const char* clientId, float speed, double lat, double lon) {
  // Chuẩn bị dữ liệu
  strncpy(outgoingData.clientId, clientId, sizeof(outgoingData.clientId) - 1);
  outgoingData.speed = speed;
  outgoingData.latitude = lat;
  outgoingData.longitude = lon;
  outgoingData.needRelay = true;
  outgoingData.hopCount = 0;
  
  // Gửi qua ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, 
                                  (uint8_t *)&outgoingData, 
                                  sizeof(outgoingData));
  
  if (result == ESP_OK) {
    Serial.println("📡 Data sent via ESP-NOW");
  } else {
    Serial.println("❌ Error sending data via ESP-NOW");
  }
}

/**
 * Kiểm tra lại WiFi định kỳ (gọi trong loop)
 * Mỗi 5 phút kiểm tra lại 1 lần xem WiFi đã khả dụng chưa
 */
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 300000;  // 5 phút

void recheckWiFi() {
  if (isMeshNode && (millis() - lastWiFiCheck >= WIFI_CHECK_INTERVAL)) {
    lastWiFiCheck = millis();
    Serial.println("\n🔄 Rechecking WiFi availability...");
    checkWiFiAvailability();
    
    // Nếu WiFi đã khả dụng, khởi tạo lại MQTT
    if (isWiFiAvailable) {
      extern bool initMQTT();
      initMQTT();
    }
  }
}

/**
 * Thêm peer cụ thể (nếu biết MAC address của node láng giềng)
 * @param macAddress MAC address dạng "AA:BB:CC:DD:EE:FF"
 */
bool addSpecificPeer(const char* macAddress) {
  uint8_t mac[6];
  
  // Parse MAC address
  if (sscanf(macAddress, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
    Serial.println("❌ Invalid MAC address format");
    return false;
  }
  
  // Thêm peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add peer");
    return false;
  }
  
  Serial.printf("✓ Added peer: %s\n", macAddress);
  return true;
}

#endif
