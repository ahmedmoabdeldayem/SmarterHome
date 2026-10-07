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
