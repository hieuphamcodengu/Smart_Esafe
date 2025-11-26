// Thư viện MPU6050: https://github.com/ElectronicCats/mpu6050
#include "config.h"
#include "mesh_network.h"
#include "mqtt_module.h"
#include "mpu_sensor.h"
#include "rain_sensor.h"
#include "gps_module.h"
#include "speed_sensor.h"
#include "relay_controller.h"
#include "sd_audio.h"

// Khai báo extern MQTT client
extern PubSubClient mqttClient;

// Biến thời gian gửi MQTT
unsigned long lastMQTTPublish = 0;
const unsigned long MQTT_PUBLISH_INTERVAL = 5000;  // Gửi mỗi 5 giây

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(500);  // Chờ serial ổn định
  
  Serial.println("\n========================================");
  Serial.println("  ESP32 Bike Controller Initializing");
  Serial.println("========================================\n");
  
  // Khởi tạo tất cả các module ngay (không đợi WiFi)
  Serial.println("--- Initializing Sensors ---");
  
  // Khởi tạo GPS module
  initGPS();
  
  // Khởi tạo cảm biến tốc độ
  initSpeedSensor();
  
  // Khởi tạo relay controller
  initRelay();
  
  // Khởi tạo cảm biến mưa
  initRainSensor();
  
  // Khởi tạo SD card và I2S audio
  if (initSD()) {
    // Liệt kê các file trong thư mục gốc
    listDir("/");
    
    // Phát âm thanh khởi động
    Serial.println("Playing startup sound...");
    playRaw(SOUND_START);
  } else {
    Serial.println("Warning: SD card not available, continuing without audio...");
  }
  
  // Khởi tạo MPU6050
  if (!initMPU()) {
    while (1);  // Dừng chương trình nếu không kết nối được
  }
  
  Serial.println("\n--- Sensors Initialization Complete ---\n");
  
  // Bây giờ mới bắt đầu kết nối WiFi (delay 15s cho USB WiFi)
  Serial.println("--- Starting Network Initialization ---");
  
  // Bước 1: Kiểm tra WiFi có khả dụng không (có delay 15s)
  checkWiFiAvailability();
  
  // Bước 2: Khởi tạo ESP-NOW (cho cả 2 chế độ)
  initESPNOW();
  
  // Bước 3: Nếu có WiFi, khởi tạo MQTT
  if (isWiFiAvailable) {
    initMQTT();
  } else {
    Serial.println("⚠️ Running in MESH-ONLY mode (no WiFi)");
  }
  
  Serial.println("\n========================================");
  Serial.println("  Initialization Complete!");
  Serial.println("========================================\n");
}

void loop() {
  // Kiểm tra lại WiFi định kỳ (nếu đang ở chế độ mesh-only)
  recheckWiFi();
  
  // Xử lý MQTT loop (chỉ khi có WiFi)
  if (isWiFiAvailable) {
    mqttLoop();
  }
  
  // Đọc GPS
  readGPS();
  
  // Đọc tốc độ
  readSpeed();
  
  // Điều khiển relay dựa trên tốc độ
  controlRelay();
  
  // Đọc cảm biến mưa
  readRainSensor();
  
  // Đọc và xử lý dữ liệu từ MPU6050
  bool fallen = readMPU();
  
  // Xử lý khi xe ngã
  if (fallen) {
    // Gửi cảnh báo MQTT ngay lập tức (chỉ gửi 1 lần)
    if (!isFallAlertSent()) {
      Serial.println("📤 Sending FALL alert to MQTT...");
      
      char fallPayload[50];
      snprintf(fallPayload, sizeof(fallPayload), "%s,FALL", MQTT_CLIENT_ID);
      
      if (isWiFiAvailable) {
        mqttClient.publish(MQTT_TOPIC_PUB, fallPayload);
        Serial.printf("MQTT Published: %s\n", fallPayload);
      }
      // TODO: Thêm ESP-NOW fallback sau
      
      setFallAlertSent();
    }
    
    return;  // Return sớm để không chạy phần code sau
  }
  
  // Gửi dữ liệu mỗi 5 giây
  if (millis() - lastMQTTPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMQTTPublish = millis();
    
    // Lấy dữ liệu GPS
    double lat, lon;
    getLocation(lat, lon);
    
    // Lấy tốc độ từ Hall sensor
    float speed = getSpeed();
    
    // Nếu có WiFi → gửi trực tiếp qua MQTT
    if (isWiFiAvailable) {
      publishData(MQTT_CLIENT_ID, speed, lat, lon);
    } 
    // Nếu không có WiFi → gửi qua ESP-NOW mesh network
    else {
      Serial.println("📡 Sending via ESP-NOW mesh network...");
      sendDataViaESPNOW(MQTT_CLIENT_ID, speed, lat, lon);
    }
  }
  
  delay(LOOP_DELAY_MS);
}
