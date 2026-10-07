cmake_minimum_required(VERSION 3.16)
project(smarthome_hub CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch shared dependencies (downloaded once, shared by both binaries) ───────
include(FetchContent)

# Paho MQTT C
FetchContent_Declare(paho-mqtt-c
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
    GIT_TAG        v1.3.13
)
set(PAHO_BUILD_STATIC        ON  CACHE BOOL "" FORCE)
set(PAHO_WITH_SSL            ON  CACHE BOOL "" FORCE)
set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_SAMPLES       OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(paho-mqtt-c)

# Paho MQTT C++
FetchContent_Declare(paho-mqtt-cpp
    GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
    GIT_TAG        v1.4.0
)
set(PAHO_MQTT_C_LIBRARIES    paho-mqtt3as-static CACHE STRING "" FORCE)
set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_SAMPLES       OFF CACHE BOOL "" FORCE)
set(PAHO_BUILD_TESTS         OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(paho-mqtt-cpp)

# nlohmann/json (header-only JSON library)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Subdirectories ─────────────────────────────────────────────────────────────
add_subdirectory(bridge)
add_subdirectory(automation)
