@echo off
setlocal

:: Puts the menu build on the SC64's SD card over USB, using the deployer's own
:: SD card commands. The menu is not involved, unlike "localdeploy.bat /dur",
:: which leaves sc64menu.n64 empty on the card.
::
::   deploy-sd.bat       copy the build to the card
::   deploy-sd.bat /d    copy, then stay connected and show the menu's log

set "DEPLOYER=%~dp0tools\sc64\sc64deployer.exe"
set "ROM=%~dp0output\sc64menu.n64"

if not exist "%DEPLOYER%" (
    echo sc64deployer.exe was not found in tools\sc64.
    exit /b 1
)
if not exist "%ROM%" (
    echo output\sc64menu.n64 was not found. Build it first with: make sc64
    exit /b 1
)

echo.
echo Turn the N64 OFF now. The console and the PC must not use the card at
echo the same time. Leave the USB cable connected.
echo.
pause

echo Copying the build to the SD card...
"%DEPLOYER%" sd upload "%ROM%" /sc64menu.n64
if errorlevel 1 goto :failed

echo.
echo The file on the card is now:
"%DEPLOYER%" sd stat /sc64menu.n64
if errorlevel 1 goto :failed

:: Back to "boot the menu from the SD card", in case an earlier upload left
:: the cart set to boot from its memory.
"%DEPLOYER%" reset

echo.
echo Done. Turn the N64 on.
echo.

if /i "%~1"=="/d" "%DEPLOYER%" debug --no-writeback
exit /b 0

:failed
echo.
echo The copy did not complete. The card may hold an incomplete sc64menu.n64:
echo copy output\sc64menu.n64 to the card with a card reader before relying on it.
exit /b 1
