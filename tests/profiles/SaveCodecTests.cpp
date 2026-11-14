#include "check.h"
#include "romance/SaveCodec.h"

#include <cstring>
#include <limits>
#include <random>
#include <string>

using namespace romantasy::save;

namespace
{
    struct MemWriter : Writer
    {
        std::vector<std::uint8_t> bytes;
        bool Write(const void* src, std::uint32_t n) override
        {
            const auto* p = static_cast<const std::uint8_t*>(src);
            bytes.insert(bytes.end(), p, p + n);
            return true;
        }
    };

    // Accepts writes until the next one would pass `limit` bytes, then fails.
    struct FailingWriter : Writer
    {
        std::size_t limit;
        std::size_t written = 0;
        explicit FailingWriter(std::size_t k) : limit(k) {}
        bool Write(const void*, std::uint32_t n) override
        {
            if (written + n > limit) {
                return false;
            }
            written += n;
            return true;
        }
    };

    struct MemReader : Reader
    {
        const std::vector<std::uint8_t>& bytes;
        std::size_t pos = 0;
        explicit MemReader(const std::vector<std::uint8_t>& b) : bytes(b) {}
        std::uint32_t Read(void* dst, std::uint32_t n) override
        {
            const std::size_t avail = bytes.size() - pos;
            const std::uint32_t got = static_cast<std::uint32_t>(n < avail ? n : avail);
            if (got) {
                std::memcpy(dst, bytes.data() + pos, got);
            }
            pos += got;
            return got;
        }
    };

    std::vector<PointsEntry> SamplePoints()
    {
        PointsEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.points = 1250;
        PointsEntry b;
        b.base = { "Other.esl", 0x123, 0xFE0AB123 };
        b.points = -40;
        PointsEntry c;
        c.reference = { "", 0, 0xFF001234 };
        c.base = { "Follower.esp", 0x800, 0x05000800 };
        c.points = 0;
        return { a, b, c };
    }

    std::vector<PlayerBond> SampleBonds()
    {
        PlayerBond a;
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.enabled = true;
        a.preferences = { { "ROM_StatMurders", -1 }, { "ROM_StatQuestsCompleted", 1 } };
        PlayerBond b;
        b.base = { "Other.esp", 0x900, 0x06000900 };
        b.enabled = false;
        return { a, b };
    }
}

TEST(SaveCodec_PointsRoundTrip)
{
    const auto in = SamplePoints();
    MemWriter w;
    CHECK(WritePoints(w, in));
    MemReader r(w.bytes);
    const auto out = ReadPoints(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries == in);
}

TEST(SaveCodec_PlayerBondsRoundTrip)
{
    const auto in = SampleBonds();
    MemWriter w;
    CHECK(WritePlayerBonds(w, in));
    MemReader r(w.bytes);
    const auto out = ReadPlayerBonds(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries == in);
}

TEST(SaveCodec_TruncatedPointsKeepsPrefix)
{
    const auto in = SamplePoints();
    MemWriter w;
    CHECK(WritePoints(w, in));
    // ends[i] = byte offset where entry i ends in the full record.
    std::vector<std::size_t> ends;
    for (std::size_t n = 1; n <= in.size(); ++n) {
        MemWriter prefix;
        CHECK(WritePoints(prefix, { in.begin(), in.begin() + n }));
        ends.push_back(prefix.bytes.size());
    }
    for (std::size_t cut = 0; cut < w.bytes.size(); ++cut) {
        std::size_t fit = 0;
        while (fit < ends.size() && ends[fit] <= cut) {
            ++fit;
        }
        const std::vector<std::uint8_t> part(w.bytes.begin(), w.bytes.begin() + cut);
        MemReader r(part);
        const auto out = ReadPoints(r, static_cast<std::uint32_t>(cut));
        CHECK(!out.complete);
        CHECK(out.entries.size() == fit);
        for (std::size_t i = 0; i < out.entries.size() && i < in.size(); ++i) {
            CHECK(out.entries[i] == in[i]);
        }
        CHECK(r.pos <= cut);
    }
}

TEST(SaveCodec_TruncatedBondsKeepsPrefix)
{
    const auto in = SampleBonds();
    MemWriter w;
    CHECK(WritePlayerBonds(w, in));
    // ends[i] = byte offset where entry i ends in the full record.
    std::vector<std::size_t> ends;
    for (std::size_t n = 1; n <= in.size(); ++n) {
        MemWriter prefix;
        CHECK(WritePlayerBonds(prefix, { in.begin(), in.begin() + n }));
        ends.push_back(prefix.bytes.size());
    }
    for (std::size_t cut = 0; cut < w.bytes.size(); ++cut) {
        std::size_t fit = 0;
        while (fit < ends.size() && ends[fit] <= cut) {
            ++fit;
        }
        const std::vector<std::uint8_t> part(w.bytes.begin(), w.bytes.begin() + cut);
        MemReader r(part);
        const auto out = ReadPlayerBonds(r, static_cast<std::uint32_t>(cut));
        CHECK(!out.complete);
        CHECK(out.entries.size() == fit);
        for (std::size_t i = 0; i < out.entries.size() && i < in.size(); ++i) {
            CHECK(out.entries[i] == in[i]);
        }
        CHECK(r.pos <= cut);
    }
}

