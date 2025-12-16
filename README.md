# Hệ thống Đỗ xe Thông minh (IoT Smart Parking System)

Đây là dự án mã nguồn mở cho một hệ thống đỗ xe thông minh sử dụng vi điều khiển ESP8266, được xây dựng với PlatformIO. Hệ thống có khả năng phát hiện xe, điều khiển rào chắn, giám sát môi trường và kết nối với Firebase để lưu trữ dữ liệu và điều khiển từ xa.

## Tính năng chính

- **Phát hiện chỗ đỗ xe thời gian thực:** Sử dụng 2 cảm biến siêu âm để giám sát 2 vị trí đỗ xe.
- **Điều khiển rào chắn (Barrier):** Dùng động cơ Servo để mô phỏng việc mở/đóng rào chắn cho từng vị trí.
- **Giám sát môi trường:** Đo nhiệt độ và độ ẩm bằng cảm biến DHT11.
- **Cảnh báo cháy:** Phát hiện lửa bằng cảm biến lửa và gửi cảnh báo khẩn cấp.
- **Hiển thị tại chỗ:** Màn hình OLED hiển thị trạng thái các chỗ đỗ, nhiệt độ và cảnh báo.
- **Tích hợp Firebase:**
    - Gửi toàn bộ dữ liệu (trạng thái chỗ đỗ, nhiệt độ, độ ẩm, cảnh báo cháy) lên Firebase Realtime Database.
    - Nhận lệnh điều khiển rào chắn từ ứng dụng di động/web thông qua Firebase.
- **Kiến trúc 2-Board (Master-Slave):** Tải được chia ra 2 board ESP8266 để tăng tính ổn định và dễ mở rộng.

## Phần cứng cần thiết

### Board 1 (Master)
- 1x ESP8266 (NodeMCU)
- 1x Cảm biến siêu âm HC-SR04
- 1x Động cơ Servo (ví dụ: SG90)
- 1x Cảm biến nhiệt độ & độ ẩm DHT11
- 1x Màn hình OLED 0.96" I2C (SSD1306)
- Breadboard và dây cắm

### Board 2 (Slave)
- 1x ESP8266 (NodeMCU)
- 1x Cảm biến siêu âm HC-SR04
- 1x Động cơ Servo (ví dụ: SG90)
- 1x Cảm biến lửa (Flame Sensor)
- Breadboard và dây cắm

## Phần mềm và Thư viện

- **IDE:** Visual Studio Code với extension PlatformIO.
- **Cloud:** Google Firebase Realtime Database.
- **Thư viện chính (khai báo trong `platformio.ini`):**
    - `mobizt/Firebase Arduino Client Library for ESP8266 and ESP32`
    - `adafruit/DHT sensor library`
    - `adafruit/Adafruit GFX Library`
    - `adafruit/Adafruit SSD1306`

## Cấu trúc Project

Dự án được tổ chức để hỗ trợ 2 board ESP8266 trong cùng một project:

- `src/board1_master/main.cpp`: Mã nguồn cho board Master, chịu trách nhiệm quản lý Slot 1, DHT, OLED và giao tiếp tổng thể với Firebase.
- `src/board2_slave/main.cpp`: Mã nguồn cho board Slave, chịu trách nhiệm quản lý Slot 2, cảm biến lửa và báo cáo trạng thái lên Firebase.
- `include/config.h`: File cấu hình chứa thông tin nhạy cảm (WiFi, Firebase API Key). **File này không được đưa lên Git.**
- `platformio.ini`: File cấu hình của PlatformIO, định nghĩa 2 môi trường build riêng biệt cho mỗi board.

## Hướng dẫn Cài đặt và Sử dụng

1.  **Clone Repository:**
    ```bash
    git clone <URL-repository-cua-ban>
    cd <ten-thu-muc-project>
    ```

2.  **Cài đặt PlatformIO:** Mở project bằng VS Code, PlatformIO sẽ tự động cài đặt các thư viện cần thiết.

3.  **Thiết lập Firebase:**
    - Tạo một project mới trên Firebase Console.
    - Tạo một Realtime Database.
    - Lấy **Database URL** và **Database Secret** (API Key).

4.  **Tạo file `config.h`:**
    - Trong thư mục `include`, tạo một file mới tên là `config.h`.
    - Thêm nội dung sau và điền thông tin của bạn:
      ```cpp
      #ifndef CONFIG_H
      #define CONFIG_H

      // Cấu hình WiFi
      #define WIFI_SSID "TEN_WIFI_CUA_BAN"
      #define WIFI_PASS "MAT_KHAU_WIFI"

      // Cấu hình Firebase
      #define DATABASE_URL "URL_DATABASE_CUA_BAN.firebaseio.com"
      #define DATABASE_SECRET "SECRET_API_KEY_CUA_BAN"

      #endif
      ```

5.  **Kết nối phần cứng:** Nối dây các linh kiện theo sơ đồ chân đã định nghĩa trong file `main.cpp` của mỗi board.

6.  **Build và Nạp code:**
    - Cắm cả hai board ESP8266 vào máy tính.
    - Xác định cổng COM cho mỗi board và cập nhật trong file `platformio.ini` ở các mục `upload_port` và `monitor_port`.
    - Nhấn nút **Upload** (biểu tượng mũi tên) ở thanh trạng thái dưới cùng của VS Code. PlatformIO sẽ tự động build và nạp code cho cả hai board một cách tuần tự.

---
*Dự án được thực hiện bởi [Tôi :D].*