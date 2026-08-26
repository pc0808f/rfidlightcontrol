# NFC RFID 燈光控制系統 - 版本記錄

## 版本控制說明

本文件記錄 NFC RFID 燈光控制系統的版本變更歷史，所有備份檔案都保存在專案根目錄下。

---

## 版本歷史

### v1.6.0 - 穩定性強化版 ✅
**檔案**: `rgidlightcontrol/rgidlightcontrol.ino` (現行主程式)
**日期**: 2025-08-28
**狀態**: 連線與讀卡穩定性全面強化（已用 arduino-cli 編譯驗證，1.08MB / 51%）

#### 🐛 Bug 修正（重要）
- **WiFi 永遠離線 Bug**: 原本開機時所有 WiFi 都連不上的話，之後永遠不會再重試
  （重連邏輯只在「曾連線過」時觸發）。現改為離線時每 60 秒定期重試。
- **非 NDEF 卡片當機 Bug**: 空白卡 / 一般感應卡靠近時，`getNdefMessage()` 會解參考
  NULL 指標導致 ESP32 當機重啟。現先以 `hasNdefMessage()` 檢查。

#### 連線穩定性強化
- `WiFi.setAutoReconnect(true)`：底層短暫斷線自動重連，不必等 10 秒檢查週期
- 執行期重連優先嘗試「上次成功的 SSID」，避免每次都從頭掃 4 組帳密（最壞阻塞 60 秒）
- 心跳發送失敗即標記 `mqttConnected = false`，下一輪檢查（≤10 秒）立即重連

#### 讀卡穩定性強化
- `tagPresent(100)`：原預設每次輪詢可阻塞近 1 秒，拖慢主迴圈與 MQTT keepalive；
  縮短為 100ms 後迴圈更順、讀卡反應更快
- 讀取失敗先快速重試 2 次（間隔 100ms）再進冷卻，提升卡片邊緣的一次刷成功率
- 連續讀取失敗 5 次自動重新初始化 PN532（防模組卡死，免人工斷電）
- `getPayloadAsString()` 防 payload 長度為 0 的越界讀取

#### 新增參數
```cpp
const unsigned long WIFI_RETRY_INTERVAL = 60000;  // WiFi 離線定期重試間隔
const unsigned long NFC_POLL_TIMEOUT = 100;       // tagPresent 輪詢逾時 (ms)
const int NFC_READ_ATTEMPTS = 3;                  // 讀取嘗試次數（1 + 2 次重試）
const int NFC_MAX_CONSECUTIVE_FAILURES = 5;       // 連續失敗達此數重新初始化 PN532
```

---

### v1.5.9 - 開機進度燈版 ✅
**檔案**: `rgidlightcontrol/rgidlightcontrol.ino` (現行主程式)
**日期**: 2025-08-28
**狀態**: 新增彩色開機進度指示燈、強化心跳訊息，並整理程式碼與文件

#### v1.5.9 新增 / 變更項目
- 🌈 **彩色開機進度燈**: FastLED 初始化後，依開機階段以約 20% 亮度顯示不同顏色，方便診斷卡在哪一步
  - 🔴 紅：系統 / FastLED 基礎初始化完成
  - 🟠 橙：I2C 匯流排就緒
  - 🟡 黃：NFC (PN532) 就緒
  - 🟢 綠：WiFi 連線成功
  - 🔵 藍：MQTT 連線成功
  - 🟣 紫：系統啟動完成
  - 🌈 彩虹：最後閃示彩虹燈代表「開機成功」後進入待機
- 💗 **心跳訊息強化**: `puffin-heartbeat` 除了 device/status/timestamp，再帶上 `ip`、`rssi`(訊號強度)、`ssid`
- 🌐 **MQTT Broker 位址**: 由 `broker.MQTTGO.io` 改為 `MQTTGO.io` (port 1883 不變)
- 🔌 **改善閒置斷線**: WiFi 連上後呼叫 `WiFi.setSleep(false)` 關閉省電模式；MQTT keepalive 由 15 秒拉長至 60 秒；心跳間隔由 60 秒改為 30 秒 (< keepalive)
- 🧹 **程式碼整理**:
  - 移除第 362–363 行「其餘函數省略」的誤導性註解
  - `printStatistics()` 接上呼叫（冷卻結束時輸出累計統計）
  - `showStartupRainbow()` 保留，改作為開機成功指示