TEST(SaveCodec_LengthBoundsTheRead)
{
    MemWriter first;
    CHECK(WritePoints(first, SamplePoints()));
    MemWriter both;
    both.bytes = first.bytes;
    CHECK(WritePoints(both, SamplePoints()));
    MemReader r(both.bytes);
    const auto out = ReadPoints(r, static_cast<std::uint32_t>(first.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries.size() == 3);
    CHECK(r.pos == first.bytes.size());
}

TEST(SaveCodec_RejectsImpossibleCount)
{
    const std::vector<std::uint8_t> bytes = { 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0 };
    MemReader r(bytes);
    const auto out = ReadPoints(r, static_cast<std::uint32_t>(bytes.size()));
    CHECK(out.entries.empty());
    CHECK(!out.complete);
    MemReader r2(bytes);
    const auto bonds = ReadPlayerBonds(r2, static_cast<std::uint32_t>(bytes.size()));
    CHECK(bonds.entries.empty());
    CHECK(!bonds.complete);
}

TEST(SaveCodec_RejectsBadDirection)
{
    for (const std::uint8_t bad : { std::uint8_t{ 0 }, std::uint8_t{ 3 } }) {
        PlayerBond good;
        good.base = { "A.esp", 1, 0x01000001 };
        good.preferences = { { "Y", -1 } };
        PlayerBond broken;
        broken.base = { "B.esp", 2, 0x02000002 };
        broken.preferences = { { "X", 1 } };
        MemWriter w;
        CHECK(WritePlayerBonds(w, { good, broken }));
        w.bytes.back() = bad;
        MemReader r(w.bytes);
        const auto out = ReadPlayerBonds(r, static_cast<std::uint32_t>(w.bytes.size()));
        CHECK(out.entries.size() == 1);
        CHECK(out.entries.size() == 1 && out.entries[0] == good);
        CHECK(!out.complete);
    }
}

TEST(SaveCodec_BondWeightsRoundTrip)
{
    PlayerBond b;
    b.base = { "A.esp", 1, 0x01000001 };
    b.preferences = { { "W", -2 }, { "X", -1 }, { "Y", 1 }, { "Z", 2 } };
    MemWriter w;
    CHECK(WritePlayerBonds(w, { b }));
    MemReader r(w.bytes);
    const auto out = ReadPlayerBonds(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries.size() == 1 && out.entries[0] == b);
}

TEST(SaveCodec_BondRejectsBadWeight)
{
    for (const std::int8_t bad : { std::int8_t{ 0 }, std::int8_t{ 3 }, std::int8_t{ -3 } }) {
        PlayerBond b;
        b.base = { "A.esp", 1, 0x01000001 };
        b.preferences = { { "X", bad } };
        MemWriter w;
        CHECK(!WritePlayerBonds(w, { b }));
    }
    PlayerBond good;
    good.base = { "A.esp", 1, 0x01000001 };
    good.preferences = { { "Y", 2 } };
    PlayerBond broken;
    broken.base = { "B.esp", 2, 0x02000002 };
    broken.preferences = { { "X", -2 } };
    MemWriter w;
    CHECK(WritePlayerBonds(w, { good, broken }));
    w.bytes.back() = 3;
    MemReader r(w.bytes);
    const auto out = ReadPlayerBonds(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1 && out.entries[0] == good);
}

TEST(SaveCodec_RefusesLongPluginName)
{
    PointsEntry e;
    e.base.plugin = std::string(256, 'a');
    MemWriter w;
    CHECK(!WritePoints(w, { e }));
}

TEST(SaveCodec_SplitFormID)
{
    const auto a = SplitFormID(0x05012345);
    CHECK(!a.dynamic && !a.light && a.index == 5 && a.localID == 0x012345);
    const auto b = SplitFormID(0xFE0AB123);
    CHECK(!b.dynamic && b.light && b.index == 0x0AB && b.localID == 0x123);
    const auto c = SplitFormID(0xFF000ABC);
    CHECK(c.dynamic);
}

TEST(SaveCodec_DecideRestoreTable)
{
    using S = IdentityState;
    using R = Restore;
    struct Row
    {
        S reference;
        S base;
        R expected;
    };
    const Row rows[] = {
        { S::NamedResolved, S::NamedResolved, R::UseReference },
        { S::NamedResolved, S::NamedUnresolved, R::UseReference },
        { S::NamedResolved, S::Dynamic, R::UseReference },
        { S::NamedResolved, S::Empty, R::UseReference },
        { S::NamedUnresolved, S::NamedResolved, R::UseBase },
        { S::NamedUnresolved, S::NamedUnresolved, R::Orphan },
        { S::NamedUnresolved, S::Dynamic, R::Orphan },
        { S::NamedUnresolved, S::Empty, R::Orphan },
        { S::Dynamic, S::NamedResolved, R::UseReference },
        { S::Dynamic, S::NamedUnresolved, R::Orphan },
        { S::Dynamic, S::Dynamic, R::UseReference },
        { S::Dynamic, S::Empty, R::UseReference },
        { S::Empty, S::NamedResolved, R::UseBase },
        { S::Empty, S::NamedUnresolved, R::Orphan },
        { S::Empty, S::Dynamic, R::UseBase },
        { S::Empty, S::Empty, R::Drop },
    };
    static_assert(sizeof(rows) / sizeof(rows[0]) == 16);
    for (const auto& row : rows) {
        CHECK(DecideRestore(row.reference, row.base) == row.expected);
    }
}

TEST(SaveCodec_WriterFailurePropagates)
{
    const auto points = SamplePoints();
    MemWriter fullPoints;
    CHECK(WritePoints(fullPoints, points));
    // Inside the count, inside the first identity's plugin name, inside the last entry.
    for (const std::size_t k : { std::size_t{ 2 }, std::size_t{ 7 }, fullPoints.bytes.size() - 2 }) {
        FailingWriter w(k);
        CHECK(!WritePoints(w, points));
    }

    const auto bonds = SampleBonds();
    MemWriter fullBonds;
    CHECK(WritePlayerBonds(fullBonds, bonds));
    for (const std::size_t k : { std::size_t{ 2 }, std::size_t{ 7 }, fullBonds.bytes.size() - 2 }) {
        FailingWriter w(k);
        CHECK(!WritePlayerBonds(w, bonds));
    }
}

TEST(SaveCodec_WriteRefusesBadBond)
{
    for (const std::int8_t bad : { std::int8_t{ 0 }, std::int8_t{ 3 } }) {
        PlayerBond b;
        b.base = { "A.esp", 1, 0x01000001 };
        b.preferences = { { "ROM_StatMurders", bad } };
        MemWriter w;
        CHECK(!WritePlayerBonds(w, { b }));
    }
    PlayerBond longName;
    longName.base = { "A.esp", 1, 0x01000001 };
    longName.preferences = { { std::string(256, 's'), 1 } };
    MemWriter w;
    CHECK(!WritePlayerBonds(w, { longName }));
}

TEST(SaveCodec_LengthLargerThanData)
{
    const auto in = SamplePoints();
    MemWriter full;
    CHECK(WritePoints(full, in));
    MemWriter firstTwo;
    CHECK(WritePoints(firstTwo, { in.begin(), in.begin() + 2 }));
    // The reader can supply the first two entries and half of the third; the length claims more.
    const std::vector<std::uint8_t> part(full.bytes.begin(), full.bytes.begin() + firstTwo.bytes.size() + 5);
    for (const std::uint32_t length : { static_cast<std::uint32_t>(full.bytes.size()), std::uint32_t{ 0xFFFFFFFF } }) {
        MemReader r(part);
        const auto out = ReadPoints(r, length);
        CHECK(!out.complete);
        CHECK(out.entries.size() == 2);
        CHECK(out.entries.size() == 2 && out.entries[0] == in[0] && out.entries[1] == in[1]);
    }
}

TEST(SaveCodec_HugeCountDoesNotReserve)
{
    // Count 0xFFFFFFFF, one real entry, then the data ends; the length claims 4 GiB.
    MemWriter one;
    CHECK(WritePoints(one, { SamplePoints()[0] }));
    std::vector<std::uint8_t> points = one.bytes;
    points[0] = points[1] = points[2] = points[3] = 0xFF;
    MemReader r(points);
    const auto out = ReadPoints(r, 0xFFFFFFFF);
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries.capacity() <= 4096);

    MemWriter oneBond;
    CHECK(WritePlayerBonds(oneBond, { SampleBonds()[0] }));
    std::vector<std::uint8_t> bonds = oneBond.bytes;
    bonds[0] = bonds[1] = bonds[2] = bonds[3] = 0xFF;
    MemReader r2(bonds);
    const auto bondsOut = ReadPlayerBonds(r2, 0xFFFFFFFF);
    CHECK(!bondsOut.complete);
    CHECK(bondsOut.entries.size() == 1);
    CHECK(bondsOut.entries.capacity() <= 4096);
}

