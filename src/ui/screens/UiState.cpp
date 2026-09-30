#include "ui/screens/UiState.h"

#include <nlohmann/json.hpp>

#include <format>

namespace romantasy::ui
{
    namespace
    {
        std::string ReadString(const nlohmann::json& json, const char* key)
        {
            const auto found = json.find(key);
            if (found == json.end() || !found->is_string()) {
                return {};
            }
            return found->get<std::string>();
        }

        std::int32_t ReadInt(const nlohmann::json& json, const char* key)
        {
            const auto found = json.find(key);
            if (found == json.end() || !found->is_number_integer()) {
                return 0;
            }
            return found->get<std::int32_t>();
        }

        bool ReadBool(const nlohmann::json& json, const char* key, bool fallback)
        {
            const auto found = json.find(key);
            if (found == json.end() || !found->is_boolean()) {
                return fallback;
            }
            return found->get<bool>();
        }

        std::string ReadFormID(const nlohmann::json& json, const char* key)
        {
            const auto found = json.find(key);
            if (found == json.end()) {
                return {};
            }
            if (found->is_string()) {
                return found->get<std::string>();
            }
            if (found->is_number_unsigned() || found->is_number_integer()) {
                return std::format("{:08X}", found->get<std::uint32_t>());
            }
            return {};
        }

        std::vector<std::string> ReadStringArray(const nlohmann::json& json, const char* key)
        {
            std::vector<std::string> out;
            const auto found = json.find(key);
            if (found == json.end() || !found->is_array()) {
                return out;
            }
            for (const auto& item : *found) {
                if (item.is_string()) {
                    out.push_back(item.get<std::string>());
                }
            }
            return out;
        }

        FollowerRow ReadFollower(const nlohmann::json& json)
        {
            FollowerRow row;
            row.name = ReadString(json, "name");
            row.levelName = ReadString(json, "levelName");
            row.nextGoal = ReadString(json, "nextGoal");
            row.role = ReadString(json, "role");
            row.profileOrigin = ReadString(json, "profileOrigin");
            row.sourcePlugin = ReadString(json, "sourcePlugin");
            row.profileFile = ReadString(json, "profileFile");
            row.baseFormID = ReadFormID(json, "baseFormID");
            row.referenceFormID = ReadFormID(json, "referenceFormID");
            row.points = ReadInt(json, "points");
            row.isFollowing = ReadBool(json, "isFollowing", false);
            row.personalityEditable = ReadBool(json, "personalityEditable", false);
            row.removable = ReadBool(json, "removable", false);
            row.likes = ReadStringArray(json, "likes");
            row.dislikes = ReadStringArray(json, "dislikes");

            const auto recent = json.find("recent");
            if (recent != json.end() && recent->is_array()) {
                for (const auto& item : *recent) {
                    if (!item.is_object()) {
                        continue;
                    }
                    RecentEvent event;
                    event.label = ReadString(item, "label");
                    event.points = ReadInt(item, "points");
                    event.when = ReadString(item, "when");
                    row.recent.push_back(std::move(event));
                }
            }
            return row;
        }
    }

    UiState UiStateFromJson(const nlohmann::json& json)
    {
        UiState state;
        if (!json.is_object()) {
            return state;
        }
        state.message = ReadString(json, "message");
        state.showAwayFollowers = ReadBool(json, "showAwayFollowers", true);
        state.openWithFavorites = ReadBool(json, "openWithFavorites", false);
        state.showGainModals = ReadBool(json, "showGainModals", true);
        state.showLossModals = ReadBool(json, "showLossModals", true);

        const auto romances = json.find("romances");
        if (romances != json.end() && romances->is_array()) {
            for (const auto& item : *romances) {
                if (item.is_object()) {
                    state.followers.push_back(ReadFollower(item));
                }
            }
        }

        const auto candidates = json.find("enrollmentCandidates");
        if (candidates != json.end() && candidates->is_array()) {
            for (const auto& item : *candidates) {
                if (!item.is_object()) {
                    continue;
                }
                EnrollmentCandidate candidate;
                candidate.name = ReadString(item, "name");
                candidate.role = ReadString(item, "role");
                candidate.sourcePlugin = ReadString(item, "sourcePlugin");
                candidate.referenceFormID = ReadFormID(item, "referenceFormID");
                candidate.baseFormID = ReadFormID(item, "baseFormID");
                state.candidates.push_back(std::move(candidate));
            }
        }

        const auto options = json.find("preferenceOptions");
        if (options != json.end() && options->is_array()) {
            for (const auto& item : *options) {
                if (!item.is_object()) {
                    continue;
                }
                PreferenceOption option;
                option.editorID = ReadString(item, "editorID");
                option.label = ReadString(item, "label");
                option.category = ReadString(item, "category");
                option.points = ReadInt(item, "points");
                if (!option.editorID.empty()) {
                    state.preferenceOptions.push_back(std::move(option));
                }
            }
        }
        return state;
    }

    LevelChange LevelChangeFromJson(const nlohmann::json& json)
    {
        LevelChange change;
        if (!json.is_object()) {
            return change;
        }
        change.followerName = ReadString(json, "followerName");
        change.previousLevelName = ReadString(json, "previousLevelName");
        change.levelName = ReadString(json, "levelName");
        change.nextLevelName = ReadString(json, "nextLevelName");
        change.points = ReadInt(json, "points");
        change.pointsDelta = ReadInt(json, "pointsDelta");
        change.isLoss = ReadString(json, "changeDirection") == "loss";
        return change;
    }

    std::string RowIdentity(const FollowerRow& row)
    {
        if (row.referenceFormID.empty() || row.referenceFormID == "00000000") {
            return row.baseFormID;
        }
        return row.referenceFormID;
    }
}
