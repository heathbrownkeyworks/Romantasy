#pragma once

#include "health/Health.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace MAGELIGHT_API
{
    struct EventData;
    struct MagelightApi4;
}

namespace romantasy::ui
{
    // Magelight UI 1.6.0, packed as the host packs its version (MAJOR * 10000 + MINOR * 100 + PATCH).
    inline constexpr std::uint32_t kMinHostVersion = 10600;

    // The ledger page (Data/Magelight/Romantasy/index.html), hosted by Magelight UI as one fullscreen Panel view. The
    // view serves two modes: Ledger holds a pausing UI mode, Popup an unpaused one for a level-change card over play.
    // Everything here runs on the game thread except OnListener, which Magelight calls on its render or page thread
    // and which only copies its argument and posts it back with the host's PostGameTask.
    class MagelightHost
    {
    public:
        enum class Mode { Hidden, Ledger, Popup };
        struct Callbacks                                     // all called on the game thread
        {
            void (*onAction)(std::string_view name, const std::string& payload);
            void (*onExited)();     // UIModeExited, RenderDead, or the view died: the host is already Hidden
            void (*onRefused)();    // UIModeRefused after an Enter that returned true: the host is already Hidden
            // Every host event, before the ledger looks at it: the whisper view's own events. Optional.
            void (*onEvent)(const MAGELIGHT_API::EventData* event) = nullptr;
        };

        static MagelightHost& GetSingleton();
        void Initialize(Callbacks callbacks);                // kDataLoaded
        // Magelight 1.6.0 or newer, the page found, the view alive, the overlay not dead.
        [[nodiscard]] bool HasHost() const;
        // What Initialize found: Ok once the view is made, otherwise why there is no host. For the health report.
        [[nodiscard]] romantasy::health::HostState HostStatus() const { return _hostState; }
        // The running Magelight UI's version as "major.minor.patch"; empty when it is not loaded.
        [[nodiscard]] const std::string& HostVersion() const { return _hostVersion; }
        // A usable host, and either a mode already up or: the player's 3D loaded, the game not paused by a menu, no
        // MainMenu or LoadingMenu.
        [[nodiscard]] bool CanEnter() const;
        // Readable from any thread (RomanceManager asks through RomantasyUI::IsOpen).
        [[nodiscard]] Mode CurrentMode() const;
        // Ledger asks for UI mode with the pause; Popup without it. From Hidden it shows the view first and hides it
        // again when Magelight does not answer Ok. From the other mode it asks again on the same view, which retargets
        // the pause in place.
        bool Enter(Mode mode);
        // Tells the page "hidden", withdraws a pending entry and releases a held one, then hides the view. Nothing when
        // already Hidden.
        void Leave();
        // InteropCall of the value's JSON text; nothing while Hidden.
        void Publish(const char* topic, const nlohmann::json& value);
        // The page has said romantasyReady since its view was made: its listeners are up, so a popup sent to it is
        // heard. Cleared when the overlay or the view dies.
        // The API and the registered mod, shared with the whisper view; null and 0 when there is no usable host.
        [[nodiscard]] const MAGELIGHT_API::MagelightApi4* Api() const { return _api; }
        [[nodiscard]] std::uint32_t Mod() const { return _mod; }
        [[nodiscard]] bool PageReady() const { return _pageReady; }
        void MarkPageReady() { _pageReady = true; }

    private:
        MagelightHost() = default;

        [[nodiscard]] std::string EnterProblem() const;      // why CanEnter() is false, for the log
        [[nodiscard]] bool OtherModHoldsUIMode() const;      // GetUIModeOwner() is set and is not our mod
        void OnEvent(const MAGELIGHT_API::EventData* event);
        void ExitedByHost(const char* why);                  // our mode ended without Leave(): hide, then onExited
        void TellPageHidden();                               // romantasyMode "hidden", while still shown
        static void OnListener(std::uint64_t view, const char* argument, void* user);
        static void RunAction(void* user);
        static void OnHostEvent(const MAGELIGHT_API::EventData* event, void* user);

        const MAGELIGHT_API::MagelightApi4* _api = nullptr;
        std::uint32_t _mod = 0;   // MAGELIGHT_API::ModId
        std::uint64_t _view = 0;  // MAGELIGHT_API::ViewId
        Callbacks _callbacks{};
        romantasy::health::HostState _hostState = romantasy::health::HostState::Ok;
        std::string _hostVersion;
        std::atomic<Mode> _mode{ Mode::Hidden };
        // A UIModeEntered confirmed that we hold UI mode for the current mode. Until then an UIModeExited can only
        // belong to an earlier session (one source's events keep their order), so it is ignored.
        bool _entered = false;
        bool _renderDead = false, _pageFailed = false;
        bool _pageReady = false;   // game thread
        // A refusal other than Busy was logged at warn; repeats go to debug until the next UIModeEntered.
        bool _refusalLogged = false;
    };
}
