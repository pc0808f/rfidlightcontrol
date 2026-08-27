/**
 * NFC RFID 燈光控制系統 - v1.7.1 互動燈光模式版
 *
 * 版本資訊: v1.7.1 - 新增板載 LED 狀態指示（行動電源供電無 log 時判斷模式）
 * 建立日期: 2025-08-28
 * 更新內容:
 * - v1.5.7: 新增 MQTT 重連保護機制，防止狀態機重置
 * - v1.5.8: 縮短 Record 不足的冷卻時間，提升讀取靈敏度
 * - v1.5.9: 新增彩色開機進度指示燈 (紅→橙→黃→綠→藍→紫)
 * - v1.5.9: 心跳訊息帶上 IP 位址與 WiFi 訊號強度 (RSSI)
 * - v1.5.9: MQTT Broker 位址改為 MQTTGO.io
 * - v1.5.9: 保留開機彩虹燈，改作為所有階段完成後的「開機成功」指示
 * - v1.5.9: 關閉 WiFi 省電 (setSleep false)、keepalive 拉長至 60 秒、心跳改 30 秒，改善閒置斷線
 * - v1.6.0: [Bug修正] 開機時 WiFi 全部失敗後，改為每 60 秒定期重試（原本會永遠離線）
 * - v1.6.0: [Bug修正] 讀到非 NDEF 卡片時先以 hasNdefMessage() 檢查，避免解參考 NULL 當機
 * - v1.6.0: 啟用 WiFi.setAutoReconnect，底層短暫斷線自動重連
 * - v1.6.0: 執行期重連優先嘗試上次成功的 SSID，大幅縮短阻塞時間
 * - v1.6.0: 心跳發送失敗即標記 MQTT 斷線，加速下一輪重連
 * - v1.6.0: tagPresent 輪詢逾時縮短為 100ms，主迴圈更順暢、MQTT keepalive 更即時
 * - v1.6.0: NFC 讀取失敗先快速重試 2 次再進冷卻，提升一次刷卡成功率
 * - v1.6.0: 連續讀取失敗 5 次自動重新初始化 PN532（防模組卡死）
 * - v1.6.0: getPayloadAsString 防 payload 長度為 0 的越界讀取
 * - v1.6.1: [Bug修正] WiFi 連線內層「卡住 5 秒重置」不再重設外層 15 秒逾時計時，
 *           避免 WiFi 一直連不上時卡死在重連迴圈（⚠️重置無限循環）
 * - v1.7.0: 新增互動燈光模式：訂閱 puffin-control，App 可遙控
 *           INTERACTIVE_START（全暗待命）/ MODE1_START（8 秒白光漸亮漸暗至 50%）/
 *           MODE2_START（7 秒單顆隨機星光殘影漸暗）/ INTERACTIVE_EXIT（緊急退出）
 * - v1.7.0: 互動模式中暫停 NFC 掃描與待機燈更新，退出後自動恢復；
 *           獨立 InteractiveState 狀態機，不動原本五狀態機與 puffin-test 邏輯
 * - v1.7.1: 模式二改為 7 秒蓄力爆發：星光點亮速度與密度隨時間加速，
 *           6 秒時全條閃白定格全亮（提升遠距離觀看的視覺效果）
 * - v1.7.1: 新增板載 LED (GPIO2, UNO D13 位置) 狀態指示：
 *           恆亮=待機掃描正常 / 慢閃=互動模式中 / 快閃=WiFi 或 MQTT 連線異常，
 *           阻塞重連期間 LED 也保持閃爍以區分「重連中」與「當機」
 *
 * 已實作功能：
 * 1. FastLED 燈條控制功能
 * 2. 彩色開機進度指示燈 (依開機階段顯示不同顏色)
 * 3. 可調整參數設定
 * 4. Record 資料解析和驗證
 * 5. 燈光控制邏輯 (家族顏色)
 * 6. 五狀態機完整標籤處理流程
 * 7. WiFi 多帳密自動輪詢連線
 * 8. MQTT 資料發送功能 (puffin-test)
 * 9. MQTT 心跳機制 (puffin-heartbeat，帶 IP / RSSI)
 * 10. MQTT 即時斷線重連機制
 * 11. WiFi 連線成功指示燈 (執行期重連時顯示粉紅色)
 * 12. 待機狀態燈 (網路正常時顯示 50% 白光)
 * 13. 標籤處理統計資訊輸出
 * 14. App 遙控互動燈光模式 (訂閱 puffin-control，模式一/模式二燈效)
 *
 * 開機進度燈說明 (皆為約 20% 亮度)：
 * - 🔴 紅: 系統 / FastLED 基礎初始化完成
 * - 🟠 橙: I2C 匯流排就緒
 * - 🟡 黃: NFC (PN532) 就緒
 * - 🟢 綠: WiFi 連線成功
 * - 🔵 藍: MQTT 連線成功
 * - 🟣 紫: 系統啟動完成
 * - 🌈 彩虹: 開機成功，最後閃示一次彩虹後進入待機
 *   (若燈光停在某個顏色未前進，代表該階段卡住，可用於診斷)
 *
 * 功能特色：
 * - 徹底移除對 Adafruit 函式庫的依賴，只使用 Don Coleman 的 NDEF 函式庫
 * - 手動初始化 I2C 匯流排，確保 PN532 晶片穩定運作
 * - 完整的 Record 資料驗證機制，支援多種格式
 * - 非阻塞式狀態管理，優先保證燈光準時熄滅
 * - 彩色開機進度 / WiFi 連線成功 / 待機狀態燈光指示
 * - WiFi 和 MQTT 強韌連線管理
 *
 * 前置作業：
 * - 請確保已安裝 "NDEF library for Arduino by Don Coleman"
 * - 請確保已安裝 "FastLED library"
 * - 請確保已安裝 "PubSubClient library"
 * - 請在 wifiCredentials 陣列中設定正確的 WiFi SSID 和密碼
 */

#include <Wire.h>
#include <SPI.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
// NDEF 函式庫的 NfcAdapter.h 會 #define IRQ/RESET，與 FastLED 的識別字 (RESET 欄位/列舉) 衝突，
// 必須在引入 FastLED 前解除這兩個巨集，否則在 ESP32 core 3.x + FastLED 3.x 會編譯失敗。
#undef IRQ
#undef RESET
#include <FastLED.h>
#include <WiFi.h>          // WiFi 連線功能
#include <PubSubClient.h>  // MQTT 通訊功能

// --- 可調整參數設定 ---
const unsigned long LIGHT_DURATION = 15000;
const unsigned long READ_DEBOUNCE = 3000;
const unsigned long READ_FAIL_DEBOUNCE = 3000;  // 一般讀取失敗冷卻時間
const unsigned long RECORD_INSUFFICIENT_DEBOUNCE = 1000;  // Record 不足冷卻時間（更短）
const unsigned long BOOT_STAGE_DELAY = 400;  // 每個開機進度燈階段顯示時間（毫秒）
const unsigned long STARTUP_LIGHT_DURATION = 1000;  // 開機成功彩虹燈顯示時間（毫秒）

