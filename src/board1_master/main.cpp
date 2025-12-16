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
// CẤU HÌNH PHẦN CỨNG - BOARD 1
// ================================================================
// Slot 1
#define S1_TRIG_PIN D0
#define S1_ECHO_PIN D4
#define S1_SERVO_PIN D3

// DHT11
#define DHTPIN D7
#define DHTTYPE DHT11

//Oled 0.96inch
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

const int DISTANCE_THRESHOLD = 10;

// ================================================================
// KHAI BÁO BIẾN
// ================================================================
Servo servo1;
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// Trạng thái của chính nó
int s1_lastState = -1; // 0: Trống, 1: Có xe

// Biến để lưu trạng thái đọc từ Firebase về
int s2_status_from_firebase = 0;
bool fire_status_from_firebase = false;

int total_lastState = -1;

unsigned long lastCheckTime = 0;
unsigned long lastEnvTime = 0;
unsigned long lastDisplayTime = 0;

float currentTemp = 0;
float currentHum = 0;

// ================================================================
// HÀM HỖ TRỢ (HELPER FUNCTIONS)
// ================================================================
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  if(duration == 0) return 999;
  return (duration*0.034)/2;
}

// ================================================================
// XỬ LÝ CẢM BIẾN XE (Task Parking)
// ================================================================
void taskParking() {
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
      } else {
         Serial.printf("LOI: %s\n", fbdo.errorReason().c_str());
      }
    }

    // --- Tính toán và gửi tổng chỗ trống ---
    int currentEmptySlots = 0;
    if (s1_lastState == 0) currentEmptySlots++;
    if (s2_status_from_firebase == 0) currentEmptySlots++; // Dùng biến đọc từ Firebase

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
  if (millis() - lastEnvTime > 5000) {
    lastEnvTime = millis();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
      Serial.println("Loi: Khong doc duoc DHT11!");
      return;
    }
    currentTemp = t;
    currentHum = h;
    Serial.println("Độ ẩm: " + String(h) + " % | Nhiệt độ: " + String(t) + " °C");
    if (Firebase.ready()) {
        Firebase.RTDB.setFloatAsync(&fbdo, "/thong_so_moi_truong/nhiet_do", t);
        Firebase.RTDB.setFloatAsync(&fbdo, "/thong_so_moi_truong/do_am", h);
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

    // --- Ưu tiên nếu có cháy (đọc từ Firebase) ---
    if (fire_status_from_firebase) {
      display.setTextSize(2);
      display.setCursor(10, 10);
      display.println("WARNING!");
      display.setCursor(20, 40);
      display.println("FIRE !!!");
      display.display();
      return;
    }

    // --- Hiển thị thông thường ---
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("SMART PARKING   ");
    display.print((int)currentTemp); 
    display.print("C");
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    int slots = 0;
    if (s1_lastState == 0) slots++;
    if (s2_status_from_firebase == 0) slots++; // Dùng biến từ Firebase

    display.setTextSize(2);
    display.setCursor(0, 25);
    display.print("FREE: ");
    display.print(slots);
    display.print("/2");

    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print(s1_lastState == 1 ? "S1:[X]" : "S1:[ ]"); 
    display.setCursor(70, 55);
    display.print(s2_status_from_firebase == 1 ? "S2:[X]" : "S2:[ ]"); // Dùng biến từ Firebase

    display.display();
  }
}

// ================================================================
// XỬ LÝ LỆNH & DỮ LIỆU TỪ FIREBASE
// ================================================================
void streamCallback(FirebaseStream data){
  String path = data.dataPath();

  // 1. Lắng nghe lệnh mở cửa cho Slot 1
  if(path == "/barrier_slot/slot_1" && data.dataType() == "boolean") {
    bool isOpen = data.boolData();
    if(isOpen) {
      Serial.println("LENH: MO CUA SLOT 1");
      servo1.write(90);
    } else {
      Serial.println("LENH: DONG CUA SLOT 1");
      servo1.write(0);
    }
  }

  // 2. Lắng nghe trạng thái của Slot 2
  if(path == "/trang_thai_do/slot_2" && data.dataType() == "int") {
    s2_status_from_firebase = data.intData();
    Serial.printf("Nhan cap nhat Slot 2: %s\n", s2_status_from_firebase == 1 ? "CO XE" : "TRONG");
  }

  // 3. Lắng nghe cảnh báo cháy
  if(path == "/canh_bao/co_chay" && data.dataType() == "boolean") {
    fire_status_from_firebase = data.boolData();
    Serial.printf("Nhan cap nhat bao chay: %s\n", fire_status_from_firebase ? "CO CHAY" : "BINH THUONG");
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

  pinMode(S1_TRIG_PIN, OUTPUT);
  pinMode(S1_ECHO_PIN, INPUT);
  servo1.attach(S1_SERVO_PIN);
  servo1.write(0);
  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("LOI OLED!"));
  } else {
    display.clearDisplay();
    display.display();
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi is connected");

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Lắng nghe toàn bộ thay đổi trên Firebase
  if(!Firebase.RTDB.beginStream(&stream, "/")) {
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
