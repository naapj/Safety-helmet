# Safety-helmet

**Safety-helmet** là hệ thống giám sát thông minh sử dụng ESP8266 nhằm bảo vệ người lao động trong môi trường công nghiệp. Hệ thống này phát hiện khí gas, kiểm tra trạng thái đội mũ bảo hộ, và cảnh báo va chạm thông qua kết nối WiFi, MQTT và tích hợp giao diện người dùng với Blynk.

---

## 📦 Chức năng chính

- 🔥 Phát hiện khí gas theo thời gian thực
- 🧠 Giám sát trạng thái đội mũ bảo hộ
- 🌡️ Theo dõi nhiệt độ và độ ẩm (nhận từ node sender)
- 🛑 Nút khẩn cấp thủ công để gửi cảnh báo
- 📲 Tích hợp Blynk hiển thị thông số và cảnh báo
- 📡 Tự động kết nối mạng WiFi mạnh nhất
- 🔔 Báo động bằng đèn LED và còi

---

## 🧰 Phần cứng (Receiver)

- ESP8266 (NodeMCU)
- Còi (D0)
- LED báo hiệu (D1, D2, D7)
- Nút nhấn khẩn cấp (D3)
- Kết nối WiFi

---

## 🌐 WiFi cấu hình

Receiver sẽ tự động quét và kết nối với mạng WiFi mạnh nhất từ danh sách đã lưu:

```cpp
KnownNetwork knownNetworks[] = {
  {"Goldenland P402", "golden1234"},
  {"WIFI_B", "password123"},
  {"A1306", "thanha1306"}
};
