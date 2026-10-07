#pragma once

#include <string>
#include <string_view>

namespace romantasy::ui
{
    // Uppercase for UTF-8 text in any alphabet (Latin with accents, Cyrillic,
    // Greek...), using Windows' locale-independent case mapping. Scripts without
    // case (Chinese, Japanese, Korean) and symbols pass through unchanged.
    [[nodiscard]] std::string UppercaseUtf8(std::string_view text);
}