namespace
{
    std::vector<MoodEntry> SampleMoods()
    {
        MoodEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.mood = { 60, 12.5f };
        a.company = { true, false, false, 0.0f, 12.5f, 30.25f };
        MoodEntry b;
        b.base = { "Other.esl", 0x123, 0xFE0AB123 };
        b.mood = { -75, 3.0f };
        b.company = { true, true, false, 10.0f, 9.5f, 4.0f };
        MoodEntry c;
        c.reference = { "", 0, 0xFF001234 };
        c.base = { "Follower.esp", 0x800, 0x05000800 };
        c.mood = { -1, 100.0f };
        c.company = { false, true, true, 90.0f, 88.0f, 0.0f };
        return { a, b, c };
    }

    void PutBytes(std::vector<std::uint8_t>& bytes, std::size_t at, const void* src, std::size_t n)
    {
        std::memcpy(bytes.data() + at, src, n);
    }
}

TEST(SaveCodec_MoodsRoundTrip)
{
    const auto in = SampleMoods();
    MemWriter w;
    CHECK(WriteMoods(w, in));
    MemReader r(w.bytes);
    const auto out = ReadMoods(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries == in);
    CHECK(r.pos == w.bytes.size());
}

TEST(SaveCodec_MoodsTruncatedKeepsPrefix)
{
    const auto in = SampleMoods();
    MemWriter one;
    CHECK(WriteMoods(one, { in.begin(), in.begin() + 1 }));
    MemWriter two;
    CHECK(WriteMoods(two, { in.begin(), in.begin() + 2 }));
    // Cut inside the second entry.
    const std::size_t cut = one.bytes.size() + (two.bytes.size() - one.bytes.size()) / 2;
    MemWriter full;
    CHECK(WriteMoods(full, in));
    const std::vector<std::uint8_t> part(full.bytes.begin(), full.bytes.begin() + cut);
    MemReader r(part);
    const auto out = ReadMoods(r, static_cast<std::uint32_t>(cut));
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries.size() == 1 && out.entries[0] == in[0]);
}

TEST(SaveCodec_MoodsSanitised)
{
    // Empty identities are 9 bytes each, so the fields start at 4 + 18.
    MoodEntry e;
    MemWriter w;
    CHECK(WriteMoods(w, { e }));
    CHECK(w.bytes.size() == 4 + 18 + 2 + 4 + 1 + 4 + 4 + 4);
    constexpr std::size_t kScore = 22, kAt = 24, kFlags = 28, kAway = 29, kSeen = 33, kTogether = 37;

    const auto read = [&](std::int16_t score, float at, std::uint8_t flags, float away, float seen, float together) {
        std::vector<std::uint8_t> bytes = w.bytes;
        PutBytes(bytes, kScore, &score, 2);
        PutBytes(bytes, kAt, &at, 4);
        PutBytes(bytes, kFlags, &flags, 1);
        PutBytes(bytes, kAway, &away, 4);
        PutBytes(bytes, kSeen, &seen, 4);
        PutBytes(bytes, kTogether, &together, 4);
        MemReader r(bytes);
        auto out = ReadMoods(r, static_cast<std::uint32_t>(bytes.size()));
        CHECK(out.complete);
        CHECK(out.entries.size() == 1);
        return out.entries.empty() ? MoodEntry{} : out.entries[0];
    };

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    CHECK(read(500, 1.0f, 0, 0, 0, 0).mood.score == 100);
    CHECK(read(-500, 1.0f, 0, 0, 0, 0).mood.score == -100);
    CHECK(read(-100, 1.0f, 0, 0, 0, 0).mood.score == -100);

    const MoodEntry bad = read(5, nan, 0, inf, -inf, 1.0f);
    CHECK(bad.mood.at == 0.0f);
    CHECK(bad.company.awaySince == 0.0f);
    CHECK(bad.company.lastSeen == 0.0f);

    CHECK(read(0, nan, 0, nan, nan, nan).company.daysTogether == 0.0f);
    CHECK(read(0, 0, 0, 0, 0, inf).company.daysTogether == 0.0f);
    CHECK(read(0, 0, 0, 0, 0, -2.0f).company.daysTogether == 0.0f);
    CHECK(read(0, 0, 0, 0, 0, 2.5f).company.daysTogether == 2.5f);

    const MoodEntry all = read(0, 0, 0xFF, 0, 0, 0);
    CHECK(all.company.seenFollowing && all.company.away && all.company.apartSent);
    const MoodEntry none = read(0, 0, 0xF8, 0, 0, 0);
    CHECK(!none.company.seenFollowing && !none.company.away && !none.company.apartSent);
}

