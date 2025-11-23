#ifndef CONFIG_H
#define CONFIG_H

// Serial Configuration
#define SERIAL_BAUD_RATE 115200

// WiFi Configuration
#define WIFI_SSID "Esafe-id-01"
#define WIFI_PASSWORD "3141592654"

// WiFi phụ (dự phòng)
#define WIFI_SSID_BACKUP "HP"
#define WIFI_PASSWORD_BACKUP "3141592654"

// MQTT Configuration
#define MQTT_SERVER "526149152b184bce88dda61234c737f8.s1.eu.hivemq.cloud"
#define MQTT_PORT 8883
#define MQTT_USER "Esp32"
#define MQTT_PASSWORD "Hp123456"
#define MQTT_CLIENT_ID "ESP32_1"
#define MQTT_TOPIC_PUB "ESP32_pub"
#define MQTT_TOPIC_SUB "ESP32_sub"

// I2C Configuration (ESP32 default pins)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// MPU6050 Configuration
#define MPU_ACCEL_SCALE 16384.0  // ±2g scale
#define FALL_DETECTION_ANGLE 50  // Góc nghiêng tối đa (độ)

// Rain Sensor Configuration
#define RAIN_SENSOR_PIN 34       // Chân analog đọc cảm biến mưa (ADC1_CH6)
#define RAIN_THRESHOLD 20        // Ngưỡng phát hiện mưa (%)
                                 // Giá trị cao = khô, giá trị thấp = ướt
                                 // Nếu độ ẩm > 50% → có mưa

// GPS Configuration - NEO-7M
#define GPS_RX_PIN 16            // RX2 pin (ESP32 Serial2 RX)
#define GPS_TX_PIN 17            // TX2 pin (ESP32 Serial2 TX)
#define GPS_BAUD_RATE 9600       // Baud rate của module GPS

// GPS Default Location (nếu không có tín hiệu GPS)
#define DEFAULT_LAT 12.665242
#define DEFAULT_LNG 108.037230

// Speed Sensor Configuration - Hall Sensor
#define SPEED_SENSOR_PIN 13          // Chân đọc xung Hall sensor
#define PULSES_PER_REVOLUTION 8      // 4 nam châm x 2 cạnh = 8 xung/vòng
#define KMPH_PER_PULSE_PER_SEC 0.75f // Hệ số quy đổi từ thực nghiệm

// Relay Configuration
#define RELAY_PIN 2                  // Chân điều khiển relay (D2/GPIO2)
#define DEFAULT_SPEED_LIMIT 50       // Tốc độ giới hạn mặc định (km/h)

// SD Card Configuration
#define SD_CS_PIN 5  // Chân CS cho SD card (GPIO5)
// SPI pins (ESP32 default): MOSI=23, MISO=19, SCK=18

// I2S Audio Configuration - MAX98357A
#define I2S_BCLK_PIN 26   // Bit Clock
#define I2S_LRC_PIN 25    // Left/Right Clock (Word Select)
#define I2S_DIN_PIN 27    // Data In
#define I2S_SAMPLE_RATE 8000  // Sample rate của file RAW
#define AUDIO_VOLUME 2.0      // Âm lượng (0.0 → 2.0)

// Audio Configuration - Tên các file RAW
#define SOUND_START "/start_boost.raw"    // Âm thanh khởi động
#define SOUND_RAIN "/rain_boost.raw"      // TODO: Âm thanh mưa
#define SOUND_SPEED "/speed_boost.raw"    // TODO: Âm thanh tốc độ
#define SOUND_SPEED1 "/speed1_boost.raw"  // TODO: Âm thanh tốc độ 1

// Loop delay
#define LOOP_DELAY_MS 500  // Tăng delay để giảm tải CPU và RAM

#endif
