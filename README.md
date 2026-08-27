# RFID/NFC LED 燈光控制系統

Arduino wemos d1 r32 (ESP32) 專案：感應 PN532 讀取的 NFC 標籤，依標籤內容點亮對應顏色的 WS2812B 燈條，並透過 WiFi/MQTT 回報資料與心跳。詳細架構與開發說明請見 [CLAUDE.md](CLAUDE.md)、版本歷史請見 [ver.md](ver.md)。

## 硬體

- Arduino wemos d1 r32 (ESP32)
- PN532 NFC 讀卡模組（I2C：SDA=GPIO21, SCL=GPIO22）
- WS2812B LED 燈條 ×100（GPIO12）

## ⚡ 電源供應注意事項（2026-08 現場實測記錄）

### 現象

- 接**電腦 USB** 或 **AC Type-C 充電器**：一切正常。
- 接**行動電源（5V/3A）**：可開機、可連 WiFi，但開機卡在綠燈很久（MQTT 連不上）、執行中會當機，久了還會自動重開機。
- 同樣接行動電源，改用**手機熱點就近提供 WiFi** 之後：一切正常。

### 原因分析

100 顆 WS2812B 的用電遠超直覺：50% 白色待機燈約 3A（把行動電源額定踩滿）、全白理論值約 6A。
再加上關鍵變數 —— **WiFi 訊號強度**：AP 距離遠、訊號弱時，ESP32 射頻會以最大功率發射，
電流尖峰疊上燈條負載，行動電源的瞬態響應撐不住，5V 被瞬間拉低 → ESP32 brownout 重開機
或射頻工作不穩（WiFi 連得上但 MQTT/TCP 一直失敗）。熱點靠近後發射功率下降、尖峰變小，行動電源就撐得住了。

### 目前的因應方式

- 正式場合用 **AC 充電器**供電，或確保 **WiFi AP / 熱點離裝置夠近**（訊號強度可從心跳的 RSSI 欄位確認，建議 > -60 dBm）。

### 未來硬體改善方向（待驗證）

- **在 5V 電源端（板子 VIN 與燈條供電端）並聯大容量低 ESR 電解電容（建議 1000~2200µF/6.3V 以上，可再並 0.1µF 陶瓷電容），
  吸收 WiFi 發射與燈條點亮的瞬間電流尖峰**——理論上可讓行動電源供電穩定，是最低成本的解法。
- 燈條 5V 與開發板分開供電（共地），避免燈條電流全部流經板上線路。
- 軟體限流方案已實作過（FastLED `setMaxPowerInVoltsAndMilliamps` 壓在 2A + NVS 記錄異常重開原因 + D13 區分 WiFi/MQTT 斷線），
  保存在 git stash（`v1.7.2-v1.7.3 power diagnostics`），需要時可 `git stash pop` 取回。

## 開發板套件 (Board Package)

| 名稱 | 版本 | 來源 |
|---|---|---|
| esp32:esp32 (Arduino ESP32 Boards) | 3.3.10（已驗證）／3.3.11（同樣可編譯） | Arduino Boards Manager |

FQBN: `esp32:esp32:d1_uno32`（WEMOS D1 R32）
Partition Scheme: **No OTA (2MB APP/2MB SPIFFS)**（`PartitionScheme=no_ota`）

## 使用的函式庫 (Libraries)

| 函式庫 | 版本 | 來源 | 用途 |
|---|---|---|---|
| Wire | (ESP32 core 內建，隨 board package 版本) | Arduino ESP32 core | I2C 通訊 |
| SPI | (ESP32 core 內建，隨 board package 版本) | Arduino ESP32 core | 備用 SPI 通訊 |
| WiFi | (ESP32 core 內建，隨 board package 版本) | Arduino ESP32 core | WiFi 連線 |
| [FastLED](https://github.com/FastLED/FastLED) | 3.10.5 | Arduino Library Manager | WS2812B LED 燈條控制 |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | 2.8.0 | Arduino Library Manager | MQTT 通訊 |
| [PN532（elechouse 版）](https://github.com/elechouse/PN532)<br>提供 `PN532.h` / `PN532_I2C.h` / `PN532_SPI.h` / `PN532_HSU.h` | 無正式版號（GitHub `PN532_HSU` 分支）| GitHub（非 Library Manager）| PN532 NFC 讀卡驅動 |
| NDEF（Don Coleman 原作，隨上述 elechouse/PN532 repo 一併提供）<br>提供 `NfcAdapter.h` / `NdefMessage.h` / `NdefRecord.h` / `NfcTag.h` | 無正式版號 | GitHub（隨 elechouse/PN532 repo）| NDEF 標籤資料解析 |

> ⚠️ **不要**使用 Library Manager 裡的「Adafruit PN532」或「TheNitek/NDEF」——前者沒有本專案所需的 NDEF 高階解析 API，後者是給 MFRC522 讀卡器用的分支，不支援 PN532。詳見 [CLAUDE.md 的編譯環境需求](CLAUDE.md#編譯環境需求已用-arduino-cli-驗證2025-08)。

## 安裝函式庫

1. Arduino IDE → **Library Manager** 安裝：
   - `FastLED`（3.10.5）
   - `PubSubClient`（2.8.0）
2. 從 GitHub 下載 [elechouse/PN532](https://github.com/elechouse/PN532)（`PN532_HSU` 分支），將其中的 `PN532`、`PN532_I2C`、`NDEF` 等子資料夾**分別**複製到 Arduino 的 `libraries/` 目錄下。

編譯前的已知修正、Partition Scheme 設定等細節，請參考 [CLAUDE.md](CLAUDE.md) 中的「編譯環境需求」章節。
