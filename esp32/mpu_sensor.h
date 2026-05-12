#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

#include <Wire.h>
#include <MPU6050.h>
#include "config.h"

// Khai báo đối tượng MPU6050
MPU6050 mpu;

// Biến lưu góc chuẩn ban đầu (góc 0)
float pitchOffset = 0;
float rollOffset = 0;

// Biến trạng thái ngã
bool isFallen = false;
bool fallAlertSent = false;

/**
 * Tính góc pitch và roll từ giá trị gia tốc
 */
void calculateAngles(int16_t ax, int16_t ay, int16_t az, float &pitch, float &roll) {
  // Chuyển đổi giá trị gia tốc sang đơn vị g
  float ax_g = ax / MPU_ACCEL_SCALE;
  float ay_g = ay / MPU_ACCEL_SCALE;
  float az_g = az / MPU_ACCEL_SCALE;

  // Tính góc pitch và roll
  pitch = atan2(ax_g, sqrt(ay_g * ay_g + az_g * az_g)) * 180 / PI;
  roll = atan2(ay_g, sqrt(ax_g * ax_g + az_g * az_g)) * 180 / PI;
}

/**
 * Khởi tạo cảm biến MPU6050 và hiệu chuẩn góc ban đầu
 * @return true nếu khởi tạo thành công, false nếu thất bại
 */
bool initMPU() {
  Wire.begin();  // Sử dụng SDA = GPIO21, SCL = GPIO22 mặc định
  
  Serial.println("Initializing MPU6050...");
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    return false;
  } else {
    Serial.println("MPU6050 connected.");
    
    // Hiệu chuẩn góc ban đầu (đọc góc hiện tại làm mốc 0)
    Serial.println("Calibrating initial position...");
    delay(100);  // Chờ ổn định
    
    int16_t ax, ay, az;
    float pitch, roll;
    
    // Đọc trung bình 10 lần để có giá trị chính xác hơn
    float sumPitch = 0, sumRoll = 0;
    for (int i = 0; i < 10; i++) {
      mpu.getAcceleration(&ax, &ay, &az);
      calculateAngles(ax, ay, az, pitch, roll);
      sumPitch += pitch;
      sumRoll += roll;
      delay(10);
    }
    
    pitchOffset = sumPitch / 10.0;
    rollOffset = sumRoll / 10.0;
    
    // Serial.print("Calibration complete. Pitch offset: ");
    // Serial.print(pitchOffset, 2);
    // Serial.print("° | Roll offset: ");
    // Serial.print(rollOffset, 2);
    // Serial.println("°");
    // Serial.println("Current position set as 0°, 0°");
    
    return true;
  }
}

/**
 * Đọc dữ liệu từ MPU6050 và tính góc nghiêng
 * Hiển thị thông tin và cảnh báo nếu phát hiện xe ngã
 * @return true nếu xe đang ngã
 */
bool readMPU() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Tính góc pitch và roll tuyệt đối
  float pitchRaw, rollRaw;
  calculateAngles(ax, ay, az, pitchRaw, rollRaw);

  // Trừ đi góc offset để có góc tương đối so với vị trí ban đầu
  float pitch = pitchRaw - pitchOffset;
  float roll = rollRaw - rollOffset;

  // Hiển thị giá trị góc (tương đối so với vị trí ban đầu)
  // Serial.print("Pitch: ");
  // Serial.print(pitch, 2);
  // Serial.print("° | Roll: ");
  // Serial.print(roll, 2);
  // Serial.println("°");

  // Kiểm tra phát hiện ngã (dựa trên độ lệch so với vị trí ban đầu)
  if (abs(pitch) > FALL_DETECTION_ANGLE || abs(roll) > FALL_DETECTION_ANGLE) {
    if (!isFallen) {
      Serial.println("⚠️⚠️⚠️ CẢNH BÁO: XE ĐÃ BỊ NGÃ!");
      Serial.printf("   Pitch: %.1f° | Roll: %.1f°\n", pitch, roll);
      isFallen = true;
      fallAlertSent = false;  // Reset flag để gửi MQTT
    }
    return true;  // Đang ngã
  } else {
    if (isFallen) {
      Serial.println("✓ Xe đã trở về vị trí bình thường");
      isFallen = false;
      fallAlertSent = false;
    }
    return false;  // Không ngã
  }
}

/**
 * Kiểm tra xe có đang ngã không
 */
bool isBikeFallen() {
  return isFallen;
}

/**
 * Kiểm tra đã gửi cảnh báo ngã chưa
 */
bool isFallAlertSent() {
  return fallAlertSent;
}

/**
 * Đánh dấu đã gửi cảnh báo ngã
 */
void setFallAlertSent() {
  fallAlertSent = true;
}

/**
 * In dữ liệu IMU để debug kiểm tra ngã
 */
void printIMUDebug() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float pitchRaw, rollRaw;
  calculateAngles(ax, ay, az, pitchRaw, rollRaw);

  float pitch = pitchRaw - pitchOffset;
  float roll  = rollRaw  - rollOffset;

  bool fallen = (abs(pitch) > FALL_DETECTION_ANGLE || abs(roll) > FALL_DETECTION_ANGLE);

  Serial.printf("[IMU] ax:%6d ay:%6d az:%6d | Pitch:%7.2f° Roll:%7.2f° | Threshold:±%d° | %s\n",
                ax, ay, az,
                pitch, roll,
                FALL_DETECTION_ANGLE,
                fallen ? ">>> FALLEN <<<" : "OK");
}

#endif
