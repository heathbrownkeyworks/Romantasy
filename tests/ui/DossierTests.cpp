#include "check.h"

#include "ui/screens/Dossier.h"
#include "headless.h"

#include <imgui_internal.h>
#include <cstring>

using namespace romantasy::ui;

TEST(seal_for_author_defined_profile)
{
    FollowerRow row;
    row.profileOrigin = "author";
    row.sourcePlugin = "CSV_Aelira.esp";
    const SealInfo seal = SealFor(row);
    CHECK(std::string(seal.label) == "AU");
    CHECK(std::string(seal.title) == "Author-defined personality");
    CHECK(seal.description == "Sealed by the follower author in CSV_Aelira.esp. Romantasy will not alter or remove it.");
    CHECK(seal.color == theme::Copper);

    row.profileFile = "Mornhilde.toml";
    const SealInfo fileSeal = SealFor(row);
    CHECK(fileSeal.description == "Preferences loaded from Mornhilde.toml.");
    CHECK(std::string(fileSeal.title) == "Configured personality");
    CHECK(std::string(fileSeal.label) == "CFG");
}

TEST(seal_for_author_without_plugin_name)
{
    FollowerRow row;
    row.profileOrigin = "author";
    CHECK(SealFor(row).description == "Sealed by the follower author. Romantasy will not alter or remove it.");
}

TEST(seal_for_player_created_profile)
{
    FollowerRow row;
    row.profileOrigin = "player";
    row.personalityEditable = true;
    const SealInfo seal = SealFor(row);
    CHECK(std::string(seal.label) == "YOU");
    CHECK(std::string(seal.title) == "Player-created personality");
    CHECK(seal.description == "This ledger page belongs to you. Its likes and dislikes may be rewritten.");
    CHECK(seal.color == theme::Amber);
    row.profileFile = "skyrim.esm.000A2C8E.toml";
    CHECK(std::string(SealFor(row).title) == "Player-created personality");
    CHECK(std::string(SealFor(row).label) == "YOU");
}

TEST(seal_for_external_or_locked_player_profile_is_managed)
{
    FollowerRow external;
    external.profileOrigin = "external";
    CHECK(std::string(SealFor(external).title) == "Managed personality");
    CHECK(SealFor(external).description == "Supplied by another compatible mod at runtime. Romantasy treats it as read-only.");
    FollowerRow lockedPlayer;
    lockedPlayer.profileOrigin = "player";
    lockedPlayer.personalityEditable = false;
    CHECK(std::string(SealFor(lockedPlayer).title) == "Managed personality");
    CHECK(std::string(SealFor(lockedPlayer).label) == "AU");
}

TEST(legend_and_bond_text_follow_the_ladder)
{
    CHECK(LegendRight(2240) == "260 until Spouse");
    CHECK(LegendRight(0) == "500 until Acquaintance");
    CHECK(LegendRight(2630) == "Vow sealed in the ledger");
    CHECK(BondText(2240) == "2,240 / 2,500 bond");
    CHECK(BondText(2630) == "2,630 bond");
}

TEST(preference_lists_scroll_to_the_last_entry_with_mouse_and_controller)
{
    HeadlessFrame frame(1600.0f, 900.0f);
    UiState state = FixtureState();
    auto& row = state.followers.front();
    row.profileOrigin = "author";
    row.personalityEditable = false;
    row.likes.assign(24, "Dungeons Cleared");
    row.dislikes.assign(8, "Items Stolen");
    LedgerViewState view;
    view.OnOpen();
    auto draw = [&] {
        frame.Begin();
        DrawLedger(state, view, frame.fonts, frame.canvas, 1.0f / 60.0f, true);
        frame.End();
    };
    for (int i = 0; i < 40; ++i) draw();
    ImGuiWindow* likes = nullptr;
    ImGuiWindow* dislikes = nullptr;
    for (auto* window : ImGui::GetCurrentContext()->Windows) {
        if (std::strstr(window->Name, "/##likes_")) likes = window;
        if (std::strstr(window->Name, "/##dislikes_")) dislikes = window;
    }
    CHECK(likes != nullptr && dislikes != nullptr);
    if (!likes || !dislikes) return;
    CHECK(likes->ScrollMax.y > 500.0f);
    CHECK(dislikes->ScrollMax.y > 50.0f);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(likes->Pos.x + 30.0f, likes->Pos.y + 30.0f);
    draw();
    io.AddMouseWheelEvent(0.0f, -100.0f);
    draw();
    draw();
    CHECK_NEAR(likes->Scroll.y, likes->ScrollMax.y, 1.0f);
    CHECK_NEAR(dislikes->Scroll.y, 0.0f, 1.0f);

    // Reach the other scrollable child through normal keyboard navigation,
    // then exercise the same left-stick input path used by the gamepad.
    io.AddMousePosEvent(-1000.0f, -1000.0f);
    for (int i = 0; i < 12 && ImGui::GetCurrentContext()->NavId != dislikes->ChildId; ++i) {
        io.AddKeyEvent(ImGuiKey_Tab, true);
        draw();
        io.AddKeyEvent(ImGuiKey_Tab, false);
        draw();
    }
    CHECK(ImGui::GetCurrentContext()->NavId == dislikes->ChildId);
    io.AddKeyEvent(ImGuiKey_Enter, true);
    draw();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    draw();
    CHECK(ImGui::GetCurrentContext()->NavWindow == dislikes);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, true, 1.0f);
    for (int i = 0; i < 90; ++i) draw();
    io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, false, 0.0f);
    CHECK_NEAR(dislikes->Scroll.y, dislikes->ScrollMax.y, 1.0f);
}
