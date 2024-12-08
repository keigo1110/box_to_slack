// slackに通知するプログラム
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "write your ssid";           // Wi-Fi SSID
const char* password = "write your password";      // Wi-Fi パスワード
const char* slackWebhookUrl = "write your slack webhook url"; // Slack Webhook URL

// GPIOピンの設定
const int monitorPin = 6; // GPIOピン番号（例: GPIO4）

// メッセージ送信間隔（ミリ秒）
const long sendInterval = 5000; // 5秒

unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200);
  pinMode(monitorPin, INPUT); // ピンのモードを入力に設定

  // Wi-Fi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("WiFiに接続中...");
  }
  Serial.println("WiFiに接続完了");
}

void loop() {
  unsigned long currentMillis = millis();

  // 送信間隔が経過した場合
  if (currentMillis - previousMillis >= sendInterval) {
    previousMillis = currentMillis;

    // GPIOピンの状態をチェック
    int pinState = digitalRead(monitorPin);
    if (pinState == HIGH) { // ピンが何も接続されていない場合
      sendSlackMessage("GPIOピンがHIGHです。");
    } else {
      sendSlackMessage("GPIOピンがLOWです。");
    }
  }
}

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

    // POSTリクエストの送信
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