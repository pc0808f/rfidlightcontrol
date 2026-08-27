# 韌體 v1.7.1 燒錄包 (WEMOS D1 R32 / ESP32)

對應原始碼 commit `d6f35b8`（v1.7.1：互動燈光模式 + 板載 LED 狀態指示 + 模式二蓄力爆發）。
bin 檔取自實機驗證成功的 Arduino IDE 編譯結果（ESP32 core 3.3.11，Partition Scheme: No OTA）。

## 檔案說明

| 檔案 | 燒錄位址 | 說明 |
|---|---|---|
| `bootloader.bin` | 0x1000 | 開機載入程式 |
| `partitions.bin` | 0x8000 | 分割表 (No OTA, Large APP) |
| `boot_app0.bin` | 0xe000 | OTA 資料初始區 |
| `app.bin` | 0x10000 | 主程式 (v1.7.1) |
| `merged_full_4MB.bin` | 0x0 | 以上全部合併的完整 4MB 映像（備用） |

## 燒錄方式（每片板子重複一次）

1. 用 USB 線接上板子（確認是資料線，不是純充電線）
2. 關閉 Arduino IDE 的序列埠監控視窗（避免佔用 COM 埠）
3. 開啟命令提示字元，執行：

```bat
flash.bat COM5
```

（`COM5` 換成實際的埠號；不帶參數執行會列出可用的埠讓你選）

## 備用方式：燒完整映像

若分開燒錄有問題，可改燒合併映像（較慢但最單純）：

```bat
%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe --chip esp32 --port COM5 --baud 921600 write-flash 0x0 merged_full_4MB.bin
```

## 燒錄成功的判斷

板子重開機後：
- 燈條依序顯示彩色開機進度燈（紅→橙→黃→綠→藍→紫→彩虹）
- 板載藍色 LED（D13 位置）：恆亮 = 待機正常；快閃 = WiFi/MQTT 未連上
