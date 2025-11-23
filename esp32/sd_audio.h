#ifndef SD_AUDIO_H
#define SD_AUDIO_H

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#include "config.h"

// Biến trạng thái SD card
bool sdCardReady = false;

/**
 * Khởi tạo I2S cho MAX98357A
 */
void setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_LRC_PIN,
    .data_out_num = I2S_DIN_PIN,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  
  Serial.println("I2S Audio initialized.");
}

/**
 * Khởi tạo thẻ SD card
 * @return true nếu khởi tạo thành công, false nếu thất bại
 */
bool initSD() {
  Serial.println("Initializing SD card...");
  
  // Khởi tạo SD card với chân CS được định nghĩa trong config
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card initialization failed!");
    sdCardReady = false;
    return false;
  }
  
  uint8_t cardType = SD.cardType();
  
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached!");
    sdCardReady = false;
    return false;
  }
  
  // Hiển thị loại thẻ SD
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  
  // Hiển thị dung lượng thẻ
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  
  sdCardReady = true;
  Serial.println("SD card initialized successfully.");
  
  // Khởi tạo I2S Audio
  setupI2S();
  
  return true;
}

/**
 * Kiểm tra file có tồn tại trên SD card không
 * @param filename Tên file cần kiểm tra (bao gồm đường dẫn)
 * @return true nếu file tồn tại
 */
bool fileExists(const char* filename) {
  if (!sdCardReady) {
    Serial.println("SD card not ready!");
    return false;
  }
  
  bool exists = SD.exists(filename);
  if (exists) {
    Serial.printf("File found: %s\n", filename);
  } else {
    Serial.printf("File not found: %s\n", filename);
  }
  return exists;
}

/**
 * Liệt kê tất cả file trong thư mục
 * @param dirname Tên thư mục
 */
void listDir(const char* dirname) {
  if (!sdCardReady) {
    Serial.println("SD card not ready!");
    return;
  }
  
  Serial.printf("Listing directory: %s\n", dirname);
  
  File root = SD.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

/**
 * Đọc file WAV từ SD card
 * @param filename Tên file WAV cần đọc
 * @return File object (cần close() sau khi sử dụng)
 */
File readWavFile(const char* filename) {
  if (!sdCardReady) {
    Serial.println("SD card not ready!");
    return File();
  }
  
  File file = SD.open(filename);
  if (!file) {
    Serial.printf("Failed to open file: %s\n", filename);
    return File();
  }
  
  Serial.printf("Opened WAV file: %s (Size: %d bytes)\n", filename, file.size());
  return file;
}

/**
 * Phát âm thanh từ file RAW (PCM 16-bit, mono, 8kHz)
 * @param filename Tên file RAW cần phát
 */
void playRaw(const char* filename) {
  if (!sdCardReady) {
    Serial.println("SD card not ready!");
    return;
  }
  
  File f = SD.open(filename);
  if (!f) {
    Serial.printf("Cannot open RAW file: %s\n", filename);
    return;
  }
  
  Serial.printf("Playing: %s (Size: %d bytes)\n", filename, f.size());
  
  int16_t buffer[256];
  size_t bytesRead, bytesWritten;
  
  while (f.available()) {
    bytesRead = f.read((uint8_t *)buffer, sizeof(buffer));
    
    // Áp dụng volume
    int samples = bytesRead / 2;
    for (int i = 0; i < samples; i++) {
      float v = (float)buffer[i] * AUDIO_VOLUME;
      if (v > 32767) v = 32767;
      if (v < -32768) v = -32768;
      buffer[i] = (int16_t)v;
    }
    
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }
  
  f.close();
  
  // Gửi im lặng để reset DMA (50ms)
  int16_t silence[256] = { 0 };
  for (int i = 0; i < 20; i++) {
    i2s_write(I2S_NUM_0, silence, sizeof(silence), &bytesWritten, portMAX_DELAY);
  }
  
  // Xóa sạch DMA để không bị "bụp bụp"
  i2s_zero_dma_buffer(I2S_NUM_0);
  
  Serial.println("Playback complete.");
}

/**
 * Đặt âm lượng (không cần thiết vì đã có AUDIO_VOLUME trong config)
 * Hàm này giữ lại để tương thích
 */
void setVolume(uint8_t vol) {
  Serial.printf("Volume is configured in config.h: %.1f\n", AUDIO_VOLUME);
}

/**
 * Phát âm thanh beep ngắn (píp)
 * Tạo tín hiệu vuông 1000Hz trong 100ms
 */
void playBeep() {
  const int SAMPLE_RATE = 8000;
  const int BEEP_FREQ = 1000;  // 1kHz
  const int BEEP_DURATION_MS = 100;
  const int SAMPLES = (SAMPLE_RATE * BEEP_DURATION_MS) / 1000;
  
  int16_t beepBuffer[SAMPLES];
  
  // Tạo sóng vuông
  for (int i = 0; i < SAMPLES; i++) {
    if ((i % (SAMPLE_RATE / BEEP_FREQ / 2)) < (SAMPLE_RATE / BEEP_FREQ / 4)) {
      beepBuffer[i] = 8000;  // Biên độ dương
    } else {
      beepBuffer[i] = -8000;  // Biên độ âm
    }
  }
  
  // Gửi qua I2S
  size_t bytes_written;
  i2s_write(I2S_NUM_0, beepBuffer, SAMPLES * sizeof(int16_t), &bytes_written, portMAX_DELAY);
}

#endif
