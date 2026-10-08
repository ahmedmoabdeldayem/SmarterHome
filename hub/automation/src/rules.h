#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ── Rule definition ───────────────────────────────────────────────────────────
struct Rule {
    // Topic this rule listens to
    std::string trigger_topic;

    // Returns true if the rule should fire given the incoming payload
    std::function<bool(const json&)> condition;

    // Topic to publish the action to
    std::string action_topic;

    // Returns the JSON payload to publish (receives the trigger payload)
    std::function<json(const json&)> action_payload;

    // Minimum time between firings (prevents rapid re-triggering)
    std::chrono::milliseconds cooldown{10000};

    // Human-readable description (logged when rule fires)
    std::string description;

    // Internal: last time this rule fired
    std::chrono::steady_clock::time_point last_fired{};
};

// ── Rule definitions ──────────────────────────────────────────────────────────
// Add or remove rules here. No changes needed anywhere else.

inline std::vector<Rule> build_rules() {
    return {
        {
            .trigger_topic  = "smarthome/entrance/motion/event",
            .condition      = [](const json& p) {
                return p.value("detected", false);
            },
            .action_topic   = "smarthome/living_room/light/1/set",
            .action_payload = [](const json&) -> json {
                return {{"state", "on"}};
            },
            .cooldown       = std::chrono::milliseconds(5000),
            .description    = "Motion at entrance → turn on living room light",
        },
        {
            .trigger_topic  = "smarthome/kitchen/gas/status",
            .condition      = [](const json& p) {
                return p.value("alert", false);
            },
            .action_topic   = "smarthome/alerts/gas",
            .action_payload = [](const json& p) -> json {
                int ppm = p.value("ppm", 0);
                if (ppm < 0 || ppm > 10000) {
                    return {{"room", "kitchen"}, {"fault", true},
                            {"message", "Gas sensor reading out of range"}};
                }
                return {{"room", "kitchen"}, {"ppm", ppm}, {"alert", true}};
            },
            .cooldown       = std::chrono::milliseconds(10000),
            .description    = "Gas alert in kitchen → broadcast gas alert",
        },
        {
            .trigger_topic  = "smarthome/bedroom/climate/status",
            .condition      = [](const json& p) {
                return p.value("temp", 0.0) > 28.0;
            },
            .action_topic   = "smarthome/bedroom/ac/set",
            .action_payload = [](const json&) -> json {
                return {{"mode", "cool"}, {"temp", 22}};
            },
            .cooldown       = std::chrono::milliseconds(60000),
            .description    = "Bedroom temp > 28°C → auto-cool to 22°C",
        },
        {
            .trigger_topic  = "smarthome/entrance/doorbell/event",
            .condition      = [](const json& p) {
                return p.value("pressed", false);
            },
            .action_topic   = "smarthome/alerts/doorbell",
            .action_payload = [](const json&) -> json {
                return {{"message", "Someone is at the door!"}};
            },
            .cooldown       = std::chrono::milliseconds(3000),
            .description    = "Doorbell pressed → publish doorbell alert",
        },
    };
}
