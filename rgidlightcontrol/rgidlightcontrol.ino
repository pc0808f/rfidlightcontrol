/**
 * NFC RFID 燈光控制系統 - v1.5.8 靈敏度優化版
 * * 版本資訊: v1.5.8 - 優化讀取靈敏度和 Record 不足處理
 * 建立日期: 2025-08-28
 * 更新內容: 
 * - v1.5.7: 新增 MQTT 重連保護機制，防止狀態機重置
 * - v1.5.8: 縮短 Record 不足的冷卻時間，提升讀取靈敏度
 * - 優化標籤穩定性檢測，加快響應速度
 * - 區分不同類型錯誤的冷卻時間
 * * 已實作功能：
 * 1. FastLED 燈條控制功能
 * 2. 開機彩虹燈光指示
 * 3. 可調整參數設定
 * 4. Record 資料解析和驗證
 * 5. 燈光控制邏輯
 * 6. 完整的標籤處理流程
 * 7. WiFi 和 MQTT 連線功能
 * 8. MQTT 資料發送功能 (puffin-test)
 * 9. WiFi 連線成功指示燈 (粉紅色) [新功能]
 * 10. MQTT 即時斷線重連機制 [新功能]
 * * v1.5.8 新增項目：
 * - 🔧 新增 MQTT 重連保護機制，防止干擾 NFC 檢測
 * - ⏱️  縮短標籤穩定檢測時間為 100ms，提升響應速度
 * - 🛡️  區分 Record 不足和其他錯誤的冷卻時間
 * - 🚀 Record 不足時僅冷卻 1 秒，快速重新感應
 * * 功能特色：
 * - 徹底移除對 Adafruit 函式庫的依賴，只使用 Don Coleman 的 NDEF 函式庫
 * - 手動初始化 I2C 匯流排，確保 PN532 晶片穩定運作
 * - 完整的 Record 資料驗證機制，支援多種格式
 * - 非阻塞式狀態管理，優先保證燈光準時熄滅
 * - 開機/WiFi 連線成功燈光指示
 * - WiFi 和 MQTT 強韌連線管理
 * * 前置作業：
 * - 請確保已安裝 "NDEF library for Arduino by Don Coleman"
 * - 請確保已安裝 "FastLED library"
 * - 請確保已安裝 "PubSubClient library"
 * - 請在程式中設定正確的 WiFi SSID 和密碼
 */

#include <Wire.h>
#include <SPI.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
#include <FastLED.h>
#include <WiFi.h>          // WiFi 連線功能
#include <PubSubClient.h>  // MQTT 通訊功能

// --- 可調整參數設定 ---
const unsigned long LIGHT_DURATION = 15000;
const unsigned long READ_DEBOUNCE = 3000;
const unsigned long READ_FAIL_DEBOUNCE = 3000;  // 一般讀取失敗冷卻時間
const unsigned long RECORD_INSUFFICIENT_DEBOUNCE = 1000;  // Record 不足冷卻時間（更短）
const unsigned long STARTUP_LIGHT_DURATION = 1000;

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
const char* MQTT_BROKER = "broker.MQTTGO.io";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "puffin-test";
const unsigned long WIFI_CONNECT_TIMEOUT = 15000;
const unsigned long MQTT_CONNECT_TIMEOUT = 5000;
const unsigned long CONNECTION_CHECK_INTERVAL = 10000;
const unsigned long MQTT_KEEPALIVE_SECONDS = 15;
const unsigned long HEARTBEAT_INTERVAL = 60000;  // 心跳間隔 60 秒

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
unsigned long lastHeartbeat = 0;

// --- 函數宣告 ---
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

