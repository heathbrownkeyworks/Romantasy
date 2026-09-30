#pragma once

#include "ui/screens/Anim.h"
#include "ui/screens/Shell.h"
#include "ui/screens/Theme.h"
#include "ui/screens/Tiers.h"
#include "ui/screens/UiState.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace romantasy::ui
{
    enum class LedgerPane { Dossier, Settings };
    enum class LedgerScreen { Console, Enrollment };
    enum class LedgerModal { None, ConfirmReset, ConfirmRemove, Password };
    enum class EnrollMode { Create, Edit };

    // The enrollment takeover's staging. `staged` holds only stamped deeds
    // (absent = neutral); the complete page is built at commit time.
    struct EnrollmentViewState
    {
        EnrollMode mode = EnrollMode::Create;
        std::string identity;                       // chosen candidate (Create) or edited row (Edit)
        std::map<std::string, std::int8_t> staged;  // editorID -> -1 / 1
        char search[64] = {};
        std::string lastSearch;                     // scroll-to-top detection
        anim::Tween in;
        anim::Tween out;
        bool closing = false;
        bool focusPending = false;                  // first frame: take nav focus
    };

    // Bottom-centre outcome toast. Armed when an action is emitted; shows the
    // next message that arrives with a new state revision.
    struct NoticeState
    {
        enum class Phase { Hidden, In, Hold, Out };
        std::string text;
        bool warn = false;
        Phase phase = Phase::Hidden;
        anim::Tween tween;
        float shown = 0.0f;
        bool armed = false;
        std::uint32_t seenRevision = 0;

        void Arm(std::uint32_t currentRevision) noexcept
        {
            armed = true;
            seenRevision = currentRevision;
        }
    };

    inline constexpr int SettingsKnobCount = 5;

    struct LedgerViewState
    {
        int selected = 0;
        int lastSelected = -1;
        bool focusPending = false;
        LedgerPane pane = LedgerPane::Dossier;
        LedgerPane lastPane = LedgerPane::Dossier;
        anim::Tween appIn;
        anim::Tween dossierSlide;
        anim::Tween paneSwap;
        float time = 0.0f;
        float knobs[SettingsKnobCount] = {};  // settings toggle knob positions
        bool knobsPrimed = false;

        LedgerScreen screen = LedgerScreen::Console;
        LedgerModal modal = LedgerModal::None;
        std::string modalIdentity;            // subject of a confirm modal
        anim::Tween modalIn;
        anim::Tween modalOut;
        bool modalClosing = false;
        bool modalFocusPending = false;
        EnrollmentViewState enroll;
        NoticeState notice;
        char password[64] = {};
        bool passwordRejected = false;

        void OnOpen()
        {
            appIn.Start(0.16f);
            dossierSlide = {};
            paneSwap = {};
            lastSelected = -1;
            focusPending = true;
            time = 0.0f;
            pane = LedgerPane::Dossier;
            lastPane = LedgerPane::Dossier;
            knobsPrimed = false;
            screen = LedgerScreen::Console;
            modal = LedgerModal::None;
            modalIdentity.clear();
            modalIn = {};
            modalOut = {};
            modalClosing = false;
            modalFocusPending = false;
            enroll = {};
            notice = {};
            password[0] = '\0';
            passwordRejected = false;
        }

        void OpenModal(LedgerModal kind, std::string identity)
        {
            modal = kind;
            modalIdentity = std::move(identity);
            modalClosing = false;
            modalIn.Start(0.20f);
            modalOut = {};
            modalFocusPending = true;
            password[0] = '\0';
            passwordRejected = false;
        }

        // Starts the fade out; DrawModal flips `modal` to None when it finishes.
        void CloseModal() noexcept
        {
            if (modal == LedgerModal::None || modalClosing) {
                return;
            }
            modalClosing = true;
            modalOut.Start(0.15f);
        }
    };

    struct SettingsChange
    {
        std::optional<bool> openWithFavorites;
        std::optional<bool> showGainModals;
        std::optional<bool> showLossModals;
        std::optional<bool> showAwayFollowers;
    };

    // A player-created bond action for the facade. `preferences` is the
    // complete page (every deed, neutral unless stamped) for Enroll / Replace.
    struct BondRequest
    {
        enum class Kind { Enroll, Replace, Reset, Remove };
        Kind kind = Kind::Enroll;
        std::string identity;  // hex FormID text
        std::vector<std::pair<std::string, std::int32_t>> preferences;
    };

    struct DebugRequest
    {
        std::optional<std::int32_t> points;  // DebugAddPoints
        std::string stat;                    // DebugApplyStat(stat, delta) when `points` is empty
        std::int32_t delta = 0;
    };

    struct LedgerActions
    {
        bool closeRequested = false;
        bool refreshRequested = false;
        std::optional<SettingsChange> settings;
        std::optional<BondRequest> bond;
        std::optional<bool> developerTools;  // true = password accepted, false = locked
        std::optional<DebugRequest> debug;
    };

    struct PaneResult
    {
        bool editRequested = false;
        bool resetRequested = false;
        bool removeRequested = false;
        bool addCompanionRequested = false;
    };

    using tiers::TierProgress;
    // "1,240" style formatting without locale dependence.
    [[nodiscard]] std::string FormatPoints(std::int32_t points);
    // "2,240 / 2,500" or just "2,630" at Spouse.
    [[nodiscard]] std::string FormatPointsWithThreshold(std::int32_t points);

    // Right-pane content (Dossier or Settings).
    PaneResult DrawPaneContent(const widgets::Ctx& c, const UiState& state, const FollowerRow* selected,
        LedgerViewState& view, const ShellLayout& layout, bool interactive, float dt, LedgerActions& actions);

    // Draws the whole console for one frame inside an ImGui frame: the ledger
    // window, then (when open) the enrollment and modal windows above it, then
    // the notice on the foreground draw list.
    // interactive = false draws without reading keys (a popup overlay owns input).
    LedgerActions DrawLedger(
        const UiState& state,
        LedgerViewState& view,
        const theme::Fonts& fonts,
        const theme::Canvas& canvas,
        float dt,
        bool interactive,
        bool controllerActive = false);
}
