#pragma once

#include "ui/screens/Ledger.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace romantasy::ui
{
    using PreferencePage = std::vector<std::pair<std::string, std::int32_t>>;

    struct StagedLists
    {
        std::vector<std::string> likes;
        std::vector<std::string> dislikes;
    };

    // Every option in order with its staged direction, 0 when unstamped.
    [[nodiscard]] PreferencePage BuildCompletePage(const std::vector<PreferenceOption>& options, const std::map<std::string, std::int8_t>& staged);
    // Labels of the staged likes / dislikes, in option order.
    [[nodiscard]] StagedLists StagedListsFor(const std::vector<PreferenceOption>& options, const std::map<std::string, std::int8_t>& staged);
    // Edit mode: a row's likes / dislikes (labels) mapped back to option editor IDs.
    [[nodiscard]] std::map<std::string, std::int8_t> ProfileFromRow(const std::vector<PreferenceOption>& options, const FollowerRow& row);
    // Case-insensitive substring match on "label category"; blank queries match everything.
    [[nodiscard]] bool MatchesSearch(const PreferenceOption& option, std::string_view query);
    // "role - plugin" (middle dot at render time) with either side omitted when empty; "Companion" when both are.
    [[nodiscard]] std::string CandidateMeta(const EnrollmentCandidate& candidate);
    // "1 deed" or "N deeds" for a category header's visible (search-matched) row count.
    [[nodiscard]] std::string DeedCountLabel(int visible);

    [[nodiscard]] const EnrollmentCandidate* FindCandidate(const UiState& state, std::string_view identity);
    [[nodiscard]] const FollowerRow* FindRow(const UiState& state, std::string_view identity);
    [[nodiscard]] bool HasSubject(const UiState& state, const EnrollmentViewState& enroll);
    // Create mode: re-pick the first candidate when the chosen one is gone.
    void EnsureSelection(const UiState& state, EnrollmentViewState& enroll);

    // Opens the takeover and starts its fade in. Edit mode needs a player-created,
    // editable row; returns false (changing nothing) otherwise.
    bool OpenEnrollment(const UiState& state, LedgerViewState& view, EnrollMode mode, std::string_view identity);
    // Starts the fade out; DrawEnrollment flips the screen back to Console when it finishes.
    void CloseEnrollment(LedgerViewState& view) noexcept;
    // Enroll (Create) or Replace (Edit) with the complete page.
    [[nodiscard]] BondRequest MakeEnrollmentRequest(const UiState& state, const EnrollmentViewState& enroll);

    struct EnrollmentResult
    {
        bool cancelled = false;      // Back link, Cancel
        bool committed = false;      // Begin bond / Seal personality
        bool closeRequested = false; // the shell close glyph
    };

    // Draws the takeover in its own top-level window (above the console's
    // child windows), including the shell close glyph. Call after the ledger
    // window's End().
    EnrollmentResult DrawEnrollment(const widgets::Ctx& c, const UiState& state, LedgerViewState& view, const ShellLayout& layout, bool interactive, float dt);
}
