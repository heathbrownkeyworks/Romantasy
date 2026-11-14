#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace romantasy::profilecheck
{
    struct Finding
    {
        enum class Level { Error, Warning } level = Level::Error;
        std::int32_t line = 0;  // 1-based; 0 when the finding has no known line
        std::string message;
    };

    // Everything the real profile parser rejects is reported as an Error (its own message),
    // then Warnings are added for likely mistakes. Sorted by line, errors before warnings.
    // `fileName` is only used in the parser's own messages; it is stripped from the findings.
    [[nodiscard]] std::vector<Finding> Lint(std::string_view text, std::string_view fileName, bool playerOwned = false);
}
