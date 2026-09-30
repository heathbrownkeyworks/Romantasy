#include "check.h"

#include "ui/screens/Widgets.h"

#include <imgui.h>

using namespace romantasy::ui;

TEST(widgets_uppercase_keeps_non_ascii_bytes)
{
    CHECK(widgets::Uppercase("Your Company") == "YOUR COMPANY");
    CHECK(widgets::Uppercase("nord \xC2\xB7 bard") == "NORD \xC2\xB7 BARD");
    CHECK(widgets::Uppercase("") == "");
}

TEST(widgets_pip_fractions_span_the_meter)
{
    CHECK_NEAR(widgets::PipFraction(0), 0.0f, 1e-6);
    CHECK_NEAR(widgets::PipFraction(1), 0.2f, 1e-6);
    CHECK_NEAR(widgets::PipFraction(5), 1.0f, 1e-6);
}

TEST(widgets_knob_position_is_eased_and_clamped)
{
    CHECK_NEAR(widgets::KnobPosition(0.0f), 0.0f, 1e-5);
    CHECK_NEAR(widgets::KnobPosition(1.0f), 1.0f, 1e-5);
    CHECK_NEAR(widgets::KnobPosition(2.0f), 1.0f, 1e-5);
    CHECK(widgets::KnobPosition(0.5f) > 0.5f);
}

TEST(widgets_clip_text_adds_three_periods_when_too_wide)
{
    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImFont* font = io.Fonts->AddFontDefault();
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // headless: no renderer backend to set this for us; NewFrame() null-derefs in release (NDEBUG) without it
    ImGui::NewFrame();  // bakes glyphs on demand; no renderer backend needed
    const float wide = widgets::MeasureTracked(font, 13.0f, "Ysara Whitemane", 0.0f).x;
    CHECK(widgets::ClipText(font, 13.0f, "Ysara Whitemane", wide + 1.0f) == "Ysara Whitemane");
    const std::string clipped = widgets::ClipText(font, 13.0f, "Ysara Whitemane", wide * 0.5f);
    CHECK(clipped.size() >= 3 && clipped.compare(clipped.size() - 3, 3, "...") == 0);
    CHECK(clipped.size() < std::string("Ysara Whitemane").size() + 3);
    CHECK(widgets::ClipText(font, 13.0f, "abc", 0.0f) == "...");
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
}

TEST(widgets_tracked_measure_grows_with_tracking)
{
    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImFont* font = io.Fonts->AddFontDefault();
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // headless: no renderer backend to set this for us; NewFrame() null-derefs in release (NDEBUG) without it
    ImGui::NewFrame();  // bakes glyphs on demand; no renderer backend needed
    const float plain = widgets::MeasureTracked(font, 13.0f, "BONDS", 0.0f).x;
    const float tracked = widgets::MeasureTracked(font, 13.0f, "BONDS", 2.0f).x;
    CHECK_NEAR(tracked - plain, 2.0f * 4, 1e-3);  // tracking between 5 glyphs = 4 gaps
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
}

TEST(widgets_clip_text_never_splits_a_multibyte_sequence)
{
    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImFont* font = io.Fonts->AddFontDefault();
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // headless: no renderer backend to set this for us; NewFrame() null-derefs in release (NDEBUG) without it
    ImGui::NewFrame();  // bakes glyphs on demand; no renderer backend needed
    const std::string source = "Nord \xC2\xB7 Bard \xC2\xB7 Ysara Whitemane";
    const float wide = widgets::MeasureTracked(font, 13.0f, source, 0.0f).x;
    const std::string clipped = widgets::ClipText(font, 13.0f, source, wide * 0.5f);
    CHECK(clipped.size() >= 3 && clipped.compare(clipped.size() - 3, 3, "...") == 0);
    // Every U+00B7 lead byte must still be followed by its continuation byte.
    bool severed = false;
    for (std::size_t i = 0; i < clipped.size(); ++i) {
        if (static_cast<unsigned char>(clipped[i]) != 0xC2) continue;
        if (i + 1 >= clipped.size() || (static_cast<unsigned char>(clipped[i + 1]) & 0xC0) != 0x80) severed = true;
    }
    CHECK(!severed);
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
}

#include "ui/screens/Theme.h"

TEST(widgets_buttons_fields_stamps_and_cards_emit_items_headless)
{
    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    io.DisplaySize = ImVec2(800.0f, 600.0f);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // headless: no renderer backend to set this for us
    ImGui::NewFrame();
    theme::Fonts fonts;  // all null: every widget falls back to ImGui's font
    const theme::Canvas canvas = theme::Canvas::Fit(io.DisplaySize);
    const widgets::Ctx c{ fonts, canvas, 1.0f };
    ImGui::Begin("##t", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    CHECK(widgets::ButtonWidth(c, "Begin bond", widgets::ButtonKind::Primary) > canvas.S(32.0f));
    CHECK(widgets::ButtonWidth(c, "Begin bond", widgets::ButtonKind::Primary) >= widgets::ButtonWidth(c, "Cancel", widgets::ButtonKind::Ghost));
    ImGui::SetCursorScreenPos({ 10.0f, 10.0f });
    CHECK(!widgets::Button(c, "##p", "Begin bond", widgets::ButtonKind::Primary, true));
    ImGui::SetCursorScreenPos({ 10.0f, 60.0f });
    CHECK(!widgets::Button(c, "##d", "Remove bond", widgets::ButtonKind::Danger, false));
    char buffer[16] = "";
    ImGui::SetCursorScreenPos({ 10.0f, 110.0f });
    CHECK(!widgets::TextField(c, "##f", buffer, sizeof(buffer), { canvas.S(256.0f), canvas.S(34.0f) }, "Search deeds", false));
    CHECK(buffer[0] == 0);
    std::int8_t direction = 0;
    ImGui::SetCursorScreenPos({ 10.0f, 160.0f });
    CHECK(!widgets::StampGroup(c, "##s", direction));
    CHECK(direction == 0);
    CHECK_NEAR(widgets::StampGroupWidth(c), canvas.S(77.0f) * 3.0f + canvas.S(6.0f), 1e-3);
    ImGui::SetCursorScreenPos({ 10.0f, 210.0f });
    CHECK(!widgets::RadioCard(c, "##r", canvas.S(300.0f), "Illia", "Imperial \xC2\xB7 Mage", true, false));
    ImGui::SetCursorScreenPos({ 10.0f, 280.0f });
    CHECK(!widgets::RadioCard(c, "##l", canvas.S(300.0f), "Lydia", "Nord", true, true));
    ImGui::SetCursorScreenPos({ 10.0f, 350.0f });
    CHECK(!widgets::BackLink(c, "##b", "Back to company"));
    CHECK(widgets::LockPillWidth(c, "Unlocked") > canvas.S(20.0f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    widgets::LockPill(c, dl, { 10.0f, 400.0f }, "Unlocked");
    const float noteH = widgets::DashedNote(c, dl, { 10.0f, 430.0f }, canvas.S(300.0f), "No one to enter", "Only followers travelling with you right now can be entered.");
    CHECK(noteH > canvas.S(26.0f));
    CHECK(widgets::WrappedHeight(nullptr, 13.0f, "one line", 400.0f) < widgets::WrappedHeight(nullptr, 13.0f, "a much longer sentence that has to wrap onto several lines", 60.0f));
    ImGui::End();
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
}
