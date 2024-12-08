// チルトセンサーのプログラムのリファクタリングバージョン

// 定数定義
const int TILT_PIN = 6;         // センサーピン
const unsigned long DEBOUNCE_DELAY = 50;  // チャタリング防止の待機時間(ms)
const unsigned long SERIAL_BAUD = 115200;  // シリアル通信速度
const unsigned long LOOP_DELAY = 100;      // メインループの待機時間(ms)

// グローバル変数
unsigned long lastDebounceTime = 0;  // 前回の状態変化時刻
int lastTiltState = LOW;             // 前回の傾き状態
int tiltState = LOW;                 // 現在の傾き状態（デバウンス後）

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) {
    ; // シリアルポートの準備待ち（USB接続の場合）
  }

  Serial.println(F("SW520D Tilt Sensor - Production Version"));
  pinMode(TILT_PIN, INPUT);

  // 内部プルアップ抵抗を有効化（ノイズ対策）
  digitalWrite(TILT_PIN, HIGH);
}

void loop() {
  // センサー値の読み取り
  int reading = digitalRead(TILT_PIN);

  // チャタリング対策
  if (reading != lastTiltState) {
    lastDebounceTime = millis();
  }

  // デバウンス時間経過後に状態を更新
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != tiltState) {
      tiltState = reading;
      reportTiltState();
    }
  }

  lastTiltState = reading;

  // システムの安定性のための短い遅延
  delay(LOOP_DELAY);
}

// 傾き状態のレポート
void reportTiltState() {
  // メモリ使用量を抑えるためにF()マクロを使用
  Serial.print(F("Time: "));
  Serial.print(millis());
  Serial.print(F("ms, Tilt State: "));
  Serial.println(tiltState);

  if (tiltState == HIGH) {
    Serial.println(F("Sensor is tilted!"));
  } else {
    Serial.println(F("Sensor is level"));
  }
}