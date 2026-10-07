cmake_minimum_required(VERSION 3.16)
project(smarthome_bridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch Paho MQTT C (required by Paho MQTT C++) ─────────────────────────────
include(FetchContent)

FetchContent_Declare(paho-mqtt-c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG        v1.3.13
)
set(PAHO_BUILD_STATIC      ON  CACHE BOOL "" FORCE)
set(PAHO_WITH_SSL          ON  CACHE BOOL "" FORCE)
set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_SAMPLES     OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(paho-mqtt-c)

# ── Fetch Paho MQTT C++ ────────────────────────────────────────────────────────
FetchContent_Declare(paho-mqtt-cpp
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
    GIT_TAG        v1.4.0
)
set(PAHO_MQTT_C_LIBRARIES  paho-mqtt3as-static CACHE STRING "" FORCE)
set(PAHO_BUILD_STATIC      ON  CACHE BOOL "" FORCE)
set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_SAMPLES     OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(paho-mqtt-cpp)

# ── Bridge executable ─────────────────────────────────────────────────────────
add_executable(smarthome_bridge src/main.cpp)

target_link_libraries(smarthome_bridge
    PRIVATE
        paho-mqttpp3-static
        paho-mqtt3as-static
        ssl
        crypto
)

target_include_directories(smarthome_bridge PRIVATE src)

install(TARGETS smarthome_bridge DESTINATION /usr/local/bin)
