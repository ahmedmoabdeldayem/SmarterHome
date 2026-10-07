cmake_minimum_required(VERSION 3.16)
project(smarthome_hub CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

option(BUILD_AUTOMATION "Build the automation engine (requires Paho MQTT)" ON)
option(BUILD_TESTS      "Build unit tests" ON)

# ── Paho MQTT (only needed for the automation engine) ─────────────────────────
if(BUILD_AUTOMATION)
    FetchContent_Declare(paho-mqtt-c
        GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
        GIT_TAG        v1.3.13
    )
    set(PAHO_BUILD_STATIC        ON  CACHE BOOL "" FORCE)
    set(PAHO_WITH_SSL            OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_SAMPLES       OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(paho-mqtt-c)

    FetchContent_Declare(paho-mqtt-cpp
        GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.cpp.git
        GIT_TAG        v1.4.0
    )
    set(PAHO_MQTT_C_LIBRARIES    paho-mqtt3a-static CACHE STRING "" FORCE)
    set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_SAMPLES       OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_TESTS         OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(paho-mqtt-cpp)
endif()

# ── nlohmann/json (needed by both automation engine and tests) ────────────────
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ── GoogleTest ────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

# ── Subdirectories ─────────────────────────────────────────────────────────────
add_subdirectory(automation)