// --- WiFi 和 MQTT 連線參數設定 ---
struct WiFiCredential {
  const char* ssid;
  const char* password;
};
const WiFiCredential wifiCredentials[] = {
  {"HPEES_Internal", "Hpees2733"},
  {"Sam", "0928666624"},
  {"Sam&Betty", "0928666624"},
  {"YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"}
};
const int WIFI_CREDENTIAL_COUNT = sizeof(wifiCredentials) / sizeof(wifiCredentials[0]);
const char* MQTT_BROKER = "MQTTGO.io";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "puffin-test";
const char* MQTT_CONTROL_TOPIC = "puffin-control";  // 互動燈光模式控制指令（App 端發送，字串須與 App 完全一致）
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;
const unsigned long MQTT_CONNECT_TIMEOUT = 5000;
const unsigned long CONNECTION_CHECK_INTERVAL = 10000;
const unsigned long MQTT_KEEPALIVE_SECONDS = 60;  // MQTT keep-alive（拉長以減少閒置斷線）
const unsigned long HEARTBEAT_INTERVAL = 30000;  // 心跳間隔 30 秒（< keepalive，維持連線活躍）
const unsigned long WIFI_RETRY_INTERVAL = 60000;  // WiFi 離線時的定期重試間隔（防開機失敗後永遠離線）

// --- NFC 讀取穩定性參數 ---
const unsigned long NFC_POLL_TIMEOUT = 100;    // tagPresent 輪詢逾時（毫秒），縮短以維持主迴圈順暢
const int NFC_READ_ATTEMPTS = 3;               // NFC 讀取嘗試次數（1 次 + 2 次快速重試）
const int NFC_MAX_CONSECUTIVE_FAILURES = 5;    // 連續讀取失敗達此次數即重新初始化 PN532

// --- ESP32 預設的 I2C 接腳 ---
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;

// --- FastLED 燈條設定 ---
#define DATA_PIN    12
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    100
#define BRIGHTNESS  255

// --- NFC 物件宣告 ---
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfcAdapter = NfcAdapter(pn532_i2c);

// --- LED 陣列宣告 ---
CRGB leds[NUM_LEDS];

// --- WiFi 和 MQTT 物件宣告 ---
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
String mqttClientId = "";

// --- 系統狀態管理變數 ---
enum SystemState { STATE_IDLE, STATE_TAG_DETECTED, STATE_PROCESSING, STATE_LIGHT_ON, STATE_COOLDOWN };
SystemState currentState = STATE_IDLE;
bool isLightOn = false;
unsigned long lightStartTime = 0;
unsigned long stateChangeTime = 0;
bool isIdleStatusLight = false;  // 待機狀態燈是否開啟
bool bootInProgress = true;      // 開機進度燈顯示中（開機完成後設為 false）

// --- 標籤穩定性檢測變數 ---
bool lastTagState = false;
unsigned long tagDetectedTime = 0;
const unsigned long TAG_STABLE_DURATION = 100;  // 標籤需穩定存在 100ms（提升響應速度）

// --- 防連續讀取變數 ---
bool isInCooldown = false;
unsigned long cooldownStartTime = 0;
unsigned long cooldownDuration = 0;

// --- 防重複發送變數 ---
String lastSentData = "";

// --- 統計和調試變數 ---
unsigned long totalTagsProcessed = 0;
unsigned long successfulReads = 0;
unsigned long failedReads = 0;

// --- 網路操作保護機制變數 ---
bool isNetworkOperationInProgress = false;  // 網路操作進行中標記
unsigned long networkOperationStartTime = 0;
const unsigned long MAX_NETWORK_OPERATION_TIME = 8000;  // 最大網路操作時間 8 秒

// --- WiFi 和 MQTT 連線狀態變數 ---
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastConnectionCheck = 0;
unsigned long wifiConnectStartTime = 0;
unsigned long mqttConnectStartTime = 0;
int connectedCredentialIndex = -1;
int lastSuccessfulCredentialIndex = -1;  // 上次成功連線的帳密索引（重連時優先嘗試）
unsigned long lastWiFiRetryAttempt = 0;  // 上次 WiFi 定期重試的時間
unsigned long lastHeartbeat = 0;

// --- NFC 穩定性變數 ---
int consecutiveReadFailures = 0;  // 連續讀取失敗計數（達上限即重新初始化 PN532）

// --- 互動燈光模式參數 ---
const unsigned long MODE1_DURATION = 8000;       // 模式一總長（毫秒）
const unsigned long MODE1_FADE_IN_TIME = 5000;   // 模式一漸亮時間（0~5 秒漸亮，之後漸暗）
const unsigned long MODE2_DURATION = 7000;       // 模式二總長（毫秒）
const uint8_t MODE1_PEAK_BRIGHTNESS = 128;       // 模式一最亮亮度（約 50%）
const unsigned long MODE2_FADE_INTERVAL = 25;    // 模式二殘影淡出的更新間隔（毫秒）
const uint8_t MODE2_FADE_AMOUNT = 20;            // 模式二每次淡出的幅度（越大殘影消失越快，約 1.2 秒淡完）
const unsigned long MODE2_BURST_TIME = 6000;         // 模式二蓄力階段長度，時間到全條閃白定格全亮
const unsigned long MODE2_SPARK_INTERVAL_START = 300; // 蓄力開始時的點燈間隔（毫秒，稀疏慢閃）
const unsigned long MODE2_SPARK_INTERVAL_END = 15;    // 接近爆發時的點燈間隔（毫秒，全條狂閃）
const int MODE2_MAX_SPARKS_PER_TICK = 5;              // 接近爆發時每次額外多點亮的顆數上限

// --- 互動燈光模式狀態變數（與 NFC 狀態機完全獨立，不共用變數）---
enum InteractiveState {
  INTERACTIVE_IDLE,          // 一般待機，NFC 正常掃描
  INTERACTIVE_WAITING_MODE1, // 已進入互動模式，燈已關，等待「模式一」指令
  INTERACTIVE_MODE1_RUNNING, // 模式一動作中（8 秒漸亮漸暗）
  INTERACTIVE_WAITING_MODE2, // 模式一結束，等待「模式二」指令
  INTERACTIVE_MODE2_RUNNING  // 模式二動作中（7 秒隨機燈光）
};
InteractiveState interactiveState = INTERACTIVE_IDLE;
unsigned long mode1StartTime = 0;
unsigned long mode2StartTime = 0;
uint8_t mode1LastBrightness = 255;      // 上次模式一顯示的亮度（值沒變就不重複 show）
unsigned long mode2LastChangeTime = 0;
unsigned long mode2NextInterval = 0;
unsigned long mode2LastFadeTime = 0;    // 上次殘影淡出更新的時間
bool mode2BurstShown = false;           // 爆發閃白是否已顯示（只需 show 一次後定格）

// --- 板載狀態指示 LED（WEMOS D1 R32 板載藍色 LED，GPIO2，UNO 佈局 D13 位置）---
// 行動電源供電看不到 Serial log 時，靠這顆 LED 判斷目前模式：
//   恆亮 = 待機掃描模式（正常）；慢閃 = 互動模式中；快閃 = WiFi/MQTT 連線異常
const int STATUS_LED_PIN = 2;                            // 板載藍色 LED 固定在 GPIO2（不用 LED_BUILTIN，通用 ESP32 板定義沒有它）
const unsigned long STATUS_LED_INTERACTIVE_BLINK = 500;  // 互動模式：慢閃半週期（毫秒）
const unsigned long STATUS_LED_ERROR_BLINK = 150;        // 連線異常：快閃半週期（毫秒）
unsigned long statusLedLastToggle = 0;
bool statusLedOn = false;

