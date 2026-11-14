#pragma once

#include "ui/Whispers.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace MAGELIGHT_API
{
    struct EventData;
    struct MagelightApi4;
}

namespace romantasy::ui
{
    // The whisper card's page (Data/Magelight/Romantasy/whisper.html) in a second Magelight view: click-through, on the
    // HUD layer, hidden until a whisper shows. It never asks for UI mode, so it never pauses the game or takes input.
    // It shares only the API pointer and the registered mod with the ledger's MagelightHost. Game thread only, except
    // OnListener, which Magelight calls on its render or page thread and which only copies its argument and posts it.
    class WhisperView
    {
    public:
        static constexpr const char* kWhisper = "romantasyWhisper";          // plugin -> page, one whisper
        static constexpr const char* kWhisperDone = "romantasyWhisperDone";  // page -> plugin, { seq }

        // Called on the game thread with the page's answer text.
        using DoneFn = void (*)(const std::string& payload);

        static WhisperView& GetSingleton();
        // kDataLoaded, after MagelightHost. With no API, or when the view cannot be made, it logs once and stays
        // unavailable; whispers then go to the game's notifications.
        void Initialize(const MAGELIGHT_API::MagelightApi4* api, std::uint32_t mod, DoneFn onDone);
        // The view was made and has not died since. Any thread.
        [[nodiscard]] bool Available() const;
        // The corner for the next Show (and the one on screen, if any).
        void Place(whispers::WhisperCorner corner);
        // Places the card from the display size, shows the view and sends the whisper with this seq. False, with
        // nothing shown, when the view is unavailable or the display size is not known yet.
        // `comfort`: extra payload fields from the comfort settings (whispers::ComfortFields).
        bool Show(const whispers::Whisper& whisper, std::uint32_t seq, const std::vector<whispers::ComfortField>& comfort);
        void Hide();
        // The page stopped answering: the card is hidden and unavailable for the rest of the session.
        void GiveUp();
        // The host's events for this view (ModDesc callbacks run on the game thread).
        void OnEvent(const MAGELIGHT_API::EventData* event);

    private:
        WhisperView() = default;

        bool ApplyPlacement();
        static void OnListener(std::uint64_t view, const char* argument, void* user);
        static void RunDone(void* user);

        const MAGELIGHT_API::MagelightApi4* _api = nullptr;
        std::uint32_t _mod = 0;   // MAGELIGHT_API::ModId
        std::uint64_t _view = 0;  // MAGELIGHT_API::ViewId
        DoneFn _onDone = nullptr;
        // Read by Available(), which the health check may call from any thread.
        std::atomic<bool> _created{ false };  // the view was made
        std::atomic<bool> _dead{ false };     // the view failed to load, was destroyed, or the overlay is off
        bool _visible = false;
        whispers::WhisperCorner _corner = whispers::WhisperCorner::TopRight;
        whispers::Placement _placed{};  // what the view was last given; scale 1 at creation
    };
}
