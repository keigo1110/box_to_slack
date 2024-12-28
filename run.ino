#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUDP.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>

// ========== WiFi設定 ==========
const char* ssid = "write your ssid";           // Wi-Fi SSID
const char* password = "write your password";      // Wi-Fi パスワード
const char* slackWebhookUrl = "write your slack webhook url"; // Slack Webhook URL

// ========== システム設定 ==========
// GPIO設定
const int TILT_PIN = 10;
const int LED_PIN = 2;  // ステータスLED用

// タイミング設定
const unsigned long DEBOUNCE_DELAY = 1000;
const unsigned long NOTIFICATION_INTERVAL = 20000;
const unsigned long WIFI_RETRY_INTERVAL = 30000;
const unsigned long ERROR_RESET_THRESHOLD = 3600000;
const unsigned long HEALTH_CHECK_INTERVAL = 86400000; // 24時間

// エラー設定
const int ERROR_LIMIT = 5;
const int MAX_WIFI_RETRIES = 5;
const int SENSOR_ERROR_THRESHOLD = 10;
const int WEAK_SIGNAL_THRESHOLD = -80;

// デバイス設定
const char* DEVICE_NAME = "TiltSensor";
const char* OTA_PASSWORD = "admin";

// ========== システム状態構造体 ==========
struct SystemStatus {
  bool isWifiConnected;
  int errorCount;
  int wifiRetryCount;
  int lastHttpCode;
  String lastErrorMsg;
  int wifiSignalStrength;
  unsigned long uptime;
} status;

enum ErrorType {
  WIFI_ERROR,
  HTTP_ERROR,
  SENSOR_ERROR,
  CONFIG_ERROR
};

// ========== グローバル変数 ==========
unsigned long lastHealthCheckTime = 0;
unsigned long lastErrorResetTime = 0;
unsigned long lastWiFiRetryTime = 0;
unsigned long lastDebounceTime = 0;
unsigned long lastNotificationTime = 0;
bool lastReading = false;
bool isInSafeMode = false;

// ========== 関数宣言 ==========
void setupOTA();
void handleTiltState(bool state);
void handleSystemError(ErrorType errorType, const char* message);
bool connectToWiFi();
void sendSlackMessage(const char* message);
void enterSafeMode();

// ========== 初期化 ==========
void setup() {
  Serial.begin(115200);
  pinMode(TILT_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("システム起動開始");

  // WiFi初回接続
  if (!connectToWiFi()) {
    Serial.println("初期WiFi接続失敗");
    return;
  }

  // OTAセットアップ
  setupOTA();

  // 起動通知
  sendSlackMessage("🟢 システム起動完了");
}

// ========== OTA更新 ==========
void setupOTA() {
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("OTAアップデート開始");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTAアップデート完了");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    String errorMsg = "OTAエラー: " + String(error);
    Serial.println(errorMsg);
  });

  ArduinoOTA.begin();
}

// ========== エラー処理 ==========
void handleSystemError(ErrorType errorType, const char* message) {
  status.errorCount++;
  status.lastErrorMsg = String(message);
  Serial.println("エラー発生: " + String(message));

  String errorMessage = "⚠️ システムエラー\n";
  errorMessage += "種別: ";

  switch (errorType) {
    case WIFI_ERROR:
      errorMessage += "WiFi接続";
      break;
    case HTTP_ERROR:
      errorMessage += "HTTP通信";
      break;
    case SENSOR_ERROR:
      errorMessage += "センサー";
      break;
    case CONFIG_ERROR:
      errorMessage += "設定";
      break;
  }

  errorMessage += "\n" + String(message);
  sendSlackMessage(errorMessage.c_str());

  if (status.errorCount >= ERROR_LIMIT) {
    Serial.println("エラー回数が上限を超えました");
    sendSlackMessage("🔄 エラー回数が上限を超えたため再起動します");
    delay(1000);
    ESP.restart();
  }
}

// ========== WiFi接続処理 ==========
bool connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    status.isWifiConnected = true;
    return true;
  }

  if (status.wifiRetryCount >= MAX_WIFI_RETRIES) {
    handleSystemError(WIFI_ERROR, "WiFi接続リトライ回数超過");
    return false;
  }

  WiFi.disconnect();
  delay(1000);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // 接続中はLED点滅
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    status.isWifiConnected = true;
    status.wifiRetryCount = 0;
    status.wifiSignalStrength = WiFi.RSSI();
    digitalWrite(LED_PIN, HIGH);  // 接続成功でLED点灯
    Serial.println("WiFi接続成功");
    return true;
  } else {
    status.isWifiConnected = false;
    status.wifiRetryCount++;
    digitalWrite(LED_PIN, LOW);  // 接続失敗でLED消灯
    handleSystemError(WIFI_ERROR, "WiFi接続失敗");
    return false;
  }
}

