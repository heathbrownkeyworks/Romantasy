#include "ui/PageRequests.h"

#include "ui/magelight/Channels.h"

#include <algorithm>
#include <cstdint>

namespace romantasy::ui
{
    namespace
    {
        using nlohmann::json;

        // The payload as an object, or discarded when it is oversized, not JSON, or not an object.
        json ParseObject(std::string_view text)
        {
            if (text.size() > kMaxPayloadBytes) {
                return json(json::value_t::discarded);
            }
            json value = json::parse(text.begin(), text.end(), nullptr, false);
            if (value.is_discarded() || !value.is_object()) {
                return json(json::value_t::discarded);
            }
            return value;
        }

        // An integer in [lo, hi]. A float such as 1.5 or 1e3 is not an integer.
        std::optional<std::int32_t> IntegerIn(const json& value, std::int64_t lo, std::int64_t hi)
        {
            if (!value.is_number_integer()) {
                return std::nullopt;
            }
            if (value.is_number_unsigned()) {
                const auto n = value.get<std::uint64_t>();
                if (n > static_cast<std::uint64_t>(hi)) {
                    return std::nullopt;
                }
                return static_cast<std::int32_t>(n);
            }
            const auto n = value.get<std::int64_t>();
            if (n < lo || n > hi) {
                return std::nullopt;
            }
            return static_cast<std::int32_t>(n);
        }

        const json* Find(const json& object, const char* key)
        {
            const auto it = object.find(key);
            return it == object.end() ? nullptr : &*it;
        }

        bool IsHex(std::string_view text)
        {
            return std::all_of(text.begin(), text.end(), [](char c) {
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            });
        }

        // 1 to 8 hex digits: a FormID's text.
        bool ValidIdentity(std::string_view identity)
        {
            return !identity.empty() && identity.size() <= 8 && IsHex(identity);
        }

        std::optional<bool> BooleanAt(const json& object, const char* key)
        {
            const json* value = Find(object, key);
            if (value && value->is_boolean()) {
                return value->get<bool>();
            }
            return std::nullopt;
        }
    }

    std::optional<SettingsChange> ParseSettingsChange(std::string_view text)
    {
        const json object = ParseObject(text);
        if (object.is_discarded()) {
            return std::nullopt;
        }
        const auto& keys = channels::kSettingsKeys;
        SettingsChange change;
        change.openWithFavorites = BooleanAt(object, keys[0]);
        change.showGainModals = BooleanAt(object, keys[1]);
        change.showLossModals = BooleanAt(object, keys[2]);
        change.showAwayFollowers = BooleanAt(object, keys[3]);
        change.detailedLog = BooleanAt(object, keys[4]);
        if (const json* value = Find(object, keys[5]); value && value->is_string()) {
            change.pace = pacing::ParsePace(value->get_ref<const std::string&>());
        }
        if (const json* value = Find(object, keys[6]); value && value->is_string()) {
            change.tierMoment = pacing::ParseTierMoment(value->get_ref<const std::string&>());
        }
        change.whispers = BooleanAt(object, keys[7]);
        if (const json* value = Find(object, keys[8]); value && value->is_string()) {
            change.whisperCorner = whispers::ParseWhisperCorner(value->get_ref<const std::string&>());
        }
        if (const json* value = Find(object, keys[9]); value && value->is_string()) {
            if (const auto order = ParseRosterOrder(value->get_ref<const std::string&>())) {
                change.rosterOrder = std::string(*order);
            }
        }
        if (const json* value = Find(object, keys[10]); value && value->is_string()) {
            if (const auto size = ParseLedgerSize(value->get_ref<const std::string&>())) {
                change.ledgerSize = std::string(*size);
            }
        }
        change.highContrast = BooleanAt(object, keys[11]);
        change.reduceMotion = BooleanAt(object, keys[12]);
        change.skyrimNetBridge = BooleanAt(object, keys[13]);
        change.skyrimNetConversation = BooleanAt(object, keys[14]);
        change.ostimMoments = BooleanAt(object, keys[15]);
        change.ostimHold = BooleanAt(object, keys[16]);
        if (!change.openWithFavorites && !change.showGainModals && !change.showLossModals && !change.showAwayFollowers &&
            !change.detailedLog && !change.pace && !change.tierMoment && !change.whispers && !change.whisperCorner &&
            !change.rosterOrder && !change.ledgerSize && !change.highContrast && !change.reduceMotion &&
            !change.skyrimNetBridge && !change.skyrimNetConversation && !change.ostimMoments && !change.ostimHold) {
            return std::nullopt;
        }
        return change;
    }

