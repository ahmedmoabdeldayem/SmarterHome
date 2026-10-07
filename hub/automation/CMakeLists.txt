if(BUILD_AUTOMATION)
    add_executable(smarthome_automation src/main.cpp)

    target_link_libraries(smarthome_automation
        PRIVATE
            paho-mqttpp3-static
            paho-mqtt3a-static
            nlohmann_json::nlohmann_json
    )

    target_include_directories(smarthome_automation PRIVATE src)

    install(TARGETS smarthome_automation DESTINATION /usr/local/bin)
endif()

# ── Tests ──────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    add_executable(test_rules tests/test_rules.cpp)

    target_link_libraries(test_rules
        PRIVATE
            nlohmann_json::nlohmann_json
            GTest::gtest_main
    )

    target_include_directories(test_rules PRIVATE src)
    add_test(NAME RulesTests COMMAND test_rules)
endif()
