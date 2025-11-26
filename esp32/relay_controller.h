#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

// Khai báo hàm từ các module khác
float getSpeed();
void playRaw(const char* filename);

// Biến tốc độ giới hạn
float speedLimit = DEFAULT_SPEED_LIMIT;
bool relayState = false;

// Biến trạng thái quá tốc độ
bool isSpeeding = false;
unsigned long speedingStartTime = 0;
bool speed1Played = false;

// Biến cho điều kiện mở relay
bool waitingToRelease = false;
unsigned long releaseWaitStart = 0;

/**
 * Khởi tạo relay
 */
void initRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Mặc định LOW (tắt)
  
  Serial.println("Relay controller initialized.");
  Serial.printf("Relay pin: GPIO%d, Speed limit: %.1f km/h\n", RELAY_PIN, speedLimit);
}

/**
 * Cập nhật tốc độ giới hạn
 * @param newLimit Tốc độ giới hạn mới (km/h)
 */
void setSpeedLimit(float newLimit) {
  if (newLimit > 0 && newLimit <= 200) {  // Giới hạn hợp lý
    speedLimit = newLimit;
    Serial.printf("Speed limit updated: %.1f km/h\n", speedLimit);
  } else {
    Serial.println("Invalid speed limit!");
  }
}

/**
 * Lấy tốc độ giới hạn hiện tại
 * @return Tốc độ giới hạn (km/h)
 */
float getSpeedLimit() {
  return speedLimit;
}

/**
 * Kiểm tra và điều khiển relay dựa trên tốc độ
 */
void controlRelay() {
  float currentSpeed = getSpeed();
  
  // Trường hợp 1: Chưa quá tốc độ
  if (!isSpeeding) {
    if (currentSpeed > speedLimit) {
      // Bắt đầu quá tốc độ
      isSpeeding = true;
      speedingStartTime = millis();
      speed1Played = false;
      
      Serial.printf("⚠️ SPEEDING DETECTED! Speed: %.1f km/h > Limit: %.1f km/h\n", 
                    currentSpeed, speedLimit);
      
      // Phát âm thanh cảnh báo đầu tiên
      playRaw(SOUND_SPEED);
    }
  }
  // Trường hợp 2: Đang quá tốc độ
  else {
    unsigned long speedingDuration = millis() - speedingStartTime;
    
    // Kiểm tra sau 15 giây vẫn quá tốc độ
    if (speedingDuration >= SPEED_WARNING_DELAY && !speed1Played) {
      Serial.printf("⚠️⚠️ STILL SPEEDING AFTER %d seconds - ACTIVATING RELAY!\n", SPEED_WARNING_DELAY / 1000);
      
      // Kích hoạt relay trước
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
      speed1Played = true;
      
      // Sau đó phát âm thanh cảnh báo mức 2
      playRaw(SOUND_SPEED1);
      
      // Bắt đầu chế độ chờ để mở relay
      waitingToRelease = true;
      releaseWaitStart = millis();
    }
    
    // Kiểm tra điều kiện mở relay (nếu đang chờ)
    if (waitingToRelease) {
      float releaseThreshold = speedLimit - SPEED_RELEASE_MARGIN;
      
      if (currentSpeed < releaseThreshold) {
        // Tốc độ đã giảm xuống dưới (limit - margin)
        unsigned long belowThresholdTime = millis() - releaseWaitStart;
        
        if (belowThresholdTime >= SPEED_RELEASE_DELAY) {
          // Đã duy trì dưới ngưỡng đủ lâu
          Serial.printf("✓ Speed maintained below %.1f km/h for %d seconds - RELEASING RELAY\n", 
                        releaseThreshold, SPEED_RELEASE_DELAY / 1000);
          
          digitalWrite(RELAY_PIN, LOW);
          relayState = false;
          isSpeeding = false;
          waitingToRelease = false;
        }
      } else {
        // Tốc độ vẫn cao, reset thời gian chờ
        releaseWaitStart = millis();
      }
    }
    // Nếu chưa kích hoạt relay, kiểm tra về tốc độ bình thường
    else if (!relayState && currentSpeed <= speedLimit) {
      // Về tốc độ bình thường trước 15s
      Serial.println("✓ Speed back to normal");
      isSpeeding = false;
    }
  }
}

/**
 * Kiểm tra trạng thái relay
 * @return true nếu relay đang bật
 */
bool isRelayOn() {
  return relayState;
}

#endif
