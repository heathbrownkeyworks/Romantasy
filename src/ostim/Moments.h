#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

// The rules for a shared moment with a bond: what kind of scene it was, whether
// it counts, and whether one may be started. Pure and standard headers only.
namespace romantasy::ostim
{
    enum class Kind : std::uint8_t { None = 0, Affection = 1, Closeness = 2, Intimacy = 3 };
    enum class Feeling : std::uint8_t { Warm, Treasured, Reserved };

    inline constexpr std::array<std::string_view, 7> kAffectionActions{ "holdinghand", "hugging", "kissingcheek", "kissinghand", "pattinghead", "strokinghead", "dancing" };
    inline constexpr std::array<std::string_view, 7> kClosenessActions{ "kissing", "frenchkissing", "kissingneck", "cuddling", "spooning", "holdingbody", "massaging" };
    inline constexpr std::string_view kForceSceneTag = "aggressive";

    namespace detail
    {
        [[nodiscard]] constexpr char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

        [[nodiscard]] constexpr bool EqualsNoCase(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (Lower(a[i]) != Lower(b[i])) return false;
            }
            return true;
        }

        template <std::size_t N>
        [[nodiscard]] constexpr bool Contains(const std::array<std::string_view, N>& list, std::string_view id)
        {
            for (const auto& item : list) {
                if (EqualsNoCase(item, id)) return true;
            }
            return false;
        }

        // floor without a constexpr <cmath>; only for finite values.
        [[nodiscard]] constexpr double Floor(double d)
        {
            if (d >= 9.0e15 || d <= -9.0e15) return d;
            const auto i = static_cast<std::int64_t>(d);
            const double f = static_cast<double>(i);
            return f > d ? f - 1.0 : f;
        }

