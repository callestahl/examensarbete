REM arduino-cli compile --fqbn esp32:esp32:esp32da .\sketch_nov6a\wifi_application --build-path=build --only-compilation-database
arduino-cli compile --fqbn esp32:esp32:esp32da .\sketch_nov6a\spp_application

IF %errorlevel% EQU 0 (
    echo Build Completed Successfully 
    REM arduino-cli upload -p COM5 --fqbn esp32:esp32:esp32da .\sketch_nov6a\bluetooth_and_screen
) ELSE (
    echo ERROR: Build Stopped due to compilation errors
)
set HHHHHHH=1
