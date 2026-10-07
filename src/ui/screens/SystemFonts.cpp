#include "ui/screens/SystemFonts.h"

// Windows.h stays out of Theme.cpp: it turns DrawText into a macro.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

namespace romantasy::ui::theme
{
    namespace
    {
        std::filesystem::path WindowsFontsFolder()
        {
            wchar_t windows[MAX_PATH]{};
            const auto length = GetWindowsDirectoryW(windows, MAX_PATH);
            if (length == 0 || length >= MAX_PATH) return {};
            return std::filesystem::path(windows) / L"Fonts";
        }

        // The first of `files` that exists and reads, cached for the process;
        // nullptr when none does. std::map keeps every cached vector in place.
        const std::vector<unsigned char>* FirstFont(const std::filesystem::path& folder, const std::vector<const wchar_t*>& files)
        {
            static std::map<std::wstring, std::vector<unsigned char>> cache;
            for (const wchar_t* file : files) {
                const auto path = folder / file;
                if (const auto it = cache.find(path.native()); it != cache.end()) return &it->second;
                std::ifstream in(path, std::ios::binary);
                if (!in) continue;
                std::vector<unsigned char> data{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
                if (data.empty()) continue;
                return &cache.emplace(path.native(), std::move(data)).first->second;
            }
            return nullptr;
        }

        // Chinese, Japanese and Korean share many characters but draw them
        // differently, so the font of Windows' display language comes first.
        std::vector<std::vector<const wchar_t*>> EastAsianFonts()
        {
            const std::vector<const wchar_t*> chinese{ L"msyh.ttc", L"simsun.ttc" };
            const std::vector<const wchar_t*> japanese{ L"YuGothR.ttc", L"meiryo.ttc", L"msgothic.ttc" };
            const std::vector<const wchar_t*> korean{ L"malgun.ttf" };
            switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
            case LANG_JAPANESE: return { japanese, chinese, korean };
            case LANG_KOREAN: return { korean, chinese, japanese };
            default: return { chinese, japanese, korean };
            }
        }
    }

    std::vector<const std::vector<unsigned char>*> FallbackFonts(const wchar_t* weightFile)
    {
        std::vector<const std::vector<unsigned char>*> fonts;
        const auto folder = WindowsFontsFolder();
        if (folder.empty()) return fonts;
        if (const auto* segoe = FirstFont(folder, { weightFile, L"segoeui.ttf" })) fonts.push_back(segoe);
        for (const auto& files : EastAsianFonts()) {
            if (const auto* font = FirstFont(folder, files)) fonts.push_back(font);
        }
        return fonts;
    }
}