TEST(SaveCodec_MoodsWriterFailurePropagates)
{
    const auto moods = SampleMoods();
    MemWriter full;
    CHECK(WriteMoods(full, moods));
    // Inside the count, inside the first identity's plugin name, inside the last field.
    for (const std::size_t k : { std::size_t{ 2 }, std::size_t{ 7 }, full.bytes.size() - 2 }) {
        FailingWriter w(k);
        CHECK(!WriteMoods(w, moods));
    }
}

TEST(SaveCodec_MoodsHugeCountDoesNotReserve)
{
    MemWriter one;
    CHECK(WriteMoods(one, { SampleMoods()[0] }));
    std::vector<std::uint8_t> bytes = one.bytes;
    bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
    MemReader r(bytes);
    const auto out = ReadMoods(r, 0xFFFFFFFF);
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries.capacity() <= 4096);

    const std::vector<std::uint8_t> few = { 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02 };
    MemReader r2(few);
    const auto tiny = ReadMoods(r2, static_cast<std::uint32_t>(few.size()));
    CHECK(!tiny.complete);
    CHECK(tiny.entries.empty());
    CHECK(tiny.entries.capacity() <= 4096);
}

namespace
{
    PendingEntry MakePending(FormIdentity ref, FormIdentity base, int oldLevel, int newLevel, int delta, float since)
    {
        PendingEntry e;
        e.reference = std::move(ref);
        e.base = std::move(base);
        e.change = { oldLevel, newLevel, delta, since };
        return e;
    }

    std::vector<PendingEntry> SamplePending()
    {
        return {
            MakePending({ "Follower.esp", 0x801, 0x05000801 }, { "Follower.esp", 0x800, 0x05000800 }, 2, 3, 520, 12.5f),
            MakePending({}, { "Other.esl", 0x123, 0xFE0AB123 }, 5, 3, -1000, 3.0f),
            MakePending({ "", 0, 0xFF001234 }, { "Follower.esp", 0x800, 0x05000800 }, 1, 6, 2500, 90.0f),
        };
    }

    // Identity "Follower.esp"/localID/rawFormID is 1 + 12 + 4 + 4 = 21 bytes; "" identity is 9.
    // The tail of an entry (levels, delta, since) is 10 bytes.
    constexpr std::size_t kPendingTail = 10;
}

TEST(SaveCodec_PendingRoundTrip)
{
    const auto in = SamplePending();
    MemWriter w;
    CHECK(WritePending(w, in));
    MemReader r(w.bytes);
    const auto out = ReadPending(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries == in);
    CHECK(r.pos == w.bytes.size());
}

TEST(SaveCodec_PendingTruncatedKeepsPrefix)
{
    const auto in = SamplePending();
    MemWriter one;
    CHECK(WritePending(one, { in.begin(), in.begin() + 1 }));
    MemWriter two;
    CHECK(WritePending(two, { in.begin(), in.begin() + 2 }));
    MemWriter full;
    CHECK(WritePending(full, in));
    // Mid-identity of the second entry, and mid-payload (inside the float) of the second entry.
    for (const std::size_t cut : { one.bytes.size() + 5, two.bytes.size() - 2 }) {
        const std::vector<std::uint8_t> part(full.bytes.begin(), full.bytes.begin() + cut);
        MemReader r(part);
        const auto out = ReadPending(r, static_cast<std::uint32_t>(cut));
        CHECK(!out.complete);
        CHECK(out.entries.size() == 1);
        CHECK(out.entries.size() == 1 && out.entries[0] == in[0]);
    }
}

TEST(SaveCodec_PendingRejectsBadLevels)
{
    auto bad = SamplePending();
    bad[0].change.oldLevel = 0;
    MemWriter w0;
    CHECK(!WritePending(w0, { bad[0] }));
    bad[0].change.oldLevel = 7;
    MemWriter w7;
    CHECK(!WritePending(w7, { bad[0] }));
    bad[0] = SamplePending()[0];
    bad[0].change.newLevel = 0;
    MemWriter wn;
    CHECK(!WritePending(wn, { bad[0] }));
    bad[0].change.newLevel = 7;
    MemWriter wn7;
    CHECK(!WritePending(wn7, { bad[0] }));

    const auto in = SamplePending();
    MemWriter one;
    CHECK(WritePending(one, { in.begin(), in.begin() + 1 }));
    MemWriter two;
    CHECK(WritePending(two, { in.begin(), in.begin() + 2 }));
    // Levels of the second entry sit kPendingTail bytes before its end.
    const std::size_t levels = two.bytes.size() - kPendingTail;
    const auto readWith = [&](std::uint8_t oldLevel, std::uint8_t newLevel) {
        std::vector<std::uint8_t> bytes = two.bytes;
        bytes[levels] = oldLevel;
        bytes[levels + 1] = newLevel;
        MemReader r(bytes);
        return ReadPending(r, static_cast<std::uint32_t>(bytes.size()));
    };
    for (const auto& pair : { std::pair<std::uint8_t, std::uint8_t>{ 5, 9 }, { 9, 3 }, { 0, 3 }, { 3, 0 }, { 4, 4 } }) {
        const auto out = readWith(pair.first, pair.second);
        CHECK(!out.complete);
        CHECK(out.entries.size() == 1);
        CHECK(out.entries.size() == 1 && out.entries[0] == in[0]);
    }
    const auto ok = readWith(5, 3);
    CHECK(ok.complete);
    CHECK(ok.entries.size() == 2);
}

TEST(SaveCodec_PendingWriterRefusesBeforeWriting)
{
    auto list = SamplePending();
    list[1].change.newLevel = 7;
    MemWriter w;
    CHECK(!WritePending(w, { list[0], list[1] }));
    CHECK(w.bytes.empty());

    // 257 would pass an int8 cast as level 1; the check must see the wide value.
    auto wide = SamplePending();
    wide[0].change.oldLevel = 257;
    wide[0].change.newLevel = 3;
    MemWriter ww;
    CHECK(!WritePending(ww, { wide[0] }));
    CHECK(ww.bytes.empty());

    auto same = SamplePending();
    same[0].change.oldLevel = same[0].change.newLevel = 3;
    MemWriter ws;
    CHECK(!WritePending(ws, { same[0] }));
    CHECK(ws.bytes.empty());
}

