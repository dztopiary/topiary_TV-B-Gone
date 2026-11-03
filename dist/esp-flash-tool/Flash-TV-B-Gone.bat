@echo off
REM TV-B-Gone ESP32-S3 SuperMini - one-click flash script (Windows)
REM
REM What this does:
REM   1. Checks if the official Espressif "ESP Flash Download Tool" is installed
REM      and opens it with our config pre-loaded.
REM   2. Falls back to esptool.py via Python if you have PlatformIO/Python set up.
REM   3. Otherwise tells the user what to do manually.

setlocal EnableDelayedExpansion
set "ROOT=%~dp0.."
set "CONFIG=%ROOT%\flash-config.json"
set "FWDIR=%ROOT%\firmware"

echo ============================================================
echo  TV-B-Gone flasher (ESP32-S3 SuperMini)
echo ============================================================
echo.
echo  Config: %CONFIG%
echo  Folders checked below:
echo    - C:\Program Files\Espressif\...
echo    - %LOCALAPPDATA%\Espressif\...
echo.

REM --- Try to find the official ESP Flash Download Tool (GUI) -------------
set "FOUND_GUI="
for %%P in (
  "%PROGRAMFILES%\Espressif"
  "%PROGRAMFILES(x86)%\Espressif"
  "%LOCALAPPDATA%\Espressif"
  "%USERPROFILE%\AppData\Local\Programs\Espressif"
) do if exist "%%~P" (
    for /r %%F in ("%%~P") do (
        if /i "%%~nxF"=="ESP Flash Download Tool.exe" set "FOUND_GUI=%%~F"
    )
)

if defined FOUND_GUI (
    echo [OK] Found: %FOUND_GUI%
    echo.
    echo  Putting the ESP in flashing mode before we open it?
    echo   - Press BOOT / IO0 on your board NOW if you haven't already, OR
    echo     hold BOOT and tap RESET once to enter download mode.
    echo.
    set /p "CONFIRM=Press Enter when ready (or Ctrl+C to cancel)... "
    start "" "%FOUND_GUI%" "/c" "%CONFIG%"
    exit /b 0
)

REM --- Fall back to esptool.py if Python + PlatformIO is available ---------
where python >nul 2>nul
if %errorlevel%==0 (
    python -m pip show esptool >nul 2>nul
    if !errorlevel!==0 goto :have_esptool
)
python3 -m pip show esptool >nul 2>nul
if %errorlevel%==0 goto :have_esptool

echo.
echo [i] Official Flash Download Tool not found, and esptool.py isn't available either.
echo.
echo  To flash manually:
echo    1. Install the official "ESP Flash Download Tool" from Espressif's site, OR
echo       install PlatformIO (python -m pip install platformio) which pulls in esptool.
echo    2. Put your board into flashing mode (hold BOOT, tap RESET).
echo    3. Open flash-config.json with the tool and click "Start".
echo.
pause
exit /b 1

:have_esptool
echo [OK] Using esptool.py to flash directly...
echo     Put your board in flashing mode (hold BOOT, tap RESET).
python -m esptool --chip esp32s3 write_flash @%CONFIG%
if %errorlevel%==0 (
    echo.
    echo  [OK] Flash complete! You can power-cycle the board now.
) else (
    echo.
    echo  [!] esptool reported an error - check the port / flashing mode.
)
pause
exit /b %errorlevel%
