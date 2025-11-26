#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h"

// Cấu trúc dữ liệu giống với mesh_network.h
typedef struct struct_message {
  char clientId[20];      // ID của xe
  float speed;            // Tốc độ
  double latitude;        // Vĩ độ
  double longitude;       // Kinh độ
  bool needRelay;         // Cần relay gửi tiếp không
  uint8_t hopCount;       // Số lần nhảy
} struct_message;

// Dữ liệu gửi đi
struct_message testData;

// Broadcast address (gửi tới tất cả ESP32)
// uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Dùng MAC cụ thể của ESP32 chính (00:70:07:19:40:0C)
uint8_t broadcastAddress[] = {0x00, 0x70, 0x07, 0x19, 0x40, 0x0C};

// Thông tin test
const char* TEST_ID = "ESP32_2";
const float TEST_SPEED = 20.0;
const double TEST_LAT = 00.000000;
const double TEST_LNG = 00.000000;

// Nút BOOT (GPIO 0) và GPIO 4
const int BOOT_BUTTON = 0;
const int GPIO_4_BUTTON = 4;
bool lastBootButtonState = HIGH;
bool lastGpio4ButtonState = HIGH;

// Biến theo dõi gửi thành công
bool sendSuccess = false;
int currentChannel = -1;  // -1 = chưa biết channel
bool foundChannel = false;

// Callback khi gửi dữ liệu
void onDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    sendSuccess = true;
    Serial.println("✓ SUCCESS!");
  } else {
    Serial.println("✗ Failed");
  }
}

// Thử gửi trên channel cụ thể
bool tryChannel(int channel) {
  Serial.printf("\n[Channel %d] Testing...\n", channel);
  
  // Xóa peer cũ (nếu có)
  esp_now_del_peer(broadcastAddress);
  
  // Set channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  delay(50);
  
  Serial.printf("[Channel %d] WiFi channel set to: %d\n", channel, WiFi.channel());
  
  // Thêm peer với channel mới
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  // Auto
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.printf("[Channel %d] Failed to add peer\n", channel);
    return false;
  }
  
  // Reset flag
  sendSuccess = false;
  
  // Gửi dữ liệu
  Serial.printf("[Channel %d] Sending... ", channel);
  esp_err_t result = esp_now_send(broadcastAddress, 
                                  (uint8_t *)&testData, 
                                  sizeof(testData));
  
  if (result != ESP_OK) {
    Serial.printf("Send error: %d\n", result);
    return false;
  }
  
  // Chờ callback (timeout 500ms)
  int timeout = 50;  // 50 x 10ms = 500ms
  while (!sendSuccess && timeout > 0) {
    delay(10);
    timeout--;
  }
  
  return sendSuccess;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("  ESP32 ESP-NOW Sender");
  Serial.println("========================================\n");
  
  // Khởi tạo các nút
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(GPIO_4_BUTTON, INPUT_PULLUP);
  
  // Khởi tạo WiFi ở chế độ Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // In MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Target MAC: 00:70:07:19:40:0C\n");
  
  // Khởi tạo ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }
  Serial.println("✓ ESP-NOW initialized\n");
  
  // Đăng ký callback
  esp_now_register_send_cb(onDataSent);
  
  // Chuẩn bị dữ liệu test
  strncpy(testData.clientId, TEST_ID, sizeof(testData.clientId) - 1);
  testData.speed = TEST_SPEED;
  testData.latitude = TEST_LAT;
  testData.longitude = TEST_LNG;
  testData.needRelay = true;
  testData.hopCount = 0;
  
  Serial.println("========================================");
  Serial.println("  Ready - Waiting for button press");
  Serial.println("========================================");
  Serial.printf("Client ID: %s\n", testData.clientId);
  Serial.printf("Speed: %.1f km/h\n", testData.speed);
  Serial.printf("Location: %.6f, %.6f\n", testData.latitude, testData.longitude);
  Serial.println("\nPress BOOT (GPIO 0) or GPIO 4 button to send data...");
  Serial.println("(First press will scan channels 1-13)");
  Serial.println("========================================\n");
}

// Hàm quét channel
void scanAndFindChannel() {
  Serial.println("\n========================================");
  Serial.println("  Scanning channels 1-13...");
  Serial.println("========================================");
  
  foundChannel = false;
  
  // Scan từng channel từ 1-13
  for (int ch = 1; ch <= 13; ch++) {
    if (tryChannel(ch)) {
      Serial.printf("\n🎯 FOUND! ESP32 is on channel %d\n", ch);
      foundChannel = true;
      currentChannel = ch;
      Serial.println("Channel saved for future use!");
      break;
    }
    delay(100);
  }
  
  if (!foundChannel) {
    Serial.println("\n❌ Could not find ESP32 on any channel!");
    currentChannel = -1;
  }
  
  Serial.println("========================================\n");
}

// Hàm gửi dữ liệu
void sendData() {
  Serial.println("\n📡 Button pressed! Sending data...");
  
  // Nếu chưa biết channel, quét trước
  if (currentChannel == -1 || !foundChannel) {
    Serial.println("Channel unknown - scanning first...");
    scanAndFindChannel();
    
    if (!foundChannel) {
      Serial.println("Cannot send - no channel found!");
      return;
    }
  }
  
  // Gửi dữ liệu
  Serial.printf("  Channel: %d\n", currentChannel);
  Serial.printf("  ID: %s\n", testData.clientId);
  Serial.printf("  Speed: %.1f km/h\n", testData.speed);
  Serial.printf("  GPS: %.6f, %.6f\n", testData.latitude, testData.longitude);
  Serial.print("  Status: ");
  
  sendSuccess = false;
  esp_err_t result = esp_now_send(broadcastAddress, 
                                  (uint8_t *)&testData, 
                                  sizeof(testData));
  
  if (result == ESP_OK) {
    // Chờ callback
    int timeout = 50;
    while (!sendSuccess && timeout > 0) {
      delay(10);
      timeout--;
    }
  } else {
    Serial.println("✗ Send failed");
  }
  
  // Tăng tốc độ mỗi lần gửi
  testData.speed += 1.0;
}

void loop() {
  // Đọc trạng thái 2 nút
  int bootButtonState = digitalRead(BOOT_BUTTON);
  int gpio4ButtonState = digitalRead(GPIO_4_BUTTON);
  
  // Kiểm tra nút BOOT (GPIO 0)
  if (bootButtonState == LOW && lastBootButtonState == HIGH) {
    delay(50);  // Debounce
    if (digitalRead(BOOT_BUTTON) == LOW) {
      sendData();
      
      // Đợi nhả nút
      while (digitalRead(BOOT_BUTTON) == LOW) {
        delay(10);
      }
    }
  }
  
  // Kiểm tra nút GPIO 4
  if (gpio4ButtonState == LOW && lastGpio4ButtonState == HIGH) {
    delay(50);  // Debounce
    if (digitalRead(GPIO_4_BUTTON) == LOW) {
      sendData();
      
      // Đợi nhả nút
      while (digitalRead(GPIO_4_BUTTON) == LOW) {
        delay(10);
      }
    }
  }
  
  lastBootButtonState = bootButtonState;
  lastGpio4ButtonState = gpio4ButtonState;
  delay(10);
}
