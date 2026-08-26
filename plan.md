# NFC RFID 燈光控制系統 - 實作計畫

## 專案概述

基於 Don Coleman NDEF 函式庫的 NFC 標籤讀取器，實現燈光控制功能

## 硬體配置

- **開發板**: ESP32 (WeMos D1 R32)
- **NFC 讀取器**: PN532 模組 (I2C: SDA=21, SCL=22)
- **LED 燈條**: WS2812B，100 顆 LED，連接到 pin 12
- **燈條設定**: GRB 色彩順序，亮度 100

## 實作步驟規劃

### 步驟 1: 添加 FastLED 函式庫和基本設定

**目標**: 整合 FastLED 燈條控制功能
**修改項目**:

- 添加 `#include <FastLED.h>`
- 添加 LED 相關常數定義：
  ```cpp
  #define DATA_PIN    12
  #define LED_TYPE    WS2812B
  #define COLOR_ORDER GRB
  #define NUM_LEDS    100
  #define BRIGHTNESS  100
  ```
- 宣告 LED 陣列：`CRGB leds[NUM_LEDS];`
- 在 setup() 中初始化 FastLED
- 實作基本燈光控制函數：
  - `void turnOffLeds()` - 熄滅所有燈
  - `void showColorLight(CRGB color)` - 顯示指定顏色

**測試點**: 確認 LED 燈條可正常控制和熄滅

### 步驟 2: 實作開機彩色燈光指示

**目標**: 系統啟動時顯示 1 秒彩虹燈光
**修改項目**:

- 在 setup() 的最後添加開機燈光指示
- 使用 HSV 彩虹色填充所有 LED
- 顯示 1 秒後熄滅進入待機

**測試點**: 確認開機時有彩虹色燈光指示

### 步驟 3: 添加可調整參數

**目標**: 提供快速修改的參數設定
**修改項目**:

- 添加參數常數定義：
  ```cpp
  const unsigned long LIGHT_DURATION = 5000;  // 燈光持續時間(毫秒)
  ```
- 其他可能需要的參數（防連續讀取延遲等）

**測試點**: 確認參數可正確控制燈光持續時間

### 步驟 4: 實作 Record 資料解析和驗證

**目標**: 正確解析和驗證 NFC 標籤的 Record 2 和 Record 3
**修改項目**:

- 實作數字提取函數：
  ```cpp
  int extractNumber(String data, String prefix)
  bool validateRecord2(String data)  // 驗證 en00~en04
  bool validateRecord3(String data)  // 驗證 en001~en120
  ```
- 修改 loop() 中的 Record 處理邏輯
- 添加資料驗證和錯誤處理

**測試點**: 使用測試資料確認數字提取和驗證正確

### 步驟 5: 實作燈光控制邏輯

**目標**: 根據 Record 2 的數字控制對應燈光
**修改項目**:

- 實作燈光控制函數：
  ```cpp
  void processLightControl(int record2Number, int record3Number)
  ```
- 顏色對應邏輯：
  - '01': 天空家族 (藍色) - `CRGB::Blue`
  - '02': 海洋家族 (綠色) - `CRGB::Green`
  - '03': 樹居家族 (紅色) - `CRGB::Red`
  - '04': 大地家族 (黃色) - `CRGB::Yellow`
  - '00': 不反應
- 添加燈光計時器邏輯

**測試點**: 確認各種數字對應正確的燈光顏色

### 步驟 6: 整合完整的標籤處理流程

**目標**: 完整的 NFC 標籤讀取到燈光控制流程
**修改項目**:

- 修改主 loop() 邏輯
- 整合標籤檢測、資料解析、驗證、燈光控制
- 添加狀態管理（如果需要）
- 添加防連續讀取機制

**測試點**: 完整流程測試，確認所有功能正常運作

### 步驟 7: 最終測試和優化

**目標**: 系統穩定性和效能優化
**修改項目**:

- 添加錯誤處理和異常情況處理
- 優化讀取頻率和效能
- 添加詳細的 Serial 輸出用於調試
- 代碼清理和註解完善

**測試點**: 長期穩定性測試和各種異常情況測試

### 步驟 8: 添加 MQTT 連線功能 (規劃階段)

**目標**: 整合 MQTT 通訊功能，連接到指定 broker
**規劃項目**:

- 添加 WiFi 和 MQTT 函式庫：
  ```cpp
  #include <WiFi.h>
  #include <PubSubClient.h>
  ```
- MQTT 連線參數設定：
  - Broker: `broker.MQTTGO.io`
  - Port: `1883`
  - Client ID: ESP32 MAC 地址
  - 無需用戶名和密碼
- 實作功能函數：
  ```cpp
  String getMacAddress()           // 獲取 ESP32 MAC 地址
  void connectToWiFi()             // WiFi 連線
  void connectToMQTT()             // MQTT broker 連線
  void checkMQTTConnection()       // 檢查並維持 MQTT 連線
  ```
- WiFi 連線設定 (待提供 SSID 和密碼)
- MQTT 連線成功後回報訊息
- 整合連線狀態檢查到主迴圈
- 暫時不實作 MQTT 訊息發送功能

**注意**: 此步驟目前僅為規劃，主程式暫不實作 MQTT 功能

## 函式庫依賴

```cpp
#include <Wire.h>
#include <SPI.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
#include <FastLED.h>
#include <WiFi.h>          // MQTT 功能需要
#include <PubSubClient.h>  // MQTT 功能需要
```

## 測試用 NFC 標籤格式

- Record 1: 網址 (任意)
- Record 2: "en01", "en02", "en03", "en04" (控制燈光)
- Record 3: "en001" ~ "en120" (額外資料)

## 預期功能

1. 開機彩虹燈光指示 (1 秒)
2. NFC 標籤感應和 NDEF 解析
3. Record 資料驗證
4. 對應燈光控制 (藍/綠/紅/黃)
5. 燈光自動熄滅 (5 秒後)
6. 防連續讀取保護

每個步驟都需要獨立測試確認功能正常後，才進行下一步驟。
