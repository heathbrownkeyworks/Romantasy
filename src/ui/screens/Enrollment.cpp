#include "ui/screens/Enrollment.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace romantasy::ui
{
    namespace
    {
        constexpr float kAsideW = 352.0f, kGap = 20.0f;
        constexpr const char* kCreateTitle = "ENTER A NEW BOND";
        constexpr const char* kEditTitle = "REWRITE THE PAGE";
        constexpr const char* kCreateCopy = "Choose a companion Romantasy does not know yet, then stamp the deeds that deepen or wound the bond.";
        constexpr const char* kEditCopy = "Only pages written by you can be rewritten. Personalities supplied by follower mods stay protected.";
        constexpr const char* kNote = "Your personalities are shared across saves using this configuration. Each save keeps its own earned bond points.";
        constexpr const char* kNoOneTitle = "No one to enter";
        constexpr const char* kNoOneCopy = "Only followers travelling with you right now, who have no Romantasy page yet, can be entered. Bring one along and reopen this page.";
        constexpr const char* kNotFoundTitle = "Bond not found";
        constexpr const char* kNotFoundCopy = "This companion is no longer in the ledger.";
        constexpr const char* kEditorSub = "Every deed starts neutral. Stamp the ones that matter to them.";
        constexpr const char* kFooterHint = "Nothing changes until the page is sealed.";

        std::string Lower(std::string_view text)
        {
            std::string out(text);
            for (auto& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return out;
        }

        std::string Trim(std::string text)
        {
            const auto first = text.find_first_not_of(' ');
            if (first == std::string::npos) return {};
            const auto last = text.find_last_not_of(' ');
            return text.substr(first, last - first + 1);
        }

        // "role · plugin" with either side omitted when empty; "Companion" when both are.
        // Shared by CandidateMeta and the Edit-mode subject card so they never drift apart.
        std::string MetaJoin(const std::string& role, const std::string& plugin)
        {
            if (role.empty() && plugin.empty()) return "Companion";
            if (role.empty()) return plugin;
            if (plugin.empty()) return role;
            return role + " \xC2\xB7 " + plugin;
        }

        // Same match test as MatchesSearch, but takes a query already trimmed and
        // lowercased once by the caller instead of re-normalizing it per option per frame.
        bool MatchesNormalized(const PreferenceOption& option, const std::string& normalizedQuery)
        {
            if (normalizedQuery.empty()) return true;
            return Lower(option.label + " " + option.category).find(normalizedQuery) != std::string::npos;
        }

        // "COMPANION" style field label with an optional right-aligned Fg4 detail.
        void FieldLabel(const widgets::Ctx& c, ImDrawList* dl, ImVec2 pos, float width, const char* label, const std::string& right)
        {
            const float px = c.S(9.5f);
            widgets::TrackedText(dl, c.fonts.bodyBold, px, pos, c.A(theme::CopperLight), widgets::Uppercase(label), px * 0.16f);
            if (!right.empty()) theme::DrawTextAligned(dl, c.fonts.body, c.S(10.0f), { pos.x + width, pos.y }, c.A(theme::Fg4), right.c_str(), 1.0f);
        }

        // 80-unit seal ring with NEW / YOU and the two amber diamonds.
        void DrawSeal(const widgets::Ctx& c, ImDrawList* dl, ImVec2 center, const char* label)
        {
            const float r = c.S(40.0f);
            theme::GlowCircle(dl, center, r * 1.35f, theme::Amber, 0.14f * c.alpha);
            dl->AddCircleFilled(center, r, c.A(theme::WithAlpha(theme::Amber, 0.06f)), 48);
            theme::GlowCircle(dl, center, r * 0.6f, theme::Amber, 0.13f * c.alpha, 32);
            dl->AddCircle(center, r - c.S(2.8f), c.A(theme::WithAlpha(theme::Copper, 0.08f)), 48, c.S(5.6f));  // inset ring
            dl->AddCircle(center, r, c.A(theme::WithAlpha(theme::Amber, 0.5f)), 48, 1.0f);
            const float px = c.S(22.0f), tracking = px * 0.08f;
            const ImVec2 extent = widgets::MeasureTracked(c.fonts.ceremonial, px, label, tracking);
            widgets::TrackedText(dl, c.fonts.ceremonial, px, { center.x - extent.x * 0.5f, center.y - extent.y * 0.5f }, c.A(theme::Amber), label, tracking);
            for (const float side : { -1.0f, 1.0f }) {
                const ImVec2 d(center.x + side * (r + c.S(14.0f)), center.y);
                const float h = c.S(3.5f);
                const ImVec2 pts[4] = { { d.x, d.y - h }, { d.x + h, d.y }, { d.x, d.y + h }, { d.x - h, d.y } };
                dl->AddConvexPolyFilled(pts, 4, c.A(theme::WithAlpha(theme::Amber, 0.65f)));
            }
        }

        float DrawNoMatch(const widgets::Ctx& c, ImDrawList* dl, ImVec2 pos, float width, const char* title, const char* copy)
        {
            const float cx = pos.x + width * 0.5f;
            theme::DrawTextAligned(dl, c.fonts.ceremonial, c.S(13.6f), { cx, pos.y + c.S(40.0f) }, c.A(theme::Fg1), title, 0.5f);
            theme::DrawTextAligned(dl, c.fonts.body, c.S(11.5f), { cx, pos.y + c.S(62.0f) }, c.A(theme::Fg3), copy, 0.5f);
            return c.S(102.0f);
        }

        void DrawAside(const widgets::Ctx& c, const UiState& state, EnrollmentViewState& e, ImVec2 min, ImVec2 max, bool live)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool editing = e.mode == EnrollMode::Edit;
            const float x = min.x + c.S(20.0f), w = max.x - min.x - c.S(40.0f);
            const float cx = (min.x + max.x) * 0.5f;
            float y = min.y + c.S(24.0f);

            DrawSeal(c, dl, { cx, y + c.S(40.0f) }, editing ? "YOU" : "NEW");
            y += c.S(80.0f) + c.S(14.0f);
            {
                const float px = c.S(18.0f), tracking = px * 0.08f;
                const char* title = editing ? kEditTitle : kCreateTitle;
                const ImVec2 extent = widgets::MeasureTracked(c.fonts.ceremonial, px, title, tracking);
                widgets::TrackedText(dl, c.fonts.ceremonial, px, { cx - extent.x * 0.5f, y }, c.A(theme::Fg1), title, tracking);
            }
            y += c.S(26.0f);
            const float copyW = std::min(w, c.S(280.0f));
            const char* copy = editing ? kEditCopy : kCreateCopy;
            y += widgets::WrappedText(c, dl, c.fonts.body, c.S(11.0f), { cx - copyW * 0.5f, y }, c.A(theme::Fg3), copy, copyW) + c.S(18.0f);

            // Companion
            const std::string count = editing ? std::string("sealed to this page") : (std::to_string(state.candidates.size()) + " travelling with you");
            FieldLabel(c, dl, { x, y }, w, "Companion", (state.candidates.empty() && !editing) ? std::string{} : count);
            y += c.S(18.0f);
            if (editing) {
                if (const FollowerRow* row = FindRow(state, e.identity)) {
                    ImGui::SetCursorScreenPos({ x, y });
                    const std::string meta = MetaJoin(row->role, row->sourcePlugin);
                    widgets::RadioCard(c, "##subject", w, row->name.c_str(), meta.c_str(), true, true);
                    y += c.S(widgets::RadioCardHeight);
                } else {
                    y += widgets::DashedNote(c, dl, { x, y }, w, kNotFoundTitle, kNotFoundCopy);
                }
            } else if (state.candidates.empty()) {
                y += widgets::DashedNote(c, dl, { x, y }, w, kNoOneTitle, kNoOneCopy);
            } else {
                const float rowH = c.S(widgets::RadioCardHeight) + c.S(7.0f);
                const float listH = std::min(c.S(184.0f), rowH * static_cast<float>(state.candidates.size()));
                ImGui::SetCursorScreenPos({ x, y });
                ImGui::BeginChild("##candidates", { w, listH }, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                for (std::size_t i = 0; i < state.candidates.size(); ++i) {
                    const EnrollmentCandidate& candidate = state.candidates[i];
                    ImGui::SetCursorScreenPos({ x, ImGui::GetCursorScreenPos().y });
                    ImGui::PushID(static_cast<int>(i));
                    const std::string meta = CandidateMeta(candidate);
                    if (widgets::RadioCard(c, "##candidate", w - c.S(10.0f), candidate.name.c_str(), meta.c_str(), candidate.referenceFormID == e.identity, false) && live) {
                        e.identity = candidate.referenceFormID;
                    }
                    ImGui::PopID();
                    ImGui::Dummy({ 0.0f, c.S(7.0f) });
                }
                ImGui::EndChild();
                y += listH;
            }
            y += c.S(18.0f);

            // The page so far
            const StagedLists lists = StagedListsFor(state.preferenceOptions, e.staged);
            {
                const float px = c.S(9.5f);
                widgets::TrackedText(dl, c.fonts.bodyBold, px, { x, y }, c.A(theme::CopperLight), "THE PAGE SO FAR", px * 0.16f);
                const std::string likes = std::to_string(lists.likes.size()), dislikes = std::to_string(lists.dislikes.size());
                const float n = c.S(10.0f);
                const char* likesWord = " likes \xC2\xB7 ";
                const char* dislikesWord = " dislikes";
                const float w1 = theme::MeasureText(c.fonts.ceremonial, n, likes.c_str()).x;
                const float w2 = theme::MeasureText(c.fonts.body, n, likesWord).x;
                const float w3 = theme::MeasureText(c.fonts.ceremonial, n, dislikes.c_str()).x;
                const float w4 = theme::MeasureText(c.fonts.body, n, dislikesWord).x;
                float rx = x + w - (w1 + w2 + w3 + w4);
                theme::DrawText(dl, c.fonts.ceremonial, n, { rx, y }, c.A(theme::Amber), likes.c_str()); rx += w1;
                theme::DrawText(dl, c.fonts.body, n, { rx, y }, c.A(theme::Fg4), likesWord); rx += w2;
                theme::DrawText(dl, c.fonts.ceremonial, n, { rx, y }, c.A(theme::RoseSoft), dislikes.c_str()); rx += w3;
                theme::DrawText(dl, c.fonts.body, n, { rx, y }, c.A(theme::Fg4), dislikesWord);
            }
            y += c.S(18.0f);

            const float noteH = widgets::WrappedHeight(c.fonts.body, c.S(9.6f), kNote, w) + c.S(14.0f);
            const float noteTop = max.y - c.S(18.0f) - noteH;
            const float previewH = std::max(c.S(40.0f), noteTop - c.S(14.0f) - y);
            ImGui::SetCursorScreenPos({ x, y });
            ImGui::BeginChild("##preview", { w, previewH }, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
            ImDrawList* pdl = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float colW = (w - c.S(14.0f)) * 0.5f;
            auto column = [&](float colX, const char* title, ImU32 color, const std::vector<std::string>& items, widgets::Icon icon) {
                widgets::TrackedText(pdl, c.fonts.bodyBold, c.S(9.5f), { colX, origin.y }, c.A(color), title, c.S(9.5f) * 0.16f);
                float cy = origin.y + c.S(20.0f);
                if (items.empty()) {
                    theme::DrawText(pdl, c.fonts.bodyLight, c.S(10.6f), { colX, cy }, c.A(theme::Fg4), "Nothing stamped yet.");
                    return cy + c.S(16.0f) - origin.y;
                }
                for (const auto& item : items) {
                    widgets::DrawIcon(pdl, icon, { colX + c.S(6.0f), cy + c.S(7.0f) }, c.S(11.0f), c.A(color), std::max(1.0f, c.S(1.4f)));
                    theme::DrawText(pdl, c.fonts.body, c.S(10.9f), { colX + c.S(20.0f), cy }, c.A(theme::Fg1), widgets::ClipText(c.fonts.body, c.S(10.9f), item, colW - c.S(22.0f)).c_str());
                    cy += c.S(18.0f);
                }
                return cy - origin.y;
            };
            const float h1 = column(origin.x, "DEEPENS THE BOND", theme::Amber, lists.likes, widgets::Icon::Heart);
            const float h2 = column(origin.x + colW + c.S(14.0f), "WOUNDS THE BOND", theme::RoseSoft, lists.dislikes, widgets::Icon::Claw);
            ImGui::SetCursorScreenPos({ origin.x, origin.y + std::max(h1, h2) });
            ImGui::Dummy({ w, 1.0f });
            ImGui::EndChild();

            dl->AddLine({ x, noteTop }, { x + w, noteTop }, c.A(theme::WithAlpha(theme::Fg1, 0.06f)), 1.0f);
            widgets::WrappedText(c, dl, c.fonts.body, c.S(9.6f), { x, noteTop + c.S(14.0f) }, c.A(theme::Fg4), kNote, w);
        }

        struct EditorResult
        {
            bool cancel = false;
            bool commit = false;
        };

        EditorResult DrawEditor(const widgets::Ctx& c, const UiState& state, EnrollmentViewState& e, ImVec2 min, ImVec2 max, bool live)
        {
            EditorResult result;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool editing = e.mode == EnrollMode::Edit;
            const float hx = min.x + c.S(20.0f);

            // Header
            const float headH = c.S(67.0f);
            widgets::TrackedText(dl, c.fonts.ceremonial, c.S(14.4f), { hx, min.y + c.S(18.0f) }, c.A(theme::Fg1), "LIKES & DISLIKES", c.S(14.4f) * 0.07f);
            theme::DrawText(dl, c.fonts.body, c.S(10.2f), { hx, min.y + c.S(40.0f) }, c.A(theme::Fg3), kEditorSub);
            ImGui::SetCursorScreenPos({ max.x - c.S(20.0f) - c.S(256.0f), min.y + c.S(16.0f) });
            widgets::TextField(c, "##search", e.search, sizeof(e.search), { c.S(256.0f), c.S(34.0f) }, "Search deeds", false, 11.8f);
            dl->AddLine({ min.x, min.y + headH }, { max.x, min.y + headH }, c.A(theme::WithAlpha(theme::Fg1, 0.07f)), 1.0f);

            // Footer
            const float footH = c.S(14.0f) * 2.0f + c.S(38.0f);
            const float footTop = max.y - footH;
            dl->AddRectFilled({ min.x, footTop }, max, c.A(theme::WithAlpha(theme::Obsidian, 0.18f)), c.S(theme::RadiusXl), ImDrawFlags_RoundCornersBottom);
            dl->AddLine({ min.x, footTop }, { max.x, footTop }, c.A(theme::WithAlpha(theme::Fg1, 0.07f)), 1.0f);
            theme::DrawText(dl, c.fonts.body, c.S(10.6f), { hx, footTop + c.S(26.0f) }, c.A(theme::Fg3), kFooterHint);
            const char* commitLabel = editing ? "Seal personality" : "Begin bond";
            const float commitW = widgets::ButtonWidth(c, commitLabel, widgets::ButtonKind::Primary);
            const float cancelW = widgets::ButtonWidth(c, "Cancel", widgets::ButtonKind::Ghost);
            ImGui::SetCursorScreenPos({ max.x - c.S(20.0f) - commitW - c.S(10.0f) - cancelW, footTop + c.S(14.0f) });
            if (widgets::Button(c, "##cancel", "Cancel", widgets::ButtonKind::Ghost, live)) result.cancel = true;
            ImGui::SetCursorScreenPos({ max.x - c.S(20.0f) - commitW, footTop + c.S(14.0f) });
            const bool canCommit = live && HasSubject(state, e) && !state.preferenceOptions.empty();
            if (widgets::Button(c, "##commit", commitLabel, widgets::ButtonKind::Primary, canCommit)) result.commit = true;

            // Deed list
            const std::string query = Trim(Lower(e.search));
            const bool searchChanged = query != e.lastSearch;
            e.lastSearch = query;
            const float listTop = min.y + headH, listBottom = footTop;
            ImGui::SetCursorScreenPos({ min.x, listTop });
            ImGui::BeginChild("##deeds", { max.x - min.x, listBottom - listTop }, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
            if (searchChanged) ImGui::SetScrollY(0.0f);
            ImDrawList* ldl = ImGui::GetWindowDrawList();
            const float lx = min.x + c.S(20.0f);
            const float lw = max.x - min.x - c.S(20.0f) - c.S(16.0f) - c.S(10.0f);  // 10 for the scrollbar
            float y = ImGui::GetCursorScreenPos().y;
            if (state.preferenceOptions.empty()) {
                y += DrawNoMatch(c, ldl, { lx, y }, lw, "LEDGER UNAVAILABLE", "Deed definitions have not arrived from the game yet.");
            } else {
                int visibleTotal = 0, categoryIndex = 0;
                const float groupW = widgets::StampGroupWidth(c);
                std::size_t i = 0;
                while (i < state.preferenceOptions.size()) {
                    const std::string& category = state.preferenceOptions[i].category;
                    std::size_t j = i;
                    int matches = 0;
                    while (j < state.preferenceOptions.size() && state.preferenceOptions[j].category == category) {
                        if (MatchesNormalized(state.preferenceOptions[j], query)) ++matches;
                        ++j;
                    }
                    if (matches > 0) {
                        y += categoryIndex == 0 ? c.S(6.0f) : c.S(14.0f);
                        widgets::TrackedText(ldl, c.fonts.bodyBold, c.S(9.3f), { lx, y + c.S(9.0f) }, c.A(theme::Amber), widgets::Uppercase(category.empty() ? "Other" : category), c.S(9.3f) * 0.18f);
                        const std::string deeds = DeedCountLabel(matches);
                        theme::DrawTextAligned(ldl, c.fonts.body, c.S(9.6f), { lx + lw, y + c.S(9.0f) }, c.A(theme::Fg4), deeds.c_str(), 1.0f);
                        y += c.S(28.0f);
                        ldl->AddLine({ lx, y }, { lx + lw, y }, c.A(theme::WithAlpha(theme::Amber, 0.16f)), 1.0f);
                        int drawn = 0;
                        for (std::size_t k = i; k < j; ++k) {
                            const PreferenceOption& option = state.preferenceOptions[k];
                            if (!MatchesNormalized(option, query)) continue;
                            ++drawn;
                            const float rowH = c.S(42.0f);
                            const auto found = e.staged.find(option.editorID);
                            std::int8_t direction = found == e.staged.end() ? std::int8_t{ 0 } : found->second;
                            ImGui::SetCursorScreenPos({ lx + lw - groupW, y + (rowH - c.S(34.0f)) * 0.5f });
                            ImGui::PushID(static_cast<int>(k));
                            if (widgets::StampGroup(c, "##stamp", direction) && live) {
                                if (direction == 0) e.staged.erase(option.editorID);
                                else e.staged[option.editorID] = direction;
                            }
                            ImGui::PopID();
                            const ImU32 labelColor = direction > 0 ? theme::AmberLight : (direction < 0 ? theme::RoseSoft : theme::Fg1);
                            const float textW = lw - groupW - c.S(16.0f);
                            theme::DrawText(ldl, c.fonts.bodySemi, c.S(12.2f), { lx, y + c.S(7.0f) }, c.A(labelColor), widgets::ClipText(c.fonts.bodySemi, c.S(12.2f), option.label, textW).c_str());
                            const std::string per = std::to_string(std::abs(option.points)) + " bond per event";
                            widgets::TrackedText(ldl, c.fonts.body, c.S(9.3f), { lx, y + c.S(24.0f) }, c.A(theme::Fg4), per, c.S(9.3f) * 0.02f);
                            y += rowH;
                            if (drawn < matches) ldl->AddLine({ lx, y }, { lx + lw, y }, c.A(theme::WithAlpha(theme::Fg1, 0.05f)), 1.0f);
                        }
                        visibleTotal += matches;
                        ++categoryIndex;
                    }
                    i = j;
                }
                if (visibleTotal == 0) y += DrawNoMatch(c, ldl, { lx, y }, lw, "NO DEED MATCHES", "Try a shorter word, or clear the search.");
            }
            ImGui::SetCursorScreenPos({ lx, y + c.S(16.0f) });
            ImGui::Dummy({ 1.0f, 1.0f });  // establishes the scroll extent
            ImGui::EndChild();
            return result;
        }
    }

    PreferencePage BuildCompletePage(const std::vector<PreferenceOption>& options, const std::map<std::string, std::int8_t>& staged)
    {
        PreferencePage page;
        page.reserve(options.size());
        for (const auto& option : options) {
            const auto found = staged.find(option.editorID);
            const std::int32_t direction = found == staged.end() ? 0 : std::clamp<std::int32_t>(found->second, -1, 1);
            page.emplace_back(option.editorID, direction);
        }
        return page;
    }

    StagedLists StagedListsFor(const std::vector<PreferenceOption>& options, const std::map<std::string, std::int8_t>& staged)
    {
        StagedLists lists;
        for (const auto& option : options) {
            const auto found = staged.find(option.editorID);
            if (found == staged.end()) continue;
            const std::string& label = option.label.empty() ? option.editorID : option.label;
            if (found->second > 0) lists.likes.push_back(label);
            else if (found->second < 0) lists.dislikes.push_back(label);
        }
        return lists;
    }

    std::map<std::string, std::int8_t> ProfileFromRow(const std::vector<PreferenceOption>& options, const FollowerRow& row)
    {
        std::map<std::string, std::int8_t> staged;
        for (const auto& option : options) {
            if (std::find(row.likes.begin(), row.likes.end(), option.label) != row.likes.end()) staged[option.editorID] = 1;
            else if (std::find(row.dislikes.begin(), row.dislikes.end(), option.label) != row.dislikes.end()) staged[option.editorID] = -1;
        }
        return staged;
    }

    bool MatchesSearch(const PreferenceOption& option, std::string_view query)
    {
        return MatchesNormalized(option, Trim(Lower(query)));
    }

    std::string CandidateMeta(const EnrollmentCandidate& candidate)
    {
        return MetaJoin(candidate.role, candidate.sourcePlugin);
    }

    std::string DeedCountLabel(int visible)
    {
        return std::to_string(visible) + (visible == 1 ? " deed" : " deeds");
    }

    const EnrollmentCandidate* FindCandidate(const UiState& state, std::string_view identity)
    {
        if (identity.empty()) return nullptr;
        for (const auto& candidate : state.candidates) {
            if (candidate.referenceFormID == identity) return &candidate;
        }
        return nullptr;
    }

    const FollowerRow* FindRow(const UiState& state, std::string_view identity)
    {
        if (identity.empty()) return nullptr;
        for (const auto& row : state.followers) {
            if (RowIdentity(row) == identity) return &row;
        }
        return nullptr;
    }

    bool HasSubject(const UiState& state, const EnrollmentViewState& enroll)
    {
        if (enroll.identity.empty()) return false;
        return enroll.mode == EnrollMode::Edit ? FindRow(state, enroll.identity) != nullptr : FindCandidate(state, enroll.identity) != nullptr;
    }

    void EnsureSelection(const UiState& state, EnrollmentViewState& enroll)
    {
        if (enroll.mode != EnrollMode::Create || FindCandidate(state, enroll.identity)) return;
        enroll.identity = state.candidates.empty() ? std::string{} : state.candidates.front().referenceFormID;
    }

    bool OpenEnrollment(const UiState& state, LedgerViewState& view, EnrollMode mode, std::string_view identity)
    {
        EnrollmentViewState& e = view.enroll;
        if (mode == EnrollMode::Edit) {
            const FollowerRow* row = FindRow(state, identity);
            if (!row || row->profileOrigin != "player" || !row->personalityEditable) return false;
            e.mode = EnrollMode::Edit;
            e.identity = std::string(identity);
            e.staged = ProfileFromRow(state.preferenceOptions, *row);
        } else {
            e.mode = EnrollMode::Create;
            e.identity = state.candidates.empty() ? std::string{} : state.candidates.front().referenceFormID;
            e.staged.clear();
        }
        e.search[0] = '\0';
        e.lastSearch.clear();
        e.closing = false;
        e.out = {};
        e.in.Start(0.30f);
        e.focusPending = true;
        view.screen = LedgerScreen::Enrollment;
        return true;
    }

    void CloseEnrollment(LedgerViewState& view) noexcept
    {
        EnrollmentViewState& e = view.enroll;
        if (view.screen != LedgerScreen::Enrollment || e.closing) return;
        e.closing = true;
        e.out.Start(0.15f);
    }

    BondRequest MakeEnrollmentRequest(const UiState& state, const EnrollmentViewState& enroll)
    {
        BondRequest request;
        request.kind = enroll.mode == EnrollMode::Edit ? BondRequest::Kind::Replace : BondRequest::Kind::Enroll;
        request.identity = enroll.identity;
        request.preferences = BuildCompletePage(state.preferenceOptions, enroll.staged);
        return request;
    }

    EnrollmentResult DrawEnrollment(const widgets::Ctx& base, const UiState& state, LedgerViewState& view, const ShellLayout& layout, bool interactive, float dt)
    {
        EnrollmentResult result;
        EnrollmentViewState& e = view.enroll;
        const float inP = anim::EaseOut(e.in.Advance(dt));
        float outP = 0.0f;
        if (e.closing) {
            outP = anim::EaseSoft(e.out.Advance(dt));
            if (!e.out.Running()) {
                e.closing = false;
                view.screen = LedgerScreen::Console;
                view.focusPending = true;  // hand nav focus back to the roster
                return result;
            }
        }
        const float alpha = std::clamp(inP * (1.0f - outP), 0.0f, 1.0f);
        const float scale = 0.985f + 0.015f * inP;
        theme::Canvas scaled = base.canvas;  // scales every c.S() with the takeover
        scaled.scale *= scale;
        const widgets::Ctx c{ base.fonts, scaled, base.alpha * alpha };
        const bool live = interactive && !e.closing;
        const bool editing = e.mode == EnrollMode::Edit;
        EnsureSelection(state, e);

        ImGui::SetNextWindowPos(layout.min);
        ImGui::SetNextWindowSize({ layout.max.x - layout.min.x, layout.max.y - layout.min.y });
        if (e.focusPending) {
            ImGui::SetNextWindowFocus();
            e.focusPending = false;
        }
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::Begin("##romantasy-enroll", nullptr, flags);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(layout.min, layout.max, base.A(theme::WithAlpha(theme::Obsidian, 0.97f * alpha)));
        widgets::GlowBackdrop(c, dl, layout.min, layout.max);

        // The content frame shrinks about the display centre with `scale`.
        const ImVec2 center((layout.min.x + layout.max.x) * 0.5f, (layout.min.y + layout.max.y) * 0.5f);
        const ImVec2 frameMin(center.x - (center.x - layout.min.x) * scale, center.y - (center.y - layout.min.y) * scale);
        const ImVec2 frameMax(center.x + (layout.max.x - center.x) * scale, center.y + (layout.max.y - center.y) * scale);
        const float left = frameMin.x + c.S(56.0f), right = frameMax.x - c.S(56.0f);
        const float bottom = frameMax.y - c.S(32.0f);
        float y = frameMin.y + c.S(40.0f);

        if (!live) ImGui::BeginDisabled();
        ImGui::SetCursorScreenPos({ left, y });
        if (widgets::BackLink(c, "##back", editing ? "Back to companion" : "Back to company") && live) result.cancelled = true;
        y += c.S(34.0f) + c.S(16.0f);

        const ImVec2 asideMin(left, y), asideMax(left + c.S(kAsideW), bottom);
        widgets::CardFrame(c, dl, asideMin, asideMax);
        DrawAside(c, state, e, asideMin, asideMax, live);

        const ImVec2 editorMin(asideMax.x + c.S(kGap), y), editorMax(right, bottom);
        widgets::CardFrame(c, dl, editorMin, editorMax);
        const EditorResult editor = DrawEditor(c, state, e, editorMin, editorMax, live);
        if (!live) ImGui::EndDisabled();
        if (editor.cancel) result.cancelled = true;
        if (editor.commit) result.committed = true;

        if (DrawShellClose(c, layout, live)) result.closeRequested = true;
        ImGui::End();
        return result;
    }
}
