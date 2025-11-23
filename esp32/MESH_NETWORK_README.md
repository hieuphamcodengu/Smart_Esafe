# ESP32 Mesh Network - Hướng Dẫn Sử Dụng

## Tổng Quan

Hệ thống này cho phép các ESP32 không có WiFi (hoặc USB WiFi bị hỏng) có thể gửi dữ liệu qua các ESP32 láng giềng có kết nối WiFi, sau đó relay lên MQTT server.

## Cơ Chế Hoạt Động

### 1. Auto-Detection WiFi
- Khi khởi động, ESP32 tự động kiểm tra WiFi có khả dụng không (timeout 10 giây)
- Nếu **có WiFi**: Chạy ở chế độ NORMAL (gửi trực tiếp qua MQTT)
- Nếu **không có WiFi**: Chạy ở chế độ MESH-ONLY (gửi qua ESP-NOW)

### 2. ESP-NOW Mesh Network
- Sử dụng giao thức ESP-NOW của ESP32 (không cần router WiFi)
- Gửi broadcast đến tất cả ESP32 trong vùng phủ sóng (~100m)
- Hỗ trợ multi-hop (tối đa 3 hop để tránh loop)

### 3. Data Relay
```
ESP32 (No WiFi) 
    ↓ ESP-NOW
ESP32 (Has WiFi)
    ↓ MQTT
Server
```

## Cấu Trúc Dữ Liệu

Mỗi message ESP-NOW chứa:
```cpp
struct_message {
  char clientId[20];      // ID của xe (VD: "ESP32_5")
  float speed;            // Tốc độ hiện tại
  double latitude;        // Vĩ độ GPS
  double longitude;       // Kinh độ GPS
  bool needRelay;         // true = cần relay lên MQTT
  uint8_t hopCount;       // Số lần nhảy (0 = gốc, 1-3 = relay)
}
```

## Workflow Chi Tiết

### Setup Phase:
1. `checkWiFiAvailability()` - Kiểm tra WiFi (10s timeout)
2. `initESPNOW()` - Khởi tạo ESP-NOW cho tất cả node
3. `initMQTT()` - Chỉ khởi tạo nếu có WiFi

### Loop Phase:

#### Node có WiFi:
```
Loop → readSensors() → publishData() qua MQTT
       ↓
   onDataRecv() callback → nhận data từ mesh
       ↓
   publishDataViaMQTT() → relay lên server
```

#### Node không WiFi:
```
Loop → readSensors() → sendDataViaESPNOW()
       ↓
   Broadcast tới các node xung quanh
       ↓
   recheckWiFi() mỗi 5 phút (tự động chuyển chế độ nếu WiFi khả dụng)
```

## Tính Năng Chống Loop

### Hop Count Mechanism:
- `hopCount = 0`: Dữ liệu gốc từ node nguồn
- `hopCount = 1-2`: Đã relay qua 1-2 node
- `hopCount ≥ 3`: Bỏ qua (không relay nữa)

### Auto-Reset Timer:
- Mỗi 5 phút, node mesh-only sẽ kiểm tra lại WiFi
- Nếu WiFi đã khả dụng → tự động chuyển về chế độ NORMAL

## Serial Monitor Output

### Khi khởi động (có WiFi):
```
🔍 Checking WiFi availability...
✓ WiFi connected
   IP: 192.168.1.100
🔧 ESP32 MAC Address: AA:BB:CC:DD:EE:FF
✓ ESP-NOW initialized
✓ Broadcast peer added
Đang kết nối MQTT broker...Đã kết nối!
```

### Khi khởi động (không WiFi):
```
🔍 Checking WiFi availability..........
⚠️ WiFi not available - switching to MESH mode
⚠️ Running in MESH-ONLY mode (no WiFi)
🔧 ESP32 MAC Address: AA:BB:CC:DD:EE:FF
✓ ESP-NOW initialized
```

### Khi nhận data từ mesh:
```
📡 ESP-NOW Data Received:
  From: AA:BB:CC:DD:EE:FF
  Client ID: ESP32_5
  Speed: 45.2 km/h
  Location: 12.665242, 108.037230
  Hop Count: 0
  ➡️ Relaying to MQTT...
MQTT Published: ESP32_5,45.20,12.665242,108.037230
```

### Khi gửi qua mesh:
```
📡 Sending via ESP-NOW mesh network...
📤 ESP-NOW Send Status: Success
```

