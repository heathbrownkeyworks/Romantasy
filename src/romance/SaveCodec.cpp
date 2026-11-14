#include "romance/SaveCodec.h"

#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>

namespace romantasy::save
{
    namespace
    {
        constexpr std::uint64_t kMinPointsEntry = 22; // 2 identities (9 each) + i32
        constexpr std::uint64_t kMinBond = 12;        // identity (9) + u8 + u16
        constexpr std::uint64_t kMinPending = 28;     // 2 identities (9 each) + 2 x i8 + i32 + f32
        constexpr std::uint64_t kMinJournal = 28;     // 2 identities (9 each) + 5 x u16
        constexpr std::uint64_t kMinMood = 37;       // 2 identities (9 each) + i16 + f32 + u8 + 3 x f32
        constexpr std::uint64_t kMinMoment = 34;     // 2 identities (9 each) + u32 + 3 x f32

        constexpr std::uint8_t kSeenFollowingBit = 1;
        constexpr std::uint8_t kAwayBit = 2;
        constexpr std::uint8_t kApartSentBit = 4;

        constexpr bool IsLevel(std::int32_t level) noexcept
        {
            return level >= 1 && level <= 6;
        }

        float Finite(float value) noexcept
        {
            return std::isfinite(value) ? value : 0.0f;
        }

        // -2 pet peeve, -1 dislike, 1 like, 2 favorite.
        constexpr bool IsBondWeight(std::int8_t w) noexcept
        {
            return w == -2 || w == -1 || w == 1 || w == 2;
        }

        template <class T>
        bool Put(Writer& w, const T& value)
        {
            using U = std::make_unsigned_t<T>;
            const U u = static_cast<U>(value);
            std::uint8_t bytes[sizeof(T)];
            for (std::size_t i = 0; i < sizeof(T); ++i) {
                bytes[i] = static_cast<std::uint8_t>(u >> (8 * i));
            }
            return w.Write(bytes, sizeof(T));
        }

        bool PutFloat(Writer& w, float value)
        {
            return Put(w, std::bit_cast<std::uint32_t>(value));
        }

        bool PutString(Writer& w, const std::string& s)
        {
            if (s.size() > 255 || !Put(w, static_cast<std::uint8_t>(s.size()))) {
                return false;
            }
            return s.empty() || w.Write(s.data(), static_cast<std::uint32_t>(s.size()));
        }

        bool PutIdentity(Writer& w, const FormIdentity& id)
        {
            return PutString(w, id.plugin) && Put(w, id.localID) && Put(w, id.rawFormID);
        }

        // Reads against a byte budget; any failure latches `ok` false.
        struct Cursor
        {
            Reader& reader;
            std::uint32_t remaining;
            bool ok = true;

            bool Raw(void* dst, std::uint32_t n)
            {
                if (!ok || n > remaining) {
                    return ok = false;
                }
                if (n == 0) {
                    return true;
                }
                const std::uint32_t got = reader.Read(dst, n);
                remaining -= got < n ? got : n;
                if (got < n) {
                    return ok = false;
                }
                return true;
            }

            template <class T>
            bool Get(T& out)
            {
                std::uint8_t bytes[sizeof(T)];
                if (!Raw(bytes, sizeof(T))) {
                    return false;
                }
                using U = std::make_unsigned_t<T>;
                U u = 0;
                for (std::size_t i = 0; i < sizeof(T); ++i) {
                    u = static_cast<U>(u | (static_cast<U>(bytes[i]) << (8 * i)));
                }
                out = static_cast<T>(u);
                return true;
            }

            bool GetFloat(float& out)
            {
                std::uint32_t bits = 0;
                if (!Get(bits)) {
                    return false;
                }
                out = Finite(std::bit_cast<float>(bits));
                return true;
            }

            // A moment day as stored: a non-finite or below -1 value is damage, not repaired.
            bool GetMomentDay(float& out)
            {
                std::uint32_t bits = 0;
                if (!Get(bits)) {
                    return false;
                }
                out = std::bit_cast<float>(bits);
                return IsMomentDay(out) || (ok = false);
            }

