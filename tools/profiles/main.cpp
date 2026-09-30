#include "romance/RomanceProfiles.h"

#include <iostream>

int main(int argc, char** argv)
{
    const bool playerOwned = argc == 3 && std::string_view(argv[2]) == "--player";
    if (argc != 2 && !playerOwned) {
        std::cerr << "Usage: romantasy-profile-check <directory> [--player]\n";
        return 2;
    }
    std::error_code directoryError;
    if (!std::filesystem::is_directory(argv[1], directoryError)) {
        std::cerr << "Profile directory does not exist: " << argv[1] << '\n';
        return 2;
    }
    const auto result = romantasy::LoadProfiles(argv[1], playerOwned);
    for (const auto& error : result.errors) std::cerr << "ERROR: " << error << '\n';
    for (const auto& profile : result.profiles) {
        std::cout << profile.source << ": " << profile.plugin << "|0x" << std::hex << profile.localID
                  << std::dec << ", starting level " << profile.startingLevel
                  << ", " << profile.preferences.size() << " preferences"
                  << (profile.enabled ? "" : ", disabled") << '\n';
    }
    std::cout << result.profiles.size() << " valid profile(s), " << result.errors.size() << " error(s).\n"
              << "NPC existence and record type are checked by Romantasy when Skyrim loads.\n";
    return result.errors.empty() ? 0 : 1;
}
