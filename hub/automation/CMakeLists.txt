add_executable(smarthome_automation src/main.cpp)

target_link_libraries(smarthome_automation
    PRIVATE
        paho-mqttpp3-static
        paho-mqtt3as-static
        nlohmann_json::nlohmann_json
        ssl
        crypto
)

target_include_directories(smarthome_automation PRIVATE src)

install(TARGETS smarthome_automation DESTINATION /usr/local/bin)
