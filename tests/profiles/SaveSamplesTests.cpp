// Binary samples of every save record the codec reads, checked in under tests/fixtures/save.
// They pin the byte layout: a change to the save format that breaks an old save fails here.
// The samples are written by SaveSamples_WriteSamples, only when ROMANTASY_WRITE_SAMPLES=1.

#include "check.h"
#include "romance/SaveCodec.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

using namespace romantasy::save;
namespace journal = romantasy::journal;

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    struct MemWriter : Writer
    {
        Bytes bytes;
        bool Write(const void* src, std::uint32_t n) override
        {
            const auto* p = static_cast<const std::uint8_t*>(src);
            bytes.insert(bytes.end(), p, p + n);
            return true;
        }
    };

    struct MemReader : Reader
    {
        const Bytes& bytes;
        std::size_t pos = 0;
        explicit MemReader(const Bytes& b) : bytes(b) {}
        std::uint32_t Read(void* dst, std::uint32_t n) override
        {
            const std::size_t avail = bytes.size() - pos;
            const auto got = static_cast<std::uint32_t>(n < avail ? n : avail);
            if (got) {
                std::memcpy(dst, bytes.data() + pos, got);
            }
            pos += got;
            return got;
        }
    };

    const std::filesystem::path kDir = std::filesystem::path("tests") / "fixtures" / "save";

    // Empty on a missing file; the caller checks the size and names the file.
    Bytes LoadSample(const std::string& file)
    {
        std::ifstream in(kDir / file, std::ios::binary);
        if (!in) {
            return {};
        }
        return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    bool HaveSample(const std::string& file, const Bytes& bytes)
    {
        if (bytes.empty()) {
            std::printf("  MISSING OR EMPTY SAMPLE: %s\n", (kDir / file).string().c_str());
            return false;
        }
        return true;
    }

    // ---- Sample data. Values are exact in binary so they compare equal after a float round trip.

    std::vector<PointsEntry> Points()
    {
        PointsEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.points = 1250;
        PointsEntry b; // no reference, light plugin base, negative points
        b.base = { "Other.esl", 0x123, 0xFE0AB123 };
        b.points = -40;
        PointsEntry c; // dynamic reference (no plugin name)
        c.reference = { "", 0, 0xFF001234 };
        c.base = { "Follower.esp", 0x800, 0x05000800 };
        c.points = 0;
        return { a, b, c };
    }

    std::vector<PlayerBond> Bonds()
    {
        PlayerBond a;
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.enabled = true;
        a.preferences = { { "ROM_StatMurders", -2 }, { "ROM_StatLocksPicked", -1 },
            { "ROM_StatQuestsCompleted", 1 }, { "ROM_StatDungeonsCleared", 2 } };
        PlayerBond b;
        b.base = { "Other.esp", 0x900, 0x06000900 };
        b.enabled = false;
        return { a, b };
    }

    std::vector<MoodEntry> Moods()
    {
        MoodEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.mood = { 60, 120.5f };
        a.company = { true, false, false, 0.0f, 121.25f, 30.5f };
        MoodEntry b;
        b.base = { "Other.esp", 0x900, 0x06000900 };
        b.mood = { -75, 99.0f };
        b.company = { true, true, true, 100.5f, 100.0f, 4.75f };
        return { a, b };
    }

    std::vector<PendingEntry> Pending()
    {
        PendingEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.change.oldLevel = 2;
        a.change.newLevel = 3;
        a.change.pointsDelta = 40;
        a.change.since = 15.5f;
        PendingEntry b;
        b.base = { "Other.esp", 0x900, 0x06000900 };
        b.change.oldLevel = 5;
        b.change.newLevel = 4;
        b.change.pointsDelta = -300;
        b.change.since = 22.0f;
        return { a, b };
    }

    // Both journals fit the limits (so WriteJournals accepts them): one is a busy life with every
    // list and milestone kind, the other is a bond that only began.
    std::vector<JournalEntry> Journals()
    {
        using journal::MilestoneKind;
        JournalEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.journal.history = { { "Slew a dragon", 40, 12.5f }, { "Picked a lock", 5, 12.0f },
            { "Murdered a guard", -80, 10.25f } };
        a.journal.days = { { 3, 10, 0 }, { 10, 0, 80 }, { 12, 45, 0 } };
        a.journal.gains = { { "Slew a dragon", 40, 12.5f }, { "Picked a lock", 5, 12.0f } };
        a.journal.losses = { { "Murdered a guard", -80, 10.25f } };
        a.journal.milestones = { { MilestoneKind::Began, 1, 3.0f }, { MilestoneKind::Reached, 2, 5.5f },
            { MilestoneKind::Reached, 3, 11.0f }, { MilestoneKind::Fell, 2, 10.25f } };
        JournalEntry b;
        b.base = { "Other.esp", 0x900, 0x06000900 };
        b.journal.milestones = { { MilestoneKind::Began, 1, 0.0f } };
        return { a, b };
    }

    // One bond with a reference, several moments and fractional days; one with no reference, a single
    // moment and no points-giving day yet (never, -1).
    std::vector<MomentEntry> Moments()
    {
        MomentEntry a;
        a.reference = { "Follower.esp", 0x801, 0x05000801 };
        a.base = { "Follower.esp", 0x800, 0x05000800 };
        a.state = { 4, 3.25f, 17.75f, 17.75f };
        MomentEntry b;
        b.base = { "Other.esp", 0x900, 0x06000900 };
        b.state = { 1, 8.5f, 8.5f, -1.0f };
        return { a, b };
    }

    // ---- One table row per record: how to write it, read it and compare it.

    struct Record
    {
        const char* name;       // "points"
        std::uint32_t version;  // the version the sample is written in
        const char* file;
        std::function<Bytes()> write;
        std::function<std::size_t(const Bytes&, std::uint32_t length, bool& complete)> read; // entry count
        std::function<bool(const Bytes&)> matchesExpected; // full read is complete and equals the sample data
        std::function<bool(const Bytes&, std::uint32_t length)> prefixOfExpected; // every entry read equals the same-index expected one
        std::size_t expectedCount;
    };

    template <class T, class WriteFn, class ReadFn>
    Record Make(const char* name, std::uint32_t version, const char* file, std::vector<T> expected, WriteFn writeFn,
        ReadFn readFn)
    {
        Record r;
        r.name = name;
        r.version = version;
        r.file = file;
        r.expectedCount = expected.size();
        r.write = [expected, writeFn] {
            MemWriter w;
            if (!writeFn(w, expected)) {
                std::printf("  WRITER REFUSED THE SAMPLE\n");
                return Bytes{};
            }
            return w.bytes;
        };
        r.read = [readFn](const Bytes& b, std::uint32_t length, bool& complete) {
            MemReader reader(b);
            const auto result = readFn(reader, length);
            complete = result.complete;
            return result.entries.size();
        };
        r.matchesExpected = [expected, readFn](const Bytes& b) {
            MemReader reader(b);
            const auto result = readFn(reader, static_cast<std::uint32_t>(b.size()));
            return result.complete && result.entries == expected;
        };
        r.prefixOfExpected = [expected, readFn](const Bytes& b, std::uint32_t length) {
            MemReader reader(b);
            const auto result = readFn(reader, length);
            if (result.entries.size() > expected.size()) {
                return false;
            }
            for (std::size_t i = 0; i < result.entries.size(); ++i) {
                if (!(result.entries[i] == expected[i])) {
                    return false;
                }
            }
            return true;
        };
        return r;
    }

    const std::vector<Record>& Records()
    {
        static const std::vector<Record> records = {
            Make("points", kPointsVersion, "points_v3.bin", Points(), WritePoints, ReadPoints),
            Make("player_bonds", kPlayerBondsVersion, "player_bonds_v1.bin", Bonds(), WritePlayerBonds, ReadPlayerBonds),
            Make("moods", kMoodsVersion, "moods_v1.bin", Moods(), WriteMoods, ReadMoods),
            Make("pending", kPendingVersion, "pending_v1.bin", Pending(), WritePending, ReadPending),
            Make("journals", kJournalsVersion, "journals_v1.bin", Journals(), WriteJournals, ReadJournals),
            Make("moments", kMomentsVersion, "moments_v1.bin", Moments(), WriteMoments, ReadMoments),
        };
        return records;
    }

    const Record& Find(const char* name)
    {
        for (const auto& r : Records()) {
            if (std::strcmp(r.name, name) == 0) {
                return r;
            }
        }
        std::printf("  NO SUCH RECORD: %s\n", name);
        std::abort();
    }

    void CheckSampleReads(const char* name)
    {
        const Record& r = Find(name);
        const Bytes sample = LoadSample(r.file);
        if (!HaveSample(r.file, sample)) {
            CHECK(false);
            return;
        }
        CHECK(r.matchesExpected(sample));
        // The current writer still produces the same bytes. A format change must bump the version
        // and add a new sample instead of editing this one.
        CHECK(r.write() == sample);
    }
}

