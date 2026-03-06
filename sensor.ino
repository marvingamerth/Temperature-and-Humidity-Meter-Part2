#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//----------don't edit; Default by PCB Design ------------------
#define SW_PIN 2
#define LED_GREEN 42
#define LED_YELLOW 41
#define LED_RED 40 
//-------------------------------------------------------
#define BUZZER 5
#define DHTPIN 6
#define DHTTYPE DHT22

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define TOPIC_ROOM_TEMP TOPIC_PREFIX"/room/temp"
#define TOPIC_ROOM_HUMID TOPIC_PREFIX"/room/humid"
#define TOPIC_NOTI_ON TOPIC_PREFIX"/noti/on"
#define TOPIC_NOTI_OFF TOPIC_PREFIX"/noti/off"




//--------------Wifi and MQTT Connecting---------------------------------
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_BROKER, 1883, wifiClient);
uint32_t last_publish;

void connect_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  printf("WiFi MAC address is %s\n", WiFi.macAddress().c_str());
  printf("Connecting to WiFi %s.\n", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    printf(".");
    fflush(stdout);
    digitalWrite(LED_RED, 1); //if wifi is not connected, red led is flashing.
    delay(100);
    digitalWrite(LED_RED, 0); //----------------
    delay(500);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Connecting...");
  }
  printf("\nWiFi connected.\n");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected!");
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(1000);
}
void connect_mqtt() {
  printf("Connecting to MQTT broker at %s.\n", MQTT_BROKER);
  if (!mqtt.connect("", MQTT_USER, MQTT_PASS)) {
    printf("Failed to connect to MQTT broker.\n");
    for (;;){
      digitalWrite(LED_YELLOW, 1);
      delay(500);
      digitalWrite(LED_YELLOW, 0);
    } // wait here forever
  }
  mqtt.setCallback(mqtt_callback);
  printf("MQTT broker connected.\n");
  //subscribe for get data from user device and dashboard

  mqtt.subscribe(TOPIC_NOTI_ON);
  mqtt.subscribe(TOPIC_NOTI_OFF);
}



enum taskled_state_t{
  LED_OFF,
  LED_ON,
};

enum taskdht_state_t{
  DHT_ON,
  DHT_OFF,
};

enum taskbuz_state_t{
  BUZ_ON,
  BUZ_OFF,
};

taskled_state_t taskled_state;
taskdht_state_t taskdht_state;
taskbuz_state_t taskbuz_state;
uint32_t timestamp1, timestamp2, timestamp3;

bool isNotiActive = false;
bool yellow_state = false;

void mqtt_callback(char* topic, byte* payload, unsigned int length){
  printf("%s\n", topic);
  char message[length+1];
  memcpy(message, payload, length);
  message[length] = '\0';
  if(strcmp(topic, TOPIC_NOTI_ON) == 0){ //temporary alert system
    if(strcmp(message, "1")==0){
      isNotiActive = true;
      digitalWrite(LED_YELLOW, 1);
    }else if(strcmp(message, "0") == 0){
      isNotiActive = false;
      digitalWrite(LED_YELLOW, 0);
      digitalWrite(BUZZER, 1);
    }
    printf("%s\n", message);
  }

}


//-------------------set-up--and--loop--------------------------
DHT dht(DHTPIN, DHTTYPE);
void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(BUZZER, 1); //make buzzer quiet when start.
  Wire.begin(48, 47);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    printf("SSD1306 allocation failed\n");
    for(;;); // หยุดถ้าจอเสีย
  }
  display.clearDisplay();
  display.display();

  dht.begin();
  connect_wifi();
  connect_mqtt();
  last_publish = 0;


  taskled_state = LED_ON;
  taskdht_state = DHT_ON;

  timestamp1 = millis();
  timestamp2 = millis();
  timestamp3 = millis();
  

  
}

void taskled() {
  uint32_t now;
  if(taskled_state == LED_ON){
    now = millis();
    if(now - timestamp1 >= 100){
      digitalWrite(LED_GREEN, 0);
      taskled_state = LED_OFF;
      timestamp1 = now;
    }
  }else if(taskled_state == LED_OFF){
    now = millis();
    if(now - timestamp1 >= 2900){
      digitalWrite(LED_GREEN, 1);
      taskled_state = LED_ON;
      timestamp1 = now;
    }
  }
}

void taskbuz() { 
  if(isNotiActive){
    uint32_t now = millis();
    if(taskbuz_state == BUZ_ON){
      if(now - timestamp3 >= 300){
        digitalWrite(BUZZER, 1);
        //digitalWrite(LED_RED, 0); // สั่ง 1 ให้เงียบ
        taskbuz_state = BUZ_OFF;
        timestamp3 = now;
      }
    }else if(taskbuz_state == BUZ_OFF){
      if(now - timestamp3 >= 100){
        digitalWrite(BUZZER, 0);
        //digitalWrite(LED_RED, 1); // สั่ง 0 ให้ดัง
        taskbuz_state = BUZ_ON;
        timestamp3 = now;
      }
    }
  } else {
    // ป้องกันกรณีระบบค้าง ให้บัซเซอร์เงียบเสมอถ้าไม่ได้อยู่ในโหมดแจ้งเตือน
    digitalWrite(BUZZER, 1);
    digitalWrite(LED_RED, 0); 
  }
}


void taskdht(){
  uint32_t now = millis();
  if(now - timestamp2 >= 5000){
    timestamp2 = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if(!isnan(t) && !isnan(h)){
      String temp = String(t, 2);
      mqtt.publish(TOPIC_ROOM_TEMP, temp.c_str());
      printf("Publishing TEMP Value (C): %.2f\n", t);

      String humid = String(h, 2);
      mqtt.publish(TOPIC_ROOM_HUMID, humid.c_str());
      printf("Publishing HUMIDITY Value: %.2f\n", h);

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,0);
      display.print("TEMP(C):");
      display.setCursor(93, 0);
      display.print(temp);
      display.setCursor(0, 35);
      display.print("HUMID(%):");
      display.setCursor(93, 35);
      display.print(humid);
      display.display();
    }
    taskdht_state = DHT_OFF;
    timestamp2 = now;
  }
  
  
}

void loop(){
  
  mqtt.loop();
  taskled();
  taskdht();
  taskbuz();

  

}