TEST(SaveCodec_PendingNonFiniteSinceReadsAsZero)
{
    MemWriter w;
    CHECK(WritePending(w, { SamplePending()[0] }));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    PutBytes(w.bytes, w.bytes.size() - sizeof(float), &nan, sizeof(float));
    MemReader r(w.bytes);
    const auto out = ReadPending(r, static_cast<std::uint32_t>(w.bytes.size()));
    CHECK(out.complete);
    CHECK(out.entries.size() == 1 && out.entries[0].change.since == 0.0f);
}

TEST(SaveCodec_PendingWriterFailurePropagates)
{
    const auto entries = SamplePending();
    MemWriter full;
    CHECK(WritePending(full, entries));
    for (const std::size_t k : { std::size_t{ 2 }, std::size_t{ 7 }, full.bytes.size() - 2 }) {
        FailingWriter w(k);
        CHECK(!WritePending(w, entries));
    }
}

TEST(SaveCodec_PendingHugeCountDoesNotReserve)
{
    MemWriter one;
    CHECK(WritePending(one, { SamplePending()[0] }));
    std::vector<std::uint8_t> bytes = one.bytes;
    bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
    MemReader r(bytes);
    const auto out = ReadPending(r, 0xFFFFFFFF);
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries.capacity() <= 4096);

    const std::vector<std::uint8_t> few = { 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02 };
    MemReader r2(few);
    const auto tiny = ReadPending(r2, static_cast<std::uint32_t>(few.size()));
    CHECK(!tiny.complete);
    CHECK(tiny.entries.empty());
    CHECK(tiny.entries.capacity() <= 4096);
}

namespace
{
    namespace jn = romantasy::journal;

    constexpr std::size_t kMilestoneLimit = 1 + 5 + jn::kMaxFell;

    JournalEntry MakeJournal(FormIdentity ref, FormIdentity base)
    {
        JournalEntry e;
        e.reference = std::move(ref);
        e.base = std::move(base);
        auto& j = e.journal;
        j.history = { { "Cleared a dungeon", 300, 12.5f }, { "Murder", -800, 11.0f } };
        j.days = { { 10, 300, 0 }, { 11, 0, 800 } };
        j.gains = { { "Cleared a dungeon", 300, 12.5f } };
        j.losses = { { "Murder", -800, 11.0f }, { "Theft", -50, 9.0f } };
        j.milestones = { { jn::MilestoneKind::Began, 1, 2.0f }, { jn::MilestoneKind::Reached, 2, 5.0f },
            { jn::MilestoneKind::Fell, 1, 11.0f } };
        return e;
    }

    std::vector<JournalEntry> SampleJournals()
    {
        std::vector<JournalEntry> list;
        list.push_back(MakeJournal({ "Follower.esp", 0x801, 0x05000801 }, { "Follower.esp", 0x800, 0x05000800 }));
        list.push_back(MakeJournal({}, { "Other.esl", 0x123, 0xFE0AB123 }));
        list.back().journal.history[0].label = "caf\xC3\xA9";
        JournalEntry empty;
        empty.base = { "Empty.esp", 0x800, 0x06000800 };
        list.push_back(empty);
        return list;
    }

    std::vector<std::uint8_t> JournalBytes(const std::vector<JournalEntry>& list)
    {
        MemWriter w;
        CHECK(WriteJournals(w, list));
        return w.bytes;
    }

    ReadResult<JournalEntry> ReadJournalBytes(const std::vector<std::uint8_t>& bytes, std::size_t length)
    {
        MemReader r(bytes);
        return ReadJournals(r, static_cast<std::uint32_t>(length));
    }

    // Offset of the first journal's history count: count(4) + two identities.
    std::size_t FirstHistoryCountOffset(const JournalEntry& e)
    {
        return 4 + 2 * 9 + e.reference.plugin.size() + e.base.plugin.size();
    }

    std::size_t ChangeSize(const jn::Change& c)
    {
        return 1 + c.label.size() + 8;
    }
}

TEST(SaveCodec_JournalsRoundTrip)
{
    const auto in = SampleJournals();
    const auto bytes = JournalBytes(in);
    const auto out = ReadJournalBytes(bytes, bytes.size());
    CHECK(out.complete);
    CHECK(out.entries == in);
    CHECK(out.entries[2].journal == jn::Journal{});
}

TEST(SaveCodec_JournalsTruncatedKeepsPrefix)
{
    const auto in = SampleJournals();
    const auto bytes = JournalBytes(in);
    const std::size_t firstEnd = JournalBytes({ in.begin(), in.begin() + 1 }).size();
    const std::size_t secondEnd = bytes.size() - (9 + 9 + in[2].base.plugin.size() + 10);

    // Entry two cut inside an identity, inside a label, between two lists, inside its last milestone.
    const std::size_t inIdentity = firstEnd + 3;
    const std::size_t historyAt = firstEnd + 9 + 9 + in[1].base.plugin.size();
    const std::size_t inLabel = historyAt + 2 + 1 + 2;
    const std::size_t betweenLists = historyAt + 2 + ChangeSize(in[1].journal.history[0]) +
        ChangeSize(in[1].journal.history[1]);
    const std::size_t inMilestone = secondEnd - 3;
    for (const std::size_t cut : { inIdentity, inLabel, betweenLists, inMilestone }) {
        const auto out = ReadJournalBytes(bytes, cut);
        CHECK(!out.complete);
        CHECK(out.entries.size() == 1);
        CHECK(out.entries[0] == in[0]);
    }

    // The same cuts inside entry one keep nothing.
    const std::size_t firstHistory = FirstHistoryCountOffset(in[0]);
    const std::size_t firstBetween = firstHistory + 2 + ChangeSize(in[0].journal.history[0]) +
        ChangeSize(in[0].journal.history[1]);
    for (const std::size_t cut : { std::size_t{ 8 }, firstHistory + 5, firstBetween, firstEnd - 3 }) {
        const auto out = ReadJournalBytes(bytes, cut);
        CHECK(!out.complete);
        CHECK(out.entries.empty());
    }

    // Every cut inside entry two or three keeps entry one and at most one more.
    for (std::size_t cut = firstEnd; cut < bytes.size(); ++cut) {
        const auto out = ReadJournalBytes(bytes, cut);
        CHECK(!out.complete);
        CHECK(!out.entries.empty() && out.entries.size() <= 2);
        CHECK(out.entries[0] == in[0]);
    }
}