            bool GetString(std::string& out)
            {
                std::uint8_t len = 0;
                if (!Get(len)) {
                    return false;
                }
                out.assign(len, '\0');
                return len == 0 || Raw(out.data(), len);
            }

            // A label is at most kMaxLabelBytes; a longer stored length is damage and nothing is allocated.
            bool GetLabel(std::string& out)
            {
                std::uint8_t len = 0;
                if (!Get(len) || len > journal::kMaxLabelBytes) {
                    return ok = false;
                }
                out.assign(len, '\0');
                return len == 0 || Raw(out.data(), len);
            }

            bool GetIdentity(FormIdentity& id)
            {
                return GetString(id.plugin) && Get(id.localID) && Get(id.rawFormID);
            }
        };

        constexpr std::uint64_t kMaxReserve = 4096;

        // Reserve is capped by what the remaining bytes could hold and by a fixed ceiling, so
        // neither a hostile count nor a hostile length drives a large allocation.
        std::size_t ReserveFor(const Cursor& c, std::uint32_t count, std::uint64_t minEntry)
        {
            const std::uint64_t fit = c.remaining / minEntry;
            std::uint64_t reserve = count < fit ? count : fit;
            if (reserve > kMaxReserve) {
                reserve = kMaxReserve;
            }
            return static_cast<std::size_t>(reserve);
        }

        // 1 Began + 5 Reached + the Fell limit.
        constexpr std::size_t kMaxMilestones = 1 + 5 + journal::kMaxFell;
        constexpr std::uint64_t kMinChange = 9; // u8 length + i32 + f32
        constexpr std::uint64_t kMinDay = 12;
        constexpr std::uint64_t kMinMilestone = 6;

        constexpr bool IsMilestone(std::uint8_t kind, std::uint8_t level) noexcept
        {
            return kind >= 1 && kind <= 3 && level <= 6;
        }

        bool ChangesFit(const std::vector<journal::Change>& list, std::size_t limit) noexcept
        {
            if (list.size() > limit) {
                return false;
            }
            for (const auto& c : list) {
                if (c.label.size() > journal::kMaxLabelBytes) {
                    return false;
                }
            }
            return true;
        }

        bool JournalFits(const journal::Journal& j) noexcept
        {
            if (!ChangesFit(j.history, journal::kMaxHistory) || !ChangesFit(j.gains, journal::kMaxMoments) ||
                !ChangesFit(j.losses, journal::kMaxMoments) || j.days.size() > journal::kMaxDays ||
                j.milestones.size() > kMaxMilestones) {
                return false;
            }
            for (const auto& m : j.milestones) {
                if (!IsMilestone(static_cast<std::uint8_t>(m.kind), m.level)) {
                    return false;
                }
            }
            return true;
        }

        bool PutChanges(Writer& w, const std::vector<journal::Change>& list)
        {
            if (!Put(w, static_cast<std::uint16_t>(list.size()))) {
                return false;
            }
            for (const auto& c : list) {
                if (!PutString(w, c.label) || !Put(w, c.points) || !PutFloat(w, c.day)) {
                    return false;
                }
            }
            return true;
        }

        // A stored count over its limit is damage; the count only bounds the loop.
        bool GetCount(Cursor& c, std::uint16_t& count, std::size_t limit)
        {
            return c.Get(count) && (count <= limit || (c.ok = false));
        }

        bool GetChanges(Cursor& c, std::vector<journal::Change>& out, std::size_t limit)
        {
            std::uint16_t count = 0;
            if (!GetCount(c, count, limit)) {
                return false;
            }
            out.reserve(ReserveFor(c, count, kMinChange));
            for (std::uint16_t i = 0; i < count; ++i) {
                journal::Change change;
                if (!c.GetLabel(change.label) || !c.Get(change.points) || !c.GetFloat(change.day)) {
                    return false;
                }
                out.push_back(std::move(change));
            }
            return true;
        }

