#include <cstdio>

#include "check.h"

#include "headless.h"
#include "ui/screens/Enrollment.h"
#include "ui/screens/Ledger.h"

using namespace romantasy::ui;

TEST(enrollment_complete_page_covers_every_option)
{
    const UiState state = FixtureState();
    std::map<std::string, std::int8_t> staged;
    staged["ROM_Murders"] = -1;
    staged["ROM_DungeonsCleared"] = 1;
    staged["ROM_NotAnOption"] = 1;  // ignored: not in the option list
    const PreferencePage page = BuildCompletePage(state.preferenceOptions, staged);
    CHECK(page.size() == 3);
    CHECK(page[0].first == "ROM_DungeonsCleared" && page[0].second == 1);
    CHECK(page[1].first == "ROM_ItemsStolen" && page[1].second == 0);
    CHECK(page[2].first == "ROM_Murders" && page[2].second == -1);
}

TEST(enrollment_staged_lists_follow_option_order)
{
    const UiState state = FixtureState();
    std::map<std::string, std::int8_t> staged;
    staged["ROM_Murders"] = -1;
    staged["ROM_ItemsStolen"] = -1;
    staged["ROM_DungeonsCleared"] = 1;
    const StagedLists lists = StagedListsFor(state.preferenceOptions, staged);
    CHECK(lists.likes.size() == 1 && lists.likes[0] == "Dungeons Cleared");
    CHECK(lists.dislikes.size() == 2 && lists.dislikes[0] == "Items Stolen" && lists.dislikes[1] == "Murders");
}

TEST(enrollment_profile_from_row_maps_labels_back_to_editor_ids)
{
    const UiState state = FixtureState();
    const auto staged = ProfileFromRow(state.preferenceOptions, state.followers[0]);
    CHECK(staged.size() == 2);
    CHECK(staged.at("ROM_DungeonsCleared") == 1);
    CHECK(staged.at("ROM_ItemsStolen") == -1);
    CHECK(staged.count("ROM_Murders") == 0);
}

TEST(enrollment_search_matches_label_and_category_case_insensitively)
{
    const PreferenceOption option{ "ROM_Murders", "Murders", "Crime & Transgression", 5 };
    CHECK(MatchesSearch(option, ""));
    CHECK(MatchesSearch(option, "   "));
    CHECK(MatchesSearch(option, "murd"));
    CHECK(MatchesSearch(option, "CRIME"));
    CHECK(MatchesSearch(option, "  transgression "));
    CHECK(!MatchesSearch(option, "bribe"));
}

TEST(enrollment_candidate_meta_joins_role_and_plugin)
{
    CHECK(CandidateMeta({ "Illia", "Imperial \xC2\xB7 Mage", "Skyrim.esm", "0001A6A6", "0001A6A5" }) == "Imperial \xC2\xB7 Mage \xC2\xB7 Skyrim.esm");
    CHECK(CandidateMeta({ "Eola", "", "Skyrim.esm", "", "" }) == "Skyrim.esm");
    CHECK(CandidateMeta({ "Eola", "Breton", "", "", "" }) == "Breton");
    CHECK(CandidateMeta({ "Eola", "", "", "", "" }) == "Companion");
}

TEST(enrollment_deed_count_label_is_singular_only_at_one)
{
    CHECK(DeedCountLabel(0) == "0 deeds");
    CHECK(DeedCountLabel(1) == "1 deed");
    CHECK(DeedCountLabel(2) == "2 deeds");
    CHECK(DeedCountLabel(3) == "3 deeds");
}

TEST(enrollment_subject_lookup_and_reselection)
{
    UiState state = FixtureState();
    EnrollmentViewState enroll;
    enroll.mode = EnrollMode::Create;
    enroll.identity = "DEADBEEF";
    CHECK(!HasSubject(state, enroll));
    EnsureSelection(state, enroll);
    CHECK(enroll.identity == "0001A6A6");
    CHECK(HasSubject(state, enroll));
    state.candidates.clear();
    EnsureSelection(state, enroll);
    CHECK(enroll.identity.empty());
    CHECK(!HasSubject(state, enroll));

    EnrollmentViewState edit;
    edit.mode = EnrollMode::Edit;
    edit.identity = "000A2C8E";
    CHECK(HasSubject(FixtureState(), edit));
    EnsureSelection(state, edit);  // edit mode never re-picks
    CHECK(edit.identity == "000A2C8E");
    CHECK(FindRow(FixtureState(), "000A2C8E") != nullptr);
    CHECK(FindRow(FixtureState(), "000A2C94") == nullptr);  // base id is not the identity while a reference exists
}

