#include "ui/Whispers.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace romantasy::ui::whispers
{
    namespace
    {
        // Age of something stamped at `then`. A clock that went backwards gives 0.
        std::uint64_t Age(std::uint64_t now, std::uint64_t then) { return now >= then ? now - then : 0; }

        std::int32_t Clamp32(std::int64_t v)
        {
            constexpr std::int64_t lo = std::numeric_limits<std::int32_t>::min();
            constexpr std::int64_t hi = std::numeric_limits<std::int32_t>::max();
            return static_cast<std::int32_t>(std::clamp(v, lo, hi));
        }

        // Length of the valid UTF-8 sequence at s[i], or 0 when it is not valid.
        std::size_t SequenceLength(std::string_view s, std::size_t i)
        {
            const auto b = [&](std::size_t k) { return static_cast<unsigned char>(s[k]); };
            const auto cont = [&](std::size_t k) { return k < s.size() && (b(k) & 0xC0) == 0x80; };
            const unsigned c = b(i);
            if (c < 0x80) return 1;
            if (c >= 0xC2 && c <= 0xDF) return cont(i + 1) ? 2 : 0;
            if (c >= 0xE0 && c <= 0xEF) {
                if (!cont(i + 1) || !cont(i + 2)) return 0;
                if (c == 0xE0 && b(i + 1) < 0xA0) return 0;   // overlong
                if (c == 0xED && b(i + 1) >= 0xA0) return 0;  // surrogate
                return 3;
            }
            if (c >= 0xF0 && c <= 0xF4) {
                if (!cont(i + 1) || !cont(i + 2) || !cont(i + 3)) return 0;
                if (c == 0xF0 && b(i + 1) < 0x90) return 0;   // overlong
                if (c == 0xF4 && b(i + 1) >= 0x90) return 0;  // above U+10FFFF
                return 4;
            }
            return 0;
        }

        // Valid UTF-8, at most maxBytes, cut on a character boundary.
        std::string Clean(std::string_view s, std::size_t maxBytes)
        {
            std::string out;
            for (std::size_t i = 0; i < s.size();) {
                const std::size_t n = SequenceLength(s, i);
                const std::string_view piece = n ? s.substr(i, n) : std::string_view{ "\xEF\xBF\xBD" };
                if (out.size() + piece.size() > maxBytes) break;
                out += piece;
                i += n ? n : 1;
            }
            return out;
        }

        std::string Number(std::int64_t v) { return std::to_string(v < 0 ? -v : v); }

        // "+N" or "-N" with U+2212 as the minus
        std::string Signed(std::int32_t points)
        {
            return std::string(points < 0 ? "\xE2\x88\x92" : "+") + Number(points);
        }
    }

    Shown Describe(const Whisper& w)
    {
        Shown out;
        out.name = Clean(w.name, kMaxNameBytes);
        const std::string label = Clean(w.label, kMaxLabelBytes);

        switch (w.kind) {
        case Kind::Deed: {
            const bool loss = w.points < 0;
            out.tone = loss ? Tone::Loss : Tone::Gain;
            out.icon = loss ? (w.strong ? "heartbroken" : "dislikes") : (w.strong ? "star" : "heart");
            const std::string lead = w.deeds > 1 ? std::to_string(w.deeds) + " deeds" : (label.empty() ? "A deed" : label);
            out.line = lead + " \xC2\xB7 " + Signed(w.points);
            break;
        }
        case Kind::MoodWarm:
            out.line = "Feels warm toward you";
            out.tone = Tone::Gain;
            out.icon = "heart";
            break;
        case Kind::MoodStrained:
            out.line = "Things are strained";
            out.tone = Tone::Loss;
            out.icon = "dislikes";
            break;
        case Kind::Reunion:
            out.line = "Glad to have you back";
            out.tone = Tone::Gain;
            out.icon = "heart";
            break;
        case Kind::TimeApart:
            out.line = "Has been away from you for days";
            out.tone = Tone::Plain;
            out.icon = "neutral";
            break;
        case Kind::TierGained:
            out.line = "Reached " + (label.empty() ? std::string("a new tier") : label);
            out.tone = Tone::Gain;
            out.icon = "heart";
            break;
        case Kind::TierLost:
            out.line = "Fell to " + (label.empty() ? std::string("a lower tier") : label);
            out.tone = Tone::Loss;
            out.icon = "dislikes";
            break;
        case Kind::MomentNotYet:
            out.line = "Not yet";
            out.tone = Tone::Plain;
            out.icon = "neutral";
            break;
        case Kind::MomentNotInMood:
            out.line = "Not in the mood";
            out.tone = Tone::Loss;
            out.icon = "dislikes";
            break;
        }
        return out;
    }

    std::string NotificationText(const Whisper& w)
    {
        const Shown s = Describe(w);
        return s.name.empty() ? s.line : s.name + ": " + s.line;
    }

    std::string NotificationMarkup(const Whisper& w)
    {
        std::string text = NotificationText(w);
        constexpr std::string_view kMinus = "\xE2\x88\x92";  // U+2212
        for (std::size_t at = 0; (at = text.find(kMinus, at)) != std::string::npos;) text.replace(at, kMinus.size(), "-");
        std::string out;
        out.reserve(text.size());
        for (const char c : text) {
            if (c == '&') out += "&amp;";
            else if (c == '<') out += "&lt;";
            else out += c;
        }
        return out;
    }

    void Pump::End()
    {
        _queue.Finished();
        _current = 0;
        _onCard.reset();
        _blocked = false;
    }

    void Pump::SetEnabled(bool enabled)
    {
        _queue.SetEnabled(enabled);
        _current = 0;
        _onCard.reset();
        _blocked = false;
    }

    void Pump::Push(Whisper whisper, std::uint64_t nowMs) { _queue.Push(std::move(whisper), nowMs); }

    void Pump::Clear()
    {
        _queue.Clear();
        _current = 0;
        _onCard.reset();
        _blocked = false;
    }

    bool Pump::Answer(std::uint32_t seq)
    {
        if (!IsCurrentAnswer(seq, _current)) return false;
        End();
        _timeouts = 0;  // the card works
        return true;
    }

    Pump::Decision Pump::Next(std::uint64_t nowMs, bool canShow, bool viewAvailable)
    {
        Decision out;
        // A card hidden by a menu cannot answer in time; its page is not to blame.
        if (_current != 0 && !canShow) _blocked = true;
        // Not answered within kAnswerMs: taken down, and counted unless a menu hid it. Looked at before the queue
        // moves on, which it does on its own once kAnswerMs has passed. A hidden card neither counts nor resets.
        if (_current != 0 && !_queue.Showing(nowMs)) {
            out.timedOut = _current;
            out.timedOutBlocked = _blocked;
            End();
            if (!out.timedOutBlocked && ++_timeouts >= kCardFailures && !_gaveUp) {
                _gaveUp = true;
                out.gaveUp = true;
            }
        }
        auto next = _queue.Next(nowMs, canShow);
        if (!next) {
            out.hide = _current == 0 || !canShow;
            return out;
        }
        out.whisper = std::move(*next);
        if (!viewAvailable || _gaveUp) {
            End();
            out.what = Decision::What::Notify;
            return out;
        }
        _last = NextSeq(_last);
        _current = _last;
        _onCard = out.whisper;
        out.what = Decision::What::Show;
        out.seq = _current;
        return out;
    }

    void Pump::ShowFailed() { End(); }

    std::optional<Whisper> Pump::Lost()
    {
        if (_current == 0) return std::nullopt;
        auto lost = std::move(_onCard);
        End();
        return lost;
    }

    std::vector<ComfortField> ComfortFields(bool highContrast, bool reduceMotion)
    {
        std::vector<ComfortField> fields;
        if (highContrast) fields.push_back({ "contrast", "high" });
        if (reduceMotion) fields.push_back({ "motion", "reduced" });
        return fields;
    }

    std::string_view KindName(Kind kind) noexcept
    {
        switch (kind) {
        case Kind::Deed: return "deed";
        case Kind::MoodWarm: return "mood warm";
        case Kind::MoodStrained: return "mood strained";
        case Kind::Reunion: return "reunion";
        case Kind::TimeApart: return "time apart";
        case Kind::TierGained: return "tier gained";
        case Kind::TierLost: return "tier lost";
        case Kind::MomentNotYet: return "moment not yet";
        case Kind::MomentNotInMood: return "moment not in the mood";
        }
        return "whisper";
    }

    std::optional<Placement> PlaceCard(std::int32_t displayWidth, std::int32_t displayHeight, WhisperCorner corner)
    {
        if (displayWidth <= 0 || displayHeight <= 0) return std::nullopt;
        const float scale = std::clamp(static_cast<float>(displayHeight) / 1080.0f, 1.0f, 3.0f);
        const auto px = [scale](std::int32_t css) { return static_cast<std::int32_t>(std::lround(css * scale)); };
        Placement out;
        out.scale = scale;
        // Rounded up, so the view is never smaller than the 420x96 CSS card at this scale.
        const auto cover = [scale](std::int32_t css) { return static_cast<std::int32_t>(std::ceil(css * scale)); };
        out.w = cover(kCardWidth);
        out.h = cover(kCardHeight);
        const bool left = corner == WhisperCorner::TopLeft || corner == WhisperCorner::BottomLeft;
        const bool top = corner == WhisperCorner::TopLeft || corner == WhisperCorner::TopRight;
        out.x = left ? px(kSideMargin) : displayWidth - px(kSideMargin) - out.w;
        out.y = top ? px(kTopMargin) : displayHeight - px(kBottomMargin) - out.h;
        out.x = (std::max)(0, (std::min)(out.x, displayWidth - out.w));
        out.y = (std::max)(0, (std::min)(out.y, displayHeight - out.h));
        return out;
    }

    bool MenuBlocks(const MenuFacts& menu)
    {
        if (menu.name == "HUD Menu" || menu.name == "Fader Menu" || menu.name == "Cursor Menu") return false;
        if (menu.name == "Dialogue Menu" || menu.name == "Main Menu" || menu.name == "Loading Menu") return true;
        return menu.pausesGame || menu.usesCursor || menu.usesMenuContext || menu.modal;
    }

    void Queue::SetEnabled(bool enabled)
    {
        Clear();
        _enabled = enabled;
    }

    void Queue::Clear()
    {
        _waiting.clear();
        _showing = false;
    }

    void Queue::Finished() { _showing = false; }

    void Queue::Push(Whisper whisper, std::uint64_t nowMs)
    {
        if (!_enabled) return;
        if (whisper.kind == Kind::Deed) {
            if (whisper.deeds < 1) whisper.deeds = 1;
            // Merge into the newest waiting deed of the same follower pushed within the window.
            for (auto it = _waiting.rbegin(); it != _waiting.rend(); ++it) {
                Whisper& w = it->whisper;
                if (w.kind != Kind::Deed || w.follower != whisper.follower) continue;
                if (nowMs < it->lastMs) it->lastMs = nowMs;  // the clock went back: count from here
                if (Age(nowMs, it->lastMs) > kMergeMs) break;
                w.points = Clamp32(std::int64_t{ w.points } + whisper.points);
                w.deeds = Clamp32(std::int64_t{ w.deeds } + whisper.deeds);
                w.strong = w.strong || whisper.strong;
                w.label = std::move(whisper.label);
                w.name = std::move(whisper.name);
                it->lastMs = nowMs;
                if (w.points == 0) _waiting.erase(std::next(it).base());
                return;
            }
            if (whisper.points == 0) return;  // nothing changed, nothing to say
        }
        if (_waiting.size() >= kMaxWaiting) _waiting.pop_front();
        _waiting.push_back({ std::move(whisper), nowMs, nowMs });
    }

    std::optional<Whisper> Queue::Next(std::uint64_t nowMs, bool canShow)
    {
        if (_showing) {
            if (nowMs < _shownMs) _shownMs = nowMs;  // the clock went back: count from here
            if (Age(nowMs, _shownMs) >= kAnswerMs) _showing = false;
        }
        for (auto& e : _waiting) {
            if (nowMs < e.firstMs) e.firstMs = nowMs;
            if (nowMs < e.lastMs) e.lastMs = nowMs;
        }
        std::erase_if(_waiting, [&](const Entry& e) { return Age(nowMs, e.firstMs) >= kMaxAgeMs; });

        if (_showing || !canShow || _waiting.empty()) return std::nullopt;
        Whisper out = std::move(_waiting.front().whisper);
        _waiting.pop_front();
        _showing = true;
        _shownMs = nowMs;
        return out;
    }

    bool Queue::Showing(std::uint64_t nowMs) const
    {
        return _showing && Age(nowMs, _shownMs) < kAnswerMs;
    }

    std::size_t Queue::Waiting() const { return _waiting.size(); }
}