        bool GetJournal(Cursor& c, journal::Journal& j)
        {
            std::uint16_t count = 0;
            if (!GetChanges(c, j.history, journal::kMaxHistory) || !GetCount(c, count, journal::kMaxDays)) {
                return false;
            }
            j.days.reserve(ReserveFor(c, count, kMinDay));
            for (std::uint16_t i = 0; i < count; ++i) {
                journal::Day d;
                if (!c.Get(d.day) || !c.Get(d.gained) || !c.Get(d.lost)) {
                    return false;
                }
                j.days.push_back(d);
            }
            if (!GetChanges(c, j.gains, journal::kMaxMoments) || !GetChanges(c, j.losses, journal::kMaxMoments) ||
                !GetCount(c, count, kMaxMilestones)) {
                return false;
            }
            j.milestones.reserve(ReserveFor(c, count, kMinMilestone));
            for (std::uint16_t i = 0; i < count; ++i) {
                journal::Milestone m;
                std::uint8_t kind = 0;
                if (!c.Get(kind) || !c.Get(m.level) || !c.GetFloat(m.day) || !IsMilestone(kind, m.level)) {
                    return c.ok = false;
                }
                m.kind = static_cast<journal::MilestoneKind>(kind);
                j.milestones.push_back(m);
            }
            return true;
        }
    }

    bool WritePoints(Writer& writer, const std::vector<PointsEntry>& entries)
    {
        if (entries.size() > std::numeric_limits<std::uint32_t>::max() ||
            !Put(writer, static_cast<std::uint32_t>(entries.size()))) {
            return false;
        }
        for (const auto& e : entries) {
            if (!PutIdentity(writer, e.reference) || !PutIdentity(writer, e.base) || !Put(writer, e.points)) {
                return false;
            }
        }
        return true;
    }

    ReadResult<PointsEntry> ReadPoints(Reader& reader, std::uint32_t length)
    {
        ReadResult<PointsEntry> result;
        Cursor c{ reader, length };
        std::uint32_t count = 0;
        if (!c.Get(count)) {
            result.complete = false;
            return result;
        }
        result.entries.reserve(ReserveFor(c, count, kMinPointsEntry));
        for (std::uint32_t i = 0; i < count; ++i) {
            PointsEntry e;
            if (!c.GetIdentity(e.reference) || !c.GetIdentity(e.base) || !c.Get(e.points)) {
                result.complete = false;
                break;
            }
            result.entries.push_back(std::move(e));
        }
        return result;
    }

    bool WritePlayerBonds(Writer& writer, const std::vector<PlayerBond>& bonds)
    {
        if (bonds.size() > std::numeric_limits<std::uint32_t>::max() ||
            !Put(writer, static_cast<std::uint32_t>(bonds.size()))) {
            return false;
        }
        for (const auto& b : bonds) {
            if (b.preferences.size() > std::numeric_limits<std::uint16_t>::max() ||
                !PutIdentity(writer, b.base) ||
                !Put(writer, static_cast<std::uint8_t>(b.enabled ? 1 : 0)) ||
                !Put(writer, static_cast<std::uint16_t>(b.preferences.size()))) {
                return false;
            }
            for (const auto& [name, direction] : b.preferences) {
                if (!IsBondWeight(direction) || !PutString(writer, name) || !Put(writer, direction)) {
                    return false;
                }
            }
        }
        return true;
    }

    ReadResult<PlayerBond> ReadPlayerBonds(Reader& reader, std::uint32_t length)
    {
        ReadResult<PlayerBond> result;
        Cursor c{ reader, length };
        std::uint32_t count = 0;
        if (!c.Get(count)) {
            result.complete = false;
            return result;
        }
        result.entries.reserve(ReserveFor(c, count, kMinBond));
        for (std::uint32_t i = 0; i < count; ++i) {
            PlayerBond b;
            std::uint8_t enabled = 0;
            std::uint16_t prefCount = 0;
            if (!c.GetIdentity(b.base) || !c.Get(enabled) || !c.Get(prefCount)) {
                result.complete = false;
                break;
            }
            b.enabled = enabled != 0;
            bool good = true;
            for (std::uint16_t p = 0; p < prefCount; ++p) {
                std::string name;
                std::int8_t direction = 0;
                if (!c.GetString(name) || !c.Get(direction) || !IsBondWeight(direction)) {
                    good = false;
                    break;
                }
                b.preferences.emplace_back(std::move(name), direction);
            }
            if (!good) {
                result.complete = false;
                break;
            }
            result.entries.push_back(std::move(b));
        }
        return result;
    }

