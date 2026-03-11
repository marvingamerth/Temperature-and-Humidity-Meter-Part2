โครงงาน Hydration Tracker เครื่องตรวจวัดและแจ้งเตือนการดื่มน้ำ สำหรับผู้สูงอายุและผู้ป่วย
จัดทำโดย
นายชยางกูร กล่อมธง
นายพรรคนาวิน อันบุรี
นายธนภัทร โพธิ์สิงห์
นายอัครวินท์ ไหมพูล
โครงงานนี้เป็นส่วนหนึ่งของรายวิชา 01204114 การพัฒนาฮาร์ดแวร์คอมพิวเตอร์เบื้องต้น
ภาควิชาวิศวกรรมคอมพิวเตอร์ คณะวิศวกรรมศาสตร์
มหาวิทยาลัยเกษตรศาสตร์

Using ESP32-S3-DevKitC-1 , DHT22, 0.96" OLED display and buzzer(for alert system). Send data via WiFi using MQTT protocol

**buzzer alert system is not available because electronic problem.
**using led for temporary alert system
sensor1.ino is parallel programming firmware.
and sensor.ino is non blocking programming firmware.


buzzer wiring 
ESP32 PIN5 --------------> Arduino UNO PIN 2
BUZZER VCC --------------> Arduino UNO 5V
BUZZER I/O --------------> Arduino UNO PIN 3
GND --------------> GND