TEST(SaveCodec_JournalsOversizedListIsDamage)
{
    const auto in = SampleJournals();
    const auto good = JournalBytes({ in.begin(), in.begin() + 1 });
    const std::size_t at = FirstHistoryCountOffset(in[0]);
    for (const std::uint16_t stored : { std::uint16_t{ 31 }, std::uint16_t{ 65535 } }) {
        auto bytes = good;
        bytes[at] = static_cast<std::uint8_t>(stored & 0xFF);
        bytes[at + 1] = static_cast<std::uint8_t>(stored >> 8);
        const auto out = ReadJournalBytes(bytes, bytes.size());
        CHECK(!out.complete);
        CHECK(out.entries.empty());
        CHECK(out.entries.capacity() <= 4096);
    }
    // A label length over 120 is damage too.
    auto longLabel = good;
    longLabel[at + 2] = 121;
    const auto labelOut = ReadJournalBytes(longLabel, longLabel.size());
    CHECK(!labelOut.complete);
    CHECK(labelOut.entries.empty());

    // A bad count inside the second entry keeps the first and reserves nothing large.
    auto two = JournalBytes({ in.begin(), in.begin() + 2 });
    const std::size_t second = good.size();
    const std::size_t at2 = second + 18 + in[1].base.plugin.size();
    two[at2] = two[at2 + 1] = 0xFF;
    const auto out2 = ReadJournalBytes(two, two.size());
    CHECK(!out2.complete);
    CHECK(out2.entries.size() == 1);
    CHECK(out2.entries.capacity() <= 4096);
}

TEST(SaveCodec_JournalsBadMilestoneIsDamage)
{
    const auto in = SampleJournals();
    const auto good = JournalBytes({ in.begin(), in.begin() + 1 });
    // The last milestone is the final 6 bytes: kind, level, day.
    const std::vector<std::pair<std::size_t, std::uint8_t>> patches = { { 6, 0 }, { 6, 4 }, { 5, 7 } };
    for (const auto& patch : patches) {
        auto bytes = good;
        bytes[bytes.size() - patch.first] = patch.second;
        const auto out = ReadJournalBytes(bytes, bytes.size());
        CHECK(!out.complete);
        CHECK(out.entries.empty());
    }
}

TEST(SaveCodec_JournalsNonFiniteDayReadsAsZero)
{
    const auto in = SampleJournals();
    auto bytes = JournalBytes({ in.begin(), in.begin() + 1 });
    const std::size_t n = bytes.size();
    bytes[n - 4] = 0x00;
    bytes[n - 3] = 0x00;
    bytes[n - 2] = 0xC0;
    bytes[n - 1] = 0x7F; // NaN
    const auto out = ReadJournalBytes(bytes, bytes.size());
    CHECK(out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries[0].journal.milestones.back().day == 0.0f);
}

TEST(SaveCodec_JournalsWriterRefusesBadData)
{
    const auto list = SampleJournals();
    const auto refusedSecond = [&](auto edit) {
        auto copy = list;
        edit(copy[1].journal);
        MemWriter w;
        CHECK(!WriteJournals(w, copy));
        CHECK(w.bytes.empty());
    };
    refusedSecond([](jn::Journal& j) { j.history.resize(jn::kMaxHistory + 1); });
    refusedSecond([](jn::Journal& j) { j.days.resize(jn::kMaxDays + 1); });
    refusedSecond([](jn::Journal& j) { j.gains.resize(jn::kMaxMoments + 1); });
    refusedSecond([](jn::Journal& j) { j.losses.resize(jn::kMaxMoments + 1); });
    refusedSecond([](jn::Journal& j) { j.milestones.resize(kMilestoneLimit + 1, j.milestones[0]); });
    refusedSecond([](jn::Journal& j) { j.history[0].label.assign(jn::kMaxLabelBytes + 1, 'x'); });
    refusedSecond([](jn::Journal& j) { j.gains[0].label.assign(jn::kMaxLabelBytes + 1, 'x'); });
    refusedSecond([](jn::Journal& j) { j.milestones[0].kind = static_cast<jn::MilestoneKind>(0); });
    refusedSecond([](jn::Journal& j) { j.milestones[0].kind = static_cast<jn::MilestoneKind>(4); });
    refusedSecond([](jn::Journal& j) { j.milestones[0].level = 7; });

    // The limits themselves are accepted and round-trip.
    auto full = list;
    auto& j = full[1].journal;
    j.history.assign(jn::kMaxHistory, j.history[0]);
    j.days.assign(jn::kMaxDays, j.days[0]);
    j.gains.assign(jn::kMaxMoments, j.gains[0]);
    j.losses.assign(jn::kMaxMoments, j.losses[0]);
    j.milestones.assign(kMilestoneLimit, j.milestones[1]);
    j.history[0].label.assign(jn::kMaxLabelBytes, 'x');
    j.milestones[0].level = 0;
    const auto bytes = JournalBytes(full);
    const auto out = ReadJournalBytes(bytes, bytes.size());
    CHECK(out.complete);
    CHECK(out.entries == full);
}

TEST(SaveCodec_JournalsWriterFailurePropagates)
{
    const auto entries = SampleJournals();
    const auto full = JournalBytes(entries);
    for (const std::size_t k : { std::size_t{ 2 }, std::size_t{ 7 }, full.size() / 2, full.size() - 2 }) {
        FailingWriter w(k);
        CHECK(!WriteJournals(w, entries));
    }
}

TEST(SaveCodec_JournalsHugeCountDoesNotReserve)
{
    const auto in = SampleJournals();
    auto bytes = JournalBytes({ in.begin(), in.begin() + 1 });
    bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
    const auto out = ReadJournalBytes(bytes, 0xFFFFFFFF);
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries.capacity() <= 4096);

    const std::vector<std::uint8_t> few = { 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x02 };
    const auto tiny = ReadJournalBytes(few, few.size());
    CHECK(!tiny.complete);
    CHECK(tiny.entries.empty());
    CHECK(tiny.entries.capacity() <= 4096);
}