// --- 函數宣告 ---
void showBootProgress(CRGB color, const char* stageMsg);
void showStartupRainbow();
void turnOffLeds();
void showColorLight(CRGB color);
void showWiFiConnectedLight(); // [新功能]
void processLightControl(int record2Number, int record3Number);
String getPayloadAsString(NdefRecord& record);
int extractNumberSmart(String data);
bool validateRecord2(String data);
bool validateRecord3(String data);
void changeSystemState(SystemState newState);
void handleIdleState();
void handleTagDetectedState();
void handleProcessingState();
void handleLightOnState();
void handleCooldownState();
int processTagData();
void printStateString(SystemState state);
void printSystemStatus();
void printStatistics();
String getMacAddress();
void connectToWiFi();
void connectToMQTT();
void checkMQTTConnection();
void printConnectionStatus();
String formatMQTTPayload(int record3Number, int record2Number);
void publishToMQTT(String topic, String payload);
void sendHeartbeat();
void updateIdleStatusLight();
void showIdleStatusLight();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleInteractiveMode();
void exitInteractiveMode(const char* reason);
void resetNfcStateMachine();
void runMode1Animation();
void runMode2RandomLeds();
void updateStatusLed();
void statusLedBlink(unsigned long halfPeriod);

// =========================================================================
void setup(void) {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  Serial.println("\nNFC RFID 燈光控制系統 v1.7.1 (互動燈光模式版)");
  Serial.println("=========================================================");
  Serial.println("新增: App 遙控互動燈光模式 / 板載 LED 狀態指示");
  Serial.println("=========================================================");

  // --- FastLED 先初始化，才能顯示開機進度燈 ---
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setDither(BRIGHTNESS < 255);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.println("FastLED 燈條已初始化完成。");
  showBootProgress(CRGB::Red, "系統 / FastLED 基礎初始化完成");   // 🔴 紅

  // --- 啟動 I2C 匯流排 ---
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("I2C 匯流排已於 GPIO 21 (SDA) 和 22 (SCL) 啟動。");
  delay(400);
  showBootProgress(CRGB::Orange, "I2C 匯流排就緒");               // 🟠 橙

  // --- 初始化 NFC (PN532) ---
  nfcAdapter.begin();
  showBootProgress(CRGB::Yellow, "NFC PN532 就緒");               // 🟡 黃

  // --- 初始化網路連線 ---
  Serial.println("\n📶 正在初始化網路連線...");
  mqttClientId = getMacAddress();
  Serial.print("🔧 MQTT Client ID: "); Serial.println(mqttClientId);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE_SECONDS);  // 設定 MQTT keep-alive 為 60 秒
  mqttClient.setCallback(mqttCallback);  // 接收 puffin-control 互動模式指令
  Serial.print("🌐 MQTT Broker: "); Serial.print(MQTT_BROKER);
  Serial.print(":"); Serial.println(MQTT_PORT);
  Serial.print("🔧 MQTT Keep-Alive: "); Serial.print(MQTT_KEEPALIVE_SECONDS); Serial.println(" 秒");

  connectToWiFi();
  if (wifiConnected) {
    showBootProgress(CRGB::Green, "WiFi 連線成功");               // 🟢 綠
    connectToMQTT();
    if (mqttConnected) {
      showBootProgress(CRGB::Blue, "MQTT 連線成功");             // 🔵 藍
    }
  }

  showBootProgress(CRGB::Purple, "系統啟動完成");                 // 🟣 紫
  delay(300);
  showStartupRainbow();    // 🌈 最後以彩虹燈表示開機成功
  bootInProgress = false;  // 開機進度燈結束，恢復待機/狀態燈邏輯

  currentState = STATE_IDLE;
  stateChangeTime = millis();
  lastConnectionCheck = millis();
  lastHeartbeat = millis();

  Serial.println("\n🎯 系統初始化完成！");
  Serial.println("📊 當前狀態: IDLE (待機中)");
  Serial.println("💡 請將 NFC 標籤靠近讀卡器開始感應...");
  Serial.println("----------------------------------------------------");
  printConnectionStatus();
  printSystemStatus();

  // 設定初始待機狀態燈
  updateIdleStatusLight();
}

// =========================================================================
void loop(void) {
  unsigned long currentTime = millis();

  // 板載 LED 狀態指示（恆亮=待機掃描 / 慢閃=互動模式 / 快閃=連線異常）
  updateStatusLed();


  if (currentTime - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    checkMQTTConnection();
    lastConnectionCheck = currentTime;
    // 連線狀態改變時更新待機狀態燈
    updateIdleStatusLight();
  }
  
  // 心跳機制：定期發送系統狀態
  if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = currentTime;
  }
  
  if (interactiveState == INTERACTIVE_IDLE) {
    // 原本的 NFC 狀態機，完全不變
    switch (currentState) {
      case STATE_IDLE:         handleIdleState();        break;
      case STATE_TAG_DETECTED: handleTagDetectedState(); break;
      case STATE_PROCESSING:   handleProcessingState();  break;
      case STATE_LIGHT_ON:     handleLightOnState();     break;
      case STATE_COOLDOWN:     handleCooldownState();    break;
    }
  } else {
    // 互動燈光模式進行中，暫停 NFC 掃描
    handleInteractiveMode();
  }
  
  // 維持 MQTT 客戶端運作
  // 即使在燈亮或延遲狀態下，也能保持 MQTT 連線
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  delay(10);
}


// =========================================================================
// [新功能] WiFi 連線成功指示燈
/**
 * @brief 顯示 WiFi 連線成功的粉紅色燈光指示
 */
void showWiFiConnectedLight() {
  Serial.println("✨ WiFi 連線成功，顯示粉紅色指示燈 1 秒...");
  fill_solid(leds, NUM_LEDS, CRGB::HotPink);
  FastLED.show();
  delay(1000); // 持續 1 秒
  turnOffLeds(); // 熄滅燈光，恢復正常狀態
}

// =========================================================================
// [修改] MQTT 相關函數 - 強化發送邏輯
/**
 * @brief 格式化 MQTT 訊息 payload
 */
String formatMQTTPayload(int record3Number, int record2Number) {
  char bufferR3[4]; char bufferR2[3];
  sprintf(bufferR3, "%03d", record3Number);
  sprintf(bufferR2, "%02d", record2Number);
  return String(bufferR3) + "," + String(bufferR2);
}

/**
 * @brief 發送訊息到指定的 MQTT 主題，帶有即時重連機制
 */
void publishToMQTT(String topic, String payload) {
  // 步驟 1: 檢查連線，如果斷線則立即嘗試重連
  if (!mqttClient.connected()) {
    Serial.println("⚠️ MQTT 未連線，在發送前嘗試立即重連...");
    connectToMQTT(); // 使用現有的重連函數
  }
  
  // 步驟 2: 再次檢查連線狀態
  if (mqttClient.connected()) {
    if (mqttClient.publish(topic.c_str(), payload.c_str())) {
      Serial.print("🚀 MQTT 訊息已發送 -> 主題 ["); Serial.print(topic);
      Serial.print("], 內容 ["); Serial.print(payload); Serial.println("]");
    } else {
      Serial.println("❌ MQTT 訊息發送失敗！ (publish failed)");
    }
  } else {
    Serial.println("❌ MQTT 重連失敗，本次訊息無法發送。");
  }
}

