#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "romance/Mood.h"

namespace romantasy::ui::whispers
{
    // What a whisper says and which one shows when. Pure: standard headers only,
    // and the clock is always passed in by the caller (milliseconds, any origin).
    enum class Kind : std::uint8_t { Deed, MoodWarm, MoodStrained, Reunion, TimeApart, TierGained, TierLost, MomentNotYet, MomentNotInMood };
    enum class Tone : std::uint8_t { Plain, Gain, Loss };

    // Where on the screen the whisper card sits. Stored in settings.json and sent to the page as its key string.
    enum class WhisperCorner : std::uint8_t { TopLeft, TopRight, BottomLeft, BottomRight };

    // Exact keys only: a wrong case or an unknown string is nullopt.
    constexpr std::optional<WhisperCorner> ParseWhisperCorner(std::string_view key) noexcept
    {
        if (key == "topLeft") return WhisperCorner::TopLeft;
        if (key == "topRight") return WhisperCorner::TopRight;
        if (key == "bottomLeft") return WhisperCorner::BottomLeft;
        if (key == "bottomRight") return WhisperCorner::BottomRight;
        return std::nullopt;
    }

    constexpr std::string_view Key(WhisperCorner corner) noexcept
    {
        switch (corner) {
        case WhisperCorner::TopLeft: return "topLeft";
        case WhisperCorner::BottomLeft: return "bottomLeft";
        case WhisperCorner::BottomRight: return "bottomRight";
        default: return "topRight";
        }
    }

    struct Whisper
    {
        Kind kind = Kind::Deed;
        std::uint32_t follower = 0;  // reference form ID, used only to tell followers apart
        std::string name;            // follower's name
        std::string label;           // Deed: the deed's label. TierGained/TierLost: the tier name.
        std::int32_t points = 0;     // Deed only, signed
        std::int32_t deeds = 1;      // Deed only: how many were merged
        bool strong = false;         // Deed only: a favorite or a pet peeve
        bool operator==(const Whisper&) const = default;
    };

    struct Shown
    {
        std::string name;
        std::string line;
        Tone tone = Tone::Plain;
        std::string_view icon;  // "star", "heart", "heartbroken", "dislikes", "neutral"
    };

    // Longest name and label kept, in bytes, cut on a character boundary. Text from
    // mods is also made valid UTF-8 (bad bytes become U+FFFD).
    inline constexpr std::size_t kMaxNameBytes = 64, kMaxLabelBytes = 80;

    [[nodiscard]] Shown Describe(const Whisper& whisper);
    [[nodiscard]] std::string NotificationText(const Whisper& whisper);  // "{Name}: {line}"
    // The text handed to the game's own HUD notification, which draws it as Scaleform HTML: NotificationText with
    // '&' and '<' escaped, and a loss written with the ASCII hyphen-minus (the game's font may lack U+2212).
    [[nodiscard]] std::string NotificationMarkup(const Whisper& whisper);

    inline constexpr std::uint64_t kMergeMs = 2000, kMaxAgeMs = 30000, kAnswerMs = 8000;
    inline constexpr std::size_t kMaxWaiting = 4;

    // Where the card goes: display pixels from the top-left, and the view's device scale. The card is laid out at
    // 420x96 CSS px with margins of 32 from the side, 32 from the bottom and 96 from the top, all at a display height
    // of 1080, scaled by max(1, height / 1080) up to 3. nullopt while the display size is not known (0 before the
    // first frame). A display smaller than the card keeps it on screen from the top-left.
    struct Placement
    {
        std::int32_t x = 0, y = 0, w = 0, h = 0;
        float scale = 1.0f;
        bool operator==(const Placement&) const = default;
    };
    inline constexpr std::int32_t kCardWidth = 420, kCardHeight = 96, kSideMargin = 32, kBottomMargin = 32, kTopMargin = 96;
    [[nodiscard]] std::optional<Placement> PlaceCard(std::int32_t displayWidth, std::int32_t displayHeight, WhisperCorner corner);

    // One open menu, as the game describes it. The HUD family (HUD, Fader, Cursor) never blocks a whisper; dialogue,
    // the main menu and a loading screen always do; any other menu does when it pauses the game or takes input.
    struct MenuFacts
    {
        std::string_view name;
        bool pausesGame = false, usesCursor = false, usesMenuContext = false, modal = false;
    };
    [[nodiscard]] bool MenuBlocks(const MenuFacts& menu);

    // When a whisper may show: the game running, no blocking menu, no dialogue, Romantasy's own view hidden,
    // and no OStim scene holding announcements.
    struct ShowFacts
    {
        bool paused = false, menuOpen = false, inDialogue = false, ownViewVisible = false, worldReady = true;
        bool inScene = false;
    };
    [[nodiscard]] constexpr bool CanShow(const ShowFacts& f) noexcept
    {
        return f.worldReady && !f.paused && !f.menuOpen && !f.inDialogue && !f.ownViewVisible && !f.inScene;
    }

