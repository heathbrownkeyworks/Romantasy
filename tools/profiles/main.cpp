#include "profiles/Lint.h"
#include "romance/RomanceProfiles.h"

#include <nlohmann/json.hpp>

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using romantasy::profilecheck::Finding;
    using Level = Finding::Level;

    constexpr const char* kUsage =
        "Usage: romantasy-profile-check [--strict] [--json] [--player] [--] <file-or-folder>...\n"
        "\n"
        "  A folder is checked for every .toml file directly inside it (not its subfolders).\n"
        "  --strict   warnings make the exit code 2\n"
        "  --json     print the findings as one JSON array on stdout and nothing else\n"
        "  --player   check as player profiles (PlayerProfiles folder rules)\n"
        "  --         end of options\n"
        "\n"
        "Exit code: 0 clean, 1 an error was found, 2 warnings only with --strict, 3 the command line was wrong.\n";

    std::string Utf8(const fs::path& path)
    {
        const auto text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    std::string LowerAscii(std::string value)
    {
        for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return value;
    }

    struct FileResult
    {
        std::string name;
        std::vector<Finding> findings;
        std::optional<romantasy::NpcProfile> profile;  // set when the file has no error
        std::string folder;                            // the folder argument it came from, if any
    };

    bool HasError(const std::vector<Finding>& findings)
    {
        return std::ranges::any_of(findings, [](const Finding& f) { return f.level == Level::Error; });
    }

    FileResult Missing(std::string name, std::string message)
    {
        FileResult result;
        result.name = std::move(name);
        result.findings.push_back({ Level::Error, 0, std::move(message) });
        return result;
    }

    FileResult CheckFile(const fs::path& path, bool playerOwned, std::string folder)
    {
        FileResult result;
        result.name = Utf8(path);
        result.folder = std::move(folder);
        std::ifstream input(path, std::ios::binary);
        if (!input) return Missing(result.name, "cannot open the file");
        const std::string text{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
        if (input.bad()) return Missing(result.name, "cannot read the file");
        result.findings = romantasy::profilecheck::Lint(text, result.name, playerOwned);
        if (!HasError(result.findings)) {
            try {
                result.profile = romantasy::ParseProfile(text, result.name, playerOwned);
            } catch (const std::exception&) {
            }
        }
        return result;
    }

    std::string IdentityKey(const romantasy::NpcProfile& profile)
    {
        return LowerAscii(profile.plugin) + "|" + std::to_string(profile.localID);
    }

    // Two files in one folder that describe the same NPC are both rejected by Romantasy.
    void FlagConflicts(std::vector<FileResult>& results, std::size_t from, std::size_t to)
    {
        std::unordered_map<std::string, std::vector<std::size_t>> byIdentity;
        for (std::size_t i = from; i < to; ++i) {
            if (results[i].profile) byIdentity[IdentityKey(*results[i].profile)].push_back(i);
        }
        for (const auto& [key, indexes] : byIdentity) {
            if (indexes.size() < 2) continue;
            std::string sources;
            for (const auto i : indexes) sources += " " + results[i].name;
            for (const auto i : indexes) {
                auto& result = results[i];
                result.findings.insert(result.findings.begin(),
                    Finding{ Level::Error, 0, "conflicting profiles for " + key + "; all definitions rejected:" + sources });
                result.profile.reset();
            }
        }
    }

    void CheckPath(const fs::path& argument, bool playerOwned, std::vector<FileResult>& results)
    {
        std::error_code ec;
        if (fs::is_directory(argument, ec)) {
            std::vector<fs::path> files;
            try {
                for (const auto& entry : fs::directory_iterator(argument)) {
                    if (entry.is_regular_file() && LowerAscii(Utf8(entry.path().extension())) == ".toml") {
                        files.push_back(entry.path());
                    }
                }
            } catch (const std::exception& e) {
                results.push_back(Missing(Utf8(argument), std::string("cannot read the folder: ") + e.what()));
                return;
            }
            std::ranges::sort(files, [](const fs::path& a, const fs::path& b) {
                const auto x = Utf8(a.filename()), y = Utf8(b.filename());
                const auto lx = LowerAscii(x), ly = LowerAscii(y);
                return lx != ly ? lx < ly : x < y;
            });
            const auto first = results.size();
            for (const auto& file : files) results.push_back(CheckFile(file, playerOwned, Utf8(argument)));
            FlagConflicts(results, first, results.size());
            return;
        }
        if (fs::is_regular_file(argument, ec)) {
            results.push_back(CheckFile(argument, playerOwned, {}));
            return;
        }
        results.push_back(Missing(Utf8(argument), fs::exists(argument, ec) ? "not a file or a folder" : "no such file or folder"));
    }

    const char* LevelName(Level level) { return level == Level::Error ? "error" : "warning"; }
}

int wmain(int argc, wchar_t** argv)
{
    // Plain LF on both streams, so scripts read the same bytes on every machine.
    (void)_setmode(_fileno(stdout), _O_BINARY);
    (void)_setmode(_fileno(stderr), _O_BINARY);
    bool strict = false, json = false, playerOwned = false, optionsEnded = false;
    std::vector<fs::path> paths;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (!optionsEnded && arg == L"--") optionsEnded = true;
        else if (!optionsEnded && arg == L"--strict") strict = true;
        else if (!optionsEnded && arg == L"--json") json = true;
        else if (!optionsEnded && arg == L"--player") playerOwned = true;
        else if (!optionsEnded && (arg == L"--help" || arg == L"-h")) {
            std::fputs(kUsage, stdout);
            return 0;
        } else if (!optionsEnded && arg.starts_with(L"-") && arg.size() > 1) {
            std::fprintf(stderr, "Unknown option: %s\n%s", Utf8(fs::path(arg)).c_str(), kUsage);
            return 3;
        } else {
            paths.emplace_back(arg);
        }
    }
    if (paths.empty()) {
        std::fputs(kUsage, stderr);
        return 3;
    }

    std::vector<FileResult> results;
    for (const auto& path : paths) CheckPath(path, playerOwned, results);

    std::size_t errors = 0, warnings = 0, valid = 0;
    for (const auto& result : results) {
        for (const auto& finding : result.findings) (finding.level == Level::Error ? errors : warnings)++;
        if (result.profile) ++valid;
    }

    if (json) {
        auto array = nlohmann::json::array();
        for (const auto& result : results) {
            for (const auto& finding : result.findings) {
                array.push_back({ { "file", result.name }, { "line", finding.line },
                    { "level", LevelName(finding.level) }, { "message", finding.message } });
            }
        }
        std::cout << array.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
    } else {
        for (const auto& result : results) {
            for (const auto& finding : result.findings) {
                std::cout << result.name;
                if (finding.line > 0) std::cout << '(' << finding.line << ')';
                std::cout << ": " << LevelName(finding.level) << ": " << finding.message << '\n';
            }
        }
        for (const auto& result : results) {
            if (!result.profile) continue;
            const auto& profile = *result.profile;
            std::cout << result.name << ": " << profile.plugin << "|0x" << std::hex << profile.localID
                      << std::dec << ", starting level " << profile.startingLevel
                      << ", " << profile.preferences.size() << " preferences"
                      << (profile.enabled ? "" : ", disabled") << '\n';
        }
        std::cout << valid << " valid profile(s), " << errors << " error(s), " << warnings << " warning(s).\n"
                  << "NPC existence and record type are checked by Romantasy when Skyrim loads.\n";
    }
    std::cout.flush();
    if (errors > 0) return 1;
    return strict && warnings > 0 ? 2 : 0;
}
