#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "romance/Pacing.h"
#include "ui/Whispers.h"

namespace romantasy::ui
{
    struct SettingsChange
    {
        std::optional<bool> openWithFavorites;
        std::optional<bool> showGainModals;
        std::optional<bool> showLossModals;
        std::optional<bool> showAwayFollowers;
        std::optional<bool> detailedLog;
        std::optional<romantasy::pacing::Pace> pace;
        std::optional<romantasy::pacing::TierMoment> tierMoment;
        std::optional<bool> whispers;
        std::optional<romantasy::ui::whispers::WhisperCorner> whisperCorner;
        std::optional<std::string> rosterOrder;  // "closest", "name" or "recent"
        std::optional<std::string> ledgerSize;   // "standard", "large" or "largest"
        std::optional<bool> highContrast;
        std::optional<bool> reduceMotion;
        std::optional<bool> skyrimNetBridge;
        std::optional<bool> skyrimNetConversation;
        std::optional<bool> ostimMoments;
        std::optional<bool> ostimHold;
    };

    // Exact, case-sensitive keys only; the result is the canonical key (static storage), or nullopt.
    [[nodiscard]] constexpr std::optional<std::string_view> ParseRosterOrder(std::string_view key) noexcept
    {
        if (key == "closest") return std::string_view("closest");
        if (key == "name") return std::string_view("name");
        if (key == "recent") return std::string_view("recent");
        return std::nullopt;
    }

    [[nodiscard]] constexpr std::optional<std::string_view> ParseLedgerSize(std::string_view key) noexcept
    {
        if (key == "standard") return std::string_view("standard");
        if (key == "large") return std::string_view("large");
        if (key == "largest") return std::string_view("largest");
        return std::nullopt;
    }

    // A player-created bond action for the facade. `preferences` is the
    // complete page (every deed, neutral unless stamped) for Enroll / Replace.
    struct BondRequest
    {
        enum class Kind { Enroll, Replace, Reset, Remove };
        Kind kind = Kind::Enroll;
        std::string identity;  // hex FormID text
        std::vector<std::pair<std::string, std::int32_t>> preferences;
    };

    struct DebugRequest
    {
        std::optional<std::int32_t> points;  // DebugAddPoints
        std::string stat;                    // DebugApplyStat(stat, delta) when `points` is empty
        std::int32_t delta = 0;
    };

    // The largest page payload the plugin will look at.
    inline constexpr std::size_t kMaxPayloadBytes = 65536;

    // Validate what the page sent. None of these throw; anything malformed,
    // oversized or out of range is nullopt.
    [[nodiscard]] std::optional<SettingsChange> ParseSettingsChange(std::string_view text);
    [[nodiscard]] std::optional<BondRequest> ParseBondRequest(std::string_view text);
    [[nodiscard]] std::optional<DebugRequest> ParseDebugRequest(std::string_view text);
    [[nodiscard]] std::optional<std::string> ParseDevUnlock(std::string_view text);
    // romantasyJournalOpen: an object whose "identity" is 1 to 8 hex digits, as a bond request's is.
    [[nodiscard]] std::optional<std::string> ParseJournalRequest(std::string_view text);
    // romantasyPopupShown / romantasyPopupDismissed: an object with exactly the key "seq", an integer in [0, 2^31 - 1].
    [[nodiscard]] std::optional<std::uint32_t> ParsePopupAck(std::string_view text);

    // Adds `revision`, `developerToolsUnlocked` and `health` to an object; anything else is returned unchanged.
    [[nodiscard]] nlohmann::json StampState(
        nlohmann::json state, std::uint32_t revision, bool developerToolsUnlocked, nlohmann::json health);
}
