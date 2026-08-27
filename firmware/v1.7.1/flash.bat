@echo off
rem =========================================================
rem  NFC RFID Light Controller v1.7.1 flasher (WEMOS D1 R32 / ESP32)
rem  Usage:  flash.bat COM5     (specify serial port)
rem          flash.bat          (no arg: list ports, then ask)
rem  Flash one board, unplug, plug the next one, run again.
rem =========================================================
setlocal

set "ESPTOOL=%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe"
set "FWDIR=%~dp0"

if exist "%ESPTOOL%" goto has_esptool
echo [ERROR] esptool not found: %ESPTOOL%
echo Please install the ESP32 board package in Arduino IDE.
exit /b 1

:has_esptool
set "PORT=%~1"
if not "%PORT%"=="" goto do_flash

echo Available serial ports:
powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames() | ForEach-Object { '  ' + $_ }"
echo.
set /p "PORT=Enter serial port, e.g. COM5 : "
if "%PORT%"=="" goto no_port

:do_flash
echo.
echo ============================================
echo  Flashing v1.7.1 to %PORT% ...
echo ============================================
"%ESPTOOL%" --chip esp32 --port %PORT% --baud 921600 write-flash ^
  0x1000  "%FWDIR%bootloader.bin" ^
  0x8000  "%FWDIR%partitions.bin" ^
  0xe000  "%FWDIR%boot_app0.bin" ^
  0x10000 "%FWDIR%app.bin"

if errorlevel 1 goto flash_fail

echo.
echo [OK] %PORT% flashed successfully! Board reboots automatically;
echo      the LED strip should show the colored boot progress lights.
exit /b 0

:no_port
echo [ERROR] No serial port specified.
exit /b 1

:flash_fail
echo.
echo [FAILED] Flashing failed. Common causes:
echo   - Wrong COM port, or port occupied by Serial Monitor (close it)
echo   - USB cable is charge-only (no data)
echo   - Board not in download mode (usually automatic; if needed,
echo     hold the IO0/BOOT button while plugging in)
exit /b 1