    bool WriteMoods(Writer& writer, const std::vector<MoodEntry>& entries)
    {
        if (entries.size() > std::numeric_limits<std::uint32_t>::max() ||
            !Put(writer, static_cast<std::uint32_t>(entries.size()))) {
            return false;
        }
        for (const auto& e : entries) {
            const std::uint8_t flags = static_cast<std::uint8_t>(
                (e.company.seenFollowing ? kSeenFollowingBit : 0) |
                (e.company.away ? kAwayBit : 0) |
                (e.company.apartSent ? kApartSentBit : 0));
            if (!PutIdentity(writer, e.reference) || !PutIdentity(writer, e.base) ||
                !Put(writer, e.mood.score) || !PutFloat(writer, e.mood.at) || !Put(writer, flags) ||
                !PutFloat(writer, e.company.awaySince) || !PutFloat(writer, e.company.lastSeen) ||
                !PutFloat(writer, e.company.daysTogether)) {
                return false;
            }
        }
        return true;
    }

    ReadResult<MoodEntry> ReadMoods(Reader& reader, std::uint32_t length)
    {
        ReadResult<MoodEntry> result;
        Cursor c{ reader, length };
        std::uint32_t count = 0;
        if (!c.Get(count)) {
            result.complete = false;
            return result;
        }
        result.entries.reserve(ReserveFor(c, count, kMinMood));
        for (std::uint32_t i = 0; i < count; ++i) {
            MoodEntry e;
            std::int16_t score = 0;
            std::uint8_t flags = 0;
            if (!c.GetIdentity(e.reference) || !c.GetIdentity(e.base) || !c.Get(score) ||
                !c.GetFloat(e.mood.at) || !c.Get(flags) || !c.GetFloat(e.company.awaySince) ||
                !c.GetFloat(e.company.lastSeen) || !c.GetFloat(e.company.daysTogether)) {
                result.complete = false;
                break;
            }
            e.mood.score = mood::detail::Clamped(score);
            e.company.seenFollowing = (flags & kSeenFollowingBit) != 0;
            e.company.away = (flags & kAwayBit) != 0;
            e.company.apartSent = (flags & kApartSentBit) != 0;
            if (!(e.company.daysTogether >= 0.0f)) {
                e.company.daysTogether = 0.0f;
            }
            result.entries.push_back(std::move(e));
        }
        return result;
    }

    bool WritePending(Writer& writer, const std::vector<PendingEntry>& entries)
    {
        // Refuse a bad list before the first byte, so a refusal leaves no short record.
        for (const auto& e : entries) {
            if (!IsLevel(e.change.oldLevel) || !IsLevel(e.change.newLevel) ||
                e.change.oldLevel == e.change.newLevel) {
                return false;
            }
        }
        if (entries.size() > std::numeric_limits<std::uint32_t>::max() ||
            !Put(writer, static_cast<std::uint32_t>(entries.size()))) {
            return false;
        }
        for (const auto& e : entries) {
            if (!PutIdentity(writer, e.reference) || !PutIdentity(writer, e.base) ||
                !Put(writer, static_cast<std::int8_t>(e.change.oldLevel)) ||
                !Put(writer, static_cast<std::int8_t>(e.change.newLevel)) ||
                !Put(writer, e.change.pointsDelta) || !PutFloat(writer, e.change.since)) {
                return false;
            }
        }
        return true;
    }

    ReadResult<PendingEntry> ReadPending(Reader& reader, std::uint32_t length)
    {
        ReadResult<PendingEntry> result;
        Cursor c{ reader, length };
        std::uint32_t count = 0;
        if (!c.Get(count)) {
            result.complete = false;
            return result;
        }
        result.entries.reserve(ReserveFor(c, count, kMinPending));
        for (std::uint32_t i = 0; i < count; ++i) {
            PendingEntry e;
            std::int8_t oldLevel = 0;
            std::int8_t newLevel = 0;
            if (!c.GetIdentity(e.reference) || !c.GetIdentity(e.base) || !c.Get(oldLevel) ||
                !c.Get(newLevel) || !c.Get(e.change.pointsDelta) || !c.GetFloat(e.change.since) ||
                !IsLevel(oldLevel) || !IsLevel(newLevel) || oldLevel == newLevel) {
                result.complete = false;
                break;
            }
            e.change.oldLevel = oldLevel;
            e.change.newLevel = newLevel;
            result.entries.push_back(std::move(e));
        }
        return result;
    }

