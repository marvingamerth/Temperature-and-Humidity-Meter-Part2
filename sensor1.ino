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
#define BUZZER 39
#define DHTPIN 6
#define DHTTYPE DHT22

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define TOPIC_ROOM_TEMP TOPIC_PREFIX"/room/temp"
#define TOPIC_ROOM_HUMID TOPIC_PREFIX"/room/humid"

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
  }
  printf("\nWiFi connected.\n");
}
void connect_mqtt() {
  printf("Connecting to MQTT broker at %s.\n", MQTT_BROKER);
  if (!mqtt.connect("", MQTT_USER, MQTT_PASS)) {
    printf("Failed to connect to MQTT broker.\n");
    for (;;){
      digitalWrite(LED_YELLOW, 1); //if MQTT is not connected, yellow LED is flashing
      delay(500);
      digitalWrite(LED_YELLOW, 0);
    } // wait here forever
  }
  mqtt.setCallback(mqtt_callback);
  printf("MQTT broker connected.\n");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
//   pinMode(LED_RED, OUTPUT);
//   pinMode(LED_YELLOW, OUTPUT);
//   pinMode(LED_GREEN, OUTPUT);

//   digitalWrite(LED_RED, HIGH);
//   digitalWrite(LED_GREEN,LOW);

//   setup_wifi();
//   client.setServer(mqtt_server, 1883);

//   if ()
}




//-------------------set-up--and--loop--------------------------
DHT dht(DHTPIN, DHTTYPE);
void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  dht.begin();
  connect_wifi();
  connect_mqtt();
  last_publish = 0;

  Wire.begin(48, 47);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);


}

void loop() {
  delay(2000);

  //digitalWrite(BUZZER, 1);

  digitalWrite(LED_GREEN, 1); //show the machine status
  delay(200);
  digitalWrite(LED_GREEN, 0);

  DHT dht(DHTPIN, DHTTYPE);

  // if (!isnan(h)){
  //   String humidStr = String(h);
  //   client.publish("home/sensor/temp", humidStr.c_str());
  // }
  // if (isnan(h) || isnan(t) || isnan(f)) {
  // Serial.println(F("Failed to read from DHT sensor!"));
  // return 0;
  // }
  // Serial.print(F("Humidity: "));
  // Serial.print(h);
  // Serial.print(F("% Temperature: "));
  // Serial.print(t);
  // Serial.print('-\n');

  mqtt.loop();
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // uint8_t now = millis();
  // if(now - last_publish >= 2000){

  
  String payload((int)t);
  String temp = String(t, 2);
  mqtt.publish(TOPIC_ROOM_TEMP, temp.c_str());
  printf("Publishing TEMP Value (C): %.2f\n", t);

  String humid = String(h, 2);
  mqtt.publish(TOPIC_ROOM_HUMID, humid.c_str());
  printf("Publishing HUMIDITY Value: %.2f\n", h);


    
  // }

  // String payload(t);
  // printf("Publishing room temp: %.2f\n", t);
  // mqtt.publish(TOPIC_ROOM_TEMP, payload.c_str());

  // String payload(h);
  // printf("Publishing room humidity: %.2f\n",h );
  // mqtt.publish(TOPIC_ROOM_HUMID, payload.c_str());

  // mqtt.setCallback(mqtt_callback);
  // mqtt.subscribe(TOPIC_LED_GREEN);
  // printf("MQTT broker connected.\n")

  //------------------display--------code----------------------
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

  //--------------------------------------------------------------------

}