    // The page answers romantasyWhisperDone with the seq it was sent. Only the answer for the whisper on screen ends
    // it: a late answer for one that already timed out must not end the next. 0 means nothing is on screen.
    [[nodiscard]] constexpr bool IsCurrentAnswer(std::uint32_t answered, std::uint32_t current) noexcept
    {
        return current != 0 && answered == current;
    }
    // The page takes a seq of 1..2147483647; after the last it starts again at 1.
    [[nodiscard]] constexpr std::uint32_t NextSeq(std::uint32_t seq) noexcept
    {
        return seq >= 2147483647u ? 1u : seq + 1u;
    }

    // A mood that moved into Warm or Strained says so; one that went back to Settled, or did not move, does not.
    [[nodiscard]] constexpr std::optional<Kind> MoodCrossing(romantasy::mood::State before, romantasy::mood::State after) noexcept
    {
        if (before == after) return std::nullopt;
        if (after == romantasy::mood::State::Warm) return Kind::MoodWarm;
        if (after == romantasy::mood::State::Strained) return Kind::MoodStrained;
        return std::nullopt;
    }

    class Queue
    {
    public:
        void SetEnabled(bool enabled);  // either way starts clean; while false, Push does nothing
        void Push(Whisper whisper, std::uint64_t nowMs);
        // The one to show now, if any. It is then "showing" until Finished or kAnswerMs.
        [[nodiscard]] std::optional<Whisper> Next(std::uint64_t nowMs, bool canShow);
        void Finished();                                        // the page answered
        void Clear();                                           // drops waiting ones and forgets the shown one
        [[nodiscard]] bool Showing(std::uint64_t nowMs) const;  // false once kAnswerMs has passed
        [[nodiscard]] std::size_t Waiting() const;

    private:
        struct Entry
        {
            Whisper whisper;
            std::uint64_t firstMs = 0;  // when it began to wait (its age)
            std::uint64_t lastMs = 0;   // last push merged into it (the merge window)
        };

        bool _enabled = true;
        bool _showing = false;
        std::uint64_t _shownMs = 0;
        std::deque<Entry> _waiting;
    };

    // The queue and the whisper on the card, with every decision the plugin makes about them: what to show next,
    // which answer from the page counts, when an unanswered card is taken down, when the game's notification stands
    // in. The caller only carries the decisions out.
    class Pump
    {
    public:
        struct Decision
        {
            enum class What : std::uint8_t { None, Show, Notify };
            What what = What::None;
            Whisper whisper;            // Show and Notify
            std::uint32_t seq = 0;      // Show: the seq sent with it
            bool hide = false;          // None: take the card down (nothing on it, or it may not show now)
            std::uint32_t timedOut = 0; // the seq of a card the kAnswerMs rule just ended, 0 when none
            bool timedOutBlocked = false; // that card was hidden by a menu at some point: not counted as silent
            bool gaveUp = false;        // that was the kCardFailures-th in a row: from now on every whisper is Notify
        };

        // Cards in a row ended by the kAnswerMs rule before the card is given up for the session.
        static constexpr std::uint32_t kCardFailures = 2;

        void SetEnabled(bool enabled);  // either way: the waiting ones are dropped and the one on the card ended
        void Push(Whisper whisper, std::uint64_t nowMs);
        void Clear();                   // drops the waiting ones and ends the one on the card
        // The page answered with this seq. Only the card's own seq ends it; true when it did.
        bool Answer(std::uint32_t seq);
        // One decision. Show: put it on the card (call ShowFailed when the card cannot take it). Notify: send it as a
        // notification now; it is already finished. Call again after Show failed or Notify, until None.
        [[nodiscard]] Decision Next(std::uint64_t nowMs, bool canShow, bool viewAvailable);
        void ShowFailed();              // the card could not take the last Show: it is finished; notify it instead
        // The view died with a whisper on the card: it is finished and given back, to be notified instead.
        [[nodiscard]] std::optional<Whisper> Lost();
        [[nodiscard]] std::uint32_t Current() const { return _current; }  // the seq on the card, 0 when none
        [[nodiscard]] bool Pending() const { return _current != 0 || _queue.Waiting() > 0; }
        [[nodiscard]] bool GaveUp() const { return _gaveUp; }  // the card stopped answering; Notify for the session

    private:
        void End();

        Queue _queue;
        std::uint32_t _current = 0;
        std::uint32_t _last = 0;
        std::optional<Whisper> _onCard;    // the whisper with seq _current
        std::uint32_t _timeouts = 0;       // cards in a row ended by kAnswerMs; an answer resets it
        bool _blocked = false;             // the card on screen was hidden by a menu since it went up
        bool _gaveUp = false;
    };

    // The comfort settings the whisper page follows, as extra payload fields: "contrast": "high" and
    // "motion": "reduced", each only while its setting is on (absent when off).
    struct ComfortField
    {
        std::string_view key, value;
        bool operator==(const ComfortField&) const = default;
    };
    [[nodiscard]] std::vector<ComfortField> ComfortFields(bool highContrast, bool reduceMotion);

    // A kind's name for the log.
    [[nodiscard]] std::string_view KindName(Kind kind) noexcept;
}
