#pragma once

namespace PointsConditionHook
{
    bool Install();
    // The GetFactionRank replacement is in place.
    [[nodiscard]] bool Installed();
    // Install found ROM_RomancePoints (false before Install runs, or when CS_Romantasy.esp lacks it).
    [[nodiscard]] bool FactionFound();
}
