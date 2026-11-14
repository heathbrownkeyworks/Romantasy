#pragma once

#include "ostim/Moments.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The player's OStim scene as the game thread sees it: one record, built from plain copies the listeners made.
// Pure and standard headers only. Game thread only; no lock.
namespace romantasy::ostim
{
    // A plain copy of one node.
    struct NodeSeen
    {
        std::string nodeID;
        Kind kind = Kind::None;
        bool force = false;
    };

    // A scene Romantasy asked for; nodeID lower case, "" for intimacy.
    struct Ours
    {
        std::uint32_t follower = 0;
        Kind kind = Kind::None;
        std::string nodeID;
    };

    struct SceneEnd
    {
        std::vector<std::uint32_t> participants;
        Kind kind = Kind::None;
        bool force = false;
        float seconds = 0.0f;
        bool ours = false;
        Kind oursKind = Kind::None;
        bool stoppedForMismatch = false;
        bool lost = false;
    };

    // OStim's interface version for the log. Above 0xFFFF it is OStim's packed plugin version (REL::Version::pack:
    // major in the top byte, minor in the next, patch in the next 12 bits, build in the low 4): 0x07050020 is "7.5.2",
    // and a non-zero build is added (0x07050013 is "7.5.1.3"). Smaller values (1 to 3) are plain numbers.
    [[nodiscard]] inline std::string InterfaceText(std::uint32_t version)
    {
        if (version <= 0xFFFF) return std::to_string(version);
        std::string text = std::to_string((version >> 24) & 0xFF) + "." + std::to_string((version >> 16) & 0xFF) + "." +
                           std::to_string((version >> 4) & 0xFFF);
        if ((version & 0xF) != 0) text += "." + std::to_string(version & 0xF);
        return text;
    }

    // `epoch` is the bridge's scene epoch, raised by every stop event: a start or node event carries the epoch it saw,
    // a stop event the epoch from before it raised it. A start or node event whose epoch is not newer than the last
    // stop's is from a scene already ended (OStim lets a node change race the stop) and changes nothing. With no epoch
    // nothing is dropped.
    class SceneTracker
    {
    public:
        // Creates the record if no node came first; keeps an earlier start time and the nodes seen.
        void Started(std::vector<std::uint32_t> participants, std::uint64_t nowMs, std::optional<std::uint32_t> epoch = std::nullopt)
        {
            if (Stale(epoch)) return;
            Record& record = Ensure();
            if (!record.startMs) record.startMs = nowMs;
            record.participants = std::move(participants);
        }

        // Creates the record if needed. True exactly once per record: when a gentle scene of ours first leaves its
        // kind, or its first node is not the one asked for. The record is then marked stoppedForMismatch.
        [[nodiscard]] bool Node(const NodeSeen& node, std::uint64_t nowMs, std::optional<std::uint32_t> epoch = std::nullopt)
        {
            if (Stale(epoch)) return false;
            Record& record = Ensure();
            if (!record.firstNodeMs) record.firstNodeMs = nowMs;
            record.kind = Deeper(record.kind, node.kind);
            record.force = record.force || node.force;

            const bool firstOfOurs = !record.oursNodeSeen;
            record.oursNodeSeen = true;
            if (!record.ours || !Gentle(record.ours->kind) || record.stoppedForMismatch) return false;
            const bool wrongFirst = firstOfOurs && !record.ours->nodeID.empty() && node.nodeID != record.ours->nodeID;
            if (wrongFirst || !NodeFits(record.ours->kind, node.kind)) {
                record.stoppedForMismatch = true;
                return true;
            }
            return false;
        }

        // nullopt when there is no record. The participants given replace the earlier ones.
        [[nodiscard]] std::optional<SceneEnd> Stopped(
            std::vector<std::uint32_t> participants, std::uint64_t nowMs, std::optional<std::uint32_t> epoch = std::nullopt)
        {
            _misses = 0;
            if (epoch && (!_lastStopped || *epoch > *_lastStopped)) _lastStopped = epoch;
            if (!_record) return std::nullopt;
            _record->participants = std::move(participants);
            return End(nowMs, false);
        }

