# Safety-helmet

Hệ thống giám sát an toàn mũ bảo hộ sử dụng ESP32 và ESP8266 kết nối qua MQTT, tích hợp hiển thị dữ liệu thời gian thực qua Blynk.

##  Thiết bị phần cứng

### Sender (ESP32)
- **DHT11**: đo nhiệt độ và độ ẩm.
- **MQ2**: phát hiện khí gas.
- **FSR (Force Sensor)**: phát hiện lực tác động.
- **Cảm biến đội mũ**: phát hiện mũ có được đội hay không.
- **Nút nhấn vật lý**: gửi tín hiệu SOS.
- **WiFi + MQTT**: gửi dữ liệu đến ESP8266.

### Receiver (ESP8266)
- **LED cảnh báo nhiệt độ/độ ẩm/đội mũ** (D1)
- **LED phản hồi nút nhấn** (D2)
- **LED nhấp nháy cảnh báo** (D7)
- **Buzzer cảnh báo** (D0)
- **MQTT + Blynk**: hiển thị dữ liệu và cảnh báo.

##  Chức năng chính

- Gửi dữ liệu từ ESP32 đến ESP8266 qua MQTT:
  - Nhiệt độ, độ ẩm, trạng thái khí gas, lực va chạm, trạng thái đội mũ.
- ESP8266:
  - Hiển thị dữ liệu trên Blynk (temperature, humidity, gas, force, mũ đội).
  - Cảnh báo bằng LED và Buzzer khi có sự cố (gas, va chạm, không đội mũ).
  - Cho phép gửi lệnh điều khiển lại sang ESP32 (bằng nút nhấn hoặc Blynk).
- Tự động chọn mạng WiFi có tín hiệu mạnh nhất từ danh sách cấu hình.

##  Giao tiếp

- **Giao thức**: MQTT (qua TLS/SSL).
- **Broker**: HiveMQ Cloud.
- **Chủ đề (MQTT topics)**:
  - `helmet/sender/data`: dữ liệu sensor từ ESP32.
  - `helmet/sender/status`: trạng thái nút nhấn từ ESP32.
  - `helmet/sender/alert`: cảnh báo từ ESP32.
  - `helmet/sender/ledcontrol`: lệnh điều khiển thủ công.
  - `helmet/receiver/request`: ESP8266 gửi yêu cầu đến ESP32.
  - `helmet/sender/wifi_status`: SSID mà ESP32 đang kết nối.

##  Blynk Dashboard

Các chân ảo sử dụng:
- `V0`: Nhiệt độ.
- `V1`: Độ ẩm.
- `V2`: Trạng thái khí gas.
- `V3`: Trạng thái LED mũ bảo hộ.
- `V4`: Giá trị FSR (lực).
- `V5`: Cảnh báo tổng hợp.
- `V6`: Nút điều khiển từ xa (gửi từ Blynk đến ESP32).

## 🧾 Cấu trúc thư mục

