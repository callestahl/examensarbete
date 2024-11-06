cmake_minimum_required(VERSION 3.12)

# Include and initialize the Pico SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
pico_sdk_init()

# Set the project name
project(main)

# Add the executable
add_executable(main
    src/main.c
    src/MCP4822.c
)

# Include directories for header files
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link the executable to the pico_stdlib and hardware_adc libraries
target_link_libraries(main pico_stdlib hardware_gpio hardware_spi hardware_adc)

# Enable USB output, disable UART output
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(main)

# Treat warnings as errors
target_compile_options(main PRIVATE -Werror)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)