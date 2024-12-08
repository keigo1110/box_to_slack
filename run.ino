// 実際に運用しているプログラム
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ========== パラメータ設定 ==========
const char* ssid = "write your ssid";           // Wi-Fi SSID
const char* password = "write your password";      // Wi-Fi パスワード
const char* slackWebhookUrl = "write your slack webhook url"; // Slack Webhook URL

// GPIO設定
const int TILT_PIN = 6;                     // 傾きセンサー接続ピン（GPIO 6）
const unsigned long DEBOUNCE_DELAY = 500;  // デバウンス時間(ms)
const unsigned long NOTIFICATION_INTERVAL = 20000;  // 通知間隔(ms)

// 平均化フィルタ設定
const int NUM_READINGS = 10;                // 平均を取るサンプル数

// ========== グローバル変数 ==========
unsigned long lastDebounceTime = 0;         // 前回の状態変化時刻
unsigned long lastNotificationTime = 0;     // 最後の通知時刻
int readings[NUM_READINGS];                 // センサー値を保存
int readIndex = 0;                          // 配列内の現在の読み取り位置
int total = 0;                              // 合計
int average = 0;                            // 平均値
int lastTiltState = LOW;                    // 前回の安定した状態

// ========== 設定と初期化 ==========
void setup() {
  Serial.begin(115200);
  pinMode(TILT_PIN, INPUT_PULLUP);

  // Wi-Fi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("WiFiに接続中...");
  }
  Serial.println("WiFiに接続完了");

  // 平均化フィルタの初期化
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = 0;
  }
}

// ========== メインループ ==========
void loop() {
  // センサー値の読み取りと平均化
  total -= readings[readIndex];             // 古い値を削除
  readings[readIndex] = digitalRead(TILT_PIN);  // 新しい値を追加
  total += readings[readIndex];             // 合計に加算
  readIndex = (readIndex + 1) % NUM_READINGS;  // 次の位置に移動
  average = total / NUM_READINGS;           // 平均値を計算

  // 状態変化をチェック
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (average != lastTiltState) {
      lastDebounceTime = millis();
      lastTiltState = average;
      handleTiltState(lastTiltState);
    }
  }

  delay(50); // ループの安定化
}

// ========== 傾き状態の処理 ==========
void handleTiltState(int state) {
  unsigned long currentTime = millis();

  if (state == HIGH) {
    if (currentTime - lastNotificationTime >= NOTIFICATION_INTERVAL) {
      lastNotificationTime = currentTime;
      Serial.println(F("書類が提出されました！"));
      sendSlackMessage("📄 書類が提出されました！");
    } else {
      Serial.println(F("通知を抑制中 (インターバル未経過)。"));
    }
  } else {
    Serial.println(F("センサーがリセットされました。"));
  }
}

// ========== Slackへのメッセージ送信 ==========
void sendSlackMessage(const char* message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(slackWebhookUrl);
    http.addHeader("Content-Type", "application/json");

    // JSONデータの作成
    StaticJsonDocument<200> json;
    json["text"] = message;
    String requestBody;
    serializeJson(json, requestBody);

    int httpResponseCode = http.POST((uint8_t*)requestBody.c_str(), requestBody.length());
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Slackへのメッセージ送信成功:");
      Serial.println(response);
    } else {
      Serial.print("エラーコード: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFiに接続されていません");
  }
}