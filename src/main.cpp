#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <config.h>

// ================================================================
// CẤU HÌNH PHẦN CỨNG
// ================================================================
// Slot 1
#define S1_TRIG_PIN D0
#define S1_ECHO_PIN D4
#define S1_SERVO_PIN D3

// Slot 2
#define S2_TRIG_PIN D5
#define S2_ECHO_PIN D8
#define S2_SERVO_PIN D6

// DHT11
#define DHTPIN D7
#define DHTTYPE DHT11

// Flame sensor
#define FLAME_PIN A0
#define FLAME_THRESHOLD 500
bool isFire = false; // biến trạng thái cháy

//Oled 0.96inch
// ESP8266 mặc định: SCL = D1, SDA = D2 (Không cần define chân nếu dùng mặc định)
#define SCREEN_WIDTH 128    // Chiều rộng
#define SCREEN_HEIGHT 64    // Chiều cao
#define OLED_RESET    -1    // Chân Reset (-1 là dùng chung với chân Reset của mạch)
#define SCREEN_ADDRESS 0x3C // Địa chỉ I2C phổ biến nhất

const int DISTANCE_THRESHOLD = 10;


// ================================================================
// KHAI BÁO BIẾN
// ================================================================
Servo servo1;
Servo servo2;

DHT dht(DHTPIN, DHTTYPE);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

int s1_lastState = -1; // 0: Trống, 1: Có xe
int s2_lastState = -1;
int total_lastState = -1; // Lưu trạng thái tổng số xe để so sánh

unsigned long lastCheckTime = 0; // Thời gian quét cảm biến
unsigned long lastEnvTime = 0;
unsigned long lastDisplayTime = 0;

float currentTemp = 0;
float currentHum = 0;


// ================================================================
// HÀM HỖ TRỢ (HELPER FUNCTIONS)
// ================================================================
long getDistance(int trigPin, int echoPin) {
  // Phát xung 10 micro giây
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Đọc xung high tại chân echo
  long duration = pulseIn(echoPin, HIGH, 30000); // Timeout 30ms
  if(duration == 0) return 999; // Lỗi hoặc quá xa

  return (duration*0.034)/2;
}


// ================================================================
// XỬ LÝ CẢM BIẾN XE (Task Parking)
// ================================================================
void taskParking() {
  // Quét cảm biến mỗi 500ms
  if (millis() - lastCheckTime > 500) {
    lastCheckTime = millis();

    // --- Xử lý Slot 1 ---
    long dist1 = getDistance(S1_TRIG_PIN, S1_ECHO_PIN);
    int s1_status = (dist1 > 0 && dist1 < DISTANCE_THRESHOLD) ? 1 : 0; 
    
    if (s1_status != s1_lastState) {
      Serial.print("SLOT 1 - Dang gui... ");
      if (Firebase.RTDB.setInt(&fbdo, "/trang_thai_do/slot_1", s1_status)) {
         Serial.println("OK!");
         s1_lastState = s1_status; 
         Serial.printf("Dist: %ld cm -> Status: %d\n", dist1, s1_status);
      } else {
         Serial.printf("LOI: %s\n", fbdo.errorReason().c_str());
      }
    }

    // --- Xử lý Slot 2 ---
    long dist2 = getDistance(S2_TRIG_PIN, S2_ECHO_PIN);
    int s2_status = (dist2 > 0 && dist2 < DISTANCE_THRESHOLD) ? 1 : 0;
    
    if (s2_status != s2_lastState) {
      Serial.print("SLOT 2 - Dang gui... ");
      if (Firebase.RTDB.setInt(&fbdo, "/trang_thai_do/slot_2", s2_status)) {
         Serial.println("OK!");
         s2_lastState = s2_status;
         Serial.printf("Dist: %ld cm -> Status: %d\n", dist2, s2_status);
      } else {
         Serial.printf("LOI: %s\n", fbdo.errorReason().c_str());
      }
    }

    // --- Tính toán và gửi chỗ trống (Yêu cầu mới) ---
    // Tính tổng số chỗ trống hiện tại
    int currentEmptySlots = 0;
    if (s1_lastState == 0) currentEmptySlots++;
    if (s2_lastState == 0) currentEmptySlots++;

    // Nếu số chỗ trống thay đổi so với lần gửi trước -> Gửi lên Firebase
    if(currentEmptySlots != total_lastState) {
      Serial.printf("Cap nhat cho trong: %d\n", currentEmptySlots);
      if(Firebase.RTDB.setIntAsync(&fbdo, "/thong_so_moi_truong/so_luong_do_xe", currentEmptySlots)) {
        total_lastState = currentEmptySlots;
      }
    }
  }
}


// ================================================================
// ĐỌC NHIỆT ĐỘ & ĐỘ ẨM (Task Environment)
// ================================================================
void taskDHT() {
  // Chỉ đọc mỗi 5 giây (5000ms)
  if (millis() - lastEnvTime > 5000) {
    lastEnvTime = millis();

    // Đọc dữ liệu
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Kiểm tra xem đọc có lỗi không (quan trọng)
    if (isnan(h) || isnan(t)) {
      Serial.println("Loi: Khong doc duoc DHT11!");
      return;
    }

    // Cập nhật biến toàn cục
    currentTemp = t;
    currentHum = h;

    Serial.println("Độ ẩm: " + String(h) + " % | Nhiệt độ: " + String(t) + " °C");

    // Gửi lên Firebase (Dùng setFloat)
    // Lưu vào nhánh: /thong_so_moi_truong
    if (Firebase.ready()) {
        Firebase.RTDB.setFloat(&fbdo, "/thong_so_moi_truong/nhiet_do", t);
        Firebase.RTDB.setFloat(&fbdo, "/thong_so_moi_truong/do_am", h);
    }
  }
}


// ================================================================
// TASK KHẨN CẤP: PHÁT HIỆN LỬA
// ================================================================
void taskSafety() {
  // Đọc giá trị Analog (0 - 1024)
  // Càng gần lửa, giá trị càng thấp
  int flameValue = analogRead(FLAME_PIN);
  
  // Nếu giá trị thấp hơn ngưỡng -> CÓ CHÁY
  if (flameValue < FLAME_THRESHOLD) {
    if (!isFire) { // Nếu mới phát hiện cháy lần đầu
      isFire = true;
      Serial.println("!!! CANH BAO CHAY !!!");
      
      // 1. Mở hết cửa ngay lập tức (Chế độ thoát hiểm)
      servo1.write(90);
      servo2.write(90);
      
      // 2. Gửi cảnh báo lên Firebase
      if (Firebase.ready()) {
         Firebase.RTDB.setBoolAsync(&fbdo, "/canh_bao/co_chay", true);
      }
    }
  } else {
    // Hết cháy
    if (isFire) {
      isFire = false;
      Serial.println("Da het chay. He thong binh thuong.");
      // Tắt báo cháy trên Firebase
      if (Firebase.ready()) {
         Firebase.RTDB.setBoolAsync(&fbdo, "/canh_bao/co_chay", false);
      }
    }
  }
}


// ================================================================
// HIỂN THỊ MÀN HÌNH OLED
// ================================================================
void taskDisplay() {
  if (millis() - lastDisplayTime > 1000) {
    lastDisplayTime = millis();
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // --- Ưu tiên nếu có cháy ---
    if (isFire) {
      display.setTextSize(2);
      display.setCursor(10, 10);
      display.println("WARNING!");
      display.setTextSize(2);
      display.setCursor(20, 40);
      display.println("FIRE !!!");
      display.display();
      return; // Dừng hàm tại đây, không hiện thông tin xe nữa
    }

    // --- Dòng 1: Tiêu đề ---
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("SMART PARKING   ");
    display.print((int)currentTemp); 
    display.print("C");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // --- Dòng 2: Số chỗ trống ---
    // Tính lại để hiển thị cho khớp
    int slots = 0;
    if (s1_lastState == 0) slots++;
    if (s2_lastState == 0) slots++;

    display.setTextSize(2); // Chữ to
    display.setCursor(0, 25);
    display.print("FREE: ");
    display.print(slots);
    display.print("/2");

    // --- Dòng 3: Trạng thái chi tiết ---
    display.setTextSize(1); // Chữ nhỏ
    display.setCursor(0, 55);
    // Toán tử 3 ngôi: Nếu 1 (có xe) thì hiện [X], nếu 0 thì hiện [ ]
    display.print(s1_lastState == 1 ? "S1:[X]" : "S1:[ ]"); 
    display.setCursor(70, 55);
    display.print(s2_lastState == 1 ? "S2:[X]" : "S2:[ ]");

    display.display();
  }
}


// ================================================================
// XỬ LÝ LỆNH TỪ APP (Stream Callback)
// ================================================================
void streamCallback(FirebaseStream data){
  if(data.dataType() == "boolean") {
    bool isOpen = data.boolData();

    // Xử lý slot 1
    if(data.dataPath() == "/slot_1") {
      if(isOpen) {
        Serial.println("LENH: MO CUA SLOT 1");
        servo1.write(90);
      }
      else {
        Serial.println("LENH: DONG CUA SLOT 1");
        servo1.write(0);
      }
    }

    // Xử lý slot 2
    if(data.dataPath() == "/slot_2") {
      if(isOpen) {
        Serial.println("LENH: MO CUA SLOT 2");
        servo2.write(90);
      }
      else {
        Serial.println("LENH: DONG CUA SLOT 2");
        servo2.write(0);
      }
    }
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) Serial.println("Stream timeout...");
}


// ================================================================
// MAIN SETUP & LOOP
// ================================================================
void setup() {
  Serial.begin(115200);

  // Setup cảm biến
  pinMode(S1_TRIG_PIN, OUTPUT);
  pinMode(S1_ECHO_PIN, INPUT);
  pinMode(S2_TRIG_PIN, OUTPUT);
  pinMode(S2_ECHO_PIN, INPUT);

  // Setup servo
  servo1.attach(S1_SERVO_PIN);
  servo2.attach(S2_SERVO_PIN);
  servo1.write(0);
  servo2.write(0);

  // Setup hdt11
  dht.begin();

  // Setup Oled
  // Dùng chân D1 (SCL) và D2 (SDA) mặc định
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("LOI OLED! Kiem tra day D1, D2"));
  } else {
    display.clearDisplay();
    display.display();
  }

  //Kết nối wifi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi is connected");

  // Kết nối firebase
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Lắng nghe điều khiển cửa (Lắng nghe thư mục cha barrier_slot)
  if(!Firebase.RTDB.beginStream(&stream, "/barrier_slot")) {
    Serial.printf("Stream error: %s\n", stream.errorReason().c_str());
  }
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
}

void loop() {
  if (Firebase.ready()) {
    taskParking(); 
    taskDHT();
    taskDisplay();
  }
}