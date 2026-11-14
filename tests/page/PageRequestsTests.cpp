#include "check.h"

#include "ui/PageRequests.h"

#include <string>

using namespace romantasy::ui;
using nlohmann::json;

TEST(Settings_SingleBoolean)
{
    const auto s = ParseSettingsChange(R"({"showGainModals":false})");
    CHECK(s.has_value());
    CHECK(s->showGainModals.has_value() && !*s->showGainModals);
    CHECK(!s->openWithFavorites && !s->showLossModals && !s->showAwayFollowers);
}

TEST(Settings_DetailedLog)
{
    const auto s = ParseSettingsChange(R"({"detailedLog":true})");
    CHECK(s.has_value());
    CHECK(s->detailedLog.has_value() && *s->detailedLog);
    CHECK(!s->openWithFavorites && !s->showGainModals && !s->showLossModals && !s->showAwayFollowers);
}

TEST(Settings_Pace)
{
    const auto s = ParseSettingsChange(R"({"pace":"fast"})");
    CHECK(s.has_value());
    CHECK(s->pace.has_value() && *s->pace == romantasy::pacing::Pace::Fast);
    CHECK(!s->tierMoment && !s->openWithFavorites && !s->showGainModals && !s->showLossModals && !s->showAwayFollowers && !s->detailedLog);
}

TEST(Settings_PaceAndTierMomentWithBoolean)
{
    const auto s = ParseSettingsChange(R"({"tierMoment":"immediate","showGainModals":false})");
    CHECK(s.has_value());
    CHECK(s->tierMoment.has_value() && *s->tierMoment == romantasy::pacing::TierMoment::Immediate);
    CHECK(s->showGainModals.has_value() && !*s->showGainModals);
    CHECK(!s->pace);
}

TEST(Settings_UnknownPaceIsRefusedAlone)
{
    CHECK(!ParseSettingsChange(R"({"pace":"turbo"})"));
    CHECK(!ParseSettingsChange(R"({"pace":"Fast"})"));
    CHECK(!ParseSettingsChange(R"({"pace":2})"));
    CHECK(!ParseSettingsChange(R"({"tierMoment":"later"})"));
}

TEST(Settings_UnknownPaceKeepsOtherFields)
{
    const auto s = ParseSettingsChange(R"({"pace":"turbo","detailedLog":true})");
    CHECK(s.has_value());
    CHECK(!s->pace);
    CHECK(s->detailedLog.has_value() && *s->detailedLog);
}

TEST(Settings_Whispers)
{
    const auto off = ParseSettingsChange(R"({"whispers":false})");
    CHECK(off.has_value() && off->whispers.has_value() && !*off->whispers);
    CHECK(off.has_value() && !off->whisperCorner && !off->pace && !off->detailedLog);
    const auto on = ParseSettingsChange(R"({"whispers":true})");
    CHECK(on.has_value() && on->whispers.has_value() && *on->whispers);
}

TEST(Settings_WhisperCorner)
{
    using romantasy::ui::whispers::WhisperCorner;
    const std::pair<const char*, WhisperCorner> cases[] = {
        { R"({"whisperCorner":"topLeft"})", WhisperCorner::TopLeft },
        { R"({"whisperCorner":"topRight"})", WhisperCorner::TopRight },
        { R"({"whisperCorner":"bottomLeft"})", WhisperCorner::BottomLeft },
        { R"({"whisperCorner":"bottomRight"})", WhisperCorner::BottomRight },
    };
    for (const auto& [text, corner] : cases) {
        const auto s = ParseSettingsChange(text);
        CHECK(s.has_value() && s->whisperCorner.has_value() && *s->whisperCorner == corner);
        CHECK(s.has_value() && !s->whispers && !s->pace && !s->tierMoment);
    }
}

