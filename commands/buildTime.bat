arduino-cli compile --fqbn esp32:esp32:esp32da .\sketch_nov6a\bluetooth_and_screen --build-path=build --only-compilation-database

IF %errorlevel% EQU 0 (
    echo Build Completed Successfully 
    REM arduino-cli upload -p COM5 --fqbn esp32:esp32:esp32da .\sketch_nov6a\bluetooth_and_screen
) ELSE (
    echo ERROR: Build Stopped due to compilation errors
)
set HHHHHHH=1
