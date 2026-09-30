// Romantasy desktop preview: renders the exact screen code the DLL uses,
// against a JSON fixture, in a plain Win32 + DX11 window. Supports headless
// PNG capture for visual sign-off (--shot).

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "ui/screens/Anim.h"
#include "ui/screens/ControllerHints.h"
#include "ui/screens/Enrollment.h"
#include "ui/screens/Ledger.h"
#include "ui/screens/LevelUpPopup.h"
#include "ui/screens/Theme.h"
#include "ui/screens/Tiers.h"
#include "ui/screens/UiState.h"
#include "ui/screens/Widgets.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    using namespace romantasy::ui;

    struct Options
    {
        std::string fixture = "tools/preview/fixture.json";
        std::string shot;
        int frames = 90;
        std::string popup;   // "", "gain", "loss"
        int select = 0;
        bool ledger = true;
        bool gallery = false;
        int width = 1600;
        int height = 900;
        std::string pane;
        std::string screen;  // "", "enroll", "edit"
        std::string modal;   // "", "reset", "remove", "password"
        bool dev = false;
        bool controller = false;  // force legends for screenshot capture
    };

    struct PreviewApp
    {
        Options options;
        UiState state;
        theme::Fonts fonts;
        theme::Canvas canvas;
        float time = 0.0f;
        LedgerViewState ledger;
        bool ledgerOpen = true;
        std::optional<LevelChange> popup;
        PopupViewState popupView;
        float knobs[2] = { 0.0f, 1.0f };
        bool toggles[2] = { false, true };
        std::uint32_t revision = 0;
        bool devUnlocked = false;
        ControllerInput controllerInput;
    };

    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    IDXGISwapChain* g_swapChain = nullptr;
    ID3D11RenderTargetView* g_renderTarget = nullptr;

    Options ParseArgs(int argc, char** argv)
    {
        Options options;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const bool hasValue = i + 1 < argc;
            if (arg == "--fixture" && hasValue) options.fixture = argv[++i];
            else if (arg == "--shot" && hasValue) options.shot = argv[++i];
            else if (arg == "--frames" && hasValue) options.frames = std::atoi(argv[++i]);
            else if (arg == "--popup" && hasValue) options.popup = argv[++i];
            else if (arg == "--select" && hasValue) options.select = std::atoi(argv[++i]);
            else if (arg == "--ledger" && hasValue) options.ledger = (std::string(argv[++i]) != "off");
            else if (arg == "--gallery") options.gallery = true;
            else if (arg == "--pane" && hasValue) options.pane = argv[++i];
            else if (arg == "--screen" && hasValue) options.screen = argv[++i];
            else if (arg == "--modal" && hasValue) options.modal = argv[++i];
            else if (arg == "--dev") options.dev = true;
            else if (arg == "--controller") options.controller = true;
            else if (arg == "--size" && hasValue) {
                char* end = nullptr;
                const long w = std::strtol(argv[++i], &end, 10);
                const long h = (end && *end == 'x') ? std::strtol(end + 1, nullptr, 10) : 0;
                if (w > 0 && h > 0) { options.width = static_cast<int>(w); options.height = static_cast<int>(h); }
            }
        }
        return options;
    }

    bool LoadFixture(const std::string& path, UiState& out)
    {
        std::ifstream file(path);
        if (!file) {
            std::printf("preview: cannot open fixture %s\n", path.c_str());
            return false;
        }
        try {
            const auto json = nlohmann::json::parse(file);
            out = UiStateFromJson(json);
            return true;
        } catch (const std::exception& e) {
            std::printf("preview: fixture parse failed: %s\n", e.what());
            return false;
        }
    }

    // The DLL's facade stamps these after every push; the preview does the same.
    void Bump(PreviewApp& app, const char* message)
    {
        app.state.message = message;
        app.state.revision = ++app.revision;
        app.state.developerToolsUnlocked = app.devUnlocked;
    }

    void ReloadFixture(PreviewApp& app, const char* message)
    {
        LoadFixture(app.options.fixture, app.state);
        Bump(app, message);
    }

    const FollowerRow* FirstPlayerRow(const UiState& state)
    {
        for (const auto& row : state.followers) {
            if (row.profileOrigin == "player" && row.personalityEditable) return &row;
        }
        return nullptr;
    }

    std::vector<std::string> LabelsFor(const UiState& state, const BondRequest& request, std::int32_t direction)
    {
        std::vector<std::string> out;
        for (const auto& option : state.preferenceOptions) {
            for (const auto& [editorID, dir] : request.preferences) {
                if (dir == direction && editorID == option.editorID) out.push_back(option.label);
            }
        }
        return out;
    }

    void ApplyBondMimic(PreviewApp& app, const BondRequest& request)
    {
        using Kind = BondRequest::Kind;
        auto& followers = app.state.followers;
        auto rowIt = std::find_if(followers.begin(), followers.end(), [&](const FollowerRow& r) { return RowIdentity(r) == request.identity; });
        switch (request.kind) {
        case Kind::Enroll: {
            const EnrollmentCandidate* candidate = FindCandidate(app.state, request.identity);
            if (!candidate) { Bump(app, "That companion could not be added."); return; }
            FollowerRow row;
            row.name = candidate->name;
            row.role = candidate->role;
            row.sourcePlugin = candidate->sourcePlugin;
            row.referenceFormID = candidate->referenceFormID;
            row.baseFormID = candidate->baseFormID;
            row.profileOrigin = "player";
            row.personalityEditable = true;
            row.removable = true;
            row.isFollowing = true;
            row.levelName = "Stranger";
            row.nextGoal = "Acquaintance at 500";
            row.likes = LabelsFor(app.state, request, 1);
            row.dislikes = LabelsFor(app.state, request, -1);
            followers.push_back(row);
            std::erase_if(app.state.candidates, [&](const EnrollmentCandidate& c) { return c.referenceFormID == request.identity; });
            Bump(app, "A new bond was entered in the ledger.");
            return;
        }
        case Kind::Replace:
            if (rowIt == followers.end()) { Bump(app, "That personality is protected."); return; }
            rowIt->likes = LabelsFor(app.state, request, 1);
            rowIt->dislikes = LabelsFor(app.state, request, -1);
            Bump(app, "Companion personality sealed.");
            return;
        case Kind::Reset:
            if (rowIt == followers.end()) { Bump(app, "That bond cannot be reset here."); return; }
            rowIt->points = 0;
            rowIt->recent.clear();
            rowIt->levelName = "Stranger";
            Bump(app, "Player-created bond reset.");
            return;
        case Kind::Remove:
            if (rowIt == followers.end()) { Bump(app, "That bond is protected."); return; }
            followers.erase(rowIt);
            Bump(app, "Player-created bond removed.");
            return;
        }
    }

    void ApplyDebugMimic(PreviewApp& app, const DebugRequest& request)
    {
        for (auto& row : app.state.followers) {
            if (request.points) {
                row.points = std::max(0, row.points + *request.points);
                row.levelName = tiers::Names[tiers::TierIndex(row.points)];
            } else {
                row.recent.insert(row.recent.begin(), RecentEvent{ request.stat, request.delta, "Today" });
            }
        }
        Bump(app, request.points ? "Bond points adjusted." : "Stat applied.");
    }

    void CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer) {
            g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
            backBuffer->Release();
        }
    }

    void ReleaseRenderTarget()
    {
        if (g_renderTarget) {
            g_renderTarget->Release();
            g_renderTarget = nullptr;
        }
    }

    bool CreateDevice(HWND window)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Width = 0;
        desc.BufferDesc.Height = 0;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferDesc.RefreshRate.Numerator = 60;
        desc.BufferDesc.RefreshRate.Denominator = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL chosen{};
        const HRESULT result = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
            &desc, &g_swapChain, &g_device, &chosen, &g_context);
        if (FAILED(result)) {
            return false;
        }
        CreateRenderTarget();
        return true;
    }

    void DestroyDevice()
    {
        ReleaseRenderTarget();
        if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
        if (g_context) { g_context->Release(); g_context = nullptr; }
        if (g_device) { g_device->Release(); g_device = nullptr; }
    }

    bool SaveBackBufferPng(const std::string& path)
    {
        ID3D11Texture2D* backBuffer = nullptr;
        if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) || !backBuffer) {
            return false;
        }
        D3D11_TEXTURE2D_DESC desc{};
        backBuffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ID3D11Texture2D* staging = nullptr;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &staging)) || !staging) {
            backBuffer->Release();
            return false;
        }
        g_context->CopyResource(staging, backBuffer);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            staging->Release();
            backBuffer->Release();
            return false;
        }
        std::vector<unsigned char> pixels(static_cast<size_t>(desc.Width) * desc.Height * 4);
        for (UINT y = 0; y < desc.Height; ++y) {
            const auto* row = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
            std::memcpy(pixels.data() + static_cast<size_t>(y) * desc.Width * 4, row, static_cast<size_t>(desc.Width) * 4);
        }
        for (size_t i = 3; i < pixels.size(); i += 4) {
            pixels[i] = 255;
        }
        g_context->Unmap(staging, 0);
        staging->Release();
        backBuffer->Release();

        const int ok = stbi_write_png(path.c_str(), static_cast<int>(desc.Width), static_cast<int>(desc.Height), 4, pixels.data(), static_cast<int>(desc.Width) * 4);
        std::printf("preview: %s %s\n", ok ? "wrote" : "FAILED to write", path.c_str());
        return ok != 0;
    }

    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            return 1;
        }
        switch (msg) {
        case WM_SIZE:
            if (g_device && wParam != SIZE_MINIMIZED) {
                ReleaseRenderTarget();
                g_swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }
    }

    void FirePopup(PreviewApp& app, bool loss)
    {
        LevelChange change;
        change.followerName = app.state.followers.empty() ? "Ambellina" : app.state.followers.front().name;
        change.isLoss = loss;
        change.previousLevelName = loss ? "Friend" : "Acquaintance";
        change.levelName = loss ? "Acquaintance" : "Friend";
        change.nextLevelName = loss ? "Friend" : "Confidant";
        change.points = loss ? 990 : 1000;
        change.pointsDelta = loss ? -500 : 500;
        app.popup = change;
        app.popupView.Reset();
    }

    void DrawGallery(PreviewApp& app, float dt)
    {
        const theme::Canvas& cv = app.canvas;
        widgets::Ctx c{ app.fonts, cv, 1.0f };
        ImGui::SetNextWindowPos({ 0.0f, 0.0f });
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::Begin("##gallery", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        widgets::GlowBackdrop(c, dl, { 0.0f, 0.0f }, ImGui::GetIO().DisplaySize);

        widgets::Wordmark(c, dl, cv.P(40.0f, 30.0f), 34.0f);
        widgets::Eyebrow(c, dl, cv.P(40.0f, 82.0f), "Those who walk beside you", theme::Fg3);
        widgets::ScriptTitle(c, dl, cv.P(40.0f, 96.0f), "Your Company", 34.0f, theme::Fg1);
        widgets::Pill(c, dl, cv.P(40.0f, 160.0f), "Following", theme::Emerald, true);
        widgets::Pill(c, dl, cv.P(160.0f, 160.0f), "Away", theme::Fg4, true);
        widgets::TierMeter(c, dl, cv.P(40.0f, 200.0f), cv.S(400.0f), cv.S(4.0f), 1240, true);
        widgets::TierMeter(c, dl, cv.P(40.0f, 220.0f), cv.S(400.0f), cv.S(6.0f), 2630, false);
        widgets::MedallionHeart(c, dl, cv.P(60.0f, 270.0f), cv.S(36.0f));
        widgets::SealBadge(c, dl, cv.P(120.0f, 270.0f), cv.S(40.0f), "AU", theme::Copper);
        widgets::SealBadge(c, dl, cv.P(180.0f, 270.0f), cv.S(40.0f), "YOU", theme::Amber);
        widgets::HeroHeart(c, dl, cv.P(700.0f, 200.0f), cv.S(160.0f), app.time);
        widgets::CardFrame(c, dl, cv.P(40.0f, 320.0f), cv.P(500.0f, 460.0f));
        const widgets::Icon icons[] = { widgets::Icon::Close, widgets::Icon::Gear, widgets::Icon::Refresh, widgets::Icon::Plus, widgets::Icon::Sparkle, widgets::Icon::HeartUp, widgets::Icon::HeartDown, widgets::Icon::Eye, widgets::Icon::Heart, widgets::Icon::Claw, widgets::Icon::Lock, widgets::Icon::Dot };
        for (int i = 0; i < 12; ++i) {
            widgets::DrawIcon(dl, icons[i], cv.P(70.0f + 40.0f * static_cast<float>(i), 350.0f), cv.S(18.0f), theme::Fg1, cv.S(1.6f));
        }

        ImGui::SetCursorScreenPos(cv.P(60.0f, 380.0f));
        widgets::IconButton(c, "##gear", widgets::Icon::Gear, 56.0f, true, true);
        ImGui::SameLine(0.0f, cv.S(12.0f));
        widgets::IconButton(c, "##refresh", widgets::Icon::Refresh, 56.0f, false, true);
        ImGui::SameLine(0.0f, cv.S(12.0f));
        widgets::IconButton(c, "##disabled", widgets::Icon::Close, 56.0f, false, false);
        ImGui::SetCursorScreenPos(cv.P(40.0f, 480.0f));
        widgets::DashedButton(c, "##newbond", { cv.S(320.0f), cv.S(64.0f) }, "New bond", "Write an unwritten page", false);
        ImGui::SetCursorScreenPos(cv.P(560.0f, 480.0f));
        if (widgets::ToggleRow(c, "##t0", cv.S(600.0f), widgets::Icon::Sparkle, "Open with Favorites Menu", "Off: open with Left Ctrl + R. On: cast the Romantasy power from your Favorites menu; the hotkey is disabled.", app.toggles[0], app.knobs[0], dt)) app.toggles[0] = !app.toggles[0];
        ImGui::SetCursorScreenPos(cv.P(560.0f, 556.0f));
        if (widgets::ToggleRow(c, "##t1", cv.S(600.0f), widgets::Icon::Eye, "Show away companions", "Keep bonds in the ledger even when not following.", app.toggles[1], app.knobs[1], dt)) app.toggles[1] = !app.toggles[1];

        ImGui::SetCursorScreenPos(cv.P(40.0f, 660.0f));
        widgets::Button(c, "##b1", "Begin bond", widgets::ButtonKind::Primary, true);
        ImGui::SetCursorScreenPos(cv.P(180.0f, 660.0f));
        widgets::Button(c, "##b2", "Edit personality", widgets::ButtonKind::Secondary, true);
        ImGui::SetCursorScreenPos(cv.P(360.0f, 660.0f));
        widgets::Button(c, "##b3", "Cancel", widgets::ButtonKind::Ghost, true);
        ImGui::SetCursorScreenPos(cv.P(470.0f, 660.0f));
        widgets::Button(c, "##b4", "Remove bond", widgets::ButtonKind::Danger, true);
        ImGui::SetCursorScreenPos(cv.P(620.0f, 660.0f));
        widgets::Button(c, "##b5", "Begin bond", widgets::ButtonKind::Primary, false);
        static char search[64] = "";
        ImGui::SetCursorScreenPos(cv.P(40.0f, 720.0f));
        widgets::TextField(c, "##search", search, sizeof(search), { cv.S(256.0f), cv.S(34.0f) }, "Search deeds", false, 11.8f);
        static std::int8_t stamp = 1;
        ImGui::SetCursorScreenPos(cv.P(320.0f, 720.0f));
        widgets::StampGroup(c, "##stamp", stamp);
        static std::int8_t stamp2 = -1;
        ImGui::SetCursorScreenPos(cv.P(580.0f, 720.0f));
        widgets::StampGroup(c, "##stamp2", stamp2);
        ImGui::SetCursorScreenPos(cv.P(40.0f, 780.0f));
        widgets::RadioCard(c, "##card", cv.S(320.0f), "Illia", "Imperial \xC2\xB7 Mage \xC2\xB7 Skyrim.esm", true, false);
        ImGui::SetCursorScreenPos(cv.P(380.0f, 780.0f));
        widgets::RadioCard(c, "##card2", cv.S(320.0f), "Eola", "Breton \xC2\xB7 Nightblade \xC2\xB7 Skyrim.esm", false, false);
        ImGui::SetCursorScreenPos(cv.P(720.0f, 780.0f));
        widgets::BackLink(c, "##back", "Back to company");
        widgets::LockPill(c, dl, cv.P(920.0f, 786.0f), "Unlocked");
        widgets::DashedNote(c, dl, cv.P(40.0f, 850.0f), cv.S(320.0f), "No one to enter", "Only followers travelling with you right now, who have no Romantasy page yet, can be entered.");
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // ---- Frame ---------------------------------------------------------------
    void DrawFrame(PreviewApp& app, float dt)
    {
        app.time += dt;
        if (app.options.gallery) {
            DrawGallery(app, dt);
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            LoadFixture(app.options.fixture, app.state);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
            FirePopup(app, false);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
            FirePopup(app, true);
        }

        const bool popupActive = app.popup.has_value();
        const bool controllerActive = app.controllerInput.UsingGamepad();
        if (app.ledgerOpen) {
            const LedgerActions result = DrawLedger(app.state, app.ledger, app.fonts, app.canvas, dt, !popupActive, controllerActive);
            if (result.closeRequested) {
                app.ledgerOpen = false;
            }
            if (result.refreshRequested) ReloadFixture(app, "Ledger refreshed.");
            if (result.settings) {  // the DLL's facade does this; the preview mimics it
                const auto& s = *result.settings;
                if (s.openWithFavorites) app.state.openWithFavorites = *s.openWithFavorites;
                if (s.showGainModals) app.state.showGainModals = *s.showGainModals;
                if (s.showLossModals) app.state.showLossModals = *s.showLossModals;
                if (s.showAwayFollowers) app.state.showAwayFollowers = *s.showAwayFollowers;
                Bump(app, "Preferences noted.");
            }
            if (result.developerTools) {
                app.devUnlocked = *result.developerTools;
                Bump(app, app.devUnlocked ? "Developer tools unlocked." : "Developer tools locked.");
            }
            if (result.bond) ApplyBondMimic(app, *result.bond);
            if (result.debug) ApplyDebugMimic(app, *result.debug);
        } else {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            theme::DrawTextAligned(dl, app.fonts.body, app.canvas.S(14.0f), app.canvas.P(800.0f, 440.0f), theme::Fg3, "Ledger closed. Ctrl+R reopen · F1 gain · F2 loss", 0.5f);
            if (!popupActive && ImGui::IsKeyPressed(ImGuiKey_R, false) && ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                app.ledgerOpen = true;
                app.ledger.OnOpen();
            }
        }

        if (popupActive) {
            const PopupResult result = DrawLevelUpPopup(*app.popup, app.popupView, app.fonts, app.canvas, dt, true, app.ledgerOpen, controllerActive);
            if (result.finished) {
                app.popup.reset();
            }
        }
    }
}

int main(int argc, char** argv)
{
    PreviewApp app;
    app.options = ParseArgs(argc, argv);
    if (!LoadFixture(app.options.fixture, app.state)) {
        return 2;
    }

    WNDCLASSEXW wc{ sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr, L"RomantasyPreview", nullptr };
    RegisterClassExW(&wc);
    RECT rect{ 0, 0, app.options.width, app.options.height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowW(wc.lpszClassName, L"Romantasy Preview", WS_OVERLAPPEDWINDOW, 100, 100,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDevice(window)) {
        std::printf("preview: D3D11 device creation failed\n");
        return 3;
    }
    ShowWindow(window, app.options.shot.empty() ? SW_SHOWDEFAULT : SW_HIDE);
    UpdateWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    theme::LoadFonts(io, "assets/fonts", app.fonts);
    app.ledger.selected = app.options.select;
    app.ledger.OnOpen();
    if (app.options.pane == "settings") {
        app.ledger.pane = LedgerPane::Settings;
        app.ledger.lastPane = LedgerPane::Settings;
    }
    app.devUnlocked = app.options.dev;
    Bump(app, "Romance records synchronized.");
    if (app.options.screen == "enroll") {
        OpenEnrollment(app.state, app.ledger, EnrollMode::Create, {});
    } else if (app.options.screen == "edit") {
        if (const FollowerRow* row = FirstPlayerRow(app.state)) OpenEnrollment(app.state, app.ledger, EnrollMode::Edit, RowIdentity(*row));
    }
    if (app.options.modal == "password") {
        app.ledger.OpenModal(LedgerModal::Password, {});
    } else if (app.options.modal == "reset" || app.options.modal == "remove") {
        const FollowerRow* row = FirstPlayerRow(app.state);
        app.ledger.OpenModal(app.options.modal == "reset" ? LedgerModal::ConfirmReset : LedgerModal::ConfirmRemove, row ? RowIdentity(*row) : std::string{});
    }
    app.ledgerOpen = app.options.ledger;
    if (app.options.popup == "gain") FirePopup(app, false);
    if (app.options.popup == "loss") FirePopup(app, true);

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(g_device, g_context);

    const float clear[4] = { 0x1a / 255.0f, 0x13 / 255.0f, 0x0c / 255.0f, 1.0f };  // page-floor
    int frame = 0;
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running || !g_renderTarget) {
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        // --controller simulates the engine's device signal, even when the
        // desktop backend has no XInput controller. Use the native visibility path.
        if (app.options.controller) app.controllerInput.SetGamepad(true);
        else app.controllerInput.UpdateFromBackend();

        app.canvas = theme::Canvas::Fit(io.DisplaySize);
        DrawFrame(app, io.DeltaTime);

        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_context->ClearRenderTargetView(g_renderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        ++frame;
        if (!app.options.shot.empty() && frame >= app.options.frames) {
            SaveBackBufferPng(app.options.shot);
            running = false;
        }
        g_swapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyDevice();
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