TEST(open_enrollment_edit_requires_a_player_created_row)
{
    UiState state = FixtureState();
    LedgerViewState view;
    CHECK(OpenEnrollment(state, view, EnrollMode::Edit, "000A2C8E"));
    CHECK(view.screen == LedgerScreen::Enrollment);
    CHECK(view.enroll.mode == EnrollMode::Edit);
    CHECK(view.enroll.identity == "000A2C8E");
    CHECK(view.enroll.staged.at("ROM_DungeonsCleared") == 1);
    CHECK(view.enroll.staged.at("ROM_ItemsStolen") == -1);
    CHECK(view.enroll.in.Running());
    CHECK(view.enroll.focusPending);

    LedgerViewState other;
    state.followers[0].personalityEditable = false;
    CHECK(!OpenEnrollment(state, other, EnrollMode::Edit, "000A2C8E"));
    CHECK(other.screen == LedgerScreen::Console);
    CHECK(!OpenEnrollment(state, other, EnrollMode::Edit, "DEADBEEF"));

    LedgerViewState create;
    CHECK(OpenEnrollment(state, create, EnrollMode::Create, ""));
    CHECK(create.enroll.mode == EnrollMode::Create);
    CHECK(create.enroll.identity == "0001A6A6");
    CHECK(create.enroll.staged.empty());
}

TEST(enrollment_request_carries_the_complete_page)
{
    const UiState state = FixtureState();
    LedgerViewState view;
    CHECK(OpenEnrollment(state, view, EnrollMode::Create, ""));
    view.enroll.staged["ROM_Murders"] = -1;
    const BondRequest create = MakeEnrollmentRequest(state, view.enroll);
    CHECK(create.kind == BondRequest::Kind::Enroll);
    CHECK(create.identity == "0001A6A6");
    CHECK(create.preferences.size() == 3);
    CHECK(create.preferences[2].second == -1);

    LedgerViewState edit;
    CHECK(OpenEnrollment(state, edit, EnrollMode::Edit, "000A2C8E"));
    const BondRequest replace = MakeEnrollmentRequest(state, edit.enroll);
    CHECK(replace.kind == BondRequest::Kind::Replace);
    CHECK(replace.identity == "000A2C8E");
    CHECK(replace.preferences[0].second == 1 && replace.preferences[1].second == -1 && replace.preferences[2].second == 0);
}

TEST(ledger_escape_backs_out_of_the_takeover_before_closing)
{
    HeadlessFrame frame(1280.0f, 720.0f);
    const UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();

    CHECK(OpenEnrollment(state, view, EnrollMode::Create, ""));
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();

    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
    frame.Begin();
    const LedgerActions backed = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    CHECK(!backed.closeRequested);
    CHECK(!backed.bond.has_value());
    CHECK(view.enroll.closing);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);

    // 0.19 s > the 0.15 s fade out; stop as soon as the takeover finishes closing so this
    // only checks the hand-back moment itself, not an unrelated later frame where the
    // (correct, pre-existing) roster would have already consumed the pending focus.
    for (int i = 0; i < 12 && view.screen != LedgerScreen::Console; ++i) {
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }
    CHECK(view.screen == LedgerScreen::Console);
    CHECK(view.focusPending);
}

TEST(ledger_draws_the_takeover_in_both_modes_headless)
{
    const float sizes[2][2] = { { 1600.0f, 900.0f }, { 1280.0f, 720.0f } };
    for (const auto& size : sizes) {
        for (int mode = 0; mode < 2; ++mode) {
            HeadlessFrame frame(size[0], size[1]);
            UiState state = FixtureState();
            if (mode == 1) state.candidates.clear();  // "No one to enter"
            LedgerViewState view;
            view.OnOpen();
            CHECK(OpenEnrollment(state, view, mode == 0 ? EnrollMode::Edit : EnrollMode::Create, mode == 0 ? "000A2C8E" : ""));
            std::snprintf(view.enroll.search, sizeof(view.enroll.search), "%s", mode == 0 ? "crime" : "zzz");
            for (int i = 0; i < 25; ++i) {  // past the 0.3 s fade in
                frame.Begin();
                const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
                frame.End();
                CHECK(!actions.closeRequested);
            }
            CHECK(view.screen == LedgerScreen::Enrollment);
        }
    }
}

