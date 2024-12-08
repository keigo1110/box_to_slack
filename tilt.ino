// チルトセンサーのテストプログラム
const int tiltPin = 6;

void setup() {
  Serial.begin(115200);
  Serial.println("SW520D Tilt Sensor Test");
  pinMode(tiltPin, INPUT);
}

void loop() {
  int tiltState = digitalRead(tiltPin);
  // より詳細な情報を出力
  Serial.print("Time: ");
  Serial.print(millis());
  Serial.print("ms, Tilt State: ");
  Serial.println(tiltState);

  if (tiltState == HIGH) {
    Serial.println("Sensor is tilted!");
  } else {
    Serial.println("Sensor is level");
  }

  delay(500);
}