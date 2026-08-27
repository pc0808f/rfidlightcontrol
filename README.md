# RFID/NFC LED 燈光控制系統

Arduino wemos d1 r32 (ESP32) 專案：活動現場的互動燈光裝置。參加者刷 NFC 卡片後，燈條顯示對應「家族」的顏色並透過 MQTT 回報給 App；主持人也可透過 App 遙控整條燈條做互動燈光秀。

**現行版本：v1.7.1**（原始碼 `rgidlightcontrol/rgidlightcontrol.ino`，燒錄檔 `firmware/v1.7.1/`）

## 專案目的與運作情境

本裝置是為**台北市和平實驗小學「家族儀式」**打造的互動燈光裝置。儀式流程：

1. **刷卡分家族**：小朋友把 NFC 卡片靠近讀卡器，裝置讀出卡片裡的編號與家族代碼，整條燈條亮起該家族的顏色 15 秒（天空=藍、海洋=綠、樹居=紅、大地=黃），同時把 `"編號,家族"`（如 `001,01`）發送到 MQTT 給 App 記錄。
2. **互動燈光秀**：主持人在 App 按鍵，透過 MQTT 指令讓裝置進入互動模式——模式一是 8 秒白光漸亮漸暗，模式二是 7 秒星光蓄力爆發，做為活動高潮橋段。
3. 同型裝置共有 **4 台**，都燒同一份韌體（以 MAC 位址區分 MQTT client ID），現場靠行動電源或 AC 供電。

## 系統架構總覽

### 主程式：`rgidlightcontrol/rgidlightcontrol.ino`（單一檔案）

兩個互相獨立的狀態機：

- **NFC 五狀態機**（`SystemState`）：IDLE（輪詢標籤）→ TAG_DETECTED（穩定確認 100ms）→ PROCESSING（讀取解析 NDEF、驗證、發 MQTT）→ LIGHT_ON（亮家族色 15 秒）→ COOLDOWN（冷卻 1~3 秒防連刷）→ 回 IDLE。
- **互動模式狀態機**（`InteractiveState`）：由 MQTT `puffin-control` 指令驅動，進入後暫停 NFC 掃描，退出後自動恢復。與 NFC 狀態機不共用任何變數（詳見 `Arduino互動燈光模式說明.md`，這是 App 端定案的協定規格）。

全程 `millis()` 非阻塞計時（除了 WiFi/MQTT 連線重試是阻塞的，設計上有逾時上限）。

### MQTT 協定（Broker: `MQTTGO.io:1883`，公開免費 broker）

| Topic | 方向 | 內容 |
|---|---|---|
| `puffin-test` | 裝置 → App | 刷卡資料 `"001,01"`（編號 3 碼, 家族 2 碼） |
| `puffin-heartbeat` | 裝置 → App | 每 30 秒心跳 JSON：device / ip / rssi / ssid / timestamp |
| `puffin-control` | App → 裝置 | 互動模式指令（下表），**字串須與 App 完全一致** |

| 指令 | 行為 |
|---|---|
| `INTERACTIVE_START` | 燈全暗進入互動模式、暫停刷卡（可重複補送，不檢查狀態） |
| `MODE1_START` | 8 秒白光漸亮至 50% 再漸暗（僅在等待模式一時有效） |
| `MODE2_START` | 7 秒蓄力爆發：星光加速變密 → 6 秒時全條閃白定格（僅在等待模式二時有效） |
| `INTERACTIVE_EXIT` | 緊急退出，任何狀態立即生效、恢復刷卡 |

### NFC 標籤格式（NDEF 文字記錄，可有無 `en` 語言前綴）

- Record 2：`00`~`04` = 家族代碼（`00` 不亮燈）
- Record 3：`001`~`120` = 參加者編號

### 燈光與指示對照表

**開機進度燈**（燈條，約 20% 亮度，卡在哪個顏色 = 哪個階段出問題）：
🔴 FastLED 初始化 → 🟠 I2C 就緒 → 🟡 PN532 就緒 → 🟢 WiFi 連上 → 🔵 MQTT 連上 → 🟣 啟動完成 → 🌈 彩虹 = 開機成功

**運作中**：網路正常時待機顯示 50% 白光；刷卡亮家族色 15 秒；執行期 WiFi 重連成功閃粉紅 1 秒。

