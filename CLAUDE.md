# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案概述

這是一個 Arduino wemos d1 r32 專案，實現 RFID/NFC 標籤觸發 LED 燈條顯示的系統。當感應到 NFC 標籤時，會根據標籤內容顯示不同顏色的燈光，並維持 10 秒鐘後自動熄滅。

## 硬體配置

- **開發板**: Arduino wemos d1 r32
- **NFC 讀取器**: PN532 模組，預設使用 I2C (SDA=A4, SCL=A5)
  - 可選用 UART: RX=2, TX=3
  - 可選用 SPI: SS=10
- **LED 燈條**: WS2812B，100 顆 LED，連接到 pin 10
- **燈條設定**: GRB 色彩順序，亮度 100

## 程式架構

主程式檔案：`rgidlightcontrol.ino`

### 可調整參數 (程式頂部)

- `LIGHT_DURATION`: 燈光持續時間 (預設 5 秒)
- `READ_DEBOUNCE`: 防連續讀取延遲 (預設 2 秒)
- `TAG_STABLE_TIME`: 標籤穩定檢測時間 (預設 0.5 秒)
- `STATE_TIMEOUT`: 狀態超時時間 (預設 10 秒)
- `MAX_READ_ATTEMPTS`: 最大讀取嘗試次數 (預設 3 次)

### 狀態機系統

程式採用五狀態機設計：

1. **IDLE**: 待機狀態，檢查標籤存在和燈光超時
2. **DETECTING**: 檢測標籤穩定性，確保標籤穩定存在
3. **READING**: 安全讀取標籤資料，支援多次重試
4. **PROCESSING**: 處理燈光控制邏輯
5. **COOLDOWN**: 冷卻狀態，防止連續讀取

### 核心功能

- **強化的 NFC 讀取**: 多層驗證和錯誤處理機制
- **資料格式驗證**:
  - Record 2: `en00`~`en04` (控制燈光顏色，00 不反應)
  - Record 3: `en001`~`en120` (暫存資料供後續使用)
- **燈光控制**:
  - '01': 天空家族 (藍色)
  - '02': 海洋家族 (綠色)
  - '03': 樹居家族 (紅色)
  - '04': 大地家族 (黃色)
- **穩定性機制**:
  - 狀態超時保護
  - 標籤穩定性檢測
  - 安全記憶體管理

### 關鍵函數

- `readTagSafely()`: 安全讀取 NFC 標籤，支援多次重試
- `validateRecord2()` / `validateRecord3()`: 驗證記錄格式
- `parseRecordNumber()`: 解析記錄中的數字
- `getPayloadSafely()`: 安全獲取 payload 資料
- `processLightControl()`: 處理燈光控制邏輯
- `turnOffLeds()`: 熄滅所有 LED
- `loop()`: 主迴圈，實現狀態機邏輯

## 依賴函式庫

- `Wire.h` (I2C 通訊)
- `SPI.h` (備用 SPI 通訊)
- `Adafruit_PN532.h` (Adafruit PN532 函式庫)
- `FastLED.h` (LED 燈條控制)

## 開發注意事項

- **程式碼風格**: 混合使用繁體中文註解和英文變數名稱
- **NFC 函式庫**: 使用 Adafruit PN532 函式庫，提供更好的穩定性和效能
- **記憶體管理**: 使用固定大小緩衝區避免動態分配風險
- **自製 NDEF 解析器**: 專門解析文字記錄，避免複雜的外部依賴
- **穩定性設計**:
  - 五狀態機確保讀取流程穩定
  - 短超時時間快速檢測，避免阻塞
  - 全域超時保護防止程式卡死
- **資料驗證**: 嚴格檢查 NDEF 記錄數量和格式
- **防連續讀取**: 基於狀態機的冷卻機制，不依賴簡單延遲