- 📄 **文件同步**: 更新 `CLAUDE.md`，使硬體/參數/狀態機/函式庫說明與 v1.5.9 一致

#### 心跳訊息格式範例
```json
{"device":"ESP32_XXXX","status":"alive","ip":"192.168.1.50","rssi":-58,"ssid":"Sam","timestamp":123456}
```

#### 開機進度燈相關參數
```cpp
const unsigned long BOOT_STAGE_DELAY = 400;       // 每個階段顯示時間
const unsigned long STARTUP_LIGHT_DURATION = 1000; // 開機成功彩虹燈時間
bool bootInProgress = true;                        // 開機進度燈顯示旗標
```

> 備註：v1.5.2 ~ v1.5.8 期間的變更（如 v1.5.7 MQTT 重連保護、v1.5.8 讀取靈敏度優化）
> 未各自建立獨立備份檔，相關說明可參考主程式檔頭的更新日誌。

---

### v1.5.1 - 多 WiFi 帳密支援版本 ✅
**檔案**: `rgidlightcontrol_v1.5.1_backup.ino`  
**日期**: 2025-08-26  
**狀態**: 改進 WiFi 連線功能，支援多個帳密自動嘗試  

#### v1.5.1 改進項目
- 🔗 **多 WiFi 支援**: 自動依序嘗試 4 組 WiFi 帳密
- 📶 **預設帳密**: 內建 Hpees_internal、Sam、Sam&Betty 三組帳密
- 🔄 **自動連線**: 依序嘗試直到找到可用網路，無需手動設定
- ⏱️ **連線超時**: 每個網路 15 秒超時保護
- 📋 **詳細狀態**: 顯示嘗試進度和連線結果
- 🔧 **自定義選項**: 保留自定義 WiFi 設定選項

#### WiFi 帳密配置
```cpp
const WiFiCredential wifiCredentials[] = {
  {"Hpees_internal", "Hpees2733"},           // 選項 1
  {"Sam", "0928666624"},                     // 選項 2  
  {"Sam&Betty", "0928666624"},               // 選項 3
  {"YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"}   // 自定義選項
};
```

#### 連線流程改進
1. **依序嘗試**: 按順序嘗試每個 WiFi 網路
2. **即時反饋**: 顯示嘗試進度 (1/4, 2/4 等)
3. **快速切換**: 連線失敗立即嘗試下一個
4. **狀態記錄**: 記錄成功連線的網路索引
5. **詳細報告**: 顯示所有可用選項和目前連線狀態

#### 錯誤處理
- ❌ 單一網路連線失敗：自動嘗試下一個
- 🔄 所有網路連線失敗：顯示排除指南
- 🔧 連線中斷：自動重連到之前成功的網路
- 📋 狀態追蹤：詳細的連線狀態和可用選項顯示

**優點**: 無需修改程式就能在多個環境中使用，大大提高了便利性！

---

### v1.5 - MQTT 連線功能版本 ✅
**檔案**: `rgidlightcontrol_v1.5_backup.ino`  
**日期**: 2025-08-26  
**狀態**: 完成 plan.md 步驟 8，新增 WiFi 和 MQTT 連線功能  

#### 步驟 8 主要功能
- 📶 **WiFi 連線管理**: 自動連接指定的 WiFi 網路，支援連線超時保護
- 🌐 **MQTT Broker 連線**: 連接到 broker.MQTTGO.io:1883
- 🔧 **智慧 Client ID**: 使用 ESP32 MAC 地址生成唯一的 Client ID  
- 📡 **連線狀態監控**: 每 30 秒檢查連線狀態，自動重連功能
- 🔄 **系統整合**: 完整整合到現有狀態機系統

