#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "HX711.h"

#define SW_PIN 2
#define LED_GREEN 40
#define LED_YELLOW 41
#define LED_RED 42

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- MQTT Topics ---
#define TOPIC_USER_DISTANCE   TOPIC_PREFIX"/user/distance"
#define TOPIC_USER_WEIGHT     TOPIC_PREFIX"/user/weight"
#define TOPIC_USER_WATERLEVEL TOPIC_PREFIX"/user/waterlevel" // เพิ่ม Topic สำหรับเปอร์เซ็นต์น้ำ
#define TOPIC_NOTI_ON         TOPIC_PREFIX"/noti/on"

// --- Load Cell HX711 ---
float calibration_factor = 770007.00; 
#define zero_factor 6870930
#define DOUT  6
#define CLK   5
#define DEC_POINT  2
float offset = 0;
HX711 scale(DOUT, CLK);
float get_units_kg();

// --- Network & MQTT ---
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);
uint32_t last_publish = 0;
uint32_t last_ultrasonic_time = 0;

// --- Ultrasonic Control ---
const int pingPin = 15;
const int inPin = 16;
const int MIN_DIST = 2;   // ระยะใกล้สุดที่ยอมรับได้ (cm)
const int MAX_DIST = 16;  // ระยะไกลสุดที่ต้องการ (cm)

long current_distance = 999;
int current_water_level = 0; // ตัวแปรเก็บเปอร์เซ็นต์น้ำปัจจุบัน

// --- Functions ---
long microsecondsToCentimeters(long microseconds) {
  return microseconds / 29 / 2;
}

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  printf("WiFi MAC address is %s\n", WiFi.macAddress().c_str());
  printf("Connecting to WiFi %s.\n", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    printf(".");
    digitalWrite(LED_RED, 1);
    delay(100);
    digitalWrite(LED_RED, 0);
    fflush(stdout);
    delay(500);
  }
  printf("\nWiFi connected.\n");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length){
  printf("%s\n", topic);
}

void connect_mqtt() {
  printf("Connecting to MQTT broker at %s.\n", MQTT_BROKER);
  if (!mqtt.connect("", MQTT_USER, MQTT_PASS)) {
    printf("Failed to connect to MQTT broker.\n");
    for (;;) {} 
  }
  mqtt.setCallback(mqtt_callback);
  printf("MQTT broker connected.\n");
}

enum tasksw_state_t {
  WAIT_SW_PRESS,
  DEBOUNCE1,
  WAIT_SW_RELEASE,
  DEBOUNCE2,
};

tasksw_state_t tasksw_state;
uint32_t timestamp1;

void tasksw(){
  uint32_t now;
  if(tasksw_state == WAIT_SW_PRESS){
    if(digitalRead(SW_PIN) == 0){
      tasksw_state = DEBOUNCE1;
      printf("MQTT publish for stop alert\n");
      mqtt.publish(TOPIC_NOTI_ON, "0");
      timestamp1 = millis();
    }
  }else if(tasksw_state == DEBOUNCE1){
    now = millis();
    if(now - timestamp1 >= 10){
      tasksw_state = WAIT_SW_RELEASE;
      timestamp1 = now;
    }
  }else if(tasksw_state == WAIT_SW_RELEASE){
    if(digitalRead(SW_PIN) == 1){
      tasksw_state = DEBOUNCE2;
      now = millis();
    }
  }else if(tasksw_state == DEBOUNCE2){
    now = millis();
    if(now - timestamp1 >= 10){
      tasksw_state = WAIT_SW_PRESS;
      timestamp1 = now;
    }
  }
}

