#include "ostim/OStimBridge.h"

#include "romance/RomanceManager.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <utility>

// OStim's interface headers use uint32_t, int32_t, NULL, HMODULE and GetModuleHandleA without including anything, so
// they come here, after <Windows.h> and <cstdint>, and in no other file.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdint>

#include "ostim/api/InterfaceExchangeMessage.h"
#include "ostim/api/Threading/ThreadInterface.h"

namespace
{
    using romantasy::ostim::Kind;

    // Set once by Start on the game thread; read on the game thread.
    std::atomic<OStim::ThreadInterface*> g_threads = nullptr;

    constexpr RE::FormID kPlayer = 0x14;
    // The scene search must answer within this, or the start is code 6.
    constexpr std::uint64_t kSearchAnswerMs = 5000;
    // A gentle scene of ours still running this long after it began is stopped (in case the duration is not honoured).
    constexpr std::uint64_t kFallbackStopMs = 40000;
    // A pending start that sees no scene this long after it began is forgotten, without an event.
    constexpr std::uint64_t kPendingForgetMs = 60000;

    // OLibrary.GetRandomSceneSuperloadCSV(Actor[] Actors, string FurnitureType = "", ...): the actor array, then 45
    // strings. A dispatch through the VM fills no Papyrus defaults, so every argument is packed.
    constexpr std::size_t kSuperloadArguments = 46;
    constexpr std::size_t kAnyActionType = 14;
    constexpr std::size_t kActionWhitelistTypes = 30;

    [[nodiscard]] std::uint64_t NowMs()
    {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    // Inside a listener only: the thread's actors as reference form IDs, nulls skipped.
    [[nodiscard]] std::vector<std::uint32_t> Participants(OStim::Thread* thread)
    {
        std::vector<std::uint32_t> out;
        const std::uint32_t count = thread->getActorCount();
        for (std::uint32_t i = 0; i < count; ++i) {
            OStim::ThreadActor* threadActor = thread->getActor(i);
            if (!threadActor) continue;
            const auto* actor = static_cast<RE::Actor*>(threadActor->getGameActor());
            if (!actor) continue;
            out.push_back(actor->GetFormID());
        }
        return out;
    }

    // Inside a listener only: a node actor carries one of the four force tags (a leading '-' is the swapped role).
    [[nodiscard]] bool ActorForced(OStim::NodeActor* nodeActor)
    {
        const std::uint32_t count = nodeActor->getTagCount();
        for (std::uint32_t i = 0; i < count; ++i) {
            OStim::NodeActorTag* tag = nodeActor->getTag(i);
            const char* id = tag ? tag->getTagID() : nullptr;
            if (id && romantasy::ostim::IsForceActorTag(id)) return true;
        }
        return false;
    }

    // Inside a listener only: a plain copy of the node (graph pointers live for the process).
    [[nodiscard]] romantasy::ostim::NodeSeen Copy(OStim::Node* node)
    {
        romantasy::ostim::NodeSeen seen;
        if (const char* id = node->getNodeID()) seen.nodeID = id;
        const std::uint32_t actions = node->getActionCount();
        for (std::uint32_t i = 0; i < actions; ++i) {
            OStim::Action* action = node->getAction(i);
            OStim::ActionType* type = action ? action->getType() : nullptr;
            const char* actionID = type ? type->getActionID() : nullptr;
            if (actionID) seen.kind = romantasy::ostim::Deeper(seen.kind, romantasy::ostim::KindOfAction(actionID));
        }
        seen.force = node->hasTag(romantasy::ostim::kForceSceneTag.data());
        const std::uint32_t actors = node->getActorCount();
        for (std::uint32_t i = 0; i < actors && !seen.force; ++i) {
            OStim::NodeActor* nodeActor = node->getActor(i);
            if (nodeActor && ActorForced(nodeActor)) seen.force = true;
        }
        return seen;
    }

    void Post(SKSE::TaskInterface::TaskFn task)
    {
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask(std::move(task));
        }
    }

    // The scene search's arguments: the actors first, the kind's action list as AnyActionType and ActionWhitelistTypes
    // (the whitelist alone also matches scenes with no actions), every other string empty (no furniture).
    class SceneSearchArguments final : public RE::BSScript::IFunctionArguments
    {
    public:
        SceneSearchArguments(std::vector<RE::Actor*> actors, std::string actions) : _actors(std::move(actors)), _actions(std::move(actions)) {}

