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
