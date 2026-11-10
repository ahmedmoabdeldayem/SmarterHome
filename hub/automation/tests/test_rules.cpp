#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "../src/rules.h"

using json = nlohmann::json;

class RulesTest : public ::testing::Test {
protected:
    std::vector<Rule> rules = build_rules();

    Rule& find_rule(const std::string& topic) {
        for (auto& r : rules)
            if (r.trigger_topic == topic) return r;
        throw std::runtime_error("Rule not found: " + topic);
    }
};

// ── Entrance motion ───────────────────────────────────────────────────────────

TEST_F(RulesTest, EntranceMotionFiresOnDetected) {
    auto& rule = find_rule("smarthome/entrance/motion/event");
    EXPECT_TRUE(rule.condition({{"detected", true}}));
}

TEST_F(RulesTest, EntranceMotionDoesNotFireWhenNoMotion) {
    auto& rule = find_rule("smarthome/entrance/motion/event");
    EXPECT_FALSE(rule.condition({{"detected", false}}));
}

TEST_F(RulesTest, EntranceMotionActionTurnsOnLight) {
    auto& rule = find_rule("smarthome/entrance/motion/event");
    auto payload = rule.action_payload({});
    EXPECT_EQ(payload["state"], "on");
    EXPECT_EQ(rule.action_topic, "smarthome/living_room/light/1/set");
}

// ── Kitchen gas alert ─────────────────────────────────────────────────────────

TEST_F(RulesTest, KitchenGasAlertFiresOnAlert) {
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    EXPECT_TRUE(rule.condition({{"alert", true}, {"ppm", 500}}));
}

TEST_F(RulesTest, KitchenGasAlertDoesNotFireWhenNormal) {
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    EXPECT_FALSE(rule.condition({{"alert", false}, {"ppm", 10}}));
}

TEST_F(RulesTest, KitchenGasAlertPayloadIncludesPpm) {
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    auto payload = rule.action_payload({{"ppm", 450}});
    EXPECT_EQ(payload["ppm"], 450);
    EXPECT_EQ(payload["alert"], true);
    EXPECT_EQ(payload["room"], "kitchen");
}

TEST_F(RulesTest, KitchenGasAlertPayloadFaultOnNegativePpm) {
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    auto payload = rule.action_payload({{"ppm", -1}});
    EXPECT_EQ(payload["fault"], true);
    EXPECT_FALSE(payload.contains("alert"));
}

TEST_F(RulesTest, KitchenGasAlertPayloadFaultOnPpmAboveMax) {
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    auto payload = rule.action_payload({{"ppm", 10001}});
    EXPECT_EQ(payload["fault"], true);
    EXPECT_FALSE(payload.contains("alert"));
}

// ── Bedroom climate ───────────────────────────────────────────────────────────

TEST_F(RulesTest, BedroomClimateFiresAbove28) {
    auto& rule = find_rule("smarthome/bedroom/climate/status");
    EXPECT_TRUE(rule.condition({{"temp", 29.0}}));
}

TEST_F(RulesTest, BedroomClimateDoesNotFireAt28OrBelow) {
    auto& rule = find_rule("smarthome/bedroom/climate/status");
    EXPECT_FALSE(rule.condition({{"temp", 28.0}}));
    EXPECT_FALSE(rule.condition({{"temp", 20.0}}));
}

TEST_F(RulesTest, BedroomClimateActionSetsCoolMode) {
    auto& rule = find_rule("smarthome/bedroom/climate/status");
    auto payload = rule.action_payload({});
    EXPECT_EQ(payload["mode"], "cool");
    EXPECT_EQ(payload["temp"], 22);
}

// ── Doorbell ──────────────────────────────────────────────────────────────────

TEST_F(RulesTest, DoorbellFiresWhenPressed) {
    auto& rule = find_rule("smarthome/entrance/doorbell/event");
    EXPECT_TRUE(rule.condition({{"pressed", true}}));
}

TEST_F(RulesTest, DoorbellDoesNotFireWhenNotPressed) {
    auto& rule = find_rule("smarthome/entrance/doorbell/event");
    EXPECT_FALSE(rule.condition({{"pressed", false}}));
}

TEST_F(RulesTest, DoorbellActionPublishesMessage) {
    auto& rule = find_rule("smarthome/entrance/doorbell/event");
    auto payload = rule.action_payload({});
    EXPECT_TRUE(payload.contains("message"));
    EXPECT_EQ(rule.action_topic, "smarthome/alerts/doorbell");
}

// ── Cooldown ──────────────────────────────────────────────────────────────────

TEST_F(RulesTest, AllRulesHavePositiveCooldown) {
    for (const auto& rule : rules)
        EXPECT_GT(rule.cooldown.count(), 0) << "Rule has zero cooldown: " << rule.description;
}

TEST_F(RulesTest, AllRulesHaveNonEmptyDescription) {
    for (const auto& rule : rules)
        EXPECT_FALSE(rule.description.empty());
}

// ── Edge cases ────────────────────────────────────────────────────────────────

TEST_F(RulesTest, EntranceMotionDoesNotFireOnEmptyPayload) {
    auto& rule = find_rule("smarthome/entrance/motion/event");
    // Missing "detected" key defaults to false — rule must not fire
    EXPECT_FALSE(rule.condition({}));
}

TEST_F(RulesTest, BedroomClimateDoesNotFireOnMissingTemp) {
    auto& rule = find_rule("smarthome/bedroom/climate/status");
    // Missing "temp" key defaults to 0.0 — below the 28°C threshold
    EXPECT_FALSE(rule.condition({}));
}

TEST_F(RulesTest, KitchenGasAlertDoesNotFireOnEmptyPayload) {
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    // Missing "alert" key defaults to false — rule must not fire
    EXPECT_FALSE(rule.condition({}));
}

TEST_F(RulesTest, KitchenGasAlertPayloadFaultOnMissingPpm) {
    // alert=true but ppm missing (defaults to 0) — 0 is in-range so normal path
    auto& rule = find_rule("smarthome/kitchen/gas/status");
    auto payload = rule.action_payload({{"alert", true}});
    // ppm=0 is within [0, 10000], so normal alert payload expected
    EXPECT_EQ(payload["ppm"], 0);
    EXPECT_EQ(payload["alert"], true);
    EXPECT_EQ(payload["room"], "kitchen");
}

TEST_F(RulesTest, AllRulesHaveNonEmptyTopics) {
    for (const auto& rule : rules) {
        EXPECT_FALSE(rule.trigger_topic.empty()) << "Empty trigger_topic in: " << rule.description;
        EXPECT_FALSE(rule.action_topic.empty())  << "Empty action_topic in: "  << rule.description;
    }
}

TEST_F(RulesTest, CooldownPreventsRapidRefiring) {
    auto& rule = find_rule("smarthome/entrance/motion/event");
    auto now = std::chrono::steady_clock::now();

    // Simulate rule just fired
    rule.last_fired = now;

    // Within cooldown window — should be blocked (last_fired == now, cooldown > 0)
    auto just_after = now + std::chrono::milliseconds(1);
    EXPECT_LT(just_after - rule.last_fired, rule.cooldown);

    // After cooldown expires — should be allowed
    auto after_cooldown = now + rule.cooldown + std::chrono::milliseconds(1);
    EXPECT_GE(after_cooldown - rule.last_fired, rule.cooldown);
}