TEST(Settings_BadWhisperValuesAreRefusedAlone)
{
    CHECK(!ParseSettingsChange(R"({"whisperCorner":"middle"})"));
    CHECK(!ParseSettingsChange(R"({"whisperCorner":"TopRight"})"));
    CHECK(!ParseSettingsChange(R"({"whisperCorner":"top_right"})"));
    CHECK(!ParseSettingsChange(R"({"whisperCorner":1})"));
    CHECK(!ParseSettingsChange(R"({"whispers":"yes"})"));
    CHECK(!ParseSettingsChange(R"({"whispers":1})"));
    CHECK(!ParseSettingsChange(R"({"whispers":null})"));
}

TEST(Settings_BadWhisperValueKeepsOtherFields)
{
    const auto s = ParseSettingsChange(R"({"whisperCorner":"middle","whispers":0,"pace":"slow"})");
    CHECK(s.has_value() && !s->whisperCorner && !s->whispers);
    CHECK(s.has_value() && s->pace.has_value() && *s->pace == romantasy::pacing::Pace::Slow);
}

TEST(Settings_RejectsBadInput)
{
    CHECK(!ParseSettingsChange(R"({"showGainModals":"no"})"));
    CHECK(!ParseSettingsChange("{}"));
    CHECK(!ParseSettingsChange("[]"));
    CHECK(!ParseSettingsChange("not json"));
    CHECK(!ParseSettingsChange(""));
}

TEST(Settings_IgnoresExtraKeys)
{
    const auto s = ParseSettingsChange(R"({"showAwayFollowers":true,"extra":1})");
    CHECK(s.has_value());
    CHECK(s->showAwayFollowers.has_value() && *s->showAwayFollowers);
}

TEST(Bond_Enroll)
{
    const auto b = ParseBondRequest(R"({"kind":"enroll","identity":"0001A6A6","preferences":[["ROM_Murders",-1],["ROM_BooksRead",0]]})");
    CHECK(b.has_value());
    CHECK(b->kind == BondRequest::Kind::Enroll);
    CHECK(b->identity == "0001A6A6");
    CHECK(b->preferences.size() == 2);
    CHECK(b->preferences[0].first == "ROM_Murders" && b->preferences[0].second == -1);
}

TEST(Bond_StrongWeights)
{
    const auto b = ParseBondRequest(R"({"kind":"replace","identity":"1","preferences":[["ROM_Murders",-2],["ROM_BooksRead",2]]})");
    CHECK(b.has_value());
    CHECK(b.has_value() && b->preferences.size() == 2);
    CHECK(b.has_value() && b->preferences[0].second == -2 && b->preferences[1].second == 2);
}

TEST(Bond_ResetNeedsNoPreferences)
{
    const auto b = ParseBondRequest(R"({"kind":"reset","identity":"ad005901"})");
    CHECK(b.has_value());
    CHECK(b->kind == BondRequest::Kind::Reset);
    CHECK(b->preferences.empty());
}

TEST(Bond_EnrollWithoutPreferences)
{
    CHECK(!ParseBondRequest(R"({"kind":"enroll","identity":"0001A6A6"})"));
    CHECK(!ParseBondRequest(R"({"kind":"replace","identity":"0001A6A6"})"));
}

TEST(Bond_BadIdentity)
{
    for (const char* id : { "\"0x1A6A6\"", "\"zz\"", "\"\"", "\"123456789\"", "5" }) {
        const std::string text = std::string(R"({"kind":"reset","identity":)") + id + "}";
        CHECK(!ParseBondRequest(text));
    }
}

TEST(Bond_BadKind)
{
    CHECK(!ParseBondRequest(R"({"kind":"delete","identity":"1"})"));
    CHECK(!ParseBondRequest(R"({"identity":"1"})"));
}

