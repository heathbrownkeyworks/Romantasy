#include "ui/screens/TextCase.h"

// Windows.h stays out of the drawing files: it turns DrawText into a macro.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace romantasy::ui
{
    std::string UppercaseUtf8(std::string_view text)
    {
        if (text.empty()) return {};
        const int inputBytes = static_cast<int>(text.size());
        const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputBytes, nullptr, 0);
        if (wideLength <= 0) return std::string(text);  // not valid UTF-8: leave it as it is
        std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), inputBytes, wide.data(), wideLength);

        const int upperLength = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wide.c_str(), wideLength, nullptr, 0, nullptr, nullptr, 0);
        if (upperLength <= 0) return std::string(text);
        std::wstring upper(static_cast<std::size_t>(upperLength), L'\0');
        LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, wide.c_str(), wideLength, upper.data(), upperLength, nullptr, nullptr, 0);

        const int outBytes = WideCharToMultiByte(CP_UTF8, 0, upper.c_str(), upperLength, nullptr, 0, nullptr, nullptr);
        if (outBytes <= 0) return std::string(text);
        std::string out(static_cast<std::size_t>(outBytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, upper.c_str(), upperLength, out.data(), outBytes, nullptr, nullptr);
        return out;
    }
}
