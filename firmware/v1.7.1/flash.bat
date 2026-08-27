@echo off
rem =========================================================
rem  NFC RFID 燈光控制系統 v1.7.1 燒錄腳本 (WEMOS D1 R32 / ESP32)
rem  用法:  flash.bat COM5      (指定序列埠)
rem         flash.bat           (不帶參數會先列出可用序列埠再詢問)
rem  燒完一片拔掉、插下一片，再執行一次即可。
rem =========================================================
setlocal
chcp 65001 >nul

set "ESPTOOL=%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
set "FWDIR=%~dp0"

if exist "%ESPTOOL%" goto has_esptool
echo [錯誤] 找不到 esptool: %ESPTOOL%
echo 請確認 Arduino IDE 的 ESP32 開發板套件已安裝。
exit /b 1

:has_esptool
set "PORT=%~1"
if not "%PORT%"=="" goto do_flash

echo 目前偵測到的序列埠:
powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { '  ' + $_ }"
echo.
set /p "PORT=請輸入序列埠 例如 COM5 : "
if "%PORT%"=="" goto no_port

:do_flash
echo.
echo ============================================
echo  開始燒錄 v1.7.1 到 %PORT% ...
echo ============================================
"%ESPTOOL%" --chip esp32 --port %PORT% --baud 921600 write-flash ^
  0x1000  "%FWDIR%bootloader.bin" ^
  0x8000  "%FWDIR%partitions.bin" ^
  0xe000  "%FWDIR%boot_app0.bin" ^
  0x10000 "%FWDIR%app.bin"

if errorlevel 1 goto flash_fail

echo.
echo [完成] %PORT% 燒錄成功！板子會自動重開機，燈條應顯示彩色開機進度燈。
exit /b 0

:no_port
echo [錯誤] 未指定序列埠。
exit /b 1

:flash_fail
echo.
echo [失敗] 燒錄失敗。常見原因:
echo   - COM 埠選錯或被序列埠監控視窗佔用，請關掉 Arduino IDE 的 Serial Monitor
echo   - 傳輸線只能充電不能傳資料
echo   - 板子未進入下載模式，通常會自動，必要時按住 IO0/BOOT 鍵再重插
exit /b 1