        bool operator()(RE::BSScrapArray<RE::BSScript::Variable>& a_dst) const override
        {
            a_dst.resize(static_cast<std::uint32_t>(kSuperloadArguments));
            std::vector<RE::Actor*> actors = _actors;
            a_dst[0].Pack(actors);
            for (std::size_t i = 1; i < kSuperloadArguments; ++i) {
                a_dst[static_cast<std::uint32_t>(i)].SetString(
                    i == kAnyActionType || i == kActionWhitelistTypes ? std::string_view(_actions) : std::string_view{});
            }
            return true;
        }

    private:
        std::vector<RE::Actor*> _actors;
        std::string _actions;
    };

    // OThread.Stop has no result to read.
    class NoAnswer final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        void operator()(RE::BSScript::Variable) override {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    [[nodiscard]] std::string Lower(std::string text)
    {
        std::ranges::transform(text, text.begin(), [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
        return text;
    }
}

// Receives the scene search's answer on the VM thread: copies only the string and hands it to the game thread with
// the stamp of the start it was asked for.
class OStimBridge::SceneSearchAnswer final : public RE::BSScript::IStackCallbackFunctor
{
public:
    explicit SceneSearchAnswer(std::uint32_t stamp) : _stamp(stamp) {}
    void operator()(RE::BSScript::Variable a_result) override
    {
        std::optional<std::string> sceneID;
        if (a_result.IsString()) {
            sceneID = std::string(a_result.GetString());
        }
        Post([stamp = _stamp, sceneID = std::move(sceneID)]() mutable { OStimBridge::GetSingleton().SceneFound(stamp, std::move(sceneID)); });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::uint32_t _stamp;
};

// OStim calls these on its own threads, under its own locks. They read only the Thread* passed in, copy plain data,
// touch only atomics and hand the rest to the game thread.
class OStimBridge::StartListener final : public OStim::ThreadEventListener
{
public:
    void listen(OStim::Thread* thread) override
    {
        if (!thread || !thread->isPlayerThread()) return;
        auto& bridge = OStimBridge::GetSingleton();
        bridge._inScene = true;
        Post([generation = bridge._generation.load(), epoch = bridge._epoch.load(), participants = Participants(thread),
                 now = NowMs()]() mutable {
            OStimBridge::GetSingleton().SceneStarted(generation, epoch, std::move(participants), now);
        });
    }
};

class OStimBridge::NodeListener final : public OStim::ThreadEventListener
{
public:
    void listen(OStim::Thread* thread) override
    {
        if (!thread || !thread->isPlayerThread()) return;
        OStim::Node* node = thread->getCurrentNode();
        if (!node) return;
        auto& bridge = OStimBridge::GetSingleton();
        // Only with a task posted: the game thread then keeps the ticker going while the flag is up.
        bridge._inScene = true;
        Post([generation = bridge._generation.load(), epoch = bridge._epoch.load(), participants = Participants(thread), seen = Copy(node),
                 now = NowMs()]() mutable {
            OStimBridge::GetSingleton().SceneNode(generation, epoch, std::move(participants), std::move(seen), now);
        });
    }
};

class OStimBridge::StopListener final : public OStim::ThreadEventListener
{
public:
    void listen(OStim::Thread* thread) override
    {
        if (!thread || !thread->isPlayerThread()) return;
        auto& bridge = OStimBridge::GetSingleton();
        auto participants = Participants(thread);
        // The epoch from before the raise: a start or node event that saw it belongs to this scene.
        const std::uint32_t epoch = bridge._epoch.fetch_add(1);
        bridge._inScene = false;
        Post([generation = bridge._generation.load(), epoch, participants = std::move(participants), now = NowMs()]() mutable {
            OStimBridge::GetSingleton().SceneStopped(generation, epoch, std::move(participants), now);
        });
    }
};

OStimBridge& OStimBridge::GetSingleton()
{
    static OStimBridge singleton;
    return singleton;
}

void OStimBridge::Start()
{
    if (_started.exchange(true)) {
        return;
    }
    // OStim fills the map from its own message handler, synchronously; it stays null when OStim is absent.
    OStim::InterfaceExchangeMessage message;
    if (const auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->Dispatch(OStim::InterfaceExchangeMessage::MESSAGE_TYPE, &message, sizeof(message), "OStim");
    }
    OStim::ThreadInterface* threads = nullptr;
    if (message.interfaceMap) {
        // OStim's interfaces use single inheritance from PluginInterface.
        threads = static_cast<OStim::ThreadInterface*>(message.interfaceMap->queryInterface("Threads"));
    }
    if (!threads) {
        // Only a module already loaded: Romantasy is never the reason OStim loads.
        if (GetModuleHandleW(L"OStim.dll")) {
            _tooOld = true;
            logger::warn("OStim is too old for Romantasy's bridge; it needs 7.3.3 or newer.");
        } else {
            logger::info("OStim not found; the bridge is off.");
        }
        return;
    }
    const std::uint32_t version = threads->getVersion();
    _version = version;
    g_threads = threads;
    _found = true;

    // OStim keeps raw pointers and never unregisters: static storage, alive for the whole process.
    static StartListener startListener;
    static NodeListener nodeListener;
    static StopListener stopListener;
    threads->registerThreadStartListener(&startListener);
    threads->registerNodeChangedListener(&nodeListener);
    threads->registerThreadStopListener(&stopListener);

    logger::info("OStim found (interface {}); the bridge is on.{}", romantasy::ostim::InterfaceText(version),
        version < 2 ? " Starting moments needs OStim 7.4 or newer." : "");
}

void OStimBridge::OnGameReset()
{
    _generation.fetch_add(1);
    _inScene = false;
    _tracker.Clear();
    _watched.reset();
    EndPending();
    TickerChanged();
}

bool OStimBridge::StartPending() const
{
    return _startPending.load();
}

bool OStimBridge::ClaimStart()
{
    bool expected = false;
    if (!_startPending.compare_exchange_strong(expected, true)) {
        return false;
    }
    _startStamp.fetch_add(1);
    return true;
}

std::uint32_t OStimBridge::ClaimedStamp() const
{
    return _startStamp.load();
}

void OStimBridge::EndPending()
{
    _pending.reset();
    _startStamp.fetch_add(1);
    _startPending = false;
}

void OStimBridge::CouldNotStart(const char* why)
{
    const std::uint32_t follower = _pending ? _pending->follower : 0;
    const Kind kind = _pending ? _pending->kind : Kind::None;
    logger::info("OStim: the moment could not start ({}).", why);
    EndPending();
    RomanceManager::GetSingleton().OnMomentCouldNotStart(follower, kind);
    TickerChanged();
}

void OStimBridge::BeginMoment(RE::ActorHandle follower, romantasy::ostim::Kind kind, std::uint32_t stamp)
{
    // No claim (a load ended it), a newer claim than the one this call was made for, or the claim was already taken up.
    if (!StartPending() || stamp != _startStamp.load() || _pending) {
        return;
    }
    _pending = PendingStart{ .kind = kind, .sinceMs = NowMs() };
    const auto actor = follower.get();
    auto* player = RE::PlayerCharacter::GetSingleton();
    OStim::ThreadInterface* threads = g_threads.load();
    if (actor) {
        _pending->follower = actor->GetFormID();
    }
    if (!actor || !player || !threads || !CanStart()) {
        CouldNotStart("the follower or OStim is not there");
        return;
    }
    if (player->IsInCombat() || _inScene.load() || _tracker.Active() || threads->getThread(0)) {
        CouldNotStart("the player is in combat or in a scene");
        return;
    }
    if (kind == Kind::Intimacy) {
        // OStim's defaults: it chooses the scene and the player has control, as with OThread.QuickStart.
        void* actors[2] = { player, actor.get() };
        OStim::ThreadBuilder* builder = threads->createThreadBuilder(2, actors);
        if (!builder) {
            CouldNotStart("OStim refused the participants");
            return;
        }
        _pending->ours = romantasy::ostim::Ours{ _pending->follower, Kind::Intimacy, "" };
        // The builder frees itself in start(). For the player it answers before the scene exists; the listeners confirm it.
        if (builder->start() < 0) {
            CouldNotStart("OStim did not start the scene");
            return;
        }
        logger::info("OStim: a moment ({}) was asked for.", romantasy::ostim::Key(kind));
        TickerChanged();
        return;
    }
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        CouldNotStart("the Papyrus VM is not ready");
        return;
    }
    // The VM copies the arguments while it dispatches; they are ours to free afterwards.
    const std::unique_ptr<RE::BSScript::IFunctionArguments> args(
        new SceneSearchArguments({ player, actor.get() }, romantasy::ostim::ActionCSV(kind)));
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new SceneSearchAnswer(_startStamp.load()));
    _pending->searchSinceMs = NowMs();
    if (!vm->DispatchStaticCall("OLibrary", "GetRandomSceneSuperloadCSV", args.get(), callback)) {
        CouldNotStart("the scene search could not be called");
        return;
    }
    TickerChanged();
}

void OStimBridge::SceneFound(std::uint32_t stamp, std::optional<std::string> sceneID)
{
    // An answer for a start that has since ended is dropped silently.
    if (stamp != _startStamp.load() || !_pending || !_pending->searchSinceMs) {
        return;
    }
    _pending->searchSinceMs.reset();
    if (!sceneID || sceneID->empty()) {
        CouldNotStart("no fitting scene");
        return;
    }
    const std::string nodeID = Lower(std::move(*sceneID));
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(_pending->follower);
    auto* player = RE::PlayerCharacter::GetSingleton();
    OStim::ThreadInterface* threads = g_threads.load();
    if (!actor || !player || !threads) {
        CouldNotStart("the follower or OStim is not there");
        return;
    }
    if (player->IsInCombat() || _inScene.load() || _tracker.Active() || threads->getThread(0)) {
        CouldNotStart("the player is in combat or in a scene");
        return;
    }
    void* actors[2] = { player, actor };
    OStim::ThreadBuilder* builder = threads->createThreadBuilder(2, actors);
    if (!builder) {
        CouldNotStart("OStim refused the participants");
        return;
    }
    // Never the Safe variant. An unknown ID or actors that do not fit fall back to another scene, which the watch stops.
    builder->setStartingNode(nodeID.c_str());
    builder->noAutoMode();
    builder->noPlayerControl();
    builder->noUndressing();
    builder->noFurniture();
    // A gentle moment is a short scene the player watches; OStim takes the duration in milliseconds.
    builder->setDuration(30000);
    _pending->ours = romantasy::ostim::Ours{ _pending->follower, _pending->kind, nodeID };
    if (builder->start() < 0) {
        CouldNotStart("OStim did not start the scene");
        return;
    }
    logger::info("OStim: a moment ({}) was asked for with scene {}.", romantasy::ostim::Key(_pending->kind), nodeID);
    TickerChanged();
}

void OStimBridge::AdoptIfOurs(const std::vector<std::uint32_t>& participants)
{
    if (!_pending || !_pending->ours) {
        return;
    }
    const std::uint32_t follower = _pending->follower;
    const bool exactlyUs = participants.size() == 2 && std::ranges::count(participants, kPlayer) == 1 &&
                           std::ranges::count(participants, follower) == 1;
    if (!exactlyUs) {
        // Someone else's scene: the pending start stays until it is forgotten.
        return;
    }
    _watched = *_pending->ours;
    _tracker.Adopt(*_pending->ours);
    EndPending();
}

void OStimBridge::StopScene()
{
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        logger::warn("OStim: the Papyrus VM is not ready, so the scene could not be stopped.");
        return;
    }
    // The player's thread is always ID 0.
    const std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(std::int32_t{ 0 }));
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new NoAnswer());
    if (!vm->DispatchStaticCall("OThread", "Stop", args.get(), callback)) {
        logger::warn("OStim: could not call OThread.Stop.");
    }
}

// Each of these re-evaluates the ticker, a dropped task too: a listener may have raised InScene before it posted.
// A start or node task never creates a record while OStim has no player thread: a node change that raced a finished
// scene's stop (and read the epoch after it) would otherwise leave a record that the next scene joins. A real new
// scene's thread is already in OStim's map when its task runs here. Game thread only, never inside a listener.
void OStimBridge::SceneStarted(std::uint32_t generation, std::uint32_t epoch, std::vector<std::uint32_t> participants, std::uint64_t nowMs)
{
    OStim::ThreadInterface* threads = g_threads.load();
    if (generation == _generation.load() && (_tracker.Active() || (threads && threads->getThread(0)))) {
        // Only when the start event comes first; normally the first node event (which carries the participants, set
        // in OStim's Thread constructor before the first node change) has already created the record and adopted it.
        if (!_tracker.Active()) {
            AdoptIfOurs(participants);
        }
        _tracker.Started(std::move(participants), nowMs, epoch);
    }
    TickerChanged();
}

void OStimBridge::SceneNode(
    std::uint32_t generation, std::uint32_t epoch, std::vector<std::uint32_t> participants, romantasy::ostim::NodeSeen node, std::uint64_t nowMs)
{
    OStim::ThreadInterface* threads = g_threads.load();
    if (generation == _generation.load() && (_tracker.Active() || (threads && threads->getThread(0)))) {
        // The first node comes before the start event: a new record is adopted here when it is ours.
        if (!_tracker.Active()) {
            AdoptIfOurs(participants);
        }
        if (_tracker.Node(node, nowMs, epoch)) {
            // A gentle scene of ours went somewhere it was not asked to go (or OStim fell back to another scene).
            logger::info("OStim: the scene left the moment asked for; stopping it.");
            StopScene();
            _tracker.StopAsked();
            if (_watched) {
                RomanceManager::GetSingleton().OnMomentCouldNotStart(_watched->follower, _watched->kind);
            }
        }
    }
    TickerChanged();
}

