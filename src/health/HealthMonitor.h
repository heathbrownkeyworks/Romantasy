#pragma once

#include "health/Health.h"

// Gathers the health facts from the parts of the plugin that own them. Plugin-only; game thread.
namespace romantasy::health
{
    // Reads values already held (no file access, no form scans): cheap enough for every state publish.
    [[nodiscard]] Facts Gather();
    // Writes the report to the log on every call; the first call in the process also shows its HUD line, if any.
    void AnnounceOnce();
}
