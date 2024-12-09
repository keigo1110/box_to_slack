#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ========== パラメータ設定 ==========
const char* ssid = "write your ssid";           // Wi-Fi SSID
const char* password = "write your password";      // Wi-Fi パスワード
const char* slackWebhookUrl = "write your slack webhook url"; // Slack Webhook URL

// GPIO設定
const int TILT_PIN = 6;
const unsigned long DEBOUNCE_DELAY = 500;
const unsigned long NOTIFICATION_INTERVAL = 20000;
const unsigned long WIFI_RETRY_INTERVAL = 30000;
const unsigned long ERROR_RESET_THRESHOLD = 3600000;  // エラーカウンターリセット間隔(1時間)

// 平均化フィルタ設定
const int NUM_READINGS = 10;

// ========== グローバル変数 ==========
unsigned long lastErrorResetTime = 0;
unsigned long lastWiFiRetryTime = 0;
unsigned long lastDebounceTime = 0;
unsigned long lastNotificationTime = 0;
int errorCount = 0;
const int ERROR_LIMIT = 5;

// センサー関連変数
int readings[NUM_READINGS];
int readIndex = 0;
int total = 0;
int average = 0;
int lastTiltState = LOW;

// ========== システム状態構造体 ==========
struct SystemStatus {
  bool isWifiConnected;
  int errorCount;
  int lastHttpCode;
  String lastErrorMsg;
} status;

// ========== 関数宣言 ==========
void handleTiltState(int state);
void handleSystemError();
bool connectToWiFi();
void sendSlackMessage(const char* message);

// ========== 初期化 ==========
void setup() {
  pinMode(TILT_PIN, INPUT_PULLUP);

  // 初期化
  status.isWifiConnected = false;
  status.errorCount = 0;
  status.lastErrorMsg = "";

  // WiFi初回接続
  connectToWiFi();

  // 平均化フィルタの初期化
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = 0;
  }
}

// ========== エラー処理 ==========
void handleSystemError() {
  errorCount++;
  status.errorCount = errorCount;

  String errorMessage = "⚠️ システムエラー\n";
  errorMessage += status.lastErrorMsg;

  sendSlackMessage(errorMessage.c_str());

  if (errorCount >= ERROR_LIMIT) {
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

  WiFi.disconnect();
  delay(1000);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    status.isWifiConnected = true;
    status.lastErrorMsg = "";
    return true;
  } else {
    status.isWifiConnected = false;
    status.lastErrorMsg = "WiFi接続エラー";
    handleSystemError();
    return false;
  }
}

// ========== 傾き状態の処理 ==========
void handleTiltState(int state) {
  unsigned long currentTime = millis();

  if (state == HIGH) {
    if (currentTime - lastNotificationTime >= NOTIFICATION_INTERVAL) {
      lastNotificationTime = currentTime;
      sendSlackMessage("📄 書類が提出されました！");
    }
  }
}

// ========== メインループ ==========
void loop() {
  unsigned long currentTime = millis();

  // WiFi接続の確認と再接続
  if (!status.isWifiConnected && (currentTime - lastWiFiRetryTime >= WIFI_RETRY_INTERVAL)) {
    connectToWiFi();
    lastWiFiRetryTime = currentTime;
  }

  // エラーカウンターのリセット
  if (currentTime - lastErrorResetTime >= ERROR_RESET_THRESHOLD) {
    errorCount = 0;
    lastErrorResetTime = currentTime;
  }

  // センサー処理
  total -= readings[readIndex];
  readings[readIndex] = digitalRead(TILT_PIN);
  total += readings[readIndex];
  readIndex = (readIndex + 1) % NUM_READINGS;
  average = total / NUM_READINGS;

  if ((currentTime - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (average != lastTiltState) {
      lastDebounceTime = currentTime;
      lastTiltState = average;
      handleTiltState(lastTiltState);
    }
  }

  delay(50);
}

// ========== Slackメッセージ送信 ==========
void sendSlackMessage(const char* message) {
  if (!status.isWifiConnected) {
    return;
  }

  HTTPClient http;
  http.begin(slackWebhookUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> json;
  json["text"] = message;
  String requestBody;
  serializeJson(json, requestBody);

  int httpResponseCode = http.POST((uint8_t*)requestBody.c_str(), requestBody.length());
  status.lastHttpCode = httpResponseCode;

  if (httpResponseCode <= 0) {
    status.lastErrorMsg = "HTTP通信エラー: " + String(httpResponseCode);
    status.isWifiConnected = false;
    handleSystemError();
  }
  http.end();
}