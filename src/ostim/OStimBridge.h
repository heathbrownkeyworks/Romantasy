#pragma once

#include "ostim/SceneTracker.h"

#include <atomic>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

// The optional bridge to OStim Standalone. OStim is found at run time through its own SKSE message (its native plugin
// interface), never loaded or linked by Romantasy; without it every call here does nothing. OStimBridge.cpp is the only
// file that includes OStim's interface headers.
class OStimBridge
{
public:
    static OStimBridge& GetSingleton();

    // Game thread, once, at data loaded after Settings::Load. Logs one line saying what was found and, when OStim is
    // found, registers the start, node changed and stop listeners.
    void Start();

    [[nodiscard]] bool Found() const { return _found.load(); }
    // OStim.dll is loaded but did not hand out the "Threads" interface (older than 7.3.3, or not reached); the bridge
    // stays off.
    [[nodiscard]] bool TooOld() const { return _tooOld.load(); }
    // ThreadInterface::getVersion(); 0 when OStim was not found.
    [[nodiscard]] std::uint32_t InterfaceVersion() const { return _version.load(); }
    // The native thread builder arrived with interface 2 (OStim 7.4).
    [[nodiscard]] bool CanStart() const { return Found() && InterfaceVersion() >= 2; }
    // Lock-free: the player's OStim thread is running.
    [[nodiscard]] bool InScene() const { return _inScene.load(); }

    // Game thread, at kPreLoadGame and kNewGame: OStim deletes its threads on a load without a stop event, so this
    // clears InScene, the tracked scene and any pending start.
    void OnGameReset();

    // Lock-free: a moment Romantasy asked for has not started yet (claimed, searching, or waiting for OStim's scene).
    [[nodiscard]] bool StartPending() const;
    // Any thread: claims the one pending start. False when one is already pending.
    bool ClaimStart();
    // Any thread: the current start stamp. Read right after a successful ClaimStart, it names that claim.
    [[nodiscard]] std::uint32_t ClaimedStamp() const;
    // Game thread, after a successful ClaimStart, with the stamp read for it (a call for an older claim does nothing):
    // starts a moment of `kind` between the player and the follower. When
    // it cannot (the actor is gone, combat, a scene already running, OStim refused or found no scene), the pending
    // start ends and the manager is told (code 6). The pending start also ends when the scene starts, when it is
    // forgotten after a minute without one, and in OnGameReset.
    void BeginMoment(RE::ActorHandle follower, romantasy::ostim::Kind kind, std::uint32_t stamp);

private:
    // OStim's listener objects, defined in OStimBridge.cpp; static storage, they live for the process.
    class StartListener;
    class NodeListener;
    class StopListener;
    // Receives OLibrary.GetRandomSceneSuperloadCSV's answer on the VM thread; defined in OStimBridge.cpp.
    class SceneSearchAnswer;

    // A start claimed by ClaimStart and taken up by BeginMoment. Game thread only.
    struct PendingStart
    {
        std::uint32_t follower = 0;  // the follower's reference form ID
        romantasy::ostim::Kind kind = romantasy::ostim::Kind::None;
        std::uint64_t sinceMs = 0;
        // Set while OStim's scene search has not answered.
        std::optional<std::uint64_t> searchSinceMs;
        // Set once the builder was started: the scene that becomes ours when the player and the follower are exactly
        // its participants.
        std::optional<romantasy::ostim::Ours> ours;
    };

    OStimBridge() = default;

    // Game thread, from the tasks the listeners post. `generation` is the reset count the listener saw; a task from
    // before the last OnGameReset is dropped. `epoch` is the scene epoch (see SceneTracker): a start or node task from
    // a scene whose stop was already handled changes nothing.
    void SceneStarted(std::uint32_t generation, std::uint32_t epoch, std::vector<std::uint32_t> participants, std::uint64_t nowMs);
    void SceneNode(std::uint32_t generation, std::uint32_t epoch, std::vector<std::uint32_t> participants, romantasy::ostim::NodeSeen node,
        std::uint64_t nowMs);
    void SceneStopped(std::uint32_t generation, std::uint32_t epoch, std::vector<std::uint32_t> participants, std::uint64_t nowMs);
    // Game thread: the scene search answered. `stamp` is the start stamp the search was issued with; an answer for a
    // start that has since ended is dropped. nullopt is an answer that was not a string.
    void SceneFound(std::uint32_t stamp, std::optional<std::string> sceneID);
    // Game thread: a scene with exactly the player and the pending start's follower becomes ours (and the start ends).
    void AdoptIfOurs(const std::vector<std::uint32_t>& participants);
    // Game thread: the pending start could not go on: it ends and the manager sends code 6.
    void CouldNotStart(const char* why);
    // Game thread: forgets the pending start and releases the claim.
    void EndPending();
    // Game thread: asks OStim to stop the player's thread (OThread.Stop(0) through the VM).
    void StopScene();
    // Game thread: keeps the one-second ticker running while a scene is tracked, InScene is up or a start is pending.
    void TickerChanged();
    // Game thread, once a second while the ticker runs: when the player's thread is gone two ticks in a row, InScene
    // goes down and a tracked scene is lost. Also stops a gentle scene of ours that runs too long, and ends a pending
    // start whose search went unanswered or that saw no scene in time.
    void OnTick();
    static void TickerLoop(std::stop_token stop);

    std::atomic_bool _started = false;
    std::atomic_bool _found = false;
    std::atomic_bool _tooOld = false;
    std::atomic<std::uint32_t> _version = 0;
    std::atomic_bool _inScene = false;
    // Raised by OnGameReset; read by the listeners when they post a task.
    std::atomic<std::uint32_t> _generation = 0;
    // Raised by the stop listener; read by the start and node listeners.
    std::atomic<std::uint32_t> _epoch = 0;
    // Raised by ClaimStart (any thread); lowered on the game thread when the pending start ends.
    std::atomic_bool _startPending = false;
    // Raised each time a start is claimed and each time a pending start ends.
    std::atomic<std::uint32_t> _startStamp = 0;
    // Game thread only.
    romantasy::ostim::SceneTracker _tracker;
    std::optional<PendingStart> _pending;
    // The scene of ours being tracked, for the code 6 sent when it leaves its kind. Game thread only.
    std::optional<romantasy::ostim::Ours> _watched;
    std::jthread _ticker;
};