// =========================================================================
// [修改] WiFi 連線函數 - 加入燈光指示
/**
 * @brief 連線到 WiFi 網路 - 依序嘗試多個帳密
 */
void connectToWiFi() {
  // 設定網路操作進行中標記
  isNetworkOperationInProgress = true;
  networkOperationStartTime = millis();
  
  Serial.println("📶 開始 WiFi 連線，嘗試多個網路...");
  Serial.print("🔍 可用網路數量: "); Serial.println(WIFI_CREDENTIAL_COUNT);
  WiFi.disconnect(true); delay(100);
  WiFi.mode(WIFI_STA); delay(100);
  WiFi.setSleep(false);  // 關閉 WiFi 省電模式，避免閒置時漏掉 MQTT keepalive 而斷線
  WiFi.setAutoReconnect(true);  // [v1.6.0] 底層短暫斷線時自動重連，不必等 10 秒檢查週期
  wifiConnected = false; connectedCredentialIndex = -1;

  // [v1.6.0] 優先嘗試上次成功的網路，大幅縮短執行期重連的阻塞時間
  int tryOrder[WIFI_CREDENTIAL_COUNT];
  int tryCount = 0;
  if (lastSuccessfulCredentialIndex >= 0 && lastSuccessfulCredentialIndex < WIFI_CREDENTIAL_COUNT) {
    tryOrder[tryCount++] = lastSuccessfulCredentialIndex;
  }
  for (int i = 0; i < WIFI_CREDENTIAL_COUNT; i++) {
    if (i != lastSuccessfulCredentialIndex) tryOrder[tryCount++] = i;
  }

  for (int k = 0; k < tryCount; k++) {
    int i = tryOrder[k];
    Serial.print("\n🔄 嘗試連接 ("); Serial.print(k + 1); Serial.print("/");
    Serial.print(tryCount); Serial.print("): "); Serial.println(wifiCredentials[i].ssid);
    WiFi.disconnect(false); delay(100);
    WiFi.begin(wifiCredentials[i].ssid, wifiCredentials[i].password);
    wifiConnectStartTime = millis();
    unsigned long lastResetAttemptTime = wifiConnectStartTime;
    while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED && (millis() - wifiConnectStartTime < WIFI_CONNECT_TIMEOUT)) {
      delay(500); Serial.print(".");
      statusLedBlink(STATUS_LED_ERROR_BLINK);  // 阻塞重連期間板載 LED 保持閃爍，表示重連進行中而非當機
      if (WiFi.status() == WL_DISCONNECTED && (millis() - lastResetAttemptTime > 5000)) {
        Serial.print("⚠️重置"); WiFi.disconnect(false); delay(500);
        WiFi.begin(wifiCredentials[i].ssid, wifiCredentials[i].password);
        lastResetAttemptTime = millis();
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true; connectedCredentialIndex = i;
      lastSuccessfulCredentialIndex = i;  // [v1.6.0] 記住成功的帳密，下次重連優先嘗試
      Serial.println("\n✅ WiFi 連線成功！");
      Serial.print("🌐 已連接網路: "); Serial.println(wifiCredentials[i].ssid);
      Serial.print("🌐 IP 地址: "); Serial.println(WiFi.localIP());
      Serial.print("📡 訊號強度: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");

      // [新功能] 執行期重連時顯示 WiFi 連線成功指示燈（開機期間改用開機進度燈）
      if (!bootInProgress) {
        showWiFiConnectedLight();
      }
      
      // 清除網路操作標記並更新待機狀態燈
      isNetworkOperationInProgress = false;
      updateIdleStatusLight();

      return;
    } else {
      Serial.print(" ❌ 連線失敗 (狀態: "); Serial.print(WiFi.status()); Serial.println(")");
      WiFi.disconnect(true); delay(2000);
    }
  }
  wifiConnected = false; 
  Serial.println("\n❌ 所有 WiFi 網路連線都失敗！");
  
  // 清除網路操作標記
  isNetworkOperationInProgress = false;
}