    std::optional<BondRequest> ParseBondRequest(std::string_view text)
    {
        const json object = ParseObject(text);
        if (object.is_discarded()) {
            return std::nullopt;
        }
        const auto& keys = channels::kBondKeys;

        const json* kindValue = Find(object, keys[0]);
        const json* identityValue = Find(object, keys[1]);
        if (!kindValue || !kindValue->is_string() || !identityValue || !identityValue->is_string()) {
            return std::nullopt;
        }

        BondRequest request;
        const auto& kind = kindValue->get_ref<const std::string&>();
        if (kind == "enroll") {
            request.kind = BondRequest::Kind::Enroll;
        } else if (kind == "replace") {
            request.kind = BondRequest::Kind::Replace;
        } else if (kind == "reset") {
            request.kind = BondRequest::Kind::Reset;
        } else if (kind == "remove") {
            request.kind = BondRequest::Kind::Remove;
        } else {
            return std::nullopt;
        }

        const auto& identity = identityValue->get_ref<const std::string&>();
        if (!ValidIdentity(identity)) {
            return std::nullopt;
        }
        request.identity = identity;

        if (request.kind == BondRequest::Kind::Enroll || request.kind == BondRequest::Kind::Replace) {
            const json* prefs = Find(object, keys[2]);
            if (!prefs || !prefs->is_array() || prefs->size() > 256) {
                return std::nullopt;
            }
            request.preferences.reserve(prefs->size());
            for (const json& pair : *prefs) {
                if (!pair.is_array() || pair.size() != 2 || !pair[0].is_string()) {
                    return std::nullopt;
                }
                const auto& editorId = pair[0].get_ref<const std::string&>();
                const auto direction = IntegerIn(pair[1], -2, 2);
                if (editorId.empty() || editorId.size() > 128 || !direction) {
                    return std::nullopt;
                }
                request.preferences.emplace_back(editorId, *direction);
            }
        }
        return request;
    }

    std::optional<std::string> ParseJournalRequest(std::string_view text)
    {
        const json object = ParseObject(text);
        if (object.is_discarded()) {
            return std::nullopt;
        }
        const json* identity = Find(object, channels::kJournalKeys[0]);
        if (!identity || !identity->is_string() || !ValidIdentity(identity->get_ref<const std::string&>())) {
            return std::nullopt;
        }
        return identity->get<std::string>();
    }

    std::optional<DebugRequest> ParseDebugRequest(std::string_view text)
    {
        const json object = ParseObject(text);
        if (object.is_discarded()) {
            return std::nullopt;
        }
        const auto& keys = channels::kDebugKeys;
        DebugRequest request;

        if (const json* points = Find(object, keys[0])) {
            request.points = IntegerIn(*points, -10000, 10000);
            if (!request.points) {
                return std::nullopt;
            }
            return request;
        }

        const json* stat = Find(object, keys[1]);
        const json* delta = Find(object, keys[2]);
        if (!stat || !stat->is_string() || !delta) {
            return std::nullopt;
        }
        const auto& name = stat->get_ref<const std::string&>();
        const auto amount = IntegerIn(*delta, -100, 100);
        if (name.empty() || name.size() > 64 || !amount) {
            return std::nullopt;
        }
        request.stat = name;
        request.delta = *amount;
        return request;
    }

    std::optional<std::string> ParseDevUnlock(std::string_view text)
    {
        const json object = ParseObject(text);
        if (object.is_discarded()) {
            return std::nullopt;
        }
        const json* password = Find(object, channels::kDevUnlockKeys[0]);
        if (!password || !password->is_string()) {
            return std::nullopt;
        }
        const auto& value = password->get_ref<const std::string&>();
        if (value.size() > 64) {
            return std::nullopt;
        }
        return value;
    }

    std::optional<std::uint32_t> ParsePopupAck(std::string_view text)
    {
        const json object = ParseObject(text);
        if (object.is_discarded() || object.size() != 1) {
            return std::nullopt;
        }
        const json* seq = Find(object, channels::kPopupAckKeys[0]);
        if (!seq) {
            return std::nullopt;
        }
        const auto value = IntegerIn(*seq, 0, 2147483647);
        if (!value) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(*value);
    }

    nlohmann::json StampState(nlohmann::json state, std::uint32_t revision, bool developerToolsUnlocked, nlohmann::json health)
    {
        if (state.is_object()) {
            state["revision"] = revision;
            state["developerToolsUnlocked"] = developerToolsUnlocked;
            state["health"] = std::move(health);
        }
        return state;
    }
}