namespace
{
    // Accepted by the reader, never produced by the rules: no Began, every tier reached twice,
    // a Reached at level 0, twenty Fell, and days that are out of order, negative or not finite.
    romantasy::journal::Journal HostileJournal()
    {
        using namespace romantasy::journal;
        Journal j;
        for (int round = 0; round < 2; ++round) {
            for (std::uint8_t level = 2; level <= 6; ++level) {
                j.milestones.push_back({ MilestoneKind::Reached, level, static_cast<float>(level) });
            }
        }
        j.milestones.push_back({ MilestoneKind::Reached, 0, 1.0f });
        for (int i = 0; i < 20; ++i) {
            j.milestones.push_back({ MilestoneKind::Fell, 1, i == 0 ? std::numeric_limits<float>::quiet_NaN() : static_cast<float>(i) });
        }
        j.days = { { 9, -4, 2 }, { -1, 1, 1 }, { 9, 3, 0 } };
        j.history = { { "h", 5, -1.0f } };
        j.gains = { { "small", 1, 1.0f }, { "big", 9, std::numeric_limits<float>::infinity() } };
        j.losses = { { "gain in losses", 2, 1.0f } };
        return j;
    }
}

TEST(SaveCodec_NormalizedHostileJournalStaysWritableAfterRecord)
{
    using namespace romantasy::journal;
    auto j = Normalize(HostileJournal());
    Record(j, "up", 2500, 1, 6, 30.0f);
    JournalEntry entry;
    entry.base = { "Follower.esp", 0x800, 0x05000800 };
    entry.journal = j;
    MemWriter w;
    CHECK(WriteJournals(w, { entry }));
    CHECK(j.milestones.front().kind == MilestoneKind::Began);
}

TEST(SaveCodec_NormalizeIsIdempotent)
{
    using romantasy::journal::Normalize;
    const auto once = Normalize(HostileJournal());
    CHECK(Normalize(once) == once);
}

namespace
{
    std::vector<std::uint8_t> WriteOne(const romantasy::journal::Journal& journal)
    {
        JournalEntry entry;
        entry.base = { "Follower.esp", 0x800, 0x05000800 };
        entry.journal = journal;
        MemWriter w;
        CHECK(WriteJournals(w, { entry }));
        return w.bytes;
    }

    bool Writable(const romantasy::journal::Journal& journal)
    {
        JournalEntry entry;
        entry.journal = journal;
        MemWriter w;
        return WriteJournals(w, { entry });
    }

    // Readable (it fits the codec) but never made by the rules.
    romantasy::journal::Journal HostileReadableJournal()
    {
        using namespace romantasy::journal;
        Journal j;
        for (int round = 0; round < 2; ++round) {
            for (std::uint8_t level = 2; level <= 6; ++level) {
                j.milestones.push_back({ MilestoneKind::Reached, level, static_cast<float>(level) });
            }
        }
        j.milestones.push_back({ MilestoneKind::Reached, 0, 1.0f });
        j.milestones.push_back({ MilestoneKind::Began, 3, -5.0f });
        for (int i = 0; i < 14; ++i) j.milestones.push_back({ MilestoneKind::Fell, 0, static_cast<float>(i) });
        j.days = { { 9, -4, 2 }, { -1, 1, 1 }, { 9, 3, 0 } };
        j.history = { { "h", 5, std::numeric_limits<float>::quiet_NaN() } };
        j.gains = { { "small", 1, 1.0f }, { "big", 9, std::numeric_limits<float>::infinity() }, { "neg", -1, 1.0f } };
        j.losses = { { "gain in losses", 2, 1.0f } };
        return j;
    }

    romantasy::journal::Journal FullJournal()
    {
        using namespace romantasy::journal;
        Journal j;
        Record(j, "start", 1500, 1, 4, 1.0f);
        for (int i = 0; i < 80; ++i) {
            const float day = 2.0f + static_cast<float>(i);
            Record(j, std::string(130, 'x'), -(10 + i), 4, 3, day);
            Record(j, "rise " + std::to_string(i), 10 + i, 3, 4, day + 0.5f);
        }
        return j;
    }
}

TEST(SaveCodec_OrphanJournalCycleIsStable)
{
    using romantasy::journal::Normalize;
    CHECK(Writable(HostileReadableJournal()));
    for (const auto& start : { FullJournal(), HostileReadableJournal() }) {
        auto bytes = WriteOne(start);
        std::vector<std::uint8_t> first;
        for (int cycle = 0; cycle < 20; ++cycle) {
            MemReader r(bytes);
            const auto read = ReadJournals(r, static_cast<std::uint32_t>(bytes.size()));
            CHECK(read.complete);
            CHECK(read.entries.size() == 1);
            if (read.entries.size() != 1) break;
            bytes = WriteOne(Normalize(read.entries[0].journal));
            if (cycle == 0) {
                first = bytes;
            } else {
                CHECK(bytes == first);
            }
        }
    }
}

TEST(SaveCodec_JournalSequencesAlwaysFit)
{
    using namespace romantasy::journal;
    std::mt19937 rng(20261113u);
    const float days[] = { 0.0f, 1.5f, 3.0f, -2.0f, std::numeric_limits<float>::quiet_NaN(), 1e30f, 77.25f };
    const std::int32_t points[] = { 1, -1, 500, -500, 2500, std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::min(), 0, 37, -999 };
    Journal j;
    int failedAt = -1;
    for (int step = 0; step < 4000; ++step) {
        const auto pick = rng() % 10;
        const float day = days[rng() % std::size(days)];
        const auto level = static_cast<std::int32_t>(rng() % 10);
        const auto other = static_cast<std::int32_t>(rng() % 10);
        const std::string label(rng() % 2 ? 300 : 5, static_cast<char>('a' + rng() % 26));
        if (pick == 0) {
            Begin(j, level, day);
        } else if (pick <= 6) {
            Record(j, label, points[rng() % std::size(points)], level, other, day);
        } else if (pick <= 8) {
            RecordReset(j, label, points[rng() % std::size(points)], level, day);
        } else {
            j = Normalize(j);
        }
        if (!Writable(j) || j.milestones.size() > 26) {
            failedAt = step;
            break;
        }
        if (rng() % 500 == 0) j = Journal{};
    }
    CHECK(failedAt == -1);
}