**板載藍色 LED（GPIO2，UNO 佈局 D13 位置）**——行動電源供電看不到 Serial log 時的主要判斷依據：

| 閃法 | 意義 |
|---|---|
| 恆亮 | 待機掃描中，一切正常 |
| 慢閃（每秒 1 次） | 互動模式中 |
| 快閃（急促） | WiFi 或 MQTT 斷線（重連中 LED 仍會閃，完全不動 = 當機） |

## 硬體

- Arduino wemos d1 r32 (ESP32)
- PN532 NFC 讀卡模組（I2C：SDA=GPIO21, SCL=GPIO22）
- WS2812B LED 燈條 ×100（GPIO12，GRB 順序）

## ⚡ 電源供應注意事項（2026-08 現場實測記錄）

### 現象

- 接**電腦 USB** 或 **AC Type-C 充電器**：一切正常。
- 接**行動電源（5V/3A）**：可開機、可連 WiFi，但開機卡在綠燈很久（MQTT 連不上）、執行中會當機，久了還會自動重開機。
- 同樣接行動電源，改用**手機熱點就近提供 WiFi**（現用「TDS」= 上誠老師的熱點）之後：一切正常。

### 原因分析

100 顆 WS2812B 的用電遠超直覺：50% 白色待機燈約 3A（把行動電源額定踩滿）、全白理論值約 6A。
再加上關鍵變數 —— **WiFi 訊號強度**：AP 距離遠、訊號弱時，ESP32 射頻會以最大功率發射，
電流尖峰疊上燈條負載，行動電源的瞬態響應撐不住，5V 被瞬間拉低 → ESP32 brownout 重開機
或射頻工作不穩（WiFi 連得上但 MQTT/TCP 一直失敗）。熱點靠近後發射功率下降、尖峰變小，行動電源就撐得住了。

### 目前的因應方式

- 正式場合用 **AC 充電器**供電，或確保 **WiFi AP / 熱點離裝置夠近**（訊號強度可從心跳的 RSSI 欄位確認，建議 > -60 dBm）。

### 未來硬體改善方向（待驗證）

- **大電容補在 5V→3.3V 穩壓器（AMS1117）輸入端，即板子 5V/VIN 端**（建議 1000~2200µF/10V 低 ESR 電解電容 + 0.1µF 陶瓷電容並聯）。
  原理：AMS1117-3.3 壓差約 1.1V，輸入必須維持 4.4V 以上，3.3V 才穩；brownout 就是尖峰瞬間 5V 被拉破 4.4V 造成的，
  電容的任務是扛住這條底線。
  - 燈條供電輸入端也建議並一顆 1000µF（WS2812B 官方建議做法，燈條的瞬間電流比 ESP32 更大）。
  - 3.3V 端可加 220~470µF 當第二道保險，就地吸收 WiFi 發射尖峰（非必須）。
- 燈條 5V 與開發板分開供電（共地），避免燈條電流全部流經板上線路。
- 軟體限流方案已實作過（FastLED `setMaxPowerInVoltsAndMilliamps` 壓在 2A + NVS 記錄異常重開原因 + D13 區分 WiFi/MQTT 斷線），
  保存在 git stash（`v1.7.2-v1.7.3 power diagnostics`），需要時可 `git stash pop` 取回。

## 開發歷程記事（事件與決策脈絡）

依時間順序，讓接手的人知道每個功能「為什麼長這樣」：

| 版本/事件 | 內容與原因 |
|---|---|
| v1.6.0 | 穩定性總整理：WiFi 永遠離線 bug、非 NDEF 卡 NULL 當機、PN532 卡死自動復位等（細節見 `ver.md`） |
| v1.6.1 | **現場事故**：裝置長時間放置後 Serial 狂印「⚠️重置」無限循環、NFC 完全失效。原因是 WiFi 重連內層「卡 5 秒重置」誤共用外層 15 秒逾時的計時變數，逾時永遠不觸發、主迴圈卡死在重連。改用獨立計時變數修正 |
| v1.7.0 | 依 App 端定案的協定（`Arduino互動燈光模式說明.md`）新增互動燈光模式：訂閱 `puffin-control`、獨立狀態機、四指令。模式一 = 8 秒白光漸亮漸暗（顏色是我方定的，規格未指定）；模式二原為單顆隨機星光殘影 |
| v1.7.1 | **模式二重設計**：實測發現觀眾距離遠，單顆星光看不清楚，改成「蓄力爆發」——星光加速變密 6 秒後全條閃白定格。另新增 D13 板載 LED 狀態指示（因為現場用行動電源，看不到 Serial log） |
| 量產燒錄 | 4 台裝置需求 → 建立 `firmware/v1.7.1/` 燒錄包（bin 取自實機驗證成功的 IDE 編譯）與 `flash.bat`。**教訓**：bat 檔訊息不能用中文（cmd 的 Big5 編碼會把 UTF-8 中文弄壞導致腳本爆炸），已改純英文 |
| 電源事件 | 行動電源供電時當機/重開，一度加了診斷版 v1.7.2/v1.7.3（限流+NVS 重開紀錄），後以「AC 正常、熱點就近後行動電源也正常」確診為電源瞬態問題而非程式問題，程式回退 v1.7.1，診斷版收入 git stash。硬體解法（大電容）規劃見上節 |

