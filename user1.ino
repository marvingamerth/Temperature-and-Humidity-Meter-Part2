#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

#define SW_PIN 2
#define LED_GREEN 40
#define LED_YELLOW 41
#define LED_RED 42

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define TOPIC_USER_DISTANCE TOPIC_PREFIX"/user/distance"
#define TOPIC_NOTI_ON TOPIC_PREFIX"/noti/on"

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);
uint32_t last_publish;

//ultrasonic control------------------------
const int pingPin = 5;
const int inPin = 6;

const int MIN_DIST = 2;   // ระยะใกล้สุดที่ยอมรับได้ (cm)
const int MAX_DIST = 30;  // ระยะไกลสุดที่ต้องการ (cm)

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

// --- ย้ายฟังก์ชันมาไว้ข้างบน เพื่อความชัวร์และลด Error ---
long microsecondsToCentimeters(long microseconds) {
  // ความเร็วเสียงคือ 29 ไมโครวินาทีต่อเซนติเมตร
  // หาร 2 เพราะสัญญาณไปและกลับ
  return microseconds / 29 / 2;
}
//-----------------------------------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(48, 47);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  connect_wifi();
}

void loop() {
  long duration, cm;

  // ส่งสัญญาณ Trigger
  pinMode(pingPin, OUTPUT);
  digitalWrite(pingPin, LOW);
  delayMicroseconds(2);
  digitalWrite(pingPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(pingPin, LOW);

  // รับสัญญาณ Echo
  pinMode(inPin, INPUT);
  duration = pulseIn(inPin, HIGH);

  // แปลงค่า
  cm = microsecondsToCentimeters(duration);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);


  // ตรวจสอบเงื่อนไขระยะทางที่คุณกำหนด
  if (cm >= MIN_DIST && cm <= MAX_DIST) {
    Serial.print("Target Detected: ");
    Serial.print(cm);
    Serial.println(" cm");
    display.print(cm);
  } else {
    Serial.println("Out of Range / No Object");
    display.print("Out of range/too close");
  }
  display.display();

  delay(300);
}
