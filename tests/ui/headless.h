#pragma once

#include "ui/screens/Theme.h"
#include "ui/screens/UiState.h"

#include <imgui.h>

// One ImGui context with no renderer: NewFrame bakes glyphs on demand as long
// as RendererHasTextures is set. Destroyed with the frame object.
struct HeadlessFrame
{
    ImGuiContext* context = nullptr;
    romantasy::ui::theme::Fonts fonts;   // all null: widgets fall back to ImGui's font
    romantasy::ui::theme::Canvas canvas;

    HeadlessFrame(float width, float height)
    {
        context = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.Fonts->AddFontDefault();
        io.DisplaySize = ImVec2(width, height);
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        canvas = romantasy::ui::theme::Canvas::Fit(io.DisplaySize);
    }

    ~HeadlessFrame() { ImGui::DestroyContext(context); }

    HeadlessFrame(const HeadlessFrame&) = delete;
    HeadlessFrame& operator=(const HeadlessFrame&) = delete;

    void Begin() { ImGui::NewFrame(); }
    void End() { ImGui::EndFrame(); }
};

inline romantasy::ui::UiState FixtureState()
{
    using namespace romantasy::ui;
    UiState state;
    FollowerRow row;
    row.name = "Lydia";
    row.role = "Nord \xC2\xB7 Housecarl";
    row.profileOrigin = "player";
    row.personalityEditable = true;
    row.removable = true;
    row.sourcePlugin = "Skyrim.esm";
    row.referenceFormID = "000A2C8E";
    row.baseFormID = "000A2C94";
    row.points = 640;
    row.isFollowing = true;
    row.likes = { "Dungeons Cleared" };
    row.dislikes = { "Items Stolen" };
    state.followers.push_back(row);
    state.candidates.push_back({ "Illia", "Imperial \xC2\xB7 Mage", "Skyrim.esm", "0001A6A6", "0001A6A5" });
    state.preferenceOptions = {
        { "ROM_DungeonsCleared", "Dungeons Cleared", "Exploration & Growth", 2 },
        { "ROM_ItemsStolen", "Items Stolen", "Crime & Transgression", 1 },
        { "ROM_Murders", "Murders", "Crime & Transgression", 5 },
    };
    state.revision = 1;
    return state;
}