        [[nodiscard]] constexpr bool Finite(float f)
        {
            return f == f && f <= std::numeric_limits<float>::max() && f >= std::numeric_limits<float>::lowest();
        }
    }

    // ASCII case-insensitive. Anything unlisted (and "") is the deepest kind.
    [[nodiscard]] constexpr Kind KindOfAction(std::string_view actionID)
    {
        if (detail::Contains(kAffectionActions, actionID)) return Kind::Affection;
        if (detail::Contains(kClosenessActions, actionID)) return Kind::Closeness;
        return Kind::Intimacy;
    }

    [[nodiscard]] constexpr Kind Deeper(Kind a, Kind b) { return static_cast<std::uint8_t>(a) >= static_cast<std::uint8_t>(b) ? a : b; }

    [[nodiscard]] constexpr bool IsForceActorTag(std::string_view tag)
    {
        if (!tag.empty() && tag.front() == '-') tag.remove_prefix(1);
        return detail::EqualsNoCase(tag, "dominant") || detail::EqualsNoCase(tag, "aggressor") || detail::EqualsNoCase(tag, "victim") ||
               detail::EqualsNoCase(tag, "submissive");
    }

    [[nodiscard]] constexpr std::optional<Kind> ParseKind(std::string_view key)
    {
        if (key == "affection") return Kind::Affection;
        if (key == "closeness") return Kind::Closeness;
        if (key == "intimacy") return Kind::Intimacy;
        return std::nullopt;
    }

    [[nodiscard]] constexpr std::string_view Key(Kind kind)
    {
        switch (kind) {
        case Kind::Affection: return "affection";
        case Kind::Closeness: return "closeness";
        case Kind::Intimacy: return "intimacy";
        default: return "";
        }
    }

    [[nodiscard]] constexpr std::string_view Label(Kind kind)
    {
        switch (kind) {
        case Kind::Affection: return "A tender moment";
        case Kind::Closeness: return "A quiet hour together";
        case Kind::Intimacy: return "Time alone together";
        default: return "";
        }
    }

    // Affection: its 7 ids; Closeness: all 14; otherwise "".
    [[nodiscard]] inline std::string ActionCSV(Kind kind)
    {
        std::string out;
        const auto add = [&out](std::string_view id) {
            if (!out.empty()) out += ',';
            out += id;
        };
        if (kind == Kind::Affection || kind == Kind::Closeness) {
            for (const auto& id : kAffectionActions) add(id);
        }
        if (kind == Kind::Closeness) {
            for (const auto& id : kClosenessActions) add(id);
        }
        return out;
    }

    // The bond level each kind is welcome from. 0 = never.
    struct Welcome
    {
        std::int32_t affection = 3, closeness = 4, intimacy = 5;
        Feeling feeling = Feeling::Warm;
        bool operator==(const Welcome&) const = default;
    };

    [[nodiscard]] constexpr std::int32_t WelcomeTier(const Welcome& welcome, Kind kind)
    {
        switch (kind) {
        case Kind::Affection: return welcome.affection;
        case Kind::Closeness: return welcome.closeness;
        case Kind::Intimacy: return welcome.intimacy;
        default: return 0;
        }
    }

    [[nodiscard]] constexpr bool Welcomed(const Welcome& welcome, Kind kind, std::int32_t level)
    {
        const std::int32_t tier = WelcomeTier(welcome, kind);
        return tier >= 1 && tier <= level;
    }

    [[nodiscard]] constexpr Kind DeepestWelcome(const Welcome& welcome, std::int32_t level)
    {
        if (Welcomed(welcome, Kind::Intimacy, level)) return Kind::Intimacy;
        if (Welcomed(welcome, Kind::Closeness, level)) return Kind::Closeness;
        if (Welcomed(welcome, Kind::Affection, level)) return Kind::Affection;
        return Kind::None;
    }

    inline constexpr float kMinSeconds = 20.0f;
    inline constexpr std::int32_t kPoints = 5, kTreasuredPoints = 10;

    struct MomentFacts
    {
        bool enabled = true;
        bool stoppedForMismatch = false;
        std::uint32_t participants = 0;
        bool playerPresent = false;
        std::uint32_t bonded = 0;
        Kind kind = Kind::None;
        bool force = false;
        float seconds = 0.0f;
        std::int32_t level = 0;
        Welcome welcome;
        float lastPointsDay = -1.0f;
        float today = 0.0f;
    };

    enum class Reason : std::uint8_t { None, NotAsked, Off, NotTwo, NotGentle, NothingHappened, TooShort, TooSoon, Reserved, AlreadyToday };

    struct Verdict
    {
        bool counted = false;
        std::int32_t points = 0;
        Reason reason = Reason::None;
    };

    [[nodiscard]] constexpr Verdict Judge(const MomentFacts& f)
    {
        if (f.stoppedForMismatch) return { false, 0, Reason::NotAsked };
        if (!f.enabled) return { false, 0, Reason::Off };
        if (!(f.participants == 2 && f.playerPresent && f.bonded == 1)) return { false, 0, Reason::NotTwo };
        if (f.force) return { false, 0, Reason::NotGentle };
        if (f.kind == Kind::None) return { false, 0, Reason::NothingHappened };
        if (!detail::Finite(f.seconds) || f.seconds < kMinSeconds) return { false, 0, Reason::TooShort };
        if (!Welcomed(f.welcome, f.kind, f.level)) return { false, 0, Reason::TooSoon };
        if (f.welcome.feeling == Feeling::Reserved) return { true, 0, Reason::Reserved };
        if (detail::Finite(f.lastPointsDay) && detail::Finite(f.today) && f.lastPointsDay >= 0.0f && f.today >= f.lastPointsDay &&
            detail::Floor(f.lastPointsDay) == detail::Floor(f.today)) {
            return { true, 0, Reason::AlreadyToday };
        }
        return { true, f.welcome.feeling == Feeling::Treasured ? kTreasuredPoints : kPoints, Reason::None };
    }

    [[nodiscard]] constexpr std::string_view Text(Reason reason)
    {
        switch (reason) {
        case Reason::NotAsked: return "not the moment asked for";
        case Reason::Off: return "moments are off";
        case Reason::NotTwo: return "not just the two of you";
        case Reason::NotGentle: return "not a gentle scene";
        case Reason::NothingHappened: return "nothing happened";
        case Reason::TooShort: return "too short";
        case Reason::TooSoon: return "too soon";
        case Reason::Reserved: return "reserved";
        case Reason::AlreadyToday: return "already counted today";
        default: return "";
        }
    }

    struct MomentState
    {
        std::uint32_t count = 0;
        float firstDay = -1.0f, lastDay = -1.0f, lastPointsDay = -1.0f;
        bool operator==(const MomentState&) const = default;
    };

    constexpr void Noted(MomentState& state, const Verdict& verdict, float today)
    {
        if (!verdict.counted) return;
        if (state.count < std::numeric_limits<std::uint32_t>::max()) ++state.count;
        if (!detail::Finite(today)) return;
        state.lastDay = today;
        if (state.firstDay < 0.0f) state.firstDay = today;
        if (verdict.points > 0) state.lastPointsDay = today;
    }

    [[nodiscard]] constexpr float DaysSince(const MomentState& state, float today)
    {
        if (!detail::Finite(state.lastDay) || state.lastDay < 0.0f) return -1.0f;
        if (!detail::Finite(today) || today < state.lastDay) return 0.0f;
        return today - state.lastDay;
    }

    enum class StartCode : std::int32_t { Accepted = 0, NotAvailable = 1, NotABond = 2, TooSoon = 3, Strained = 4, NotNow = 5, CouldNotStart = 6 };

    struct StartFacts
    {
        bool canStart = false;
        bool kindKnown = true;
        std::optional<Kind> asked;
        bool bonded = false;
        std::int32_t level = 0;
        std::int32_t mood = 2;
        Welcome welcome;
        bool inCombat = false, inScene = false, pending = false;
    };

    // kind: the kind asked for when one was asked, else the deepest welcome; None when there is none.
    struct StartAnswer
    {
        StartCode code;
        Kind kind;
    };

    [[nodiscard]] constexpr StartAnswer CheckStart(const StartFacts& f)
    {
        const Kind kind = f.asked ? *f.asked : DeepestWelcome(f.welcome, f.level);
        if (!f.canStart || !f.kindKnown) return { StartCode::NotAvailable, kind };
        if (!f.bonded) return { StartCode::NotABond, kind };
        if (kind == Kind::None || !Welcomed(f.welcome, kind, f.level)) return { StartCode::TooSoon, kind };
        if (f.mood == 1) return { StartCode::Strained, kind };
        if (f.inCombat || f.inScene || f.pending) return { StartCode::NotNow, kind };
        return { StartCode::Accepted, kind };
    }

    [[nodiscard]] constexpr bool NodeFits(Kind started, Kind nodeKind)
    {
        return static_cast<std::uint8_t>(nodeKind) <= static_cast<std::uint8_t>(started);
    }
}
