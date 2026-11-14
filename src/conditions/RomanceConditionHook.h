#pragma once

namespace RomanceConditionHook
{
    bool Install();
    // The GetGraphVariableInt replacement is in place and its parameter probe passed.
    [[nodiscard]] bool Installed();
}