// ฟังก์ชันแสดงผลบนจอ OLED (รวมข้อความและกราฟิกแทงก์น้ำ)
void updateDisplay(int waterLevel, long dist, float weight) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // ฝั่งซ้าย: แสดงข้อมูล Text
    display.setTextSize(1);
    display.setCursor(0, 5);
    display.print("Dist: ");
    if(dist == 999) display.print("Out");
    else { display.print(dist); display.print("cm"); }

    display.setCursor(0, 20);
    display.print("W: ");
    display.print(weight, 1);
    display.print("kg");

    display.setCursor(0, 35);
    display.print("Water:");
    display.setCursor(0, 48);
    display.setTextSize(2);
    display.print(waterLevel);
    display.print("%");

    // ฝั่งขวา: วาดรูปแทงก์น้ำ
    int tankX = 65;  
    int tankY = 10;
    int tankWidth = 50;  
    int tankHeight = 50;

    // กรอบแทงก์น้ำ
    display.drawRoundRect(tankX, tankY, tankWidth, tankHeight, 5, WHITE);
    // ฝาและท่อ
    display.drawLine(tankX + 5, tankY - 3, tankX + tankWidth - 5, tankY - 3, WHITE);
    display.drawLine(tankX + 5, tankY - 3, tankX, tankY, WHITE);
    display.drawLine(tankX + tankWidth - 5, tankY - 3, tankX + tankWidth, tankY, WHITE);
    // ท่อน้ำเข้า
    display.drawLine(tankX + 8, tankY - 6, tankX + 8, tankY - 12, WHITE);
    display.drawLine(tankX + 8, tankY - 12, tankX + 14, tankY - 12, WHITE);
    display.drawLine(tankX + 14, tankY - 12, tankX + 14, tankY - 6, WHITE);

    // เติมระดับน้ำข้างใน
    int waterHeight = map(waterLevel, 0, 100, 0, tankHeight);
    display.fillRoundRect(tankX + 2, tankY + tankHeight - waterHeight, tankWidth - 4, waterHeight, 5, WHITE);

    display.display();
}

void taskultrasonic(){
  if(millis() - last_ultrasonic_time >= 300){
    last_ultrasonic_time = millis();

    long duration, cm;
    pinMode(pingPin, OUTPUT);
    digitalWrite(pingPin, LOW);
    delayMicroseconds(2);
    digitalWrite(pingPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(pingPin, LOW);
    
    pinMode(inPin, INPUT);
    duration = pulseIn(inPin, HIGH, 30000);

    if(duration == 0){
      cm = 999;
      current_water_level = 0; // ไม่มีวัตถุ/อยู่นอกระยะ ให้เป็น 0%
    }else{
      cm = microsecondsToCentimeters(duration);
      // คำนวณเปอร์เซ็นต์น้ำ (ใกล้ 2cm = 100%, ไกล 16cm = 0%)
      int mappedLevel = map(cm, MIN_DIST, MAX_DIST, 100, 0);
      current_water_level = constrain(mappedLevel, 0, 100); 
    }

    current_distance = cm;
    float current_weight = get_units_kg() + offset;

    // อัปเดตหน้าจอทุกๆ 300ms
    updateDisplay(current_water_level, current_distance, current_weight);
  }
}

void task_mqtt_publish(){
  if(millis() - last_publish >= 2000){
    last_publish = millis();

    if(mqtt.connected()){
      char payload[20];
      char payload_weight[20];
      char payload_water[10];

      // 1. Publish Distance
      if(current_distance >= MIN_DIST && current_distance <= MAX_DIST){
        snprintf(payload, sizeof(payload), "%ld", current_distance);
      }else{
        snprintf(payload, sizeof(payload), "out_of_range");
      }
      mqtt.publish(TOPIC_USER_DISTANCE, payload);

      // 2. Publish Weight
      float current_weight = get_units_kg() + offset;
      snprintf(payload_weight, sizeof(payload_weight), "%.2f", current_weight);
      mqtt.publish(TOPIC_USER_WEIGHT, payload_weight);

      // 3. Publish Water Level %
      snprintf(payload_water, sizeof(payload_water), "%d", current_water_level);
      mqtt.publish(TOPIC_USER_WATERLEVEL, payload_water);

      // Serial Print เพื่อตรวจสอบ
      printf("Published -> Dist: %s cm | Weight: %s kg | Water: %s %%\n", payload, payload_weight, payload_water);
    }
  }
}

float get_units_kg() {
  return (scale.get_units() * 0.453592);
}

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  
  Wire.begin(48, 47);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  Serial.println("Load Cell Config...");
  scale.set_scale(calibration_factor); 
  scale.set_offset(zero_factor);  
  
  connect_wifi();
  connect_mqtt();
  last_publish = millis();
}

void loop() {
  mqtt.loop();
  
  // ให้ Loop รันเฉพาะ Task ที่แบ่งเวลาไว้ (Non-blocking)
  tasksw();
  taskultrasonic();
  task_mqtt_publish();
}