TEST(Bond_BadPreferences)
{
    const auto make = [](const json& prefs) {
        return json{ { "kind", "enroll" }, { "identity", "1" }, { "preferences", prefs } }.dump();
    };
    CHECK(ParseBondRequest(make(json::array())).has_value());
    CHECK(!ParseBondRequest(make(json::parse(R"([["a",3]])"))));
    CHECK(!ParseBondRequest(make(json::parse(R"([["a",-3]])"))));
    CHECK(!ParseBondRequest(make(json::parse(R"([["a",1,1]])"))));
    CHECK(!ParseBondRequest(make(json::parse(R"([["a",0.5]])"))));
    CHECK(!ParseBondRequest(make(json::parse(R"([[1,0]])"))));
    CHECK(!ParseBondRequest(make(json::parse(R"([["",0]])"))));
    CHECK(!ParseBondRequest(make(json::parse(R"("x")"))));

    json many = json::array();
    for (int i = 0; i < 256; ++i) many.push_back(json::array({ "k" + std::to_string(i), 0 }));
    CHECK(ParseBondRequest(make(many)).has_value());
    many.push_back(json::array({ "extra", 0 }));
    CHECK(!ParseBondRequest(make(many)));

    CHECK(ParseBondRequest(make(json::array({ json::array({ std::string(128, 'a'), 0 }) }))).has_value());
    CHECK(!ParseBondRequest(make(json::array({ json::array({ std::string(129, 'a'), 0 }) }))));
}

TEST(Debug_Points)
{
    const auto a = ParseDebugRequest(R"({"points":500})");
    CHECK(a.has_value() && a->points && *a->points == 500);
    const auto b = ParseDebugRequest(R"({"points":-500})");
    CHECK(b.has_value() && b->points && *b->points == -500);
    CHECK(ParseDebugRequest(R"({"points":10000})").has_value());
    CHECK(!ParseDebugRequest(R"({"points":10001})"));
    CHECK(!ParseDebugRequest(R"({"points":-10001})"));
    CHECK(!ParseDebugRequest(R"({"points":1.5})"));
    CHECK(!ParseDebugRequest(R"({"points":1e3})"));
    CHECK(!ParseDebugRequest(R"({"points":"500"})"));
}

TEST(Debug_Stat)
{
    const auto a = ParseDebugRequest(R"({"stat":"Murders","delta":1})");
    CHECK(a.has_value() && !a->points && a->stat == "Murders" && a->delta == 1);
    CHECK(!ParseDebugRequest(R"({"stat":"","delta":1})"));
    CHECK(!ParseDebugRequest(R"({"stat":"Murders","delta":101})"));
    CHECK(!ParseDebugRequest(R"({"stat":"Murders","delta":-101})"));
    CHECK(!ParseDebugRequest(R"({"stat":"Murders"})"));
    CHECK(!ParseDebugRequest("{}"));
    CHECK(ParseDebugRequest(json{ { "stat", std::string(64, 'a') }, { "delta", -100 } }.dump()).has_value());
    CHECK(!ParseDebugRequest(json{ { "stat", std::string(65, 'a') }, { "delta", 1 } }.dump()));
}

TEST(DevUnlock_Password)
{
    const auto a = ParseDevUnlock(R"({"password":"abc"})");
    CHECK(a.has_value() && *a == "abc");
    CHECK(ParseDevUnlock(json{ { "password", std::string(64, 'a') } }.dump()).has_value());
    CHECK(!ParseDevUnlock(json{ { "password", std::string(65, 'a') } }.dump()));
    CHECK(!ParseDevUnlock(R"({"password":5})"));
    CHECK(!ParseDevUnlock("{}"));
}

TEST(OversizedPayload)
{
    std::string text = R"({"password":"abc","pad":")";
    text.append(kMaxPayloadBytes + 1 - text.size() - 2, 'x');
    text += "\"}";
    CHECK(text.size() == kMaxPayloadBytes + 1);
    CHECK(json::accept(text));
    CHECK(!ParseSettingsChange(text));
    CHECK(!ParseBondRequest(text));
    CHECK(!ParseDebugRequest(text));
    CHECK(!ParseDevUnlock(text));
    CHECK(!ParsePopupAck(text));
}

TEST(Stamp_AddsRevisionAndUnlock)
{
    const json s = StampState({ { "message", "x" } }, 7, true, json::object());
    CHECK(s["revision"] == 7);
    CHECK(s["developerToolsUnlocked"] == true);
    CHECK(s["message"] == "x");
    CHECK(StampState(json::array(), 1, false, json::object()).is_array());
}

