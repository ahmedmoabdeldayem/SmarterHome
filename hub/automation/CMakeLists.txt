add_executable(smarthome_automation src/main.cpp)

target_link_libraries(smarthome_automation
    PRIVATE
        paho-mqttpp3-static
        paho-mqtt3a-static      # no TLS needed for local broker
        nlohmann_json::nlohmann_json
)

target_include_directories(smarthome_automation PRIVATE src)

install(TARGETS smarthome_automation DESTINATION /usr/local/bin)
