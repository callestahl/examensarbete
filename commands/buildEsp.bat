arduino-cli compile --fqbn esp32:esp32:esp32da .\esp32\ble_application

IF %errorlevel% EQU 0 (
    echo Build Completed Successfully 
) ELSE (
    echo ERROR: Build Stopped due to compilation errors
)
set HHHHHHH=1