TEST(Stamp_AddsHealth)
{
    const json health = { { "status", "warn" }, { "items", json::array() } };
    const json s = StampState({ { "message", "x" } }, 3, false, health);
    CHECK(s.contains("health"));
    CHECK(s["health"] == health);
    CHECK(s["message"] == "x");
    CHECK(!StampState(json::array(), 1, false, health).is_object());
}

TEST(PopupAck_Seq)
{
    const auto a = ParsePopupAck(R"({"seq":7})");
    CHECK(a.has_value() && *a == 7);
    CHECK(ParsePopupAck(R"({"seq":0})") == std::optional<std::uint32_t>(0));
    CHECK(ParsePopupAck(R"({"seq":2147483647})") == std::optional<std::uint32_t>(2147483647u));
    CHECK(!ParsePopupAck(R"({"seq":2147483648})"));
    CHECK(!ParsePopupAck(R"({"seq":-1})"));
    CHECK(!ParsePopupAck(R"({"seq":1.5})"));
    CHECK(!ParsePopupAck(R"({"seq":1e3})"));
    CHECK(!ParsePopupAck(R"({"seq":"7"})"));
    CHECK(!ParsePopupAck(R"({"seq":null})"));
    CHECK(!ParsePopupAck(R"({"seq":7,"extra":1})"));
    CHECK(!ParsePopupAck("{}"));
    CHECK(!ParsePopupAck("[7]"));
    CHECK(!ParsePopupAck("7"));
    CHECK(!ParsePopupAck(""));
    CHECK(!ParsePopupAck("not json"));
}

TEST(JournalRequest_Parses)
{
    CHECK(ParseJournalRequest(R"({"identity":"00ABCDEF"})") == std::optional<std::string>("00ABCDEF"));
    CHECK(ParseJournalRequest(R"({"identity":"1a"})") == std::optional<std::string>("1a"));
    CHECK(ParseJournalRequest(R"({"identity":"12345678"})").has_value());
}

TEST(JournalRequest_RejectsBadInput)
{
    CHECK(!ParseJournalRequest(R"({"identity":""})"));
    CHECK(!ParseJournalRequest(R"({"identity":"123456789"})"));
    CHECK(!ParseJournalRequest(R"({"identity":"xyz"})"));
    CHECK(!ParseJournalRequest(R"({"identity":12})"));
    CHECK(!ParseJournalRequest(R"({"other":"12"})"));
    CHECK(!ParseJournalRequest("{}"));
    CHECK(!ParseJournalRequest("[]"));
    CHECK(!ParseJournalRequest("\"12\""));
    CHECK(!ParseJournalRequest("not json"));
    CHECK(!ParseJournalRequest(""));
    CHECK(!ParseJournalRequest(std::string(kMaxPayloadBytes + 1, ' ')));
    CHECK(!ParseJournalRequest(std::string(5000, '[') + std::string(5000, ']')));
}

