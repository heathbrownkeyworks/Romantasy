#include "ui/magelight/WhisperView.h"

#include "ui/magelight/MagelightUI_API.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>

namespace romantasy::ui
{
    namespace
    {
        using MAGELIGHT_API::Result;

        // Beside the ledger's page in Magelight's mod tree, resolved from the game's executable as MagelightHost does.
        std::string PagePath()
        {
            wchar_t exe[MAX_PATH]{};
            if (!::GetModuleFileNameW(nullptr, exe, MAX_PATH)) return {};
            const auto page = std::filesystem::path(exe).parent_path() / L"Data" / L"Magelight" / L"Romantasy" / L"whisper.html";
            std::error_code error;
            if (!std::filesystem::exists(page, error)) return {};
            const auto utf8 = page.u8string();
            return std::string(utf8.begin(), utf8.end());
        }

        const char* ToneKey(whispers::Tone tone)
        {
            switch (tone) {
            case whispers::Tone::Gain: return "gain";
            case whispers::Tone::Loss: return "loss";
            default:                   return "plain";
            }
        }
    }

    WhisperView& WhisperView::GetSingleton()
    {
        static WhisperView view;
        return view;
    }

    void WhisperView::Initialize(const MAGELIGHT_API::MagelightApi4* api, std::uint32_t mod, DoneFn onDone)
    {
        if (_api) return;
        _onDone = onDone;
        if (!api || mod == 0) {
            logger::info("WhisperView: no usable Magelight host; whispers use the game's notifications");
            return;
        }
        const auto page = PagePath();
        if (page.empty()) {
            logger::error("WhisperView: Data/Magelight/Romantasy/whisper.html is missing; whispers use the game's notifications");
            return;
        }

        MAGELIGHT_API::ViewDesc desc{};
        desc.size = sizeof(desc);
        desc.name = "whisper";
        desc.htmlPath = page.c_str();
        desc.anchor = MAGELIGHT_API::Anchor::TopLeft;
        desc.x = 0;
        desc.y = 0;
        desc.w = whispers::kCardWidth;
        desc.h = whispers::kCardHeight;
        desc.fullscreen = false;
        desc.clickThrough = true;
        desc.startVisible = false;
        desc.layer = MAGELIGHT_API::Layer::Hud;
        desc.uiScale = 1.0f;
        MAGELIGHT_API::ViewId view = 0;
        if (const auto created = api->CreateViewEx(mod, &desc, &view); created != Result::Ok) {
            logger::error("WhisperView: Magelight could not create the whisper view ({}): {}; whispers use the game's notifications",
                static_cast<std::int32_t>(created), api->GetLastErrorMessage(mod));
            return;
        }
        _api = api;
        _mod = mod;
        _view = view;
        _placed = { 0, 0, whispers::kCardWidth, whispers::kCardHeight, 1.0f };
        _created = true;

        if (api->RegisterJSListenerEx(view, kWhisperDone, &WhisperView::OnListener, nullptr) != Result::Ok)
            logger::error("WhisperView: Magelight refused listener {}: {}", kWhisperDone, api->GetLastErrorMessage(mod));
        // The page loads when it is first shown, at the scale the first Show gives it, not at 1 and then again.
        if (api->SetViewLoadOnShow(view, true) != Result::Ok)
            logger::debug("WhisperView: Magelight did not take load-on-show: {}", api->GetLastErrorMessage(mod));
        logger::info("WhisperView: whisper view ready ({})", page);
    }

    bool WhisperView::Available() const
    {
        return _created.load() && !_dead.load();
    }

    void WhisperView::Place(whispers::WhisperCorner corner)
    {
        _corner = corner;
        if (_visible) ApplyPlacement();
    }

