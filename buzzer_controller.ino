void setup() {
  pinMode(2, INPUT);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  Serial.begin(9600);

}

void loop() {
  if(digitalRead(2)==LOW){
    Serial.println("low\n");
    digitalWrite(3, LOW);
  }else{
    Serial.println("high\n");
    digitalWrite(3, HIGH);
  }
  delay(300);
}