TEST(SaveCodec_OverLimitListsRefusedAndRepaired)
{
    using namespace romantasy::journal;
    Journal days;
    for (int i = 0; i < 61; ++i) days.days.push_back({ i, 1, 0 });
    Journal gains;
    for (int i = 0; i < 6; ++i) gains.gains.push_back({ "g", 1 + i, 1.0f });
    Journal losses;
    for (int i = 0; i < 6; ++i) losses.losses.push_back({ "l", -1 - i, 1.0f });
    Journal milestones;
    milestones.milestones.push_back({ MilestoneKind::Began, 1, 0.0f });
    for (std::uint8_t level = 2; level <= 6; ++level) milestones.milestones.push_back({ MilestoneKind::Reached, level, 1.0f });
    for (int i = 0; i < 21; ++i) milestones.milestones.push_back({ MilestoneKind::Fell, 1, static_cast<float>(i) });
    CHECK(milestones.milestones.size() == 27);
    for (const auto& j : { days, gains, losses, milestones }) {
        CHECK(!Writable(j));
        CHECK(Writable(Normalize(j)));
    }
}

namespace
{
    MomentEntry MakeMoment(FormIdentity ref, FormIdentity base, std::uint32_t count, float first, float last, float points)
    {
        MomentEntry e;
        e.reference = std::move(ref);
        e.base = std::move(base);
        e.state.count = count;
        e.state.firstDay = first;
        e.state.lastDay = last;
        e.state.lastPointsDay = points;
        return e;
    }

    std::vector<MomentEntry> SampleMoments()
    {
        return {
            MakeMoment({ "Follower.esp", 0x801, 0x05000801 }, { "Follower.esp", 0x800, 0x05000800 }, 3, 4.25f, 9.5f, 9.5f),
            MakeMoment({}, { "Other.esl", 0x123, 0xFE0AB123 }, 1, 2.0f, 2.0f, -1.0f),
            MakeMoment({ "", 0, 0xFF001234 }, { "Follower.esp", 0x800, 0x05000800 }, 0, 0.0f, -1.0f, -1.0f),
        };
    }

    std::vector<std::uint8_t> MomentBytes(const std::vector<MomentEntry>& list)
    {
        MemWriter w;
        CHECK(WriteMoments(w, list));
        return w.bytes;
    }

    ReadResult<MomentEntry> ReadMomentBytes(const std::vector<std::uint8_t>& bytes, std::size_t length)
    {
        MemReader r(bytes);
        return ReadMoments(r, static_cast<std::uint32_t>(length));
    }

    // The tail of an entry: u32 count, then first, last and points days as f32.
    constexpr std::size_t kMomentTail = 16;
}

TEST(Moments_RoundTrip)
{
    const auto in = SampleMoments();
    const auto bytes = MomentBytes(in);
    const auto out = ReadMomentBytes(bytes, bytes.size());
    CHECK(out.complete);
    CHECK(out.entries == in);
    CHECK(out.entries.size() == 3 && out.entries[1].reference == FormIdentity{});

    // Truncated inside the second entry: the first is kept, the record is damaged.
    const std::size_t oneEnd = MomentBytes({ in.begin(), in.begin() + 1 }).size();
    for (const std::size_t cut : { oneEnd + 3, MomentBytes({ in.begin(), in.begin() + 2 }).size() - 1 }) {
        const std::vector<std::uint8_t> part(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(cut));
        const auto damaged = ReadMomentBytes(part, cut);
        CHECK(!damaged.complete);
        CHECK(damaged.entries.size() == 1 && damaged.entries[0] == in[0]);
    }
}

TEST(Moments_SkipsEmpty)
{
    auto list = SampleMoments();
    list.insert(list.begin() + 1, MakeMoment({ "Follower.esp", 0x901, 0x05000901 }, { "Follower.esp", 0x900, 0x05000900 }, 0, -1.0f, -1.0f, -1.0f));
    const auto bytes = MomentBytes(list);
    CHECK(bytes == MomentBytes(SampleMoments()));
    const auto out = ReadMomentBytes(bytes, bytes.size());
    CHECK(out.complete && out.entries == SampleMoments());

    MemWriter none;
    CHECK(WriteMoments(none, { list[1] }));
    CHECK(none.bytes == std::vector<std::uint8_t>({ 0, 0, 0, 0 }));
}

TEST(Moments_RejectsBadDays)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const float bad : { nan, inf, -inf, -2.0f, -1.5f }) {
        for (int field = 0; field < 3; ++field) {
            auto list = SampleMoments();
            float* day[] = { &list[1].state.firstDay, &list[1].state.lastDay, &list[1].state.lastPointsDay };
            *day[field] = bad;
            MemWriter w;
            CHECK(!WriteMoments(w, list));
            CHECK(w.bytes.empty());
        }
    }

    // A stored bad day is damage: the entries before it are kept.
    const auto in = SampleMoments();
    const auto two = MomentBytes({ in.begin(), in.begin() + 2 });
    for (const float bad : { nan, -2.0f }) {
        for (std::size_t field = 0; field < 3; ++field) {
            auto bytes = two;
            PutBytes(bytes, bytes.size() - kMomentTail + 4 + 4 * field, &bad, sizeof(float));
            const auto out = ReadMomentBytes(bytes, bytes.size());
            CHECK(!out.complete);
            CHECK(out.entries.size() == 1 && out.entries[0] == in[0]);
        }
    }
    // -1 (never) and fractional days are fine.
    const auto ok = ReadMomentBytes(two, two.size());
    CHECK(ok.complete && ok.entries.size() == 2);
}

TEST(Moments_HugeCountDoesNotReserve)
{
    auto bytes = MomentBytes({ SampleMoments()[0] });
    bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0xFF;
    const auto out = ReadMomentBytes(bytes, 0xFFFFFFFF);
    CHECK(!out.complete);
    CHECK(out.entries.size() == 1);
    CHECK(out.entries.capacity() <= 4096);
}

TEST(Moments_WriterFailurePropagates)
{
    const auto entries = SampleMoments();
    const auto full = MomentBytes(entries);
    for (const std::size_t k : { std::size_t{ 2 }, std::size_t{ 7 }, full.size() - 2 }) {
        FailingWriter w(k);
        CHECK(!WriteMoments(w, entries));
    }
}
