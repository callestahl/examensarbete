cmake_minimum_required(VERSION 3.16.3) 
project(CBluetooth)
set(EXE ${PROJECT_NAME})

set(GLFW_USE_HYBRID_HPG ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 
set(STANDARD_FLAGS "-WL -MP -nologo -Gm- -WX -W4 -wd4505 -wd4100 -wd4201 -wd4133 -wd4057 -wd4189 -wd4101 -wd4127 -wd4311")
set(CMAKE_CXX_FLAGS_DEBUG "${STANDARD_FLAGS} -Od -Oi -Z7 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${STANDARD_FLAGS} -O2 -Oi -Z7 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${STANDARD_FLAGS} -Od -Oi -Z7 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${STANDARD_FLAGS} -O2 -Oi -Z7 -DNDEBUG")

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/c/*.c")

#add_subdirectory("${CMAKE_SOURCE_DIR}/lib/glfw")

add_executable(${EXE} ${SOURCES})

target_include_directories(${EXE}
    PUBLIC "${CMAKE_SOURCE_DIR}/c"
    #PUBLIC "${CMAKE_SOURCE_DIR}/lib/glad/include"
    #PUBLIC "${CMAKE_SOURCE_DIR}/lib/glfw/include"
)

#target_link_libraries(${EXE} glfw)

#target_link_options(${EXE} PRIVATE "/SUBSYSTEM:WINDOWS" PRIVATE "/ENTRY:mainCRTStartup") 
# For gcc and g++, clang
#target_link_options(${EXE} PRIVATE "-mwindows") 
target_link_libraries(${EXE} user32 Winmm BluetoothAPIs Ws2_32)