void OStimBridge::SceneStopped(std::uint32_t generation, std::uint32_t epoch, std::vector<std::uint32_t> participants, std::uint64_t nowMs)
{
    if (generation == _generation.load()) {
        if (const auto end = _tracker.Stopped(std::move(participants), nowMs, epoch)) {
            _watched.reset();
            RomanceManager::GetSingleton().OnSceneEnded(*end);
        }
    }
    TickerChanged();
}

void OStimBridge::OnTick()
{
    if (_tracker.Active() || _inScene.load()) {
        OStim::ThreadInterface* threads = g_threads.load();
        // The player's thread is always ID 0. A load deletes OStim's threads without a stop event, and a node change
        // can race the stop and raise InScene again. OStim removes the thread a moment before its stop event, so the
        // thread counts as gone only on the second tick in a row without it.
        if (_tracker.Missing(threads && threads->getThread(0))) {
            _inScene = false;
            std::optional<romantasy::ostim::SceneEnd> end = _tracker.Active() ? _tracker.Lost(NowMs()) : std::nullopt;
            if (!end) {
                // InScene was up with no record: a lost end judges nothing but still releases what the scene held.
                end.emplace();
                end->lost = true;
            }
            _watched.reset();
            RomanceManager::GetSingleton().OnSceneEnded(*end);
        }
    }
    const std::uint64_t now = NowMs();
    if (_tracker.OursGentleFor(now, kFallbackStopMs)) {
        logger::info("OStim: the moment ran past its time; stopping it.");
        StopScene();
        _tracker.StopAsked();
    }
    if (_pending) {
        // OStim waits for the dialogue to close before it starts the player's scene, with no time limit: the forget
        // clock does not run while the Dialogue menu is open. The search keeps its own clock.
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
            _pending->sinceMs = now;
        }
        if (_pending->searchSinceMs && now - *_pending->searchSinceMs >= kSearchAnswerMs) {
            CouldNotStart("the scene search did not answer");
        } else if (now - _pending->sinceMs >= kPendingForgetMs) {
            logger::info("OStim: no scene began for the moment asked for; it is forgotten.");
            EndPending();
        }
    }
    TickerChanged();
}

void OStimBridge::TickerChanged()
{
    if (_tracker.Active() || _inScene.load() || StartPending()) {
        if (!_ticker.joinable() || _ticker.get_stop_token().stop_requested()) {
            // A stopped ticker is let go, never joined here: this runs inside an SKSE task, and SKSE holds its task
            // lock while tasks run, so joining a ticker that is waiting in AddTask would deadlock.
            if (_ticker.joinable()) {
                _ticker.detach();
            }
            _ticker = std::jthread(&OStimBridge::TickerLoop);
        }
    } else if (_ticker.joinable()) {
        _ticker.request_stop();
    }
}

void OStimBridge::TickerLoop(std::stop_token stop)
{
    std::mutex mutex;
    std::condition_variable_any wake;
    std::unique_lock lock(mutex);
    // wait_for returns early, true, when stop is requested; otherwise false after a second.
    while (!wake.wait_for(lock, stop, std::chrono::seconds(1), [&stop] { return stop.stop_requested(); })) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            return;
        }
        tasks->AddTask([]() { OStimBridge::GetSingleton().OnTick(); });
    }
}
