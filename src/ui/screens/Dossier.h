#pragma once

#include "ui/screens/Ledger.h"

#include <string>

namespace romantasy::ui
{
    struct SealInfo
    {
        const char* label = "AU";
        const char* title = "";
        std::string description;
        ImU32 color = theme::Copper;
    };

    // Ownership seal text for authored and player-created profiles.
    [[nodiscard]] SealInfo SealFor(const FollowerRow& row);
    // "260 until Spouse" or "Vow sealed in the ledger".
    [[nodiscard]] std::string LegendRight(std::int32_t points);
    // "2,240 / 2,500 bond" or "2,630 bond".
    [[nodiscard]] std::string BondText(std::int32_t points);

    struct DossierResult
    {
        bool editRequested = false;
        bool resetRequested = false;
        bool removeRequested = false;
    };

    // The dossier pane. Player-created rows get Edit personality / Reset bond / Remove bond under the milestones.
    DossierResult DrawDossier(const widgets::Ctx& c, const FollowerRow& row, LedgerViewState& view, const ShellLayout& layout, float dt, bool interactive);
    // Empty roster page. Returns true when "Add a companion" is clicked.
    bool DrawEmptyPage(const widgets::Ctx& c, const ShellLayout& layout, bool interactive);
}