    bool WhisperView::ApplyPlacement()
    {
        std::int32_t width = 0, height = 0;
        _api->GetDisplaySize(&width, &height);
        const auto place = whispers::PlaceCard(width, height, _corner);
        if (!place) {
            logger::debug("WhisperView: the display size is not known yet");
            return false;
        }
        if (place->scale != _placed.scale) {
            // Lays the page out again at the display's scale: sharp text, the same 420x96 CSS card. A page not loaded
            // yet takes it at its first load.
            if (const auto result = _api->RebuildViewAtScale(_view, place->scale); result != Result::Ok)
                logger::warn("WhisperView: Magelight refused scale {:.2f} ({}): {}", place->scale, static_cast<std::int32_t>(result),
                    _api->GetLastErrorMessage(_mod));
        }
        if (place->x != _placed.x || place->y != _placed.y || place->w != _placed.w || place->h != _placed.h)
            _api->SetViewBounds(_view, place->x, place->y, place->w, place->h);
        _placed = *place;
        return true;
    }

    bool WhisperView::Show(const whispers::Whisper& whisper, std::uint32_t seq, const std::vector<whispers::ComfortField>& comfort)
    {
        if (!Available() || !ApplyPlacement()) return false;
        const auto shown = whispers::Describe(whisper);
        nlohmann::json payload{
            { "name", shown.name },
            { "line", shown.line },
            { "tone", ToneKey(shown.tone) },
            { "icon", std::string(shown.icon) },
            { "seq", seq },
        };
        for (const auto& field : comfort) payload[std::string(field.key)] = std::string(field.value);
        const auto text = payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        // Sent before the view is shown. The first time, the page has not loaded yet (load on show): the host keeps the
        // call for it, "DOM ready, and every call queued for the page, follow that load" (MagelightUI_API.h, the
        // SetViewLoadOnShow comment, lines 776-778). After a rescale the same holds: "calls you make meanwhile wait for
        // its DOM ready" (RebuildViewAtScale, line 812).
        _api->InteropCall(_view, kWhisper, text.c_str());
        if (!_visible) {
            _api->ShowView(_view, true);
            _visible = true;
        }
        return true;
    }

    void WhisperView::GiveUp()
    {
        Hide();
        _dead = true;
    }

    void WhisperView::Hide()
    {
        if (!_visible) return;
        _visible = false;
        if (_api && _view) _api->ShowView(_view, false);
    }

    // Game thread (ModDesc.callbackThread).
    void WhisperView::OnEvent(const MAGELIGHT_API::EventData* event)
    {
        using MAGELIGHT_API::Event;
        if (!event) return;
        if (event->type == Event::RenderDead) {
            // Recorded whatever else is known: the overlay is off for the session.
            _dead = true;
            _visible = false;
            return;
        }
        if (!_api) return;
        const char* detail = event->detail && *event->detail ? event->detail : "";
        if (event->view != _view) return;
        switch (event->type) {
        case Event::ViewLoadFailed:
            logger::error("WhisperView: Magelight could not load the whisper page: {}; whispers use the game's notifications", detail);
            _dead = true;
            Hide();
            break;
        case Event::ViewDestroyed:
            logger::error("WhisperView: the whisper view was destroyed; whispers use the game's notifications");
            _dead = true;
            _visible = false;
            break;
        case Event::ConsoleMessage:
            if (event->x == 1) logger::warn("Romantasy whisper page: {}", detail);
            if (event->x == 2) logger::error("Romantasy whisper page: {} (line {})", detail, event->y);
            break;
        default:
            break;
        }
    }

    void WhisperView::OnListener(std::uint64_t, const char* argument, void*)
    {
        // Render or page thread, inside the frame: copy the argument and let the host post it to the game thread.
        const auto* api = GetSingleton()._api;
        auto payload = std::make_unique<std::string>(argument ? argument : "");
        if (api && api->PostGameTask(&WhisperView::RunDone, payload.get()) == Result::Ok)
            payload.release();
        else
            logger::warn("WhisperView: could not post {}", kWhisperDone);
    }

    void WhisperView::RunDone(void* user)
    {
        const std::unique_ptr<std::string> payload(static_cast<std::string*>(user));
        if (const auto onDone = GetSingleton()._onDone) onDone(*payload);
    }
}