#### 新增 MQTT 參數
```cpp
const char* MQTT_BROKER = "broker.MQTTGO.io";
const int MQTT_PORT = 1883;
const unsigned long WIFI_CONNECT_TIMEOUT = 10000;    // 10秒
const unsigned long MQTT_CONNECT_TIMEOUT = 5000;     // 5秒
const unsigned long CONNECTION_CHECK_INTERVAL = 30000; // 30秒
```

#### 新增核心函數
- `getMacAddress()`: 獲取 ESP32 MAC 地址作為 MQTT Client ID
- `connectToWiFi()`: WiFi 網路連線管理
- `connectToMQTT()`: MQTT broker 連線處理
- `checkMQTTConnection()`: 連線狀態檢查和自動重連
- `printConnectionStatus()`: 網路連線狀態報告

#### 連線流程
1. **系統啟動**: 顯示彩虹燈光後開始網路連線
2. **WiFi 連線**: 嘗試連接設定的 WiFi 網路 (10秒超時)
3. **MQTT 連線**: WiFi 成功後連接 MQTT broker (5秒超時)
4. **狀態監控**: 主迴圈每 30 秒檢查連線狀態
5. **自動重連**: 檢測到斷線時自動嘗試重新連線

#### 狀態指示
- ✅ 成功連線：顯示 IP 地址、訊號強度、Client ID
- ❌ 連線失敗：顯示錯誤訊息和排除建議  
- ⚠️  連線中斷：自動檢測並嘗試重連
- 🔄 重連中：顯示重連進度和狀態

**重要**: 使用前請修改程式中的 `WIFI_SSID` 和 `WIFI_PASSWORD` 為實際的網路設定

---

### v1.4 - 完整流程整合版本 ✅
**檔案**: `rgidlightcontrol_v1.4_backup.ino`  
**日期**: 2025-08-26  
**狀態**: 完成 plan.md 步驟 6，狀態機完整整合版本  

#### 步驟 6 主要改進
- 🔄 **狀態機設計**: 實現五狀態機系統 (IDLE → TAG_DETECTED → PROCESSING → LIGHT_ON → COOLDOWN)
- 📊 **狀態管理**: 完整的系統狀態追蹤和切換機制
- 📋 **流程整合**: 統一標籤檢測→資料解析→驗證→燈光控制流程
- 📈 **統計功能**: 新增標籤處理統計 (總數、成功率、失敗次數)
- 🔧 **調試增強**: 詳細的狀態輸出和 emoji 圖示系統

#### 狀態機架構
```cpp
enum SystemState {
  STATE_IDLE,          // 待機狀態
  STATE_TAG_DETECTED,  // 標籤已檢測
  STATE_PROCESSING,    // 處理中
  STATE_LIGHT_ON,      // 燈光開啟
  STATE_COOLDOWN       // 冷卻中
};
```

#### 新增核心函數
- `changeSystemState()`: 狀態切換管理
- `handleIdleState()`: 待機狀態處理
- `handleTagDetectedState()`: 標籤檢測處理
- `handleProcessingState()`: 資料處理狀態
- `handleLightOnState()`: 燈光狀態管理
- `handleCooldownState()`: 冷卻狀態處理
- `processTagData()`: 統一標籤資料處理
- `printSystemStatus()`: 系統狀態報告
- `printStatistics()`: 統計資訊輸出

#### 流程改善
- **清晰的狀態轉換**: 每個狀態都有明確的進入和退出條件
- **統一錯誤處理**: 集中化的錯誤處理和狀態恢復機制
- **強化調試輸出**: emoji 圖示和結構化的狀態資訊
- **性能統計**: 實時追蹤處理成功率和系統運行狀態

---

### v1.3 - 完整燈光控制版本 ✅
**檔案**: `rgidlightcontrol_v1.3_backup.ino`  
**日期**: 2025-08-26  
**狀態**: 完成 plan.md 步驟 1-5，穩定版本  