TEST(ledger_begin_bond_emits_an_enroll_request)
{
    // Same geometry DrawEnrollment/DrawEditor compute, mirroring how the Task 3 modal
    // test locates its confirm button: measured once the takeover's entrance scale has
    // settled to 1.0 (25 frames > the 0.3 s fade-in), so frame == layout exactly.
    HeadlessFrame frame(1600.0f, 900.0f);
    const UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();

    CHECK(OpenEnrollment(state, view, EnrollMode::Create, ""));
    view.enroll.staged["ROM_Murders"] = -1;

    for (int i = 0; i < 25; ++i) {  // past the 0.3 s fade in
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }

    frame.Begin();
    ImGui::Begin("##measure", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    const widgets::Ctx c{ frame.fonts, frame.canvas, 1.0f };
    const ShellLayout layout = ComputeShellLayout(frame.canvas);
    const float left = layout.min.x + c.S(56.0f), right = layout.max.x - c.S(56.0f);
    const float bottom = layout.max.y - c.S(32.0f);
    float y = layout.min.y + c.S(40.0f);
    y += c.S(34.0f) + c.S(16.0f);  // back link row
    const float asideRight = left + c.S(352.0f);
    const ImVec2 editorMin(asideRight + c.S(20.0f), y), editorMax(right, bottom);
    const float footTop = editorMax.y - (c.S(14.0f) * 2.0f + c.S(38.0f));
    const float commitW = widgets::ButtonWidth(c, "Begin bond", widgets::ButtonKind::Primary);
    const ImVec2 commitCenter{ editorMax.x - c.S(20.0f) - commitW * 0.5f, footTop + c.S(14.0f) + c.S(19.0f) };
    ImGui::End();
    frame.End();

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(commitCenter.x, commitCenter.y);
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    frame.Begin();
    const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();

    CHECK(actions.bond.has_value());
    if (actions.bond.has_value()) {
        CHECK(actions.bond->kind == BondRequest::Kind::Enroll);
        CHECK(actions.bond->identity == "0001A6A6");
        CHECK(actions.bond->preferences.size() == 3);
        bool sawMurders = false;
        for (const auto& pref : actions.bond->preferences) {
            if (pref.first == "ROM_Murders") { sawMurders = true; CHECK(pref.second == -1); }
        }
        CHECK(sawMurders);
    }
    CHECK(view.notice.armed);
    CHECK(view.enroll.closing);
}

TEST(ledger_escape_in_the_search_field_does_not_back_out_of_the_takeover)
{
    // Same geometry ledger_begin_bond_emits_an_enroll_request derives for the footer,
    // extended to the search field's rect (Enrollment.cpp DrawEditor).
    HeadlessFrame frame(1600.0f, 900.0f);
    const UiState state = FixtureState();
    LedgerViewState view;
    view.OnOpen();
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();

    CHECK(OpenEnrollment(state, view, EnrollMode::Create, ""));

    for (int i = 0; i < 25; ++i) {  // past the 0.3 s fade in
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }

    frame.Begin();
    ImGui::Begin("##measure", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    const widgets::Ctx c{ frame.fonts, frame.canvas, 1.0f };
    const ShellLayout layout = ComputeShellLayout(frame.canvas);
    const float left = layout.min.x + c.S(56.0f), right = layout.max.x - c.S(56.0f);
    const float bottom = layout.max.y - c.S(32.0f);
    float y = layout.min.y + c.S(40.0f);
    y += c.S(34.0f) + c.S(16.0f);  // back link row
    const float asideRight = left + c.S(352.0f);
    const ImVec2 editorMin(asideRight + c.S(20.0f), y), editorMax(right, bottom);
    const ImVec2 searchMin(editorMax.x - c.S(20.0f) - c.S(256.0f), editorMin.y + c.S(16.0f));
    const ImVec2 searchCenter{ searchMin.x + c.S(256.0f) * 0.5f, searchMin.y + c.S(34.0f) * 0.5f };
    ImGui::End();
    frame.End();

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(searchCenter.x, searchCenter.y);
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    frame.Begin();
    DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);

    for (int i = 0; i < 2; ++i) {
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
        frame.End();
    }
    CHECK(io.WantTextInput);

    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame.Begin();
    const LedgerActions actions = DrawLedger(state, view, frame.fonts, frame.canvas, 0.016f, true);
    frame.End();
    io.AddKeyEvent(ImGuiKey_Escape, false);

    CHECK(!view.enroll.closing);
    CHECK(!actions.closeRequested);
}
