#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Thông tin WiFi
const char* ssid = "HP";
const char* password = "3141592654";

// Thông tin HiveMQ broker
const char* mqtt_server = "526149152b184bce88dda61234c737f8.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "Esp32";
const char* mqtt_password = "Hp123456";
const char* client_id = "ESP32_1";

// MQTT Topics
const char* topic_publish = "ESP32_pub";
const char* topic_subscribe = "ESP32_sub";

WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
int value = 0;

// Biến cho nút BOOT
const int buttonPin = 0;  // GPIO 0 - nút BOOT
const int ledPin = 2;     // GPIO 2 - LED D2
int buttonState = 0;
int lastButtonState = HIGH;
int toggleState = 0;  // Trạng thái toggle 0 hoặc 1
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// Hàm kết nối WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Đang kết nối WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi đã kết nối!");
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
}

// Hàm callback khi nhận message từ MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message nhận được [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Xử lý message theo định dạng id,xx
  int commaIndex = message.indexOf(',');
  if (commaIndex > 0) {
    String receivedId = message.substring(0, commaIndex);
    String stateStr = message.substring(commaIndex + 1);
    
    // Kiểm tra nếu ID trùng khớp
    if (receivedId == String(client_id)) {
      int ledState = stateStr.toInt();
      
      // Điều khiển LED D2
      if (ledState == 1) {
        digitalWrite(ledPin, HIGH);
        toggleState = 1;
        Serial.println("Đèn LED BẬT");
      } else {
        digitalWrite(ledPin, LOW);
        toggleState = 0;
        Serial.println("Đèn LED TẮT");
      }
    } else {
      Serial.println("ID không trùng khớp");
    }
  }
}

// Hàm kết nối lại MQTT
void reconnect() {
  int attempts = 0;
  while (!client.connected() && attempts < 3) {
    Serial.print("Đang kết nối MQTT...");
    
    // Kết nối đơn giản với client_id cố định
    if (client.connect(client_id, mqtt_user, mqtt_password)) {
      Serial.println("Đã kết nối!");
      
      // Subscribe topic sau khi kết nối thành công
      client.subscribe(topic_subscribe);
      Serial.print("Đã subscribe topic: ");
      Serial.println(topic_subscribe);
      
      // Publish thông báo kết nối
      client.publish(topic_publish, "ESP32 connected to HiveMQ");
      
    } else {
      Serial.print("Lỗi, rc=");
      Serial.print(client.state());
      Serial.println(" thử lại sau 2 giây");
      attempts++;
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Cấu hình nút BOOT và LED
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  // Tắt LED ban đầu
  
  // Kết nối WiFi
  setup_wifi();
  
  // Cấu hình SSL/TLS - bỏ qua xác thực certificate (insecure mode)
  espClient.setInsecure();
  
  // Cấu hình MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);
  
  Serial.println("Setup hoàn tất!");
}

void loop() {
  // Kiểm tra kết nối MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Đọc trạng thái nút BOOT với debounce
  int reading = digitalRead(buttonPin);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      // Nếu nút được nhả (chuyển từ LOW sang HIGH)
      if (buttonState == HIGH) {
        // Toggle trạng thái
        toggleState = (toggleState == 0) ? 1 : 0;
        
        // Điều khiển LED theo trạng thái mới
        digitalWrite(ledPin, toggleState);
        Serial.print("Đèn LED: ");
        Serial.println(toggleState == 1 ? "BẬT" : "TẮT");
        
        // Tạo message theo định dạng id,xx
        String msg = String(client_id) + "," + String(toggleState);
        
        Serial.print("Nút BOOT nhấn - Publish: ");
        Serial.println(msg);
        
        // Publish lên MQTT
        client.publish(topic_publish, msg.c_str());
      }
    }
  }
  
  lastButtonState = reading;

  // Gửi message mỗi 5 giây
  // unsigned long now = millis();
  // if (now - lastMsg > 5000) {
  //   lastMsg = now;
  //   value++;
    
  //   // Tạo message để gửi
  //   String msg = "Hello from ESP32 #";
  //   msg += String(value);
    
  //   Serial.print("Publish message: ");
  //   Serial.println(msg);
    
  //   // Publish lên MQTT
  //   client.publish(topic_publish, msg.c_str());
  // }
}
