/***************************************************
 * Project: Designed a fire alarm system for a Smart Home.

 * PHẦN 1: KHAI BÁO THƯ VIỆN VÀ BIẾN TOÀN CỤC  
 ***************************************************/
//Phan mem lap trinh: Arduino
//
#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Cấu hình cảm biến
#define DHTPIN 4
#define DHTTYPE DHT11
#define GAS_PIN 34
#define LED_PIN1 13 //led sd chân D18 là led cảnh báo
#define LED_PIN 18 //led1
#define SERVO_PIN 16
#define BUZZER_PIN 17  // chân D17 trên ESP32
#define GAS_THRESHOLD 1800
#define GAS_DOOR_OPEN_THRESHOLD 200  // Ngưỡng mở cửa tự động
#define TEMP_DOOR_OPEN_THRESHOLD 40.0  // Ngưỡng nhiệt độ mở cửa tự động
DHT dht(DHTPIN, DHTTYPE);

// Cấu hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cấu hình Servo
Servo myServo;
int servoPos = 0;
bool servoDirection = true;
const int SERVO_MIN = 0;
const int SERVO_MAX = 120;
const int SERVO_DELAY = 15;
bool doorAutoMode = true;  // Chế độ tự động mở cửa
bool lastDoorState = false;  // Theo dõi trạng thái cửa lần trước
// Cấu hình WiFi
const char* ssid = "Nguyen Giap";
const char* password = "88888868";

// Cấu hình web server
WebServer server(80);

// Cấu hình NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const long timeOffset = 7 * 3600;

// Cấu hình Google Sheets
const char* scriptURL = "https://script.google.com/macros/s/AKfycbwQlIsCqyJPl9IF7Y_RcRP_PURmXN4XrbFSmlyeesjYgyqSImrjEmt_LRuhnR9Phvdq2Q/exec";
const unsigned long sheetsUpdateInterval = 30000;
unsigned long lastSheetsUpdate = 0;
bool sheetsEnabled = true;

// Cấu hình lịch sử dữ liệu
#define HISTORY_SIZE 20
struct SensorData {
  String timestamp;
  float temperature;
  float humidity;
  float gasValue;
  bool gasWarning;
  String status;
  int servoPosition;
  String doorStatus;  // Thêm trạng thái cửa
};

// Biến toàn cục
SensorData sensorHistory[HISTORY_SIZE];
int historyIndex = 0;
bool isHistoryFull = false;
float temperature = NAN;
float humidity = NAN;
float gasValue = NAN;
bool gasWarning = false;
String lastUpdateTime = "Đang cập nhật...";
String currentStatus = "Bình thường";
unsigned long lastLedBlink = 0;
bool ledState = false;
unsigned long lastServoUpdate = 0;
//led cảnh báo
unsigned long lastLed1Blink = 0;
bool led1State = false;
bool tempGasWarning = false;  // Cảnh báo nhiệt độ hoặc gas vượt ngưỡng
// còi loa
unsigned long lastBuzzerToggle = 0;
bool buzzerState = false;

/***************************************************
 * KHAI BÁO PROTOTYPE CÁC HÀM
 ***************************************************/
// Hàm xử lý web server
void handleRoot();
void handleData();
void handleLED();
void handleServo();
void handleDoor();
void handleSheets();

// Hàm điều khiển
void controlDoor(bool open);
void controlWarningLED();
void controlWarningLED1();
void controlServo();
void controlBuzzer();  // Thêm dòng này

// Hàm hiển thị và cảm biến
void updateOLED();
void updateSensorData();
void sendDataToGoogleSheets();

// Hàm hỗ trợ
String urlEncode(String str);

/***************************************************
 * PHẦN 2: HÀM SETUP - KHỞI TẠO CHƯƠNG TRÌNH
 ***************************************************/
