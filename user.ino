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
#define TOPIC_USER_DISTANCE TOPIC_PREFIX"/user/distance"
#define TOPIC_USER_WEIGHT TOPIC_PREFIX"/user/weight"
#define TOPIC_NOTI_ON TOPIC_PREFIX"/noti/on"

float calibration_factor =770007.00; 
#define zero_factor 6870930
#define DOUT  6
#define CLK   5
#define DEC_POINT  2

float offset=0;
float get_units_kg();

HX711 scale(DOUT, CLK);

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);
uint32_t last_publish;
uint32_t last_ultrasonic_time =0;

//ultrasonic control------------------------
const int pingPin = 15;
const int inPin = 16;

const int MIN_DIST = 2;   // ระยะใกล้สุดที่ยอมรับได้ (cm)
const int MAX_DIST = 30;  // ระยะไกลสุดที่ต้องการ (cm)

long current_distance = 999;

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
    for (;;) {} // wait here forever
  }
  mqtt.setCallback(mqtt_callback);
  printf("MQTT broker connected.\n");
}





// --- ย้ายฟังก์ชันมาไว้ข้างบน เพื่อความชัวร์และลด Error ---
long microsecondsToCentimeters(long microseconds) {
  // ความเร็วเสียงคือ 29 ไมโครวินาทีต่อเซนติเมตร
  // หาร 2 เพราะสัญญาณไปและกลับ
  return microseconds / 29 / 2;
}
//-----------------------------------------------------

enum tasksw_state_t{
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
      printf("MQTT publish for stop alert\n");//dummy
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

void taskultrasonic(){
  if(millis()- last_ultrasonic_time >= 300){
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
    }else{
      cm = microsecondsToCentimeters(duration);
    }

    current_distance = cm;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    if (cm >= MIN_DIST && cm <= MAX_DIST) {
      Serial.print("Target Detected: ");
      Serial.print(cm);
      Serial.println(" cm");
      display.print("Dist: ");
      display.print(cm);
      display.print(" cm");
    }else {
      Serial.println("Out of Range / No Object");
      display.print("Out of range");
    }
    display.display();
  }
}

void task_mqtt_publish(){
  if(millis() - last_publish >= 2000){
    last_publish = millis();

    if(mqtt.connected()){
      char payload[20];
      if(current_distance >= MIN_DIST && current_distance <= MAX_DIST){
        snprintf(payload, sizeof(payload), "%ld", current_distance);
      }else{
        snprintf(payload, sizeof(payload), "out_of_range");
      }

      mqtt.publish(TOPIC_USER_DISTANCE, payload);
      printf("Published %s", payload);

      float current_weight = get_units_kg() + offset; // ดึงค่าน้ำหนัก
      char payload_weight[20];
      snprintf(payload_weight, sizeof(payload_weight), "%.2f", current_weight); // แปลงทศนิยม 2 ตำแหน่ง
      
      mqtt.publish(TOPIC_USER_WEIGHT, payload_weight);
      printf("Published Weight: %s kg\n", payload_weight);

    }
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  Wire.begin(48, 47);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.println("Load Cell");
  scale.set_scale(calibration_factor); 
  scale.set_offset(zero_factor);  
  connect_wifi();
  connect_mqtt();
  last_publish = 0;
}

float get_units_kg()
{
  return(scale.get_units()*0.453592);
}

void loop() {
  mqtt.loop();

  tasksw();
  taskultrasonic();
  task_mqtt_publish();
  Serial.print("Reading: ");
  String data = String(get_units_kg()+offset, DEC_POINT);
  Serial.print(data);
  Serial.println(" kg");
  
}