#### 修正項目
- 🔧 **關燈機制修正**: 修正 5 秒後不關燈或延遲關燈的問題
- 🔧 **非阻塞式設計**: 移除阻塞性 `delay()` 改用時間戳記檢查
- 🔧 **頻繁狀態更新**: 每 100ms 檢查燈光狀態，確保準確關燈

#### 技術改進
- 新增非阻塞式冷卻機制，取代原本的 `delay(delayTime)`
- 主迴圈持續檢查燈光狀態，不受標籤讀取延遲影響
- 冷卻期間仍可正常執行自動關燈檢查

#### 新增變數
```cpp
bool isInCooldown = false;           // 冷卻狀態標記
unsigned long cooldownStartTime = 0; // 冷卻開始時間
unsigned long cooldownDuration = 0;  // 冷卻持續時間
```

#### 運作改善
- **精準計時**: 燈光在 5 秒後準確關閉
- **即時響應**: 燈光期間新卡片立即切換 (500ms 冷卻)
- **持續監控**: 每 100ms 檢查狀態，不漏失關燈時機

---

### v1.2 - 燈光控制版本
**檔案**: 已整合到 v1.3 主程式  
**日期**: 2025-08-26  
**狀態**: 實作 plan.md 步驟 5 - 燈光控制邏輯  

#### 實作功能
- ✅ **步驟 5**: 燈光控制邏輯
  - Record2 控制燈光顏色 (01=藍/02=綠/03=紅/04=黃)
  - Record3 作為暫存資料
  - 支援燈光期間新卡片立即切換
  - 自動關燈機制 (存在延遲問題，已在 v1.3 修正)

#### 燈光控制邏輯
```cpp
switch (record2Number) {
  case 1: color = CRGB::Blue;   familyName = "天空家族"; break;
  case 2: color = CRGB::Green;  familyName = "海洋家族"; break;
  case 3: color = CRGB::Red;    familyName = "樹居家族"; break;
  case 4: color = CRGB::Yellow; familyName = "大地家族"; break;
}
```

#### 新增狀態變數
```cpp
bool isLightOn = false;           // 燈光狀態追蹤
unsigned long lightStartTime = 0; // 點燈時間記錄
```

---

### v1.1 - 格式修正版本
**檔案**: `rgidlightcontrol_v1.1_backup.ino`  
**日期**: 2025-08-26  
**狀態**: 修正 Record 格式驗證問題  

#### 修正項目
- 🔧 **格式支援**: 支援無 "en" 前綴的 Record 格式驗證
  - Record2: 支援 `"02"` 和 `"en02"` 格式
  - Record3: 支援 `"009"` 和 `"en009"` 格式
- 🔧 **新增智慧函數**: `extractNumberSmart()` 自動識別格式
- 🔧 **參數調整**: `READ_DEBOUNCE` 調整為 3000ms (3秒)

#### 技術改進
- 新增 `extractNumberSmart()` 函數，智慧識別有無 "en" 前綴
- `validateRecord2()` 支援 "00~04" 或 "en00~en04" 格式
- `validateRecord3()` 支援 "001~120" 或 "en001~en120" 格式
- 更完善的錯誤處理和格式相容性

#### 測試驗證
- Record2: `"02"` → 數字 `2` ✅ (海洋家族)
- Record3: `"009"` → 數字 `9` ✅ (暫存資料)

#### 參數變更
```cpp
const unsigned long READ_DEBOUNCE = 3000;      // 調整為 3 秒
```

---

### v1.0 - 初始功能版本
**檔案**: `rgidlightcontrol_v1_backup.ino`  
**日期**: 2025-08-26  
**狀態**: 已完成 plan.md 步驟 1-4  

#### 實作功能
- ✅ **步驟 1**: FastLED 燈條控制功能
- ✅ **步驟 2**: 開機彩虹燈光指示 (可調整時間參數)
- ✅ **步驟 3**: 可調整參數設定 (燈光持續時間、讀取延遲等)
- ✅ **步驟 4**: Record 資料解析和驗證 (en00~en04, en001~en120)