void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  // Khởi tạo OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Lỗi khởi tạo SSD1306"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Dang khoi dong...");
  display.display();

  // Khởi tạo cảm biến
  dht.begin();
  pinMode(GAS_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // led cảnh báo
  pinMode(LED_PIN1, OUTPUT);
  digitalWrite(LED_PIN1, LOW);
  // còi 
   pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Khởi tạo Servo
  myServo.attach(SERVO_PIN);
  myServo.write(SERVO_MIN);
  servoPos = SERVO_MIN;

  // Kết nối WiFi
  WiFi.begin(ssid, password);
  display.setCursor(0, 16);
  display.println("Dang ket noi WiFi...");
  display.display();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    display.print(".");
    display.display();
  }
  
  display.println("\nDa ket noi!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);

  // Khởi tạo NTP
  timeClient.begin();
  timeClient.setTimeOffset(timeOffset);
  timeClient.update();

  // Cấu hình web server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/led", handleLED);
  server.on("/led1", handleLED); //fix
  server.on("/servo", handleServo);
  server.on("/door", handleDoor); // Thêm route điều khiển cửa
  server.on("/sheets", handleSheets);
  server.begin();

  updateOLED();
}

/***************************************************
 * PHẦN 3: HÀM LOOP - XỬ LÝ CHÍNH CHƯƠNG TRÌNH
 ***************************************************/
void loop() {
  server.handleClient();
  static unsigned long lastUpdate = 0;
  static unsigned long lastOLEDUpdate = 0;
  
  // Cập nhật dữ liệu cảm biến mỗi 2 giây
  if (millis() - lastUpdate >= 2000) {
    lastUpdate = millis();
    updateSensorData();
    
    // Tự động mở cửa khi gas vượt ngưỡng
  if (doorAutoMode) {
    // Kiểm tra cả nhiệt độ và gas
    if (temperature > TEMP_DOOR_OPEN_THRESHOLD || gasValue > GAS_DOOR_OPEN_THRESHOLD) {
      controlDoor(true); // Mở cửa nếu cả 2 điều kiện thỏa mãn
    } else {
      controlDoor(false); // Đóng cửa nếu không thỏa mãn
    }
  }

  ///
  }

  // Cập nhật OLED mỗi 500ms
  if (millis() - lastOLEDUpdate >= 500) {
    lastOLEDUpdate = millis();
    updateOLED();
  }

  // Gửi dữ liệu lên Google Sheets mỗi 30 giây
  if (sheetsEnabled && millis() - lastSheetsUpdate >= sheetsUpdateInterval) {
    lastSheetsUpdate = millis();
    sendDataToGoogleSheets();
  }

  // Điều khiển LED cảnh báo
  controlWarningLED();
  // ... (các code hiện có)
  // controlWarningLED();   // LED cảnh báo gas (LED_PIN)
  controlWarningLED1();  // LED cảnh báo nhiệt độ/gas (LED_PIN1)
  // Điều khiển buzzer cảnh báo
  controlBuzzer();

  // Điều khiển Servo
  controlServo();
}

/***************************************************
 * CÁC HÀM HỖ TRỢ MỚI
 ***************************************************/
void controlDoor(bool open) { 
  if (open) {
    servoPos = SERVO_MIN; // Mở cửa (0 độ) ,, đã fix
    sensorHistory[historyIndex].doorStatus = "OPEN";
  } else {
    servoPos = SERVO_MAX; // Đóng cửa (0 độ)
    sensorHistory[historyIndex].doorStatus = "CLOSE";
  }
  myServo.write(servoPos);
}

/***************************************************
 * 
 ***************************************************/