// ========== 傾き状態の処理 ==========
void handleTiltState(bool state) {
  unsigned long currentTime = millis();

  // デバッグ出力を追加
  Serial.print("Sensor State: ");
  Serial.println(state ? "HIGH" : "LOW");

  // センサーの状態が変化したときのみ通知を送信
  if ((currentTime - lastNotificationTime) >= NOTIFICATION_INTERVAL) {
    if (!state) {  // センサーがLOWになったとき（傾いたとき）
      lastNotificationTime = currentTime;
      Serial.println("書類提出検知: センサー傾き検出");
      sendSlackMessage("📄 書類が提出されました！");
    }
  }
}

// ========== システム状態チェック ==========
void checkSystemHealth() {
  unsigned long currentTime = millis();

  if (currentTime - lastHealthCheckTime >= HEALTH_CHECK_INTERVAL) {
    lastHealthCheckTime = currentTime;
    status.uptime = currentTime;

    String healthMsg = "✅ システム状態レポート\n";
    healthMsg += "稼働時間: " + String(status.uptime / 3600000) + "時間\n";
    healthMsg += "WiFi強度: " + String(status.wifiSignalStrength) + "dBm\n";
    healthMsg += "エラー数: " + String(status.errorCount);

    sendSlackMessage(healthMsg.c_str());
    Serial.println("ヘルスチェック完了");
  }
}

// ========== セーフモードとリカバリー ==========
void enterSafeMode() {
  isInSafeMode = true;
  Serial.println("セーフモードで起動しました");
  // 最小限の機能のみ有効化
}

// ========== メインループ ==========
void loop() {
  unsigned long currentTime = millis();

  // セーフモードチェック
  if (isInSafeMode) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(1000);
    return;
  }

  // OTA更新チェック
  ArduinoOTA.handle();

  // WiFi接続の確認と再接続
  if (!status.isWifiConnected && (currentTime - lastWiFiRetryTime >= WIFI_RETRY_INTERVAL)) {
    connectToWiFi();
    lastWiFiRetryTime = currentTime;
  }

  // エラーカウンターのリセット
  if (currentTime - lastErrorResetTime >= ERROR_RESET_THRESHOLD) {
    status.errorCount = 0;
    lastErrorResetTime = currentTime;
  }

  // センサー処理（デバウンス付き）
  bool currentReading = digitalRead(TILT_PIN);

  // デバッグ出力を追加
  Serial.print("Current Reading: ");
  Serial.println(currentReading);

  if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (currentReading != lastReading) {
      lastDebounceTime = currentTime;
      lastReading = currentReading;
      handleTiltState(lastReading);
    }
  }

  // システム状態チェック
  checkSystemHealth();

  delay(50);
}

// ========== Slackメッセージ送信 ==========
void sendSlackMessage(const char* message) {
  if (!status.isWifiConnected || strlen(SLACK_WEBHOOK_URL) == 0) {
    Serial.println("Slackメッセージ送信スキップ: WiFi未接続またはURL未設定");
    return;
  }

  HTTPClient http;
  http.begin(SLACK_WEBHOOK_URL);
  http.addHeader("Content-Type", "application/json");

  // リトライ用の変数
  int retryCount = 0;
  const int MAX_RETRIES = 3;
  bool sendSuccess = false;

  while (!sendSuccess && retryCount < MAX_RETRIES) {
    StaticJsonDocument<512> json;
    json["text"] = message;

    String requestBody;
    serializeJson(json, requestBody);

    int httpResponseCode = http.POST((uint8_t*)requestBody.c_str(), requestBody.length());
    status.lastHttpCode = httpResponseCode;

    if (httpResponseCode > 0) {
      String response = http.getString();
      if (httpResponseCode == HTTP_CODE_OK) {
        sendSuccess = true;
        Serial.println("Slackメッセージ送信成功: " + String(message));
      } else {
        Serial.println("HTTP応答エラー: " + String(httpResponseCode));
      }
    } else {
      Serial.println("HTTP通信エラー: " + String(httpResponseCode));
    }

    if (!sendSuccess) {
      retryCount++;
      if (retryCount < MAX_RETRIES) {
        delay(1000 * retryCount);  // 再試行時は待機時間を増やす
        Serial.println("Slackメッセージ再送試行");
      }
    }
  }

  if (!sendSuccess) {
    status.lastErrorMsg = "Slack通信エラー: リトライ回数超過";
    handleSystemError(HTTP_ERROR, status.lastErrorMsg.c_str());
  }

  http.end();
}

// ========== ユーティリティ関数 ==========
void printSystemStatus() {
  Serial.println("\n=== システム状態 ===");
  Serial.println("WiFi接続: " + String(status.isWifiConnected ? "接続済" : "未接続"));
  Serial.println("WiFi強度: " + String(status.wifiSignalStrength) + " dBm");
  Serial.println("エラー数: " + String(status.errorCount));
  Serial.println("最終エラー: " + status.lastErrorMsg);
  Serial.println("稼働時間: " + String(status.uptime / 3600000) + "時間");
  Serial.println("==================\n");
}

bool isSystemHealthy() {
  return status.isWifiConnected &&
         status.errorCount < ERROR_LIMIT &&
         abs(status.wifiSignalStrength) < WEAK_SIGNAL_THRESHOLD;
}