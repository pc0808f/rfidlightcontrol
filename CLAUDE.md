# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案概述

這是一個 Arduino wemos d1 r32 (ESP32) 專案，實現 RFID/NFC 標籤觸發 LED 燈條顯示的系統。當感應到 NFC 標籤時，會根據標籤內容顯示不同顏色的燈光，並維持 15 秒鐘後自動熄滅。並具備彩色開機進度指示燈，以及 WiFi / MQTT 連線與心跳回報功能。

## 硬體配置

- **開發板**: Arduino wemos d1 r32 (ESP32)
- **NFC 讀取器**: PN532 模組，使用 I2C (SDA=GPIO21, SCL=GPIO22)
- **LED 燈條**: WS2812B，100 顆 LED，連接到 GPIO12
- **燈條設定**: GRB 色彩順序，亮度 255

## 程式架構

主程式檔案：`rgidlightcontrol.ino`

### 可調整參數 (程式頂部)

- `LIGHT_DURATION`: 燈光持續時間 (預設 15 秒)
- `READ_DEBOUNCE`: 讀取成功後冷卻延遲 (預設 3 秒)
- `READ_FAIL_DEBOUNCE`: 一般讀取失敗冷卻時間 (預設 3 秒)
- `RECORD_INSUFFICIENT_DEBOUNCE`: Record 不足冷卻時間 (預設 1 秒)
- `TAG_STABLE_DURATION`: 標籤穩定檢測時間 (預設 100 毫秒)
- `BOOT_STAGE_DELAY`: 每個開機進度燈階段顯示時間 (預設 400 毫秒)
- `STARTUP_LIGHT_DURATION`: 開機成功彩虹燈顯示時間 (預設 1 秒)
- `WIFI_RETRY_INTERVAL`: WiFi 離線時的定期重試間隔 (預設 60 秒)
- `NFC_POLL_TIMEOUT`: tagPresent 輪詢逾時 (預設 100 毫秒)
- `NFC_READ_ATTEMPTS`: NFC 讀取嘗試次數，含快速重試 (預設 3)
- `NFC_MAX_CONSECUTIVE_FAILURES`: 連續讀取失敗達此次數即重新初始化 PN532 (預設 5)

### 狀態機系統

程式採用五狀態機設計：

1. **IDLE**: 待機狀態，檢查標籤存在、維持待機狀態燈
2. **TAG_DETECTED**: 確認標籤穩定存在
3. **PROCESSING**: 讀取並解析標籤資料、驗證並發送 MQTT
4. **LIGHT_ON**: 燈光開啟並計時，時間到自動熄滅
5. **COOLDOWN**: 冷卻狀態，防止連續讀取

### 開機進度指示燈

FastLED 初始化後，依開機階段以約 20% 亮度顯示不同顏色，方便判斷卡在哪一步：

- 🔴 紅: 系統 / FastLED 基礎初始化完成
- 🟠 橙: I2C 匯流排就緒
- 🟡 黃: NFC (PN532) 就緒
- 🟢 綠: WiFi 連線成功
- 🔵 藍: MQTT 連線成功
- 🟣 紫: 系統啟動完成
- 🌈 彩虹: 開機成功，最後閃示一次彩虹燈後進入待機（若燈停在某色未前進，代表該階段卡住）

### 核心功能

- **強化的 NFC 讀取**: 多層驗證和錯誤處理機制
- **資料格式驗證** (兩種格式皆支援，可有無 `en` 前綴):
  - Record 2: `00`~`04` 或 `en00`~`en04` (控制燈光顏色，00 不反應)
  - Record 3: `001`~`120` 或 `en001`~`en120` (暫存資料供後續使用)
- **燈光控制**:
  - '01': 天空家族 (藍色)
  - '02': 海洋家族 (綠色)
  - '03': 樹居家族 (紅色)
  - '04': 大地家族 (黃色)
- **穩定性機制**:
  - 標籤穩定性檢測 (100ms) 與讀取失敗快速重試 (最多 3 次)
  - 非 NDEF 卡片以 `hasNdefMessage()` 防 NULL 解參考當機
  - 連續讀取失敗 5 次自動重新初始化 PN532
  - WiFi 離線定期重試 (60 秒) + `setAutoReconnect` + 重連優先上次成功 SSID
  - 心跳發送失敗即標記 MQTT 斷線加速重連
  - 安全記憶體管理

### 關鍵函數

- `processTagData()`: 讀取並解析 NFC 標籤、驗證、發送 MQTT
- `validateRecord2()` / `validateRecord3()`: 驗證記錄格式
- `extractNumberSmart()` / `extractNumber()`: 智慧解析記錄中的數字 (相容有無 `en` 前綴)
- `getPayloadAsString()`: 取得 NDEF 記錄 payload 文字
- `processLightControl()`: 處理燈光控制邏輯
- `showBootProgress()`: 顯示彩色開機進度指示燈
- `updateIdleStatusLight()` / `showIdleStatusLight()`: 待機狀態燈控制
- `connectToWiFi()` / `connectToMQTT()` / `publishToMQTT()`: 網路與 MQTT 連線發送
- `sendHeartbeat()`: 發送心跳 (帶 IP / RSSI / SSID) 至 `puffin-heartbeat`
- `turnOffLeds()`: 熄滅所有 LED
- `loop()`: 主迴圈，實現狀態機邏輯

## 依賴函式庫

- `Wire.h` (I2C 通訊)
- `SPI.h` (備用 SPI 通訊)
- `PN532_I2C.h` / `NfcAdapter.h` (Don Coleman NDEF 函式庫)
- `FastLED.h` (LED 燈條控制)
- `WiFi.h` (ESP32 WiFi 連線)
- `PubSubClient.h` (MQTT 通訊)

## 編譯環境需求（已用 arduino-cli 驗證，2025-08）

- **開發板 FQBN**: `esp32:esp32:d1_uno32`（WEMOS D1 R32），ESP32 core 3.3.10
- **Partition Scheme**: 必須選 **「No OTA (Large APP)」**（`PartitionScheme=no_ota`）或 Minimal SPIFFS。
  預設 partition 放不下（程式約 1.33MB，預設 app 分割區約 1.28MB 會溢出）。
- **函式庫版本**（實測可編過）:
  - FastLED `3.10.5`（Library Manager）
  - PubSubClient `2.8.0`（Library Manager）
  - **elechouse/PN532**（GitHub，非 Library Manager）—— 同時提供 `PN532_I2C.h` 與 NDEF 的 `NfcAdapter.h`；
    安裝時需把 repo 內的 `PN532`、`PN532_I2C`、`NDEF` 等子資料夾**分別**複製到 libraries/。
  - ⚠️ 不要用 **TheNitek/NDEF**（那是給 MFRC522 的，不支援 PN532）。
- **必要的程式修正**: NDEF 的 `NfcAdapter.h` 會 `#define IRQ/RESET`，與 FastLED 3.x 的 `RESET` 識別字衝突。
  主程式已在 `#include <NfcAdapter.h>` 之後、`#include <FastLED.h>` 之前加上 `#undef IRQ` / `#undef RESET`。
- **備份檔注意**: `*_backup.ino` 不可放在 sketch 資料夾內（Arduino 會一起編譯造成「重複定義」）；請放在 repo 根目錄或其他資料夾。

## 開發注意事項

- **程式碼風格**: 混合使用繁體中文註解和英文變數名稱
- **NFC 函式庫**: 使用 Don Coleman 的 NDEF 函式庫 (PN532_I2C + NfcAdapter)，已徹底移除 Adafruit 依賴
- **記憶體管理**: 使用固定大小緩衝區避免動態分配風險
- **自製 NDEF 解析器**: 專門解析文字記錄，避免複雜的外部依賴
- **穩定性設計**:
  - 五狀態機確保讀取流程穩定
  - 短超時時間快速檢測，避免阻塞
  - 全域超時保護防止程式卡死
- **資料驗證**: 嚴格檢查 NDEF 記錄數量和格式
- **防連續讀取**: 基於狀態機的冷卻機制，不依賴簡單延遲