void controlWarningLED1() {
 
// //

//Thêm hàm điều khiển buzzer:
void controlBuzzer() {
  if (tempGasWarning) {
    if (millis() - lastBuzzerToggle >= 500) {
      lastBuzzerToggle = millis();
      buzzerState = !buzzerState;
      
      if (buzzerState) {
        tone(BUZZER_PIN, 1000);  // Phát âm 1000 Hz
      } else {
        noTone(BUZZER_PIN);      // Tắt âm thanh
      }
    }
  } else {
    noTone(BUZZER_PIN);          // Đảm bảo tắt buzzer khi không cảnh báo
    buzzerState = false;
  }
}

//
void controlWarningLED() {
  if (gasWarning) {
    if (millis() - lastLedBlink >= 500) {
      lastLedBlink = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

void controlServo() {
  // Chỉ điều khiển servo nếu KHÔNG ở chế độ tự động
  if (!doorAutoMode && millis() - lastServoUpdate >= SERVO_DELAY) {
    lastServoUpdate = millis();
    
    if (servoDirection) {
      servoPos++;
      if (servoPos >= SERVO_MAX) {
        servoPos = SERVO_MAX;
        servoDirection = false;
      }
    } else {
      servoPos--;
      if (servoPos <= SERVO_MIN) {
        servoPos = SERVO_MIN;
        servoDirection = true;
      }
    }
    
    myServo.write(servoPos);
  }
}

void updateOLED() {
  display.clearDisplay();
  
  // Hiển thị tiêu đề
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ESP32 DHT11 Monitor");
  display.drawLine(0, 10, 128, 10, WHITE);
  
  // Hiển thị dữ liệu cảm biến
  display.setCursor(0, 15);
  display.print("Nhiet do: ");
  if (!isnan(temperature)) {
    display.print(temperature, 1);
    display.println(" C");
  } else {
    display.println("---");
  }
  
  display.setCursor(0, 25);
  display.print("Do am: ");
  if (!isnan(humidity)) {
    display.print(humidity, 0);
    display.println(" %");
  } else {
    display.println("---");
  }
  
  display.setCursor(0, 35);
  display.print("Khi gas: ");
  display.println(gasValue);
  if (gasWarning) {
    display.print(" !");
  }
  
  // Hiển thị trạng thái servo
 // Thay đổi dòng hiển thị trạng thái cửa
  display.print("Cua: ");
  display.print(doorAutoMode ? "AUTO" : "MANUAL");
  display.print(" (");
  display.print(servoPos > 60 ? "OPEN" : "CLOSE");
  display.print(")");
  
  // Hiển thị thời gian cập nhật
  display.setCursor(0, 55);
  display.print("Cap nhat: ");
  display.print(lastUpdateTime.substring(0, 5));
  //
  // === THÊM PHẦN CẢNH BÁO ===
  if (tempGasWarning) {
    display.setCursor(0, 50);  // Vị trí dòng cảnh báo ()
    display.setTextColor(BLACK, WHITE);  // Chữ đen trên nền trắng ()
    display.print(" CANH BAO ");
        display.print(" NGUY HIEM ! ");
    display.setTextColor(WHITE);  // 
    
    // Hoặc hiển thị đầy đủ thông báo nếu màn hình đủ rộng:
    // display.print("CANH BAO: NHIET DO/KHI GAS");
  }
  // === KẾT THÚC PHẦN THÊM ===
  
  display.display();
}

void updateSensorData() {
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  time_t rawtime = timeClient.getEpochTime();
  struct tm *ti = localtime(&rawtime);
  
  char dateStr[11];
  sprintf(dateStr, "%02d/%02d/%04d", ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
  String currentTime = formattedTime + " - " + String(dateStr);

  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  float newGas = analogRead(GAS_PIN);
  bool newGasWarning = (newGas > GAS_THRESHOLD);
  String newStatus = newGasWarning ? "CẢNH BÁO" : "Bình thường";

   // Kiểm tra cảnh báo nhiệt độ hoặc gas
  tempGasWarning = (temperature > TEMP_DOOR_OPEN_THRESHOLD) || (gasValue > GAS_DOOR_OPEN_THRESHOLD);
  // Nếu có cảnh báo, cập nhật trạng thái
  if (tempGasWarning) {
    newStatus = "CẢNH BÁO NHIỆT ĐỘ/KHÍ GAS CAO";
  }
  // 

  if (!isnan(newTemp) && !isnan(newHum)) {
   
    //
    SensorData newRecord = {
      currentTime,
      newTemp,
      newHum,
      newGas,
      newGasWarning,
      newStatus,
      servoPos,
      sensorHistory[(historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE].doorStatus // Giữ trạng thái cửa cũ
    };

    sensorHistory[historyIndex] = newRecord;
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    if (historyIndex == 0) isHistoryFull = true;
    
    temperature = newTemp;
    humidity = newHum;
    gasValue = newGas;
    gasWarning = newGasWarning;
    currentStatus = newStatus;
    lastUpdateTime = currentTime;

    Serial.print("Đã ghi dữ liệu | ");
    Serial.print(currentTime);
    Serial.print(" | Nhiệt độ: ");
    Serial.print(newTemp, 1);
    Serial.print("°C | Độ ẩm: ");
    Serial.print(newHum, 0);
    Serial.print("% | Khí gas: ");
    Serial.print(newGas);
    Serial.print(" | Trạng thái: ");
    Serial.print(newStatus);
    Serial.print(" | Servo: ");
    Serial.print(servoPos);
    Serial.print("° | LED: ");
    Serial.print(ledState ? "ON" : "OFF");
    Serial.print(" | Lịch sử: ");
    Serial.print(led1State ? "ON" : "OFF");
    Serial.print(isHistoryFull ? HISTORY_SIZE : historyIndex);
    Serial.println("/20");
  } else {
    Serial.println("Lỗi khi đọc dữ liệu từ cảm biến!");
    temperature = 0;
    humidity = 0;
  }
}

void sendDataToGoogleSheets() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    String url = String(scriptURL) + 
                 "?time=" + urlEncode(lastUpdateTime) +
                 "&temperature=" + String(temperature, 1) +
                 "&humidity=" + String(humidity, 0) +
                 "&gas=" + String(gasValue) +
                 "&status=" + urlEncode(currentStatus) +
                 "&servo=" + String(servoPos) +
                 "&door=" + urlEncode(sensorHistory[historyIndex].doorStatus) + // Thêm trạng thái cửa
                 "&led=" + String(ledState ? 1 : 0);
                 "&led1=" + String(led1State ? 1 : 0);
    
    Serial.print("Đang gửi dữ liệu lên Google Sheets: ");
    Serial.println(url);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.printf("[Sheets] Thành công! Mã: %d, Phản hồi: %s\n", httpCode, payload.c_str());
    } else {
      Serial.printf("[Sheets] Lỗi: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
  }
}

String urlEncode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

/***************************************************
 * HÀM XỬ LÝ CỬA MỚI
 ***************************************************/
void handleDoor() {
  String action = server.arg("action");
  
  if (action == "open") {
    controlDoor(true);
    server.send(200, "text/plain", "Đã mở cửa");
  } 
  else if (action == "close") {
    controlDoor(false);
    server.send(200, "text/plain", "Đã đóng cửa");
  }
  else if (action == "auto") {
    doorAutoMode = !doorAutoMode;
    server.send(200, "text/plain", "Chế độ tự động: " + String(doorAutoMode ? "BẬT" : "TẮT"));
  }
  else {
    server.send(400, "text/plain", "Lệnh không hợp lệ");
  }
}

/***************************************************
 * GIAO DIỆN WEB ()////////////////////////////////////////////
 ***************************************************/
void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 DHT11 Monitor</title>
  <style>
    /* Giữ nguyên toàn bộ CSS cũ */
    body { font-family: 'Segoe UI', sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; color: #333; }
    h1 { text-align: center; margin-bottom: 10px; color: #2c3e50; }
    h2 { text-align: center; margin-top: 0; margin-bottom: 20px; color: #34495e; font-weight: normal; }
    .table-container { max-height: 500px; overflow-y: auto; margin: 20px auto; width: 90%; box-shadow: 0 2px 10px rgba(0,0,0,0.1); border-radius: 8px; }
    table { width: 100%; border-collapse: collapse; background-color: white; border-radius: 8px; overflow: hidden; }
    th { background-color: #3498db; color: white; position: sticky; top: 0; font-weight: bold; text-align: center; padding: 12px; }
    td { border: 1px solid #ecf0f1; padding: 12px; text-align: center; }
    tr:nth-child(even) { background-color: #f8f9fa; }
    tr:hover { background-color: #e8f4fc; }
    .gas-warning { background-color: #ffdddd; font-weight: bold; color: #c0392b; }
    .control-panel { display: flex; flex-wrap: wrap; justify-content: space-between; margin: 20px auto; width: 90%; }
    .control-box { text-align: center; padding: 20px; background-color: white; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); width: 48%; margin-bottom: 20px; }
    .switch-container { display: flex; align-items: center; justify-content: center; margin-top: 15px; }
    .switch-label { margin: 0 15px; font-size: 18px; font-weight: 500; }
    .switch { position: relative; display: inline-block; width: 60px; height: 34px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #2ecc71; }
    input:checked + .slider:before { transform: translateX(26px); }
    .status-text { font-size: 16px; margin-top: 10px; }
    .status-on { color: #27ae60; font-weight: bold; }
    .status-off { color: #e74c3c; }
    .last-update { text-align: right; margin-top: 15px; font-style: italic; color: #95a5a6; padding-right: 5%; }
    .warning-banner { background-color: #f39c12; color: white; padding: 10px; text-align: center; border-radius: 5px; margin: 10px auto; width: 85%; display: none; }
    .btn { padding: 8px 16px; background-color: #3498db; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; margin: 5px; }
    .btn:hover { background-color: #2980b9; }
    .btn-servo { background-color: #9b59b6; }
    .btn-servo:hover { background-color: #8e44ad; }
    .servo-control { display: flex; justify-content: center; margin-top: 15px; }
    .servo-value { font-size: 18px; font-weight: bold; margin: 10px 0; }
    /* Thêm style cho nút cửa///////////////////////////////////////////////////////////////// */
    /* Thêm style cho nút cửa , chưa fix */
    .btn-door {
      background-color: #e67e22;
    }
    .btn-door:hover {
      background-color: #d35400;
    }
    .auto-mode {
      margin-top: 10px;
      font-style: italic;
    }
  </style>
</head>
<body>
  <h1>ESP32 WEB SERVER</h1>
  <h2>Hệ thống giám sát nhiệt độ, độ ẩm và khí gas</h2>
  
  <div id="warningBanner" class="warning-banner">
    <strong>CẢNH BÁO:</strong> Phát hiện nồng độ khí gas cao!
  </div>
  
  <div class="control-panel">
    <!-- Giữ nguyên các control-box cũ -->
    <div class="control-box">
      <h3>ĐIỀU KHIỂN ĐÈN LED </h3>
      <div class="switch-container">
        <span class="switch-label">TẮT</span>
        <label class="switch">
          <input type="checkbox" id="ledToggle" onchange="toggleLED()" )=====";
  html += ledState ? "checked" : "";
  html += R"=====(>
            <span class="slider"></span>
          </label>
          <span class="switch-label">BẬT</span>
        </div>
        <p id="ledStatus" class="status-text )=====";
  html += ledState ? "status-on" : "status-off";
  html += R"=====(">Trạng thái: )=====";
  html += ledState ? "ĐANG BẬT" : "ĐANG TẮT";
  html += R"=====(</p>
      </div>
    <!-- -->
    <div class="control-box">
      <h3>ĐIỀU KHIỂN CỬA THOÁT HIỂM</h3>
      <div class="servo-control">
        <button class="btn btn-door" onclick="controlDoor('open')">MỞ CỬA</button>
        <button class="btn btn-door" onclick="controlDoor('close')">ĐÓNG CỬA</button>
      </div>
      <div class="servo-control">
        <button class="btn btn-door" onclick="controlDoor('auto')">CHẾ ĐỘ TỰ ĐỘNG: )=====";
  html += doorAutoMode ? "BẬT" : "TẮT";
  html += R"=====(</button>
      </div>
      <p class="auto-mode">Tự động mở khi gas > )=====";
  html += String(GAS_DOOR_OPEN_THRESHOLD);
  html += R"=====(</p>
      <p>Trạng thái: <span id="doorStatus">)=====";
  html += (servoPos > 0) ? "OPEN" : "CLOSE";  //fix
  html += R"=====(</span></p>
    </div>
  </div>

  <!--  -->
  <div class="control-box">
        <h3>GOOGLE SHEETS</h3>
        <p>Tự động gửi dữ liệu mỗi 30 giây</p>
        <div class="switch-container">
          <span class="switch-label">TẮT</span>
          <label class="switch">
            <input type="checkbox" id="sheetsToggle" onchange="toggleSheets()" )=====";
  html += sheetsEnabled ? "checked" : "";
  html += R"=====(>
              <span class="slider"></span>
            </label>
            <span class="switch-label">BẬT</span>
          </div>
          <p id="sheetsStatus" class="status-text )=====";
  html += sheetsEnabled ? "status-on" : "status-off";
  html += R"=====(">Trạng thái: )=====";
  html += sheetsEnabled ? "ĐANG BẬT" : "ĐANG TẮT";
  html += R"=====(</p>
          <button class="btn" onclick="forceSendToSheets()">Gửi Ngay</button>
        </div>
      </div>
      <div class="table-container">
        <table>
          <thead>
            <tr>
              <th>Thời gian</th>
              <th>Nhiệt độ (°C)</th>
              <th>Độ ẩm (%)</th>
              <th>Khí gas</th>
              <th>Servo</th>
              <th>Trạng thái</th>
            </tr>
          </thead>
          <tbody>)=====";
  int count = isHistoryFull ? HISTORY_SIZE : historyIndex;
  for (int i = 0; i < count; i++) {
    int index = (historyIndex - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
    String gasClass = sensorHistory[index].gasWarning ? "class='gas-warning'" : "";
    
    html += "<tr>";
    html += "<td>" + sensorHistory[index].timestamp + "</td>";
    html += "<td>" + String(sensorHistory[index].temperature, 1) + "</td>";
    html += "<td>" + String(sensorHistory[index].humidity, 0) + "</td>";
    html += "<td " + gasClass + ">" + String(sensorHistory[index].gasValue) + "</td>";
    html += "<td>" + String(sensorHistory[index].servoPosition) + "°</td>";
    html += "<td " + gasClass + ">" + sensorHistory[index].status + "</td>";
    html += "</tr>";
  }

  html += R"=====(</tbody></table></div>
      <div class="last-update">Dữ liệu được cập nhật tự động lúc: <span id="updateTime">)=====";
  html += lastUpdateTime;
  html += R"=====(</span></div>
      
  <script>
    /* fix */
    function updateLEDStatus(isOn) {
          const statusElement = document.getElementById('ledStatus');
          const toggle = document.getElementById('ledToggle');
          if (isOn) {
            statusElement.textContent = "Trạng thái: ĐANG BẬT";
            statusElement.className = "status-text status-on";
            toggle.checked = true;
          } else {
            statusElement.textContent = "Trạng thái: ĐANG TẮT";
            statusElement.className = "status-text status-off";
            toggle.checked = false;
          }
        }
        
        function updateSheetsStatus(isOn) {
          const statusElement = document.getElementById('sheetsStatus');
          const toggle = document.getElementById('sheetsToggle');
          if (isOn) {
            statusElement.textContent = "Trạng thái: ĐANG BẬT";
            statusElement.className = "status-text status-on";
            toggle.checked = true;
          } else {
            statusElement.textContent = "Trạng thái: ĐANG TẮT";
            statusElement.className = "status-text status-off";
            toggle.checked = false;
          }
        }
        
        function updateServoPos(pos) {
          document.getElementById('servoPos').textContent = pos;
        }
        
        function toggleLED() {
          const ledToggle = document.getElementById('ledToggle');
          const state = ledToggle.checked ? 'on' : 'off';
          fetch('/led?state=' + state).then(response => {
            if (response.ok) updateLEDStatus(ledToggle.checked);
          }).catch(error => {
            console.error('Lỗi:', error);
            ledToggle.checked = !ledToggle.checked;
          });
        }
        
        /*function setServoPos(pos) {
          fetch('/servo?pos=' + pos).then(response => {
            if (response.ok) {
              return response.text();
            }
            throw new Error('Lỗi kết nối');
          }).then(text => {
            updateServoPos(text);
          }).catch(error => {
            console.error('Lỗi:', error);
            alert('Điều khiển servo thất bại!');
          });
        }*/
        
        function toggleSheets() {
          const sheetsToggle = document.getElementById('sheetsToggle');
          const enable = sheetsToggle.checked;
          fetch('/sheets?enable=' + (enable ? '1' : '0')).then(response => {
            if (response.ok) updateSheetsStatus(enable);
          }).catch(error => {
            console.error('Lỗi:', error);
            sheetsToggle.checked = !sheetsToggle.checked;
          });
        }
        
        function forceSendToSheets() {
          fetch('/sheets?force=1').then(response => {
            if (response.ok) {
              alert('Đã gửi dữ liệu lên Google Sheets thành công!');
            }
          }).catch(error => {
            console.error('Lỗi:', error);
            alert('Gửi dữ liệu thất bại!');
          });
        }
        
        function updateData() {
          fetch('/data').then(response => response.json()).then(data => {
            document.getElementById('updateTime').textContent = data.time;
            updateServoPos(data.servo);
            
            const warningBanner = document.getElementById('warningBanner');
            warningBanner.style.display = data.gas > )=====";
  html += String(GAS_THRESHOLD);
  html += R"=====( ? 'block' : 'none';
            
            let rows = document.querySelector('tbody').rows;
            if (rows.length > 0) {
              rows[0].cells[0].innerText = data.time;
              rows[0].cells[1].innerText = data.temp;
              rows[0].cells[2].innerText = data.hum;
              rows[0].cells[3].innerText = data.gas;
              rows[0].cells[4].innerText = data.servo + "°";
              rows[0].cells[5].innerText = data.status;
              
              rows[0].cells[3].className = data.gas > )=====";
  html += String(GAS_THRESHOLD);
  html += R"=====( ? 'gas-warning' : '';
              rows[0].cells[5].className = data.gas > )=====";
  html += String(GAS_THRESHOLD);
  html += R"=====( ? 'gas-warning' : '';
            }
            
            updateLEDStatus(data.led === "1");
          }).catch(error => console.error('Lỗi cập nhật:', error));
        }
        
        setInterval(updateData, 2000);
        updateData();  
    // Thêm hàm điều khiển cửa
    function controlDoor(action) {
      fetch('/door?action=' + action)
        .then(response => response.text())
        .then(text => {
          alert(text);
          if (action === 'auto') {
            document.querySelector(".btn-door:nth-child(3)").textContent = 
              "CHẾ ĐỘ TỰ ĐỘNG: " + (text.includes("BẬT") ? "BẬT" : "TẮT");
          }
          updateData();
        });
    }

    // hàm updateData()
    function updateData() {
      fetch('/data').then(r => r.json()).then(data => {
        // Giữ nguyên các cập nhật cũ
        document.getElementById("doorStatus").textContent = 
          data.door === "OPEN" ? "OPEN" : "CLOSE";
      });
    }
  </script>
</body>
</html>)=====";

  server.send(200, "text/html", html);
}

/* */



void handleData() {
  String json = "{";
  json += "\"time\":\"" + lastUpdateTime + "\",";
  json += "\"temp\":\"" + (isnan(temperature) ? "nan" : String(temperature,1)) + "\",";
  json += "\"hum\":\"" + (isnan(humidity) ? "nan" : String(humidity,0)) + "\",";
  json += "\"gas\":\"" + String(gasValue) + "\",";
  json += "\"status\":\"" + currentStatus + "\",";
  json += "\"servo\":\"" + String(servoPos) + "\",";
  json += "\"led\":\"" + String(ledState ? "1" : "0") + "\"";
  // ... (các trường hiện có)
  json += "\"led1\":\"" + String(led1State ? "1" : "0") + "\"";
  json += "\"tempGasWarning\":\"" + String(tempGasWarning ? "1" : "0") + "\"";
  //n
   json += "\"buzzer\":\"" + String(buzzerState ? "1" : "0") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}
// chuẩn bị sửa 





void handleLED() {
  String state = server.arg("state");
  if (state == "on") {
    digitalWrite(LED_PIN, HIGH); // Chỉ điều khiển LED_PIN (chân 13)
    ledState = true;
    server.send(200, "text/plain", "LED đã BẬT");
  } else if (state == "off") {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
    server.send(200, "text/plain", "LED đã TẮT");
  }
}



void handleServo() {
  String posStr = server.arg("pos");
  
  if (posStr == "auto") {
    // Chế độ tự động (quay từ 0-120 và ngược lại)
    server.send(200, "text/plain", String(servoPos));
  } else {
    int pos = posStr.toInt();
    if (pos >= 0 && pos <= 120) {
      servoPos = pos;
      myServo.write(servoPos);
      server.send(200, "text/plain", String(servoPos));
    } else {
      server.send(400, "text/plain", "Góc servo phải từ 0 đến 120");
    }
  }
}

void handleSheets() {
  String enable = server.arg("enable");
  String force = server.arg("force");
  
  if (enable == "1") {
    sheetsEnabled = true;
    server.send(200, "text/plain", "Đã bật gửi dữ liệu lên Google Sheets");
  } else if (enable == "0") {
    sheetsEnabled = false;
    server.send(200, "text/plain", "Đã tắt gửi dữ liệu lên Google Sheets");
  } else if (force == "1") {
    sendDataToGoogleSheets();
    server.send(200, "text/plain", "Đã gửi dữ liệu ngay lên Google Sheets");
  } else {
    server.send(400, "text/plain", "Lệnh không hợp lệ");
  }
}
// 