// =========================================================================
void handleCooldownState() {
  unsigned long currentTime = millis();
  if (currentTime - cooldownStartTime >= cooldownDuration) {
    isInCooldown = false; Serial.println("✨ 冷卻完成，系統準備就緒");
    if (lastSentData != "") {
      Serial.println("🔄 重置 MQTT 防重複紀錄");
      lastSentData = "";
    }
    printSystemStatus();
    printStatistics();  // 每次感應冷卻結束時輸出累計統計
    changeSystemState(STATE_IDLE);
    // 冷卻結束，恢復待機狀態燈
    updateIdleStatusLight();
  }
}
int processTagData() {
  // [v1.6.0] 讀取失敗時快速重試：卡片在感應邊緣常第一次失敗、第二次成功
  for (int attempt = 1; attempt <= NFC_READ_ATTEMPTS; attempt++) {
  NfcTag tag = nfcAdapter.read();
  Serial.print("📱 標籤類型: "); Serial.println(tag.getTagType());

  // [v1.6.0 Bug修正] 先確認標籤含 NDEF 訊息再取用，
  // 否則空白卡/一般感應卡會使函式庫解參考 NULL 指標導致當機重啟
  if (!tag.hasNdefMessage()) {
    Serial.print("❌ 標籤不含 NDEF 訊息，或讀取失敗 (嘗試 ");
    Serial.print(attempt); Serial.print("/"); Serial.print(NFC_READ_ATTEMPTS); Serial.println(")");
    if (attempt < NFC_READ_ATTEMPTS && nfcAdapter.tagPresent(NFC_POLL_TIMEOUT)) {
      delay(100);  // 卡片仍在場，短暫等待後快速重試
      continue;
    }
    cooldownDuration = READ_FAIL_DEBOUNCE; isInCooldown = true; cooldownStartTime = millis();
    return -1;
  }

  NdefMessage message = tag.getNdefMessage();
  if (message.getRecordCount() <= 0) {
    Serial.println("❌ 標籤 NDEF 訊息為空");
    cooldownDuration = READ_FAIL_DEBOUNCE; isInCooldown = true; cooldownStartTime = millis();
    return -1;
  }
  int recordCount = message.getRecordCount();
  Serial.print("📄 讀取成功！找到 "); Serial.print(recordCount); Serial.println(" 個 Record");
  Serial.println("------------------------------------");
  if (recordCount < 3) {
    Serial.println("⚠️  警告：Record 數量不足 3 個，快速重試");
    // Record 不足時使用較短的冷卻時間
    cooldownDuration = RECORD_INSUFFICIENT_DEBOUNCE; 
    isInCooldown = true; 
    cooldownStartTime = millis();
    return -2;  // 新的錯誤代碼，區分 Record 不足
  }
  String record2Data = "", record3Data = "";
  int record2Number = -1, record3Number = -1;
  for (int i = 0; i < recordCount; i++) {
    NdefRecord record = message[i]; String payload = getPayloadAsString(record);
    Serial.print("Record "); Serial.print(i + 1); Serial.print(" [Type: "); Serial.print(record.getType()); Serial.print("]: "); Serial.println(payload);
    if (i == 1) record2Data = payload; else if (i == 2) record3Data = payload;
  }
  bool validData = true;
  if (recordCount >= 2 && !record2Data.isEmpty()) {
    if (validateRecord2(record2Data)) {
      record2Number = extractNumberSmart(record2Data);
      Serial.print("✅ Record 2 驗證通過: "); Serial.print(record2Data); Serial.print(" → "); Serial.println(record2Number);
    } else { Serial.print("❌ Record 2 格式錯誤: "); Serial.println(record2Data); validData = false; }
  }
  if (recordCount >= 3 && !record3Data.isEmpty()) {
    if (validateRecord3(record3Data)) {
      record3Number = extractNumberSmart(record3Data);
      Serial.print("✅ Record 3 驗證通過: "); Serial.print(record3Data); Serial.print(" → "); Serial.println(record3Number);
    } else { Serial.print("❌ Record 3 格式錯誤: "); Serial.println(record3Data); validData = false; }
  }
  if (validData && record2Number >= 0 && record3Number >= 0) {
    Serial.println("🎉 資料驗證成功！");
    String mqttPayload = formatMQTTPayload(record3Number, record2Number);
    if (mqttPayload != lastSentData) {
      publishToMQTT(MQTT_TOPIC, mqttPayload);
      lastSentData = mqttPayload;
    } else { Serial.println("🛡️  資料與上次相同，已跳過重複發送 MQTT 訊息。"); }
    if (record2Number == 0) {
      cooldownDuration = READ_DEBOUNCE; isInCooldown = true; cooldownStartTime = millis(); return 0;
    } else {
      processLightControl(record2Number, record3Number);
      cooldownDuration = READ_DEBOUNCE; isInCooldown = true; cooldownStartTime = millis(); return 1;
    }
  } else {
    Serial.println("💥 資料驗證失敗！");
    cooldownDuration = READ_FAIL_DEBOUNCE; isInCooldown = true; cooldownStartTime = millis();
    return -1;
  }
  }  // end of retry for-loop（迴圈內所有路徑都會 return，此處僅為安全備援）
  cooldownDuration = READ_FAIL_DEBOUNCE; isInCooldown = true; cooldownStartTime = millis();
  return -1;
}
int extractNumber(String data, String prefix) {
  if (data.length() < prefix.length() + 1) return -1;
  if (!data.startsWith(prefix)) return -1;
  String numberPart = data.substring(prefix.length());
  for (int i = 0; i < numberPart.length(); i++) {
    if (!isDigit(numberPart.charAt(i))) return -1;
  }
  return numberPart.toInt();
}
int extractNumberSmart(String data) {
  if (data.startsWith("en")) return extractNumber(data, "en");
  else {
    bool isAllDigits = true;
    for (int i = 0; i < data.length(); i++) {
      if (!isDigit(data.charAt(i))) { isAllDigits = false; break; }
    }
    if (isAllDigits && data.length() > 0) return data.toInt();
  }
  return -1;
}
bool validateRecord2(String data) {
  int number = -1;
  if (data.startsWith("en")) number = extractNumber(data, "en");
  else {
    bool isAllDigits = true;
    for (int i = 0; i < data.length(); i++) {
      if (!isDigit(data.charAt(i))) { isAllDigits = false; break; }
    }
    if (isAllDigits && data.length() > 0) number = data.toInt();
  }
  return (number >= 0 && number <= 4);
}
bool validateRecord3(String data) {
  int number = -1;
  if (data.startsWith("en")) number = extractNumber(data, "en");
  else {
    bool isAllDigits = true;
    for (int i = 0; i < data.length(); i++) {
      if (!isDigit(data.charAt(i))) { isAllDigits = false; break; }
    }
    if (isAllDigits && data.length() > 0) number = data.toInt();
  }
  return (number >= 1 && number <= 120);
}
/**
 * @brief 顯示開機進度指示燈：以約 20% 亮度填滿指定顏色，代表目前開機階段
 * @param color    階段顏色 (紅→橙→黃→綠→藍→紫)
 * @param stageMsg 該階段的文字說明
 */
void showBootProgress(CRGB color, const char* stageMsg) {
  color.nscale8(51);  // 縮放至約 20% 亮度 (51/255)
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
  Serial.print("🔆 開機進度燈: "); Serial.println(stageMsg);
  delay(BOOT_STAGE_DELAY);  // 讓每個階段的燈光清楚可見
}
/**
 * @brief 開機成功彩虹燈：所有開機階段完成後顯示一次彩虹，代表開機成功
 */
