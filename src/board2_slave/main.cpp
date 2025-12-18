#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <Servo.h>
#include <config.h>

// ================================================================
// CẤU HÌNH PHẦN CỨNG - BOARD 2
// ================================================================
// Slot 2
#define S2_TRIG_PIN D0
#define S2_ECHO_PIN D1
#define S2_SERVO_PIN D6

// Flame sensor
#define FLAME_PIN D2
#define FLAME_THRESHOLD 500

const int DISTANCE_THRESHOLD = 10;

// ================================================================
// KHAI BÁO BIẾN
// ================================================================
Servo servo2;

FirebaseData fbdo;
FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

int s2_lastState = -1; // 0: Trống, 1: Có xe
bool isFire = false;   // biến trạng thái cháy

unsigned long lastCheckTime = 0;

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

    // --- Xử lý Slot 2 ---
    long dist2 = getDistance(S2_TRIG_PIN, S2_ECHO_PIN);
    int s2_status = (dist2 > 0 && dist2 < DISTANCE_THRESHOLD) ? 1 : 0;
    
    if (s2_status != s2_lastState) {
      Serial.print("SLOT 2 - Dang gui... ");
      if (Firebase.RTDB.setInt(&fbdo, "/trang_thai_do/slot_2", s2_status)) {
         Serial.println("OK!");
         s2_lastState = s2_status;
      } else {
         Serial.printf("LOI: %s\n", fbdo.errorReason().c_str());
      }
    }
  }
}

// ================================================================
// TASK KHẨN CẤP: PHÁT HIỆN LỬA
// ================================================================
void taskSafety() {
  // Đọc tín hiệu Digital (0 hoặc 1)
  // Cảm biến lửa: LOW (0) = CÓ CHÁY, HIGH (1) = KHÔNG CHÁY
  int flameState = digitalRead(FLAME_PIN);
  
  if (flameState == LOW) { // PHÁT HIỆN LỬA
    if (!isFire) {
      isFire = true; // Đánh dấu đang cháy để không lặp lại lệnh này
      Serial.println("!!! CANH BAO CHAY (FIRE DETECTED) !!!");
      
      servo2.write(90); // Mở cửa khẩn cấp
      
      if (Firebase.ready()) {
         // Dùng setBool thay vì setBoolAsync để đảm bảo lệnh quan trọng được gửi
         Firebase.RTDB.setBool(&fbdo, "/canh_bao/co_chay", true);
      }
    }
  } 
  else { // KHÔNG CÓ LỬA (HIGH)
    if (isFire) { // Nếu trước đó đang cháy mà giờ hết cháy
      isFire = false; // Reset trạng thái
      Serial.println("✅ Da het chay. He thong binh thuong.");
      
      if (Firebase.ready()) {
         Firebase.RTDB.setBool(&fbdo, "/canh_bao/co_chay", false);
      }
    }
  }
}

// ================================================================
// XỬ LÝ LỆNH TỪ APP (Stream Callback)
// ================================================================
void streamCallback(FirebaseStream data){
  if(data.dataType() == "boolean") {
    bool isOpen = data.boolData();
    // Chỉ xử lý slot 2
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

  pinMode(S2_TRIG_PIN, OUTPUT);
  pinMode(S2_ECHO_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  servo2.attach(S2_SERVO_PIN);
  servo2.write(0);

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

  // Chỉ lắng nghe điều khiển cửa của Slot 2
  if(!Firebase.RTDB.beginStream(&stream, "/barrier_slot")) {
    Serial.printf("Stream error: %s\n", stream.errorReason().c_str());
  }
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
}

void loop() {
  // Luôn chạy taskSafety để phản ứng nhanh nhất với lửa
  taskSafety();
  
  if (Firebase.ready()) {
    taskParking(); 
  }
}
