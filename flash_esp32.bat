@echo off
echo ====================================
echo ESP32 Binary 燒錄工具
echo ====================================
echo.

REM 檢查 esptool 是否安裝
echo [1/4] 檢查 esptool 安裝狀態...
python -m pip show esptool >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ esptool 已安裝
    python -c "import esptool; print(f'版本: {esptool.__version__}')" 2>nul
) else (
    echo ❌ esptool 未安裝，正在安裝...
    python -m pip install esptool
    if %errorlevel% neq 0 (
        echo ❌ esptool 安裝失敗，請手動執行: pip install esptool
        pause
        exit /b 1
    )
    echo ✅ esptool 安裝完成
)
echo.

REM 檢查 binary 檔案是否存在
echo [2/4] 檢查 binary 檔案...
if exist "rgidlightcontrol.ino.pocket_32.bin" (
    echo ✅ 找到檔案: rgidlightcontrol.ino.pocket_32.bin
    for %%F in ("rgidlightcontrol.ino.pocket_32.bin") do echo    檔案大小: %%~zF bytes
) else (
    echo ❌ 找不到檔案: rgidlightcontrol.ino.pocket_32.bin
    echo    請確認檔案位於當前目錄: %cd%
    pause
    exit /b 1
)
echo.

REM 自動偵測 COM 埠
echo [3/4] 偵測 ESP32 裝置...
python -c "
import serial.tools.list_ports
ports = list(serial.tools.list_ports.comports())
esp_ports = []
for port in ports:
    if 'USB' in port.description.upper() or 'SERIAL' in port.description.upper() or 'CH340' in port.description.upper() or 'CP210' in port.description.upper():
        esp_ports.append(port.device)
        print(f'找到可能的 ESP32 裝置: {port.device} - {port.description}')

if esp_ports:
    print(f'建議使用: {esp_ports[0]}')
    with open('temp_port.txt', 'w') as f:
        f.write(esp_ports[0])
else:
    print('未找到明確的 ESP32 裝置')
    if ports:
        print('可用的序列埠:')
        for port in ports:
            print(f'  {port.device} - {port.description}')
        with open('temp_port.txt', 'w') as f:
            f.write(ports[0].device if ports else 'COM3')
    else:
        print('未找到任何序列埠')
        with open('temp_port.txt', 'w') as f:
            f.write('COM3')
" 2>nul

if exist "temp_port.txt" (
    set /p AUTO_PORT=<temp_port.txt
    del temp_port.txt
) else (
    set AUTO_PORT=COM3
)

set /p COM_PORT="請輸入 COM 埠 (預設: %AUTO_PORT%): "
if "%COM_PORT%"=="" set COM_PORT=%AUTO_PORT%
echo 使用 COM 埠: %COM_PORT%
echo.

REM 開始燒錄
echo [4/4] 開始燒錄 ESP32...
echo 命令: python -m esptool --chip esp32 --port %COM_PORT% --baud 921600 write_flash 0x10000 rgidlightcontrol.ino.pocket_32.bin
echo.
echo ⚠️  請確保 ESP32 已連接並進入下載模式
echo    (按住 BOOT 按鈕，按一下 RESET，然後放開 BOOT)
echo.
pause

python -m esptool --chip esp32 --port %COM_PORT% --baud 921600 write_flash 0x10000 rgidlightcontrol.ino.pocket_32.bin

if %errorlevel% equ 0 (
    echo.
    echo ✅ 燒錄成功！
    echo 🔄 ESP32 將自動重啟...
) else (
    echo.
    echo ❌ 燒錄失敗！
    echo 請檢查:
    echo   - ESP32 是否正確連接
    echo   - COM 埠是否正確
    echo   - ESP32 是否進入下載模式
)

echo.
echo 按任意鍵結束...
pause >nul