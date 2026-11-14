#include "check.h"
#include "util/AtomicFile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using romantasy::files::WriteTextAtomically;
namespace fs = std::filesystem;

namespace
{
    fs::path FreshFolder(const char* name)
    {
        const auto folder = fs::temp_directory_path() / (std::string("romantasy_atomic_") + name);
        std::error_code ec;
        fs::remove_all(folder, ec);
        fs::create_directories(folder);
        return folder;
    }

    std::string ReadAll(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    void Remove(const fs::path& folder)
    {
        std::error_code ec;
        fs::remove_all(folder, ec);
    }
}

TEST(AtomicFile_WritesNewFile)
{
    const auto folder = FreshFolder("new");
    const auto target = folder / "settings.json";
    CHECK(WriteTextAtomically(target, "{\"a\":1}\r\nline"));
    CHECK(ReadAll(target) == "{\"a\":1}\r\nline");
    CHECK(!fs::exists(folder / "settings.json.tmp"));
    Remove(folder);
}

TEST(AtomicFile_ReplacesExistingFile)
{
    const auto folder = FreshFolder("replace");
    const auto target = folder / "settings.json";
    CHECK(WriteTextAtomically(target, "first"));
    CHECK(WriteTextAtomically(target, "second"));
    CHECK(ReadAll(target) == "second");
    CHECK(!fs::exists(folder / "settings.json.tmp"));
    Remove(folder);
}

TEST(AtomicFile_FailureKeepsOriginal)
{
    const auto folder = FreshFolder("fail");
    CHECK(!WriteTextAtomically(folder / "missing" / "settings.json", "x"));
    CHECK(!fs::exists(folder / "missing"));

    const auto target = folder / "settings.json";
    CHECK(WriteTextAtomically(target, "original"));
    fs::create_directory(folder / "settings.json.tmp");  // the temporary path is a folder: the write cannot open it
    CHECK(!WriteTextAtomically(target, "replacement"));
    CHECK(ReadAll(target) == "original");
    // Nothing new, and the folder in the way is left alone: exactly what the test put there.
    std::vector<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(folder)) entries.push_back(entry.path().filename());
    std::ranges::sort(entries);
    CHECK(entries == (std::vector<fs::path>{ "settings.json", "settings.json.tmp" }));
    CHECK(fs::is_directory(folder / "settings.json.tmp"));
    fs::remove(folder / "settings.json.tmp");
    CHECK(WriteTextAtomically(target, "after"));
    CHECK(ReadAll(target) == "after");
    Remove(folder);
}

TEST(AtomicFile_WritesEmptyText)
{
    const auto folder = FreshFolder("empty");
    const auto target = folder / "empty.txt";
    CHECK(WriteTextAtomically(target, ""));
    CHECK(fs::exists(target));
    CHECK(fs::file_size(target) == 0);
    Remove(folder);
}