        // The thread vanished without a stop event.
        [[nodiscard]] std::optional<SceneEnd> Lost(std::uint64_t nowMs)
        {
            if (!_record) return std::nullopt;
            return End(nowMs, true);
        }

        // Marks the current record as ours, or, with none, the next record created (one record only).
        void Adopt(const Ours& ours)
        {
            if (_record) {
                _record->ours = ours;
                _record->oursNodeSeen = false;
            } else {
                _pending = ours;
            }
        }

        void Clear()
        {
            _record.reset();
            _pending.reset();
            _misses = 0;
        }

        // Once a tick: whether OStim still has the player's thread. True from the second tick in a row without it
        // (a normal stop removes the thread a moment before its stop event arrives). A tick with the thread, a stop,
        // a lost scene and Clear start the count again.
        [[nodiscard]] bool Missing(bool threadPresent)
        {
            if (threadPresent) {
                _misses = 0;
                return false;
            }
            if (_misses < 2) ++_misses;
            return _misses >= 2;
        }

        [[nodiscard]] bool Active() const { return _record.has_value(); }

        // A gentle scene of ours has run at least ms and no stop was asked.
        [[nodiscard]] bool OursGentleFor(std::uint64_t nowMs, std::uint64_t ms) const
        {
            if (!_record || !_record->ours || !Gentle(_record->ours->kind) || _record->stopAsked || _record->stoppedForMismatch) {
                return false;
            }
            const std::uint64_t begin = _record->Begin();
            return nowMs >= begin && nowMs - begin >= ms;
        }

        void StopAsked()
        {
            if (_record) _record->stopAsked = true;
        }

    private:
        struct Record
        {
            std::vector<std::uint32_t> participants;
            std::optional<std::uint64_t> startMs;
            std::optional<std::uint64_t> firstNodeMs;
            Kind kind = Kind::None;
            bool force = false;
            std::optional<Ours> ours;
            bool oursNodeSeen = false;
            bool stopAsked = false;
            bool stoppedForMismatch = false;

            // The earliest of the first node time and the start time.
            [[nodiscard]] std::uint64_t Begin() const
            {
                if (startMs && firstNodeMs) return (std::min)(*startMs, *firstNodeMs);
                return startMs ? *startMs : firstNodeMs.value_or(0);
            }
        };

        [[nodiscard]] bool Stale(std::optional<std::uint32_t> epoch) const { return epoch && _lastStopped && *epoch <= *_lastStopped; }

        [[nodiscard]] static constexpr bool Gentle(Kind kind) { return kind == Kind::Affection || kind == Kind::Closeness; }

        Record& Ensure()
        {
            if (!_record) {
                _record.emplace();
                if (_pending) {
                    _record->ours = std::move(_pending);
                    _pending.reset();
                }
            }
            return *_record;
        }

        [[nodiscard]] SceneEnd End(std::uint64_t nowMs, bool lost)
        {
            Record record = std::move(*_record);
            _record.reset();
            _misses = 0;
            const std::uint64_t begin = record.Begin();
            SceneEnd end;
            end.participants = std::move(record.participants);
            end.kind = record.kind;
            end.force = record.force;
            end.seconds = nowMs > begin ? static_cast<float>(static_cast<double>(nowMs - begin) / 1000.0) : 0.0f;
            end.ours = record.ours.has_value();
            end.oursKind = record.ours ? record.ours->kind : Kind::None;
            end.stoppedForMismatch = record.stoppedForMismatch;
            end.lost = lost;
            return end;
        }

        std::optional<Record> _record;
        std::optional<Ours> _pending;
        std::optional<std::uint32_t> _lastStopped;
        std::uint8_t _misses = 0;
    };
}
