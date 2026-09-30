#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace romantasy::ui
{
    struct RecentEvent
    {
        std::string label;
        std::int32_t points = 0;
        std::string when;
    };

    struct EnrollmentCandidate
    {
        std::string name;
        std::string role;
        std::string sourcePlugin;
        std::string referenceFormID;
        std::string baseFormID;
    };

    struct PreferenceOption
    {
        std::string editorID;
        std::string label;
        std::string category;
        std::int32_t points = 0;
    };

    struct FollowerRow
    {
        std::string name;
        std::string levelName;
        std::string nextGoal;
        std::string role;
        std::string profileOrigin;
        std::string sourcePlugin;
        std::string profileFile;
        std::string baseFormID;
        std::string referenceFormID;
        std::int32_t points = 0;
        bool isFollowing = false;
        bool personalityEditable = false;
        bool removable = false;
        std::vector<std::string> likes;
        std::vector<std::string> dislikes;
        std::vector<RecentEvent> recent;
    };

    struct UiState
    {
        std::string message;
        std::vector<FollowerRow> followers;
        bool showAwayFollowers = true;
        bool openWithFavorites = false;
        bool showGainModals = true;
        bool showLossModals = true;
        std::vector<EnrollmentCandidate> candidates;      // from "enrollmentCandidates"
        std::vector<PreferenceOption> preferenceOptions;  // from "preferenceOptions"
        bool developerToolsUnlocked = false;              // stamped by the facade, never parsed
        std::uint32_t revision = 0;                       // stamped by the facade on every push
    };

    struct LevelChange
    {
        std::string followerName;
        std::string previousLevelName;
        std::string levelName;
        std::string nextLevelName;
        std::int32_t points = 0;
        std::int32_t pointsDelta = 0;
        bool isLoss = false;
    };

    // Both parsers accept the JSON RomanceManager already produces
    // (BuildStateJson / BuildLevelChangeJson). Missing or mistyped keys fall
    // back to defaults; neither function throws.
    [[nodiscard]] UiState UiStateFromJson(const nlohmann::json& json);
    [[nodiscard]] LevelChange LevelChangeFromJson(const nlohmann::json& json);

    // Prefer referenceFormID unless it is empty or "00000000", then baseFormID.
    [[nodiscard]] std::string RowIdentity(const FollowerRow& row);
}