TEST(Settings_RosterOrder)
{
    for (const char* key : { "closest", "name", "recent" }) {
        const auto s = ParseSettingsChange(std::string(R"({"rosterOrder":")") + key + R"("})");
        CHECK(s.has_value() && s->rosterOrder.has_value() && *s->rosterOrder == key);
        CHECK(s.has_value() && !s->ledgerSize && !s->highContrast && !s->reduceMotion && !s->pace);
    }
    CHECK(ParseRosterOrder("name") == std::string_view("name"));
    CHECK(!ParseRosterOrder("Name") && !ParseRosterOrder("") && !ParseRosterOrder("alphabetical"));
}

TEST(Settings_LedgerSize)
{
    for (const char* key : { "standard", "large", "largest" }) {
        const auto s = ParseSettingsChange(std::string(R"({"ledgerSize":")") + key + R"("})");
        CHECK(s.has_value() && s->ledgerSize.has_value() && *s->ledgerSize == key);
        CHECK(s.has_value() && !s->rosterOrder && !s->highContrast && !s->reduceMotion);
    }
    CHECK(ParseLedgerSize("largest") == std::string_view("largest"));
    CHECK(!ParseLedgerSize("Large") && !ParseLedgerSize("huge") && !ParseLedgerSize(""));
}

TEST(Settings_ContrastAndMotion)
{
    const auto on = ParseSettingsChange(R"({"highContrast":true,"reduceMotion":false})");
    CHECK(on.has_value() && on->highContrast && *on->highContrast && on->reduceMotion && !*on->reduceMotion);
    CHECK(on.has_value() && !on->rosterOrder && !on->ledgerSize);
}

TEST(Settings_BadComfortValuesAreRefusedAlone)
{
    CHECK(!ParseSettingsChange(R"({"rosterOrder":"Name"})"));
    CHECK(!ParseSettingsChange(R"({"rosterOrder":1})"));
    CHECK(!ParseSettingsChange(R"({"ledgerSize":"huge"})"));
    CHECK(!ParseSettingsChange(R"({"ledgerSize":null})"));
    CHECK(!ParseSettingsChange(R"({"highContrast":"yes"})"));
    CHECK(!ParseSettingsChange(R"({"reduceMotion":1})"));
    const auto s = ParseSettingsChange(R"({"rosterOrder":"nope","ledgerSize":"large","highContrast":0,"reduceMotion":true})");
    CHECK(s.has_value() && !s->rosterOrder && !s->highContrast);
    CHECK(s.has_value() && s->ledgerSize && *s->ledgerSize == "large" && s->reduceMotion && *s->reduceMotion);
}

TEST(Settings_SkyrimNetSwitches)
{
    const auto both = ParseSettingsChange(R"({"skyrimNetBridge":false,"skyrimNetConversation":true})");
    CHECK(both.has_value() && both->skyrimNetBridge && !*both->skyrimNetBridge);
    CHECK(both.has_value() && both->skyrimNetConversation && *both->skyrimNetConversation);
    CHECK(both.has_value() && !both->whispers && !both->reduceMotion && !both->pace);
    const auto one = ParseSettingsChange(R"({"skyrimNetConversation":false})");
    CHECK(one.has_value() && one->skyrimNetConversation && !*one->skyrimNetConversation && !one->skyrimNetBridge);
    // Not booleans: refused alone, and dropped beside a good key.
    CHECK(!ParseSettingsChange(R"({"skyrimNetBridge":"false"})"));
    CHECK(!ParseSettingsChange(R"({"skyrimNetConversation":0})"));
    CHECK(!ParseSettingsChange(R"({"skyrimNetBridge":null})"));
    const auto mixed = ParseSettingsChange(R"({"skyrimNetBridge":1,"skyrimNetConversation":false})");
    CHECK(mixed.has_value() && !mixed->skyrimNetBridge && mixed->skyrimNetConversation && !*mixed->skyrimNetConversation);
}

TEST(Settings_OStimSwitches)
{
    const auto both = ParseSettingsChange(R"({"ostimMoments":false,"ostimHold":true})");
    CHECK(both.has_value() && both->ostimMoments && !*both->ostimMoments);
    CHECK(both.has_value() && both->ostimHold && *both->ostimHold);
    CHECK(both.has_value() && !both->skyrimNetBridge && !both->skyrimNetConversation && !both->whispers && !both->pace);
    const auto moments = ParseSettingsChange(R"({"ostimMoments":true})");
    CHECK(moments.has_value() && moments->ostimMoments && *moments->ostimMoments && !moments->ostimHold);
    const auto hold = ParseSettingsChange(R"({"ostimHold":false})");
    CHECK(hold.has_value() && hold->ostimHold && !*hold->ostimHold && !hold->ostimMoments);
    // Not booleans: refused alone, and dropped beside a good key.
    CHECK(!ParseSettingsChange(R"({"ostimMoments":"false"})"));
    CHECK(!ParseSettingsChange(R"({"ostimHold":0})"));
    CHECK(!ParseSettingsChange(R"({"ostimMoments":null})"));
    const auto mixed = ParseSettingsChange(R"({"ostimMoments":1,"ostimHold":false})");
    CHECK(mixed.has_value() && !mixed->ostimMoments && mixed->ostimHold && !*mixed->ostimHold);
}
