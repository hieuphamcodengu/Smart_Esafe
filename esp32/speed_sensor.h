#ifndef SPEED_SENSOR_H
#define SPEED_SENSOR_H

#include <Arduino.h>
#include <esp_timer.h>
#include "config.h"

// Biến đếm xung và tốc độ
volatile unsigned long pulseCount = 0;
portMUX_TYPE speedMux = portMUX_INITIALIZER_UNLOCKED;

float currentSpeedKmh = 0;
unsigned long lastPulsesPerSec = 0;

esp_timer_handle_t speedTimer;

/**
 * ISR - Ngắt đọc xung từ Hall sensor
 */
void IRAM_ATTR onSpeedPulse() {
  portENTER_CRITICAL_ISR(&speedMux);
  pulseCount++;
  portEXIT_CRITICAL_ISR(&speedMux);
}

/**
 * Hàm timer - Tính tốc độ mỗi giây
 */
void calcSpeed(void* arg) {
  unsigned long pulses;

  portENTER_CRITICAL(&speedMux);
  pulses = pulseCount;
  pulseCount = 0;
  portEXIT_CRITICAL(&speedMux);

  lastPulsesPerSec = pulses;

  // Quy đổi từ xung/giây sang km/h (theo thực nghiệm)
  currentSpeedKmh = pulses * KMPH_PER_PULSE_PER_SEC;
}

/**
 * Khởi tạo cảm biến tốc độ Hall
 */
void initSpeedSensor() {
  Serial.println("Initializing Speed Sensor...");
  
  // Cấu hình chân đọc xung
  pinMode(SPEED_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SPEED_SENSOR_PIN), onSpeedPulse, RISING);
  
  // Tạo timer 1 giây để tính tốc độ
  const esp_timer_create_args_t timer_args = {
    .callback = &calcSpeed,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "speedTimer"
  };
  
  esp_timer_create(&timer_args, &speedTimer);
  esp_timer_start_periodic(speedTimer, 1000000);  // 1,000,000 µs = 1s
  
  Serial.printf("Speed sensor initialized on GPIO%d\n", SPEED_SENSOR_PIN);
  Serial.printf("Pulses per revolution: %d, KMPH factor: %.2f\n", 
                PULSES_PER_REVOLUTION, KMPH_PER_PULSE_PER_SEC);
}

/**
 * Đọc và hiển thị tốc độ
 */
void readSpeed() {
  // Không cần in ra serial, chỉ cập nhật giá trị
  // Tốc độ đã được tự động tính bởi timer calcSpeed()
}

/**
 * Lấy tốc độ hiện tại từ Hall sensor
 * @return Tốc độ (km/h)
 */
float getSpeed() {
  return currentSpeedKmh;
}

/**
 * Lấy số xung/giây
 * @return Số xung trong giây vừa qua
 */
unsigned long getPulsesPerSec() {
  return lastPulsesPerSec;
}

#endif
