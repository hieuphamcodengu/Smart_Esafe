#ifndef RAIN_SENSOR_H
#define RAIN_SENSOR_H

#include <Arduino.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "config.h"

// Khai báo hàm từ sd_audio.h
void playRaw(const char* filename);

// Biến trạng thái mưa
bool isRaining = false;
int lastRainValue = 4095;
esp_adc_cal_characteristics_t *adc_chars;

/**
 * Khởi tạo cảm biến mưa với ADC driver mới
 */
void initRainSensor() {
  Serial.println("Initializing Rain Sensor...");
  
  // Cấu hình ADC1 Channel 6 (GPIO34)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
  
  // Khởi tạo calibration characteristics
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);
  
  Serial.println("Rain sensor initialized.");
  Serial.printf("Rain sensor pin: GPIO%d, Threshold: %d%%\n", RAIN_SENSOR_PIN, RAIN_THRESHOLD);
}

/**
 * Đọc giá trị cảm biến mưa
 * @return Giá trị analog (0-4095), giá trị thấp = ướt, cao = khô
 */
int readRainValue() {
  return adc1_get_raw(ADC1_CHANNEL_6);
}

/**
 * Đọc và xử lý cảm biến mưa
 * Hiển thị thông tin và phát hiện mưa
 */
void readRainSensor() {
  int rainValue = readRainValue();
  lastRainValue = rainValue;
  
  // Quy đổi sang % độ ẩm (0% = hoàn toàn khô, 100% = hoàn toàn ướt)
  // Nếu cảm biến logic ngược: giá trị thấp (khô) → 0%, cao (ướt) → 100%
  int rainPercent = map(rainValue, 0, 4095, 0, 100);
  
  // Kiểm tra phát hiện mưa (% độ ẩm cao hơn threshold = có mưa)
  bool currentRainStatus = (rainPercent > RAIN_THRESHOLD);
  
  // Hiển thị giá trị
//   Serial.print("Rain Sensor: ");
//   Serial.print(rainPercent);
//   Serial.print("% humidity (");
//   Serial.print(rainValue);
//   Serial.print(") | Status: ");
  
  if (currentRainStatus) {
    // Serial.print("RAINING");
    
    // Phát hiện chuyển trạng thái từ không mưa sang có mưa
    if (!isRaining) {
    //   Serial.println(" ☔ [RAIN DETECTED!]");
      isRaining = true;
      
      // Phát âm thanh cảnh báo mưa
    //   Serial.println("Playing rain warning sound...");
      playRaw(SOUND_RAIN);
    } else {
      Serial.println();
    }
  } else {
    // Serial.println("DRY");
    if (isRaining) {
    //   Serial.println("Rain stopped.");
      isRaining = false;
    }
  }
}

/**
 * Kiểm tra có đang mưa không
 * @return true nếu đang mưa
 */
bool checkIsRaining() {
  return isRaining;
}

/**
 * Lấy giá trị cảm biến mưa gần nhất
 * @return Giá trị analog đã đọc
 */
int getLastRainValue() {
  return lastRainValue;
}

#endif