void showStartupRainbow() {
  Serial.print("🌈 開機成功，顯示彩虹燈 ("); Serial.print(STARTUP_LIGHT_DURATION); Serial.println(" 毫秒)...");
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(i * (255 / NUM_LEDS), 255, 255);
  FastLED.show(); delay(STARTUP_LIGHT_DURATION);
  turnOffLeds(); Serial.println("開機燈光指示完成");
}
void turnOffLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show();
}
void showColorLight(CRGB color) {
  fill_solid(leds, NUM_LEDS, color); FastLED.show();
}
void processLightControl(int record2Number, int record3Number) {
  if (record2Number == 0) { Serial.println("Record 2 為 00，不執行燈光控制"); return; }
  CRGB color = CRGB::Black; String familyName = "";
  switch (record2Number) {
    case 1: color = CRGB::Blue; familyName = "天空家族"; break;
    case 2: color = CRGB::Green; familyName = "海洋家族"; break;
    case 3: color = CRGB::Red; familyName = "樹居家族"; break;
    case 4: color = CRGB::Yellow; familyName = "大地家族"; break;
    default: Serial.println("未知的 Record 2 數字"); return;
  }
  Serial.print("歡迎加入"); Serial.println(familyName);
  Serial.print("Record 3 資料已暫存: "); Serial.println(record3Number);
  showColorLight(color); isLightOn = true; lightStartTime = millis();
  Serial.print("燈光已點亮，將持續 "); Serial.print(LIGHT_DURATION / 1000.0, 1); Serial.println(" 秒");
}
String getPayloadAsString(NdefRecord& record) {
  String payloadString = ""; String recordType = record.getType();
  int payloadLength = record.getPayloadLength();
  if (payloadLength <= 0) return "";  // [v1.6.0] 防空 record 造成的越界讀取
  byte payload[payloadLength];
  record.getPayload(payload);
  if (recordType == "U") {
    String prefix = "";
    switch (payload[0]) {
      case 0x01: prefix = "http://www."; break; case 0x02: prefix = "https://www."; break;
      case 0x03: prefix = "http://"; break; case 0x04: prefix = "https://"; break;
    }
    payloadString += prefix;
    for (int i = 1; i < payloadLength; i++) { payloadString += (char)payload[i]; }
  } else if (recordType == "T") {
    int langCodeLength = payload[0] & 0x3F; int textStart = 1 + langCodeLength;
    for (int i = textStart; i < payloadLength; i++) { payloadString += (char)payload[i]; }
  } else { payloadString = "Unsupported Record Type"; }
  return payloadString;
}
void changeSystemState(SystemState newState) {
  if (currentState != newState) {
    Serial.print("🔄 狀態變更: "); printStateString(currentState); Serial.print(" → "); printStateString(newState);
    Serial.println(); currentState = newState; stateChangeTime = millis();
  }
}
void handleIdleState() {
  // 如果網路操作進行中，暫停 NFC 檢測
  if (isNetworkOperationInProgress) {
    // 檢查網路操作是否超時
    if (millis() - networkOperationStartTime > MAX_NETWORK_OPERATION_TIME) {
      Serial.println("⚠️ 網路操作超時，恢復 NFC 檢測");
      isNetworkOperationInProgress = false;
    } else {
      return;  // 暫停 NFC 檢測
    }
  }
  
  // [v1.6.0] 指定 100ms 輪詢逾時：預設值每次可阻塞近 1 秒，
  // 會拖慢主迴圈與 MQTT keepalive；縮短後迴圈更順、讀卡反應更快
  bool currentTagState = nfcAdapter.tagPresent(NFC_POLL_TIMEOUT);
  unsigned long currentTime = millis();
  
  // 標籤狀態改變時重設計時器
  if (currentTagState != lastTagState) {
    lastTagState = currentTagState;
    tagDetectedTime = currentTime;
  }
  
  // 只有當標籤穩定存在足夠時間後才進行處理
  if (currentTagState && (currentTime - tagDetectedTime >= TAG_STABLE_DURATION)) {
    // 偵測到穩定標籤，保持待機狀態燈並切換狀態
    Serial.println("📋 標籤穩定偵測完成，開始處理...");
    changeSystemState(STATE_TAG_DETECTED);
  }
}
void handleTagDetectedState() {
  // 再次確認標籤仍然存在且穩定
  if (!nfcAdapter.tagPresent(NFC_POLL_TIMEOUT)) {
    Serial.println("⚠️ 標籤已移除，返回待機狀態");
    changeSystemState(STATE_IDLE);
    // 恢復待機狀態燈
    updateIdleStatusLight();
    return;
  }
  
  totalTagsProcessed++;
  Serial.println("\n📋 標籤穩定，開始資料處理...");
  changeSystemState(STATE_PROCESSING);
}
void handleProcessingState() {
  Serial.println("⚡ 正在解析標籤資料...");
  int result = processTagData();

  // [v1.6.0] 連續失敗自動復位：PN532 長時間運作偶爾會卡死（有回應但一直讀失敗），
  // 連續失敗達上限就重新初始化，免除人工斷電
  if (result >= 0) {
    consecutiveReadFailures = 0;
  } else {
    consecutiveReadFailures++;
    if (consecutiveReadFailures >= NFC_MAX_CONSECUTIVE_FAILURES) {
      Serial.println("🔧 連續讀取失敗過多，重新初始化 PN532 模組...");
      nfcAdapter.begin();
      consecutiveReadFailures = 0;
      Serial.println("🔧 PN532 重新初始化完成");
    }
  }

  if (result > 0) {
    successfulReads++; 
    // 成功讀取且需要燈光控制，關閉待機狀態燈
    if (isIdleStatusLight) {
      isIdleStatusLight = false;
    }
    changeSystemState(STATE_LIGHT_ON); 
  }
  else if (result == 0) { 
    successfulReads++; 
    Serial.println("✅ 標籤處理完成，Record2=00 不執行燈光控制"); 
    changeSystemState(STATE_COOLDOWN); 
  }
  else if (result == -2) {
    failedReads++;
    Serial.println("⚠️  Record 數量不足，快速重新感應");
    changeSystemState(STATE_COOLDOWN);
  }
  else { 
    failedReads++; 
    Serial.println("❌ 標籤處理失敗"); 
    changeSystemState(STATE_COOLDOWN); 
  }
}
void handleLightOnState() {
  unsigned long currentTime = millis();
  if (currentTime - lightStartTime >= LIGHT_DURATION) {
    isLightOn = false; 
    Serial.println("💡 燈光持續時間結束，自動熄滅");
    
    // 檢查網路和 MQTT 狀態，決定是否顯示待機燈
    if (wifiConnected && mqttConnected) {
      Serial.println("🌐 網路和 MQTT 連線正常，切換至待機狀態燈");
      showIdleStatusLight();
      isIdleStatusLight = true;
    } else {
      Serial.println("❌ 網路或 MQTT 連線異常，完全關閉燈光");
      turnOffLeds();
    }
    
    changeSystemState(STATE_COOLDOWN);
  }
}
void printStateString(SystemState state) {
  switch (state) {
    case STATE_IDLE: Serial.print("IDLE"); break;
    case STATE_TAG_DETECTED: Serial.print("TAG_DETECTED"); break;
    case STATE_PROCESSING: Serial.print("PROCESSING"); break;
    case STATE_LIGHT_ON: Serial.print("LIGHT_ON"); break;
    case STATE_COOLDOWN: Serial.print("COOLDOWN"); break;
    default: Serial.print("UNKNOWN"); break;
  }
}
void printSystemStatus() {
  Serial.println("\n📊 === 系統狀態報告 ===");
  Serial.print("🔧 當前狀態: "); printStateString(currentState); Serial.println();
  Serial.print("💡 燈光狀態: "); Serial.println(isLightOn ? "開啟" : "關閉");
  Serial.print("❄️  冷卻狀態: "); Serial.println(isInCooldown ? "進行中" : "閒置");
  Serial.println("========================\n");
}
void printStatistics() {
  Serial.println("\n📈 === 統計資訊 ===");
  Serial.print("📋 總處理標籤數: "); Serial.println(totalTagsProcessed);
  Serial.print("✅ 成功讀取次數: "); Serial.println(successfulReads);
  Serial.print("❌ 失敗讀取次數: "); Serial.println(failedReads);
  if (totalTagsProcessed > 0) {
    Serial.print("📊 成功率: "); Serial.print((successfulReads * 100) / totalTagsProcessed); Serial.println("%");
  }
  Serial.println("===================\n");
}
String getMacAddress() {
  uint64_t chipId = ESP.getEfuseMac();
  String chipIdStr = String((uint32_t)(chipId >> 32), HEX) + String((uint32_t)chipId, HEX);
  chipIdStr.toUpperCase(); return "ESP32_" + chipIdStr;
}
void connectToMQTT() {
  if (!wifiConnected) { Serial.println("❌ WiFi 未連線，無法連接 MQTT"); return; }
  
  // 設定網路操作進行中標記
  isNetworkOperationInProgress = true;
  networkOperationStartTime = millis();
  
  Serial.print("🌐 正在連接 MQTT Broker: "); Serial.println(MQTT_BROKER);
  mqttConnectStartTime = millis();
  while (!mqttClient.connected() && (millis() - mqttConnectStartTime < MQTT_CONNECT_TIMEOUT)) {
    Serial.print("🔄 嘗試 MQTT 連線...");
    if (mqttClient.connect(mqttClientId.c_str())) {
      mqttConnected = true; Serial.println("\n✅ MQTT 連線成功！");
      Serial.print("🔧 Client ID: "); Serial.println(mqttClientId);
      // 每次連線成功都要重新訂閱（斷線重連後訂閱會失效）
      if (mqttClient.subscribe(MQTT_CONTROL_TOPIC)) {
        Serial.print("📥 已訂閱控制主題: "); Serial.println(MQTT_CONTROL_TOPIC);
      } else {
        Serial.println("⚠️ 訂閱控制主題失敗，互動模式指令將收不到");
      }
    } else {
      Serial.print("❌ 連線失敗，錯誤代碼: "); Serial.print(mqttClient.state());
      Serial.println("，2 秒後重試...");
      statusLedBlink(STATUS_LED_ERROR_BLINK);  // 阻塞重連期間維持 LED 動作
      delay(2000);
    }
  }
  if (!mqttClient.connected()) { 
    mqttConnected = false; 
    Serial.println("❌ MQTT 連線超時失敗！"); 
  }
  
  // 清除網路操作標記並更新待機狀態燈
  isNetworkOperationInProgress = false;
  updateIdleStatusLight();
}
void checkMQTTConnection() {
  // 如果正在進行網路操作，跳過檢查避免衝突
  if (isNetworkOperationInProgress) {
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false; mqttConnected = false; connectedCredentialIndex = -1;
      Serial.println("⚠️  WiFi 連線中斷，嘗試重新連線...");
      lastWiFiRetryAttempt = millis();
      connectToWiFi();  // 此函數內會設定 isNetworkOperationInProgress
      if (wifiConnected) connectToMQTT();  // 此函數內也會設定標記
    } else if (millis() - lastWiFiRetryAttempt >= WIFI_RETRY_INTERVAL) {
      // [v1.6.0 Bug修正] 開機時 WiFi 全部失敗後也要定期重試
      // （原本只在「曾連線過」時才重連，開機即失敗會導致永遠離線）
      Serial.println("🔁 WiFi 離線中，執行定期重試連線...");
      lastWiFiRetryAttempt = millis();
      connectToWiFi();
      if (wifiConnected) connectToMQTT();
    }
    return;
  } else if (!wifiConnected) {
    wifiConnected = true; 
    Serial.println("✅ WiFi 重新連線成功");
    updateIdleStatusLight();
  }
  if (!mqttClient.connected()) {
    if (mqttConnected) { mqttConnected = false; Serial.println("⚠️  MQTT 連線中斷"); }
    Serial.println("🔄 正在重新連接 MQTT...");
    connectToMQTT();  // 此函數內會設定 isNetworkOperationInProgress
  } else {
    // [修改] 將 mqttClient.loop() 移至主迴圈，以獲得更好的響應性
    // mqttClient.loop();
    if (!mqttConnected) { 
      mqttConnected = true; 
      Serial.println("✅ MQTT 重新連線成功"); 
      updateIdleStatusLight();
    }
  }
}
void printConnectionStatus() {
  Serial.println("\n🌐 === 網路連線狀態 ===");
  Serial.print("📶 WiFi 狀態: "); Serial.println(wifiConnected ? "已連線" : "未連線");
  if (wifiConnected && connectedCredentialIndex >= 0) {
    Serial.print("🌐 連接網路: "); Serial.println(wifiCredentials[connectedCredentialIndex].ssid);
    Serial.print("🌐 IP 地址: "); Serial.println(WiFi.localIP());
    Serial.print("📡 訊號強度: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  }
  Serial.print("🌐 MQTT 狀態: "); Serial.println(mqttConnected ? "已連線" : "未連線");
  if (mqttConnected) {
    Serial.print("🔧 Client ID: "); Serial.println(mqttClientId);
    Serial.print("🌐 Broker: "); Serial.print(MQTT_BROKER);
    Serial.print(":"); Serial.println(MQTT_PORT);
  }
  Serial.println("\n📋 可用 WiFi 選項:");
  for (int i = 0; i < WIFI_CREDENTIAL_COUNT; i++) {
    Serial.print("  "); Serial.print(i + 1); Serial.print(". ");
    Serial.print(wifiCredentials[i].ssid);
    if (i == connectedCredentialIndex) Serial.print(" ✅ (目前連線)");
    Serial.println();
  }
  Serial.println("==========================\n");
}

// =========================================================================
/**
 * @brief 發送心跳訊息到 MQTT Broker，維持連線活躍
 */
void sendHeartbeat() {
  if (!mqttConnected || !mqttClient.connected()) {
    Serial.println("💗 心跳檢查：MQTT 未連線，跳過發送");
    return;
  }
  
  // 建立心跳訊息內容（帶上 IP、WiFi 訊號強度 RSSI 與連線的 SSID）
  String heartbeatTopic = "puffin-heartbeat";
  String ssid = (connectedCredentialIndex >= 0) ? String(wifiCredentials[connectedCredentialIndex].ssid) : "";
  String heartbeatPayload = "{\"device\":\"" + mqttClientId +
    "\",\"status\":\"alive\",\"ip\":\"" + WiFi.localIP().toString() +
    "\",\"rssi\":" + String(WiFi.RSSI()) +
    ",\"ssid\":\"" + ssid +
    "\",\"timestamp\":" + String(millis()) + "}";
  
  // 發送心跳訊息
  if (mqttClient.publish(heartbeatTopic.c_str(), heartbeatPayload.c_str())) {
    Serial.println("💗 心跳訊息已發送 -> " + heartbeatTopic);
  } else {
    Serial.println("💔 心跳訊息發送失敗，標記 MQTT 斷線以觸發重連");
    mqttConnected = false;  // [v1.6.0] 發送失敗視為斷線，下一輪連線檢查（≤10 秒）立即重連
  }
}

// =========================================================================
/**
 * @brief 更新待機狀態燈光：根據網路連線狀態決定燈光顯示
 */
void updateIdleStatusLight() {
  // 開機進度燈顯示期間，不干擾待機燈邏輯
  if (bootInProgress) {
    return;
  }
  // 互動燈光模式進行中，不讓待機燈蓋掉燈效（連線檢查等處會呼叫本函式）
  if (interactiveState != INTERACTIVE_IDLE) {
    return;
  }
  // 只在 IDLE 狀態且沒有正在顯示燈光時才更新待機狀態燈
  if (currentState != STATE_IDLE || isLightOn) {
    return;
  }
  
  bool shouldShowLight = wifiConnected && mqttConnected;
  
  if (shouldShowLight && !isIdleStatusLight) {
    // 需要開啟待機狀態燈
    showIdleStatusLight();
    isIdleStatusLight = true;
    Serial.println("💡 待機狀態燈已開啟 (網路連線正常)");
  } else if (!shouldShowLight && isIdleStatusLight) {
    // 需要關閉待機狀態燈
    turnOffLeds();
    isIdleStatusLight = false;
    Serial.println("💡 待機狀態燈已關閉 (網路連線異常)");
  }
}

/**
 * @brief 顯示待機狀態燈：50% 亮度的白色
 */
void showIdleStatusLight() {
  // 50% 亮度的白色 (255 * 0.5 = 127)
  CRGB idleColor = CRGB(127, 127, 127);
  fill_solid(leds, NUM_LEDS, idleColor);
  FastLED.show();
}

// =========================================================================
// 互動燈光模式（由 App 透過 puffin-control 指令驅動，與 NFC 流程完全獨立）
/**
 * @brief MQTT 訊息回呼：只分流處理 puffin-control 的互動模式指令
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != MQTT_CONTROL_TOPIC) return;

  // payload 沒有結尾 \0，須依 length 逐字組字串
  String cmd = "";
  for (unsigned int i = 0; i < length; i++) cmd += (char)payload[i];
  Serial.print("🎛️ 收到互動模式指令: "); Serial.println(cmd);

  // INTERACTIVE_EXIT 優先權最高：不檢查目前狀態，任何時候收到都立刻生效（主持人緊急退出鍵）
  if (cmd == "INTERACTIVE_EXIT") {
    exitInteractiveMode("🚪 離開互動模式，恢復 NFC 待機");
  }
  // INTERACTIVE_START 刻意不檢查狀態：MQTT 無送達保證，App 端允許主持人重按補送
  else if (cmd == "INTERACTIVE_START") {
    resetNfcStateMachine();
    turnOffLeds();
    isIdleStatusLight = false;
    interactiveState = INTERACTIVE_WAITING_MODE1;
    Serial.println("🎛️ 已進入互動模式，燈已全暗，等待「模式一」指令（NFC 掃描暫停）");
  }
  else if (cmd == "MODE1_START" && interactiveState == INTERACTIVE_WAITING_MODE1) {
    interactiveState = INTERACTIVE_MODE1_RUNNING;
    mode1StartTime = millis();
    mode1LastBrightness = 255;  // 設成不可能的初始值，確保第一幀就更新
    Serial.println("💡 模式一開始：8 秒白光漸亮漸暗");
  }
  else if (cmd == "MODE2_START" && interactiveState == INTERACTIVE_WAITING_MODE2) {
    interactiveState = INTERACTIVE_MODE2_RUNNING;
    mode2StartTime = millis();
    mode2LastChangeTime = 0; mode2NextInterval = 0;  // 一進入就馬上點亮第一顆
    mode2LastFadeTime = 0;
    mode2BurstShown = false;
    Serial.println("✨ 模式二開始：7 秒蓄力爆發（星光加速 → 閃白定格全亮）");
  }
  // 其餘狀態下收到不該收到的指令，直接忽略
}

/**
 * @brief 進入互動模式前重置 NFC 狀態機，避免退出後接續跑到一半的舊狀態
 */
void resetNfcStateMachine() {
  isLightOn = false;
  isInCooldown = false;
  lastTagState = false;
  changeSystemState(STATE_IDLE);
}

/**
 * @brief 退出互動模式：關燈、回到一般待機、恢復 NFC 掃描與待機狀態燈
 */
void exitInteractiveMode(const char* reason) {
  turnOffLeds();
  interactiveState = INTERACTIVE_IDLE;
  isIdleStatusLight = false;
  Serial.println(reason);
  updateIdleStatusLight();  // 網路正常時恢復 50% 白色待機燈
}

/**
 * @brief 主迴圈的互動模式處理：非阻塞計時，等待狀態不做事
 */
void handleInteractiveMode() {
  switch (interactiveState) {
    case INTERACTIVE_MODE1_RUNNING:
      runMode1Animation();
      if (millis() - mode1StartTime >= MODE1_DURATION) {
        turnOffLeds();
        interactiveState = INTERACTIVE_WAITING_MODE2;
        Serial.println("💡 模式一結束，等待「模式二」指令");
      }
      break;

    case INTERACTIVE_MODE2_RUNNING:
      runMode2RandomLeds();
      if (millis() - mode2StartTime >= MODE2_DURATION) {
        exitInteractiveMode("✨ 模式二結束，自動恢復 NFC 待機");
      }
      break;

    // INTERACTIVE_WAITING_MODE1 / INTERACTIVE_WAITING_MODE2：純等待指令
    default:
      break;
  }
}

/**
 * @brief 模式一燈效：0~5 秒白光線性漸亮到 50%，5~8 秒線性漸暗回全暗
 */
void runMode1Animation() {
  unsigned long elapsed = millis() - mode1StartTime;
  uint8_t brightness;

  if (elapsed <= MODE1_FADE_IN_TIME) {
    brightness = map(elapsed, 0, MODE1_FADE_IN_TIME, 0, MODE1_PEAK_BRIGHTNESS);
  } else {
    unsigned long fadeElapsed = elapsed - MODE1_FADE_IN_TIME;
    unsigned long fadeOutTime = MODE1_DURATION - MODE1_FADE_IN_TIME;
    if (fadeElapsed > fadeOutTime) fadeElapsed = fadeOutTime;
    brightness = map(fadeElapsed, 0, fadeOutTime, MODE1_PEAK_BRIGHTNESS, 0);
  }

  // 亮度沒變就不重送燈條資料，減少不必要的 FastLED.show()
  if (brightness != mode1LastBrightness) {
    fill_solid(leds, NUM_LEDS, CRGB(brightness, brightness, brightness));
    FastLED.show();
    mode1LastBrightness = brightness;
  }
}

/**
 * @brief 板載狀態指示 LED：依「異常 > 互動模式 > 待機」優先序決定顯示
 */
void updateStatusLed() {
  if (!wifiConnected || !mqttConnected) {
    statusLedBlink(STATUS_LED_ERROR_BLINK);        // 連線異常：快閃
  } else if (interactiveState != INTERACTIVE_IDLE) {
    statusLedBlink(STATUS_LED_INTERACTIVE_BLINK);  // 互動模式：慢閃
  } else if (!statusLedOn) {
    digitalWrite(STATUS_LED_PIN, HIGH);            // 待機掃描模式：恆亮
    statusLedOn = true;
  }
}

/**
 * @brief 以指定半週期非阻塞閃爍板載 LED
 */
void statusLedBlink(unsigned long halfPeriod) {
  unsigned long now = millis();
  if (now - statusLedLastToggle >= halfPeriod) {
    statusLedOn = !statusLedOn;
    digitalWrite(STATUS_LED_PIN, statusLedOn ? HIGH : LOW);
    statusLedLastToggle = now;
  }
}

/**
 * @brief 模式二燈效：7 秒蓄力爆發
 *        0~6 秒：隨機星光（殘影漸暗）點亮速度與密度隨時間加速，從稀疏慢閃到全條狂閃
 *        6~7 秒：全條閃白定格全亮，做出魔法蓄力到爆發的效果
 */
void runMode2RandomLeds() {
  unsigned long now = millis();
  unsigned long elapsed = now - mode2StartTime;

  // 爆發階段：全條閃白定格，維持到模式結束
  if (elapsed >= MODE2_BURST_TIME) {
    if (!mode2BurstShown) {
      fill_solid(leds, NUM_LEDS, CRGB::White);
      FastLED.show();
      mode2BurstShown = true;
      Serial.println("💥 蓄力完成，全條閃白定格！");
    }
    return;
  }

  // 蓄力階段：全體逐步淡出，做出殘影漸暗的星光尾巴
  if (now - mode2LastFadeTime >= MODE2_FADE_INTERVAL) {
    fadeToBlackBy(leds, NUM_LEDS, MODE2_FADE_AMOUNT);
    FastLED.show();
    mode2LastFadeTime = now;
  }

  // 點亮新星光：間隔隨時間線性縮短、每次點亮顆數隨時間增加（CHSV 全飽和，顏色鮮豔）
  if (now - mode2LastChangeTime >= mode2NextInterval) {
    int sparksThisTick = 1 + (int)((elapsed * MODE2_MAX_SPARKS_PER_TICK) / MODE2_BURST_TIME);
    for (int s = 0; s < sparksThisTick; s++) {
      leds[random(0, NUM_LEDS)] = CHSV(random(0, 256), 255, 255);
    }
    FastLED.show();
    mode2LastChangeTime = now;
    unsigned long baseInterval = MODE2_SPARK_INTERVAL_START
      - (elapsed * (MODE2_SPARK_INTERVAL_START - MODE2_SPARK_INTERVAL_END)) / MODE2_BURST_TIME;
    mode2NextInterval = random(baseInterval / 2, baseInterval + 1);  // 帶點隨機抖動，閃得更自然
  }
}
