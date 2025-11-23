#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include "config.h"

// Đối tượng GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);  // Serial2 trên ESP32

// Biến trạng thái GPS
bool gpsAvailable = false;
unsigned long lastGPSUpdate = 0;

/**
 * Khởi tạo GPS module NEO-7M
 */
void initGPS() {
  Serial.println("Initializing GPS module...");
  
  // Khởi tạo Serial2 với RX=16, TX=17
  gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  Serial.printf("GPS initialized on Serial2 (RX: GPIO%d, TX: GPIO%d, Baud: %d)\n", 
                GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
  Serial.println("Waiting for GPS signal...");
}

/**
 * Đọc và xử lý dữ liệu GPS
 */
void readGPS() {
  // Đọc dữ liệu từ GPS module
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    gps.encode(c);
  }
  
  // Kiểm tra có tín hiệu GPS hợp lệ không
  if (gps.location.isValid()) {
    gpsAvailable = true;
    
    // In tọa độ GPS thực
    // Serial.print("GPS Location: ");
    // Serial.print(gps.location.lat(), 6);
    // Serial.print(", ");
    // Serial.println(gps.location.lng(), 6);
    
  } else {
    // // Không có GPS → in tọa độ mặc định
    // Serial.print("GPS Location (default): ");
    // Serial.print(DEFAULT_LAT, 6);
    // Serial.print(", ");
    // Serial.println(DEFAULT_LNG, 6);
  }
}

/**
 * Kiểm tra GPS có tín hiệu không
 * @return true nếu có tín hiệu hợp lệ
 */
bool isGPSAvailable() {
  return gpsAvailable && gps.location.isValid();
}

/**
 * Lấy vị trí hiện tại
 * @param lat Latitude (out)
 * @param lng Longitude (out)
 * @return true nếu lấy được vị trí từ GPS, false nếu dùng vị trí mặc định
 */
bool getLocation(double &lat, double &lng) {
  if (isGPSAvailable()) {
    lat = gps.location.lat();
    lng = gps.location.lng();
    return true;
  } else {
    // Không có GPS → dùng tọa độ mặc định
    lat = DEFAULT_LAT;
    lng = DEFAULT_LNG;
    return false;
  }
}

#endif