    bool WriteJournals(Writer& writer, const std::vector<JournalEntry>& entries)
    {
        // Refuse a bad list before the first byte, so a refusal leaves no short record.
        for (const auto& e : entries) {
            if (!JournalFits(e.journal)) {
                return false;
            }
        }
        if (entries.size() > std::numeric_limits<std::uint32_t>::max() ||
            !Put(writer, static_cast<std::uint32_t>(entries.size()))) {
            return false;
        }
        for (const auto& e : entries) {
            const auto& j = e.journal;
            if (!PutIdentity(writer, e.reference) || !PutIdentity(writer, e.base) ||
                !PutChanges(writer, j.history) || !Put(writer, static_cast<std::uint16_t>(j.days.size()))) {
                return false;
            }
            for (const auto& d : j.days) {
                if (!Put(writer, d.day) || !Put(writer, d.gained) || !Put(writer, d.lost)) {
                    return false;
                }
            }
            if (!PutChanges(writer, j.gains) || !PutChanges(writer, j.losses) ||
                !Put(writer, static_cast<std::uint16_t>(j.milestones.size()))) {
                return false;
            }
            for (const auto& m : j.milestones) {
                if (!Put(writer, static_cast<std::uint8_t>(m.kind)) || !Put(writer, m.level) ||
                    !PutFloat(writer, m.day)) {
                    return false;
                }
            }
        }
        return true;
    }

    ReadResult<JournalEntry> ReadJournals(Reader& reader, std::uint32_t length)
    {
        ReadResult<JournalEntry> result;
        Cursor c{ reader, length };
        std::uint32_t count = 0;
        if (!c.Get(count)) {
            result.complete = false;
            return result;
        }
        result.entries.reserve(ReserveFor(c, count, kMinJournal));
        for (std::uint32_t i = 0; i < count; ++i) {
            JournalEntry e;
            if (!c.GetIdentity(e.reference) || !c.GetIdentity(e.base) || !GetJournal(c, e.journal)) {
                result.complete = false;
                break;
            }
            result.entries.push_back(std::move(e));
        }
        return result;
    }

    bool WriteMoments(Writer& writer, const std::vector<MomentEntry>& entries)
    {
        // Refuse a bad list before the first byte, so a refusal leaves no short record.
        std::uint64_t count = 0;
        for (const auto& e : entries) {
            if (!MomentFits(e.state)) {
                return false;
            }
            count += MomentEmpty(e.state) ? 0 : 1;
        }
        if (count > std::numeric_limits<std::uint32_t>::max() || !Put(writer, static_cast<std::uint32_t>(count))) {
            return false;
        }
        for (const auto& e : entries) {
            if (MomentEmpty(e.state)) {
                continue;
            }
            if (!PutIdentity(writer, e.reference) || !PutIdentity(writer, e.base) || !Put(writer, e.state.count) ||
                !PutFloat(writer, e.state.firstDay) || !PutFloat(writer, e.state.lastDay) ||
                !PutFloat(writer, e.state.lastPointsDay)) {
                return false;
            }
        }
        return true;
    }

    ReadResult<MomentEntry> ReadMoments(Reader& reader, std::uint32_t length)
    {
        ReadResult<MomentEntry> result;
        Cursor c{ reader, length };
        std::uint32_t count = 0;
        if (!c.Get(count)) {
            result.complete = false;
            return result;
        }
        result.entries.reserve(ReserveFor(c, count, kMinMoment));
        for (std::uint32_t i = 0; i < count; ++i) {
            MomentEntry e;
            if (!c.GetIdentity(e.reference) || !c.GetIdentity(e.base) || !c.Get(e.state.count) ||
                !c.GetMomentDay(e.state.firstDay) || !c.GetMomentDay(e.state.lastDay) ||
                !c.GetMomentDay(e.state.lastPointsDay)) {
                result.complete = false;
                break;
            }
            result.entries.push_back(std::move(e));
        }
        return result;
    }
}