// =========================================================================
void setup(void) {
  Serial.begin(115200);
  Serial.println("\nNFC RFID 燈光控制系統 v1.5.8 (靈敏度優化版)");
  Serial.println("=========================================================");
  Serial.println("新增: MQTT 保護機制 & 讀取靈敏度優化");
  Serial.println("=========================================================");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("I2C 匯流排已於 GPIO 21 (SDA) 和 22 (SCL) 啟動。");
  delay(400); 
  nfcAdapter.begin();
  
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setDither(BRIGHTNESS < 255);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.println("FastLED 燈條已初始化完成。");
  
  showStartupRainbow();
  
  Serial.println("\n📶 正在初始化網路連線...");
  mqttClientId = getMacAddress();
  Serial.print("🔧 MQTT Client ID: "); Serial.println(mqttClientId);
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(MQTT_KEEPALIVE_SECONDS);  // 設定 MQTT keep-alive 為 15 秒
  Serial.print("🌐 MQTT Broker: "); Serial.print(MQTT_BROKER);
  Serial.print(":"); Serial.println(MQTT_PORT);
  Serial.print("🔧 MQTT Keep-Alive: "); Serial.print(MQTT_KEEPALIVE_SECONDS); Serial.println(" 秒");
  
  connectToWiFi();
  
  if (wifiConnected) {
    connectToMQTT();
  }
  
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
  
  switch (currentState) {
    case STATE_IDLE:         handleIdleState();        break;
    case STATE_TAG_DETECTED: handleTagDetectedState(); break;
    case STATE_PROCESSING:   handleProcessingState();  break;
    case STATE_LIGHT_ON:     handleLightOnState();     break;
    case STATE_COOLDOWN:     handleCooldownState();    break;
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
  wifiConnected = false; connectedCredentialIndex = -1;
  for (int i = 0; i < WIFI_CREDENTIAL_COUNT; i++) {
    Serial.print("\n🔄 嘗試連接 ("); Serial.print(i + 1); Serial.print("/");
    Serial.print(WIFI_CREDENTIAL_COUNT); Serial.print("): "); Serial.println(wifiCredentials[i].ssid);
    WiFi.disconnect(false); delay(100);
    WiFi.begin(wifiCredentials[i].ssid, wifiCredentials[i].password);
    wifiConnectStartTime = millis();
    while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED && (millis() - wifiConnectStartTime < WIFI_CONNECT_TIMEOUT)) {
      delay(500); Serial.print(".");
      if (WiFi.status() == WL_DISCONNECTED && (millis() - wifiConnectStartTime > 5000)) {
        Serial.print("⚠️重置"); WiFi.disconnect(false); delay(500);
        WiFi.begin(wifiCredentials[i].ssid, wifiCredentials[i].password);
        wifiConnectStartTime = millis();
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true; connectedCredentialIndex = i;
      Serial.println("\n✅ WiFi 連線成功！");
      Serial.print("🌐 已連接網路: "); Serial.println(wifiCredentials[i].ssid);
      Serial.print("🌐 IP 地址: "); Serial.println(WiFi.localIP());
      Serial.print("📡 訊號強度: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");

      // [新功能] 呼叫 WiFi 連線成功指示燈
      showWiFiConnectedLight();
      
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

// ... (其餘所有函數保持不變，為節省篇幅，此處省略) ...
// (All other functions remain unchanged)

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
    changeSystemState(STATE_IDLE);
    // 冷卻結束，恢復待機狀態燈
    updateIdleStatusLight();
  }
}
int processTagData() {
  NfcTag tag = nfcAdapter.read();
  Serial.print("📱 標籤類型: "); Serial.println(tag.getTagType());
  NdefMessage message = tag.getNdefMessage();
  if (message.getRecordCount() <= 0) {
    Serial.println("❌ 標籤不包含 NDEF 格式的訊息，或讀取失敗");
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
void showStartupRainbow() {
  Serial.print("顯示開機彩虹燈光指示 ("); Serial.print(STARTUP_LIGHT_DURATION); Serial.println(" 毫秒)...");
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
  int payloadLength = record.getPayloadLength(); byte payload[payloadLength];
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
  
  bool currentTagState = nfcAdapter.tagPresent();
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
  if (!nfcAdapter.tagPresent()) {
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
    } else {
      Serial.print("❌ 連線失敗，錯誤代碼: "); Serial.print(mqttClient.state());
      Serial.println("，2 秒後重試..."); delay(2000);
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
      connectToWiFi();  // 此函數內會設定 isNetworkOperationInProgress
      if (wifiConnected) connectToMQTT();  // 此函數內也會設定標記
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
  
  // 建立心跳訊息內容
  String heartbeatTopic = "puffin-heartbeat";
  String heartbeatPayload = "{\"device\":\"" + mqttClientId + "\",\"status\":\"alive\",\"timestamp\":" + String(millis()) + "}";
  
  // 發送心跳訊息
  if (mqttClient.publish(heartbeatTopic.c_str(), heartbeatPayload.c_str())) {
    Serial.println("💗 心跳訊息已發送 -> " + heartbeatTopic);
  } else {
    Serial.println("💔 心跳訊息發送失敗");
  }
}

// =========================================================================
/**
 * @brief 更新待機狀態燈光：根據網路連線狀態決定燈光顯示
 */
void updateIdleStatusLight() {
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
