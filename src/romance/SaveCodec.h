#pragma once

// Pure byte codec for the Romantasy cosave records "RoPt" (version 3) and
// "RoPb" (player bonds), "RoMd" (moods), "RoPn" (waiting tier changes), "RoJn" (bond journals) and
// "RoMo" (shared moments).
// Standard headers only: no Skyrim or SKSE types.

#include "romance/Journal.h"
#include "romance/Mood.h"
#include "romance/Pacing.h"
#include "ostim/Moments.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace romantasy::save
{
    struct FormIdentity
    {
        std::string plugin;          // empty: no plugin (dynamic form); use rawFormID
        std::uint32_t localID = 0;   // local FormID inside `plugin`
        std::uint32_t rawFormID = 0; // runtime FormID at save time
        bool operator==(const FormIdentity&) const = default;
    };

    struct PointsEntry
    {
        FormIdentity reference; // all-zero when the bond has no reference
        FormIdentity base;
        std::int32_t points = 0;
        bool operator==(const PointsEntry&) const = default;
    };

    struct PlayerBond
    {
        FormIdentity base;
        bool enabled = true;
        std::vector<std::pair<std::string, std::int8_t>> preferences; // statistic editor ID, weight -2, -1, 1 or 2
        bool operator==(const PlayerBond&) const = default;
    };

    struct MoodEntry
    {
        FormIdentity reference;
        FormIdentity base;
        mood::Mood mood;
        mood::Company company;
        bool operator==(const MoodEntry&) const = default;
    };

    struct PendingEntry
    {
        FormIdentity reference;
        FormIdentity base;
        pacing::PendingChange change;
        bool operator==(const PendingEntry&) const = default;
    };

    struct JournalEntry
    {
        FormIdentity reference;
        FormIdentity base;
        journal::Journal journal;
        bool operator==(const JournalEntry&) const = default;
    };

    struct MomentEntry
    {
        FormIdentity reference;
        FormIdentity base;
        ostim::MomentState state;
        bool operator==(const MomentEntry&) const = default;
    };

    // A moment day is a game day, or -1 for never: finite and at least -1.
    [[nodiscard]] inline bool IsMomentDay(float day) noexcept
    {
        return std::isfinite(day) && day >= -1.0f;
    }

    [[nodiscard]] inline bool MomentFits(const ostim::MomentState& state) noexcept
    {
        return IsMomentDay(state.firstDay) && IsMomentDay(state.lastDay) && IsMomentDay(state.lastPointsDay);
    }

    // Nothing worth a save: no moment counted and no day set.
    [[nodiscard]] inline bool MomentEmpty(const ostim::MomentState& state) noexcept
    {
        return state.count == 0 && state.firstDay < 0.0f && state.lastDay < 0.0f && state.lastPointsDay < 0.0f;
    }

    class Reader
    {
    public:
        virtual ~Reader() = default;
        virtual std::uint32_t Read(void* destination, std::uint32_t bytes) = 0; // returns bytes actually read
    };

    class Writer
    {
    public:
        virtual ~Writer() = default;
        virtual bool Write(const void* source, std::uint32_t bytes) = 0;
    };

    template <class T>
    struct ReadResult
    {
        std::vector<T> entries;
        bool complete = true;
    };

    inline constexpr std::uint32_t kPointsVersion = 3;
    inline constexpr std::uint32_t kPlayerBondsVersion = 1;
    inline constexpr std::uint32_t kMoodsVersion = 1;
    inline constexpr std::uint32_t kPendingVersion = 1;
    inline constexpr std::uint32_t kJournalsVersion = 1;
    inline constexpr std::uint32_t kMomentsVersion = 1;

    [[nodiscard]] bool WritePoints(Writer& writer, const std::vector<PointsEntry>& entries);
    [[nodiscard]] ReadResult<PointsEntry> ReadPoints(Reader& reader, std::uint32_t length);
    [[nodiscard]] bool WritePlayerBonds(Writer& writer, const std::vector<PlayerBond>& bonds);
    [[nodiscard]] ReadResult<PlayerBond> ReadPlayerBonds(Reader& reader, std::uint32_t length);
    [[nodiscard]] bool WriteMoods(Writer& writer, const std::vector<MoodEntry>& entries);
    [[nodiscard]] ReadResult<MoodEntry> ReadMoods(Reader& reader, std::uint32_t length);
    [[nodiscard]] bool WritePending(Writer& writer, const std::vector<PendingEntry>& entries);
    [[nodiscard]] ReadResult<PendingEntry> ReadPending(Reader& reader, std::uint32_t length);
    [[nodiscard]] bool WriteJournals(Writer& writer, const std::vector<JournalEntry>& entries);
    [[nodiscard]] ReadResult<JournalEntry> ReadJournals(Reader& reader, std::uint32_t length);
    // Empty entries are left out; an entry with a bad day refuses the whole list before the first byte.
    [[nodiscard]] bool WriteMoments(Writer& writer, const std::vector<MomentEntry>& entries);
    // A bad day is damage, like a short record: the complete entries before it are kept.
    [[nodiscard]] ReadResult<MomentEntry> ReadMoments(Reader& reader, std::uint32_t length);

    // How one saved identity relates to the current load order.
    enum class IdentityState : std::uint8_t
    {
        Empty,           // no plugin name and rawFormID == 0
        Dynamic,         // no plugin name, rawFormID != 0 (a run-time form; resolves by pass-through, unverified)
        NamedResolved,   // plugin name present and the form ID resolved in this load order
        NamedUnresolved  // plugin name present and it did not resolve
    };

    enum class Restore : std::uint8_t
    {
        UseReference,
        UseBase,
        Orphan,
        Drop
    };

    // Where a version 3 points entry goes on load. A named identity that did not resolve
    // makes the entry an orphan unless the other one resolved by name, so a dynamic ID that
    // always "resolves" never hides a missing plugin.
    [[nodiscard]] constexpr Restore DecideRestore(IdentityState reference, IdentityState base) noexcept
    {
        if (reference == IdentityState::NamedResolved) {
            return Restore::UseReference;
        }
        if ((reference == IdentityState::NamedUnresolved || base == IdentityState::NamedUnresolved) &&
            base != IdentityState::NamedResolved) {
            return Restore::Orphan;
        }
        if (reference == IdentityState::Dynamic) {
            return Restore::UseReference;
        }
        if (base == IdentityState::NamedResolved || base == IdentityState::Dynamic) {
            return Restore::UseBase;
        }
        return Restore::Drop;
    }

    // Splits a runtime FormID into load-order index and local ID. `light` is true for FE-prefixed IDs.
    struct FormIdParts
    {
        bool dynamic = false;
        bool light = false;
        std::uint16_t index = 0;
        std::uint32_t localID = 0;
    };

    [[nodiscard]] constexpr FormIdParts SplitFormID(std::uint32_t formID) noexcept
    {
        FormIdParts parts;
        const std::uint32_t top = formID >> 24;
        if (top == 0xFF) {
            parts.dynamic = true;
            parts.localID = formID & 0x00FFFFFF;
        } else if (top == 0xFE) {
            parts.light = true;
            parts.index = static_cast<std::uint16_t>((formID >> 12) & 0xFFF);
            parts.localID = formID & 0xFFF;
        } else {
            parts.index = static_cast<std::uint16_t>(top);
            parts.localID = formID & 0x00FFFFFF;
        }
        return parts;
    }
}