## 給接手開發者的指南

### 檔案地圖

| 檔案/目錄 | 用途 |
|---|---|
| `rgidlightcontrol/rgidlightcontrol.ino` | 主程式（唯一的程式檔），頂部有完整版本更新記錄與可調參數 |
| `Arduino互動燈光模式說明.md` | App 端定案的互動模式協定規格（MQTT 指令、狀態機、燈效規格），改互動模式前必讀 |
| `CLAUDE.md` | 編譯環境需求（FQBN、Partition Scheme、函式庫版本與安裝眉角）、程式架構說明 |
| `ver.md` | v1.6.0 以前的詳細版本歷史 |
| `firmware/v1.7.1/` | 量產燒錄包：bin 檔 + `flash.bat` + 燒錄說明 |
| `PN532-PN532_HSU/` | elechouse/PN532 函式庫的實測快照（**已入版控**——上游無版本號可釘且久未維護，這份是唯一保證編譯得過的來源；安裝時把其中 `PN532`、`PN532_I2C`、`NDEF` 等子資料夾分別複製到 Arduino `libraries/`） |

### 上手三步驟

1. **看懂協定**：讀 `Arduino互動燈光模式說明.md`（互動模式）+ 本 README 的 MQTT 協定表（刷卡回報）。
2. **建編譯環境**：照 `CLAUDE.md`「編譯環境需求」安裝 board package 與函式庫。**三個必踩的坑**：
   - Partition Scheme 必須選 **No OTA**（程式 1.08MB，預設分割區放不下）
   - 函式庫要用 elechouse/PN532（**不是** Adafruit PN532、**不是** TheNitek/NDEF），子資料夾要分別複製進 libraries/
   - `NfcAdapter.h` 的 `IRQ`/`RESET` 巨集與 FastLED 衝突，主程式已在 include 之間 `#undef`，改 include 順序前先看該處註解
3. **燒錄與驗證**：改完程式用 IDE 燒一片實測 → 確認 OK 後更新 `firmware/` 燒錄包給其他板子。燒錄時 COM 埠若報 busy，先關 Serial Monitor；剛插上的板子第一次連線偶爾失敗，重試一次即可。

### 其他須知

- 程式風格：繁體中文註解 + 英文變數名；固定大小緩衝區、避免動態記憶體配置。
- WiFi 帳密清單在程式頂部 `wifiCredentials[]`，會依序嘗試並記住上次成功的優先重連；「TDS」是現場用的手機熱點。
- 燈效時間、冷卻、亮度等所有可調參數集中在程式頂部，都有註解。
- 版本號慣例：功能新增 bump 次版號（1.7.0），小修/燈效調整 bump 修訂號（1.7.1），並同步更新檔頭註解的更新記錄與 `setup()` 的開機 banner。
- 修改前先看板載 D13 LED 與開機進度燈的對照表（上文），現場除錯全靠它們。

## 開發板套件 (Board Package)

| 名稱 | 版本 | 來源 |
|---|---|---|
| esp32:esp32 (Arduino ESP32 Boards) | 3.3.10（已驗證）／3.3.11（同樣可編譯） | Arduino Boards Manager |

FQBN: `esp32:esp32:d1_uno32`（WEMOS D1 R32）；用「ESP32 Dev Module」(`esp32:esp32:esp32`) 也可編譯（實測過，但注意該變體沒有 `LED_BUILTIN` 定義，程式已改用固定 GPIO2）。
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