// Writes the missing samples. A normal run never does: only with ROMANTASY_WRITE_SAMPLES=1.
// A sample that exists is never written again (see tests/fixtures/save/README.md).
TEST(SaveSamples_WriteSamples)
{
    char* flag = nullptr;
    std::size_t flagSize = 0;
    const bool have = _dupenv_s(&flag, &flagSize, "ROMANTASY_WRITE_SAMPLES") == 0 && flag != nullptr;
    const bool write = have && std::strcmp(flag, "1") == 0;
    std::free(flag);
    if (!write) {
        return;
    }
    std::filesystem::create_directories(kDir);
    for (const auto& r : Records()) {
        if (std::filesystem::exists(kDir / r.file)) {
            std::printf("  kept %s\n", (kDir / r.file).string().c_str());
            continue;
        }
        const Bytes bytes = r.write();
        CHECK(!bytes.empty());
        std::ofstream out(kDir / r.file, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        CHECK(static_cast<bool>(out));
        std::printf("  wrote %s (%zu bytes)\n", (kDir / r.file).string().c_str(), bytes.size());
    }
}

TEST(SaveSamples_Reads_points_v3) { CheckSampleReads("points"); }
TEST(SaveSamples_Reads_player_bonds_v1) { CheckSampleReads("player_bonds"); }
TEST(SaveSamples_Reads_moods_v1) { CheckSampleReads("moods"); }
TEST(SaveSamples_Reads_pending_v1) { CheckSampleReads("pending"); }
TEST(SaveSamples_Reads_journals_v1) { CheckSampleReads("journals"); }

// The exact values the moments sample holds, pinned here as well as in Moments().
TEST(SaveSamples_Reads_moments_v1)
{
    CheckSampleReads("moments");
    const Bytes sample = LoadSample("moments_v1.bin");
    MemReader reader(sample);
    const auto read = ReadMoments(reader, static_cast<std::uint32_t>(sample.size()));
    CHECK(read.complete);
    CHECK(read.entries.size() == 2);
    if (read.entries.size() != 2) {
        return;
    }
    const auto& a = read.entries[0];
    CHECK(a.reference == FormIdentity({ "Follower.esp", 0x801, 0x05000801 }));
    CHECK(a.base == FormIdentity({ "Follower.esp", 0x800, 0x05000800 }));
    CHECK(a.state.count == 4 && a.state.firstDay == 3.25f && a.state.lastDay == 17.75f && a.state.lastPointsDay == 17.75f);
    const auto& b = read.entries[1];
    CHECK(b.reference == FormIdentity{});
    CHECK(b.base == FormIdentity({ "Other.esp", 0x900, 0x06000900 }));
    CHECK(b.state.count == 1 && b.state.firstDay == 8.5f && b.state.lastDay == 8.5f && b.state.lastPointsDay == -1.0f);
}

// Every record written back to back, as a save would carry them, then read back one by one.
TEST(SaveSamples_FullRoundTrip)
{
    Bytes all;
    std::vector<std::uint32_t> lengths;
    for (const auto& r : Records()) {
        const Bytes one = r.write();
        CHECK(!one.empty());
        lengths.push_back(static_cast<std::uint32_t>(one.size()));
        all.insert(all.end(), one.begin(), one.end());
    }
    std::size_t at = 0;
    for (std::size_t i = 0; i < Records().size(); ++i) {
        const Record& r = Records()[i];
        std::size_t count = 0;
        bool complete = false;
        {
            const Bytes slice(all.begin() + static_cast<std::ptrdiff_t>(at),
                all.begin() + static_cast<std::ptrdiff_t>(at + lengths[i]));
            count = r.read(slice, lengths[i], complete);
            CHECK(r.matchesExpected(slice));
        }
        CHECK(complete);
        CHECK(count == r.expectedCount);
        at += lengths[i];
    }
    CHECK(at == all.size());
}

// The samples joined end to end read the same way when one reader walks them in order.
TEST(SaveSamples_SamplesReadBackToBack)
{
    Bytes all;
    std::vector<std::uint32_t> lengths;
    for (const auto& r : Records()) {
        const Bytes sample = LoadSample(r.file);
        if (!HaveSample(r.file, sample)) {
            CHECK(false);
            return;
        }
        lengths.push_back(static_cast<std::uint32_t>(sample.size()));
        all.insert(all.end(), sample.begin(), sample.end());
    }
    MemReader reader(all);
    // Read through the codec directly so the same reader advances across records.
    const auto points = ReadPoints(reader, lengths[0]);
    const auto bonds = ReadPlayerBonds(reader, lengths[1]);
    const auto moods = ReadMoods(reader, lengths[2]);
    const auto pending = ReadPending(reader, lengths[3]);
    const auto journals = ReadJournals(reader, lengths[4]);
    const auto moments = ReadMoments(reader, lengths[5]);
    CHECK(points.complete && points.entries == Points());
    CHECK(bonds.complete && bonds.entries == Bonds());
    CHECK(moods.complete && moods.entries == Moods());
    CHECK(pending.complete && pending.entries == Pending());
    CHECK(journals.complete && journals.entries == Journals());
    CHECK(moments.complete && moments.entries == Moments());
    CHECK(reader.pos == all.size());
}

// Every prefix of every sample reads without a crash, is reported incomplete, and never yields
// an entry that differs from the full read or more entries than it.
TEST(SaveSamples_TruncationSweep)
{
    for (const auto& r : Records()) {
        const Bytes sample = LoadSample(r.file);
        if (!HaveSample(r.file, sample)) {
            CHECK(false);
            continue;
        }
        bool fullComplete = false;
        const std::size_t full = r.read(sample, static_cast<std::uint32_t>(sample.size()), fullComplete);
        CHECK(fullComplete);
        CHECK(full == r.expectedCount);
        for (std::size_t n = 0; n < sample.size(); ++n) {
            const Bytes part(sample.begin(), sample.begin() + static_cast<std::ptrdiff_t>(n));
            // The length the record header would carry for the short data.
            bool complete = true;
            const std::size_t got = r.read(part, static_cast<std::uint32_t>(n), complete);
            if (complete || got > full || !r.prefixOfExpected(part, static_cast<std::uint32_t>(n))) {
                std::printf("  %s: prefix of %zu bytes (length = size): complete=%d entries=%zu of %zu\n", r.file, n,
                    complete ? 1 : 0, got, full);
                CHECK(false);
            }
            // A header that promises the whole record while the data stops early.
            complete = true;
            const std::size_t got2 = r.read(part, static_cast<std::uint32_t>(sample.size()), complete);
            if (complete || got2 > full || !r.prefixOfExpected(part, static_cast<std::uint32_t>(sample.size()))) {
                std::printf("  %s: prefix of %zu bytes (length = full): complete=%d entries=%zu of %zu\n", r.file, n,
                    complete ? 1 : 0, got2, full);
                CHECK(false);
            }
        }
    }
}

// The codec reads one version of each record; older versions of RoPt (1 and 2) are read inside
// RomanceManager from raw structs and are not in the codec. This pins the versions the samples
// were made for: a version bump fails here until a new sample (and a reader for the old one) exists.
TEST(SaveSamples_VersionsPinned)
{
    for (const auto& r : Records()) {
        const std::string file = r.file;
        const std::string tag = "_v" + std::to_string(r.version) + ".bin";
        CHECK(file.size() > tag.size() && file.compare(file.size() - tag.size(), tag.size(), tag) == 0);
    }
    CHECK(kPointsVersion == 3);
    CHECK(kPlayerBondsVersion == 1);
    CHECK(kMoodsVersion == 1);
    CHECK(kPendingVersion == 1);
    CHECK(kJournalsVersion == 1);
    CHECK(kMomentsVersion == 1);
}