## Cấu Hình Nâng Cao

### 1. Thêm Peer Cụ Thể (Thay Vì Broadcast)

Nếu bạn biết MAC address của các ESP32 láng giềng, có thể thêm peer cụ thể:

```cpp
void setup() {
  // ... code khác ...
  
  // Thêm các node cụ thể
  addSpecificPeer("AA:BB:CC:DD:EE:F1");  // ESP32 bên trái
  addSpecificPeer("AA:BB:CC:DD:EE:F2");  // ESP32 bên phải
}
```

**Ưu điểm**: Tiết kiệm băng thông, giảm nhiễu

### 2. Thay Đổi Khoảng Thời Gian Kiểm Tra WiFi

Trong `mesh_network.h`, thay đổi:
```cpp
const unsigned long WIFI_CHECK_INTERVAL = 300000;  // 5 phút
```

Thành:
```cpp
const unsigned long WIFI_CHECK_INTERVAL = 60000;   // 1 phút
```

### 3. Điều Chỉnh Hop Limit

Trong `mesh_network.h`, callback `onDataRecv()`:
```cpp
else if (!isWiFiAvailable && incomingData.hopCount < 3) {
```

Thay `3` thành số hop tối đa mong muốn (1-5).

## Troubleshooting

### Vấn đề: Data không được relay
**Nguyên nhân**: Node có WiFi chưa nhận được broadcast
**Giải pháp**: 
- Kiểm tra cả 2 node có cùng channel WiFi không
- Giảm khoảng cách giữa các node (<50m)
- Thêm peer cụ thể thay vì broadcast

### Vấn đề: Hop count quá cao
**Nguyên nhân**: Quá nhiều node relay
**Giải pháp**: Giảm hop limit xuống 2

### Vấn đề: Node không tự chuyển về WiFi mode
**Nguyên nhân**: WiFi credentials sai hoặc router quá xa
**Giải pháp**: 
- Kiểm tra SSID/Password trong `config.h`
- Đặt node gần router hơn
- Giảm `WIFI_CHECK_INTERVAL` để kiểm tra thường xuyên hơn

## Lấy MAC Address của ESP32

Upload code sau để lấy MAC:
```cpp
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {}
```

## Ví Dụ Triển Khai Thực Tế

### Scenario 1: Đoàn xe đạp 10 chiếc
- ESP32_1 đến ESP32_5: Có WiFi dongle
- ESP32_6 đến ESP32_10: Không có WiFi (dùng mesh)

```
[ESP32_6] --ESP-NOW--> [ESP32_1] --MQTT--> Server
[ESP32_7] --ESP-NOW--> [ESP32_2] --MQTT--> Server
[ESP32_8] --ESP-NOW--> [ESP32_3] --MQTT--> Server
...
```

### Scenario 2: USB WiFi bị hỏng giữa chừng
1. ESP32_3 đang chạy NORMAL mode (có WiFi)
2. USB WiFi bị hỏng
3. Sau 5 phút, `recheckWiFi()` phát hiện mất WiFi
4. Tự động chuyển sang MESH-ONLY mode
5. Data được gửi qua ESP32_2 hoặc ESP32_4

### Scenario 3: Multi-hop relay
```
[ESP32_10] --> [ESP32_9] --> [ESP32_5] --> MQTT
   (hop=0)      (hop=1)       (hop=2, có WiFi)
```

## Performance

- **Latency**: ~50-200ms (ESP-NOW) + ~100-500ms (MQTT)
- **Range**: ~100m ngoài trời, ~30m trong nhà
- **Throughput**: ~250 kbps (ESP-NOW)
- **Battery Impact**: ESP-NOW tiêu thụ thấp hơn WiFi ~40%

## Lưu Ý Quan Trọng

1. **ESP-NOW và WiFi cùng channel**: ESP32 phải ở cùng channel WiFi để ESP-NOW hoạt động
2. **Broadcast limitation**: Tối đa 6 peer nếu dùng broadcast mode
3. **Data size**: Tối đa 250 bytes mỗi packet ESP-NOW
4. **Security**: ESP-NOW hỗ trợ encryption (hiện tại chưa enable)

## Tương Lai

- [ ] Thêm encryption cho ESP-NOW
- [ ] Implement routing table thông minh (chọn hop tốt nhất)
- [ ] Battery monitoring và auto sleep mode
- [ ] OTA update qua mesh network