#### 主要特色
- 徹底移除對 Adafruit 函式庫的依賴，只使用 Don Coleman 的 NDEF 函式庫
- 手動初始化 I2C 匯流排，確保 PN532 晶片穩定運作
- 完整的 Record 資料驗證機制
- 智慧化的讀取延遲時間 (成功1.5秒，失敗1秒)
- 開機彩虹燈光指示

#### 參數設定
```cpp
const unsigned long LIGHT_DURATION = 5000;     // 燈光持續時間
const unsigned long READ_DEBOUNCE = 1500;      // 讀取成功延遲
const unsigned long READ_FAIL_DEBOUNCE = 1000; // 讀取失敗延遲
const unsigned long STARTUP_LIGHT_DURATION = 1000; // 開機燈光時間
```

---

## 開發計畫 (plan.md)

### 已完成步驟
- [x] **步驟 1**: FastLED 燈條控制功能
- [x] **步驟 2**: 開機彩虹燈光指示
- [x] **步驟 3**: 可調整參數設定
- [x] **步驟 4**: Record 資料解析和驗證
- [x] **步驟 5**: 燈光控制邏輯
- [x] **步驟 6**: 整合完整的標籤處理流程
- [x] **步驟 8**: MQTT 連線功能

### 規劃中步驟
- [ ] **步驟 9**: MQTT 資料發送功能
- [ ] **步驟 7**: 最終測試和優化 (於 MQTT 功能完成後進行)

---

## 函式庫依賴

### 核心函式庫
```cpp
#include <Wire.h>           // I2C 通訊
#include <SPI.h>            // 備用 SPI 通訊
#include <PN532_I2C.h>      // Don Coleman PN532 I2C
#include <NfcAdapter.h>     // Don Coleman NDEF 函式庫
#include <FastLED.h>        // LED 燈條控制
#include <WiFi.h>           // WiFi 連線功能 (步驟 8)
#include <PubSubClient.h>   // MQTT 通訊功能 (步驟 8)
```

### 函式庫安裝需求
- **NDEF library for Arduino by Don Coleman**: NFC 標籤讀取
- **FastLED library**: LED 燈條控制  
- **PubSubClient library**: MQTT 通訊 (步驟 8 新增)
- **WiFi library**: ESP32 內建 WiFi 功能

---

## 硬體配置

### ESP32 接腳配置
- **I2C**: SDA=21, SCL=22
- **LED 燈條**: GPIO 12 (WS2812B, 100顆LED)
- **NFC 讀取器**: PN532 模組 (I2C 模式)

### 燈條設定
```cpp
#define DATA_PIN    12
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    100
#define BRIGHTNESS  100
```

### 網路設定 (步驟 8)
- **WiFi**: 支援 2.4GHz 網路
- **MQTT Broker**: broker.MQTTGO.io:1883
- **Client ID**: 使用 ESP32 MAC 地址自動生成

#### 使用前設定 (v1.5.1 更新)
**內建帳密 (直接使用)**：
- Hpees_internal : Hpees2733
- Sam : 0928666624  
- Sam&Betty : 0928666624

**自定義設定 (如需要)**：
修改程式中 wifiCredentials 陣列的最後一項：
```cpp
{"YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD"}  // 替換為實際帳密
```

---

## 測試用 NFC 標籤格式

### 目前測試標籤
- **Record 1**: `https://stu.hpees.tp.edu.tw/` (URI)
- **Record 2**: `"02"` → 海洋家族 (綠色燈光)
- **Record 3**: `"009"` → 暫存資料

### 支援格式
- **Record 2**: `"00"~"04"` 或 `"en00"~"en04"` (燈光控制)
- **Record 3**: `"001"~"120"` 或 `"en001"~"en120"` (暫存資料)

---

## 下一步開發

1. **整合測試**: 完整流程測試和穩定性驗證
2. **MQTT 整合**: 實作 WiFi 連線和 MQTT 資料發送
3. **效能最佳化**: 改善讀取頻率和回應速度
4. **錯誤處理**: 強化異常情況處理機制

---

*最後更新: 2025-08-26*