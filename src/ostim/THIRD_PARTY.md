# Third-party files in src/ostim

## OStim Standalone's native plugin interface

`src/ostim/api/` holds OStim's own interface headers, copied byte for byte. Nothing in them is changed.

- Source: https://github.com/VersuchDrei/OStimNG, folder `skse/src/PluginInterface`, tag `7.5.2`, read on 2026-11-14.
- Files copied: `InterfaceExchangeMessage.h`, `InterfaceMap.h`, `PluginInterface.h`; all of `Threading/` (`Thread.h`, `ThreadActor.h`, `ThreadActorEventListener.h`, `ThreadActorVisitor.h`, `ThreadBuilder.h`, `ThreadEventListener.h`, `ThreadInterface.h`); `Furniture/FurnitureType.h`; and every file of `Graph/` except `GraphInterface.h` (`Action.h`, `ActionActor.h`, `ActionTag.h`, `ActionTagVisitor.h`, `ActionType.h`, `ActionVisitor.h`, `Node.h`, `NodeActor.h`, `NodeActorTag.h`, `NodeActorTagVisitor.h`, `NodeActorVisitor.h`, `NodeTag.h`, `NodeTagVisitor.h`, `Role.h`). These are the files the bridge needs and the files they include.
- `src/ostim/api/LICENSE` is a copy of the repository's `LICENSE` at the same tag.
- License: GPL-3.0, the standard text, with no additional permissions and no notice in the headers themselves. Romantasy's own additional permissions (EXCEPTIONS.md) do not extend to these files.
- Only `OStimBridge.cpp` includes them. OStim is found at run time: the bridge sends OStim's own SKSE message (`'OST'`, an `InterfaceExchangeMessage`, to the plugin `"OStim"`) and OStim fills in its interface map. Romantasy never loads OStim.dll and does not link against it.

### What the bridge calls

- `InterfaceMap::queryInterface("Threads")`, and `PluginInterface::getVersion()` on what it returns.
- `ThreadInterface`: `registerThreadStartListener`, `registerNodeChangedListener`, `registerThreadStopListener`, `getThread(0)` (only on the game thread, never inside a listener), and, behind `CanStart()` (interface 2 or newer), `createThreadBuilder`.
- `ThreadBuilder` (interface 2): `setStartingNode(const char*)`, `noAutoMode`, `noPlayerControl`, `noUndressing`, `noFurniture`, `setDuration` (milliseconds), `start`.
- `Thread`: `isPlayerThread`, `getActorCount`, `getActor`, `getCurrentNode`. `ThreadActor::getGameActor`.
- `Node`: `getNodeID`, `hasTag`, `getActorCount`, `getActor`, `getActionCount`, `getAction`. `NodeActor`: `getTagCount`, `getTag`. `NodeActorTag::getTagID`. `Action::getType`. `ActionType::getActionID`.

All of these exist since interface 1 (OStim 7.3.3), apart from the thread builder (interface 2, OStim 7.4).

## OStim's Papyrus functions

The bridge calls two of OStim's Papyrus functions by name, through the Papyrus VM (`DispatchStaticCall`), only when OStim was found, with no compile-time reference and no script of ours that names them. The signatures below are read from OStim 7.5.2's `OLibrary.psc` and `OThread.psc` at tag 7.5.2.

- `OLibrary.GetRandomSceneSuperloadCSV`, to find a scene that fits the kind of moment asked for. It returns a scene ID, or `""` when nothing fits:

  ```papyrus
  string Function GetRandomSceneSuperloadCSV(Actor[] Actors, string FurnitureType = "", string AnySceneTag = "", string AllSceneTags = "", string SceneTagWhitelist = "", string SceneTagBlacklist = "", string AnyActorTagForAny = "", string AnyActorTagForAll = "", string AllActorTagsForAny = "", string AllActorTagsForAll = "", string ActorTagWhitelistForAny = "", string ActorTagWhitelistForAll = "", string ActorTagBlacklistForAny = "", string ActorTagBlacklistForAll = "", string AnyActionType = "", string AnyActionActor = "", string AnyActionTarget = "", string AnyActionPerformer = "", string AnyActionMatesAny = "", string AnyActionMatesAll = "", string AnyActionParticipantAny = "", string AnyActionParticipantAll = "", string AllActionTypes = "", string AllActionActors = "", string AllActionTargets = "", string AllActionPerformers = "", string AllActionMatesAny = "", string AllActionMatesAll = "", string AllActionParticipantsAny = "", string AllActionParticipantsAll = "", string ActionWhitelistTypes = "", string ActionWhitelistActors = "", string ActionWhitelistTargets = "", string ActionWhitelistPerformers = "", string ActionWhitelistMatesAny = "", string ActionWhitelistMatesAll = "", string ActionWhitelistParticipantsAny = "", string ActionWhitelistParticipantsAll = "", string ActionBlacklistTypes = "", string ActionBlacklistActors = "", string ActionBlacklistTargets = "", string ActionBlacklistPerformers = "", string ActionBlacklistMatesAny = "", string ActionBlacklistMatesAll= "", string ActionBlacklistParticipantsAny = "", string ActionBlacklistParticipantsAll = "") Global Native
  ```

  That is 46 arguments: the actor array (index 0), then 45 strings. Counting from 0, `FurnitureType` is 1, `AnyActionType` is 14 and `ActionWhitelistTypes` is 30. A dispatch through the VM fills no defaults, so all 46 are packed: the player and the follower (in that order), the kind's action list at 14 and 30, every other string empty.
- `OThread.Stop`, to stop the player's scene (thread 0) when it is not the one asked for, or runs past its time:

  ```papyrus
  Function Stop(int ThreadID) Global Native
  ```

## What is not used, and why

- The `Safe` builder methods (`setStartingNodeSafe`, `addStartingNodeSafe`): they arrived with interface 0x07050010, and the one that takes a scene ID dereferences null for an unknown ID.
- `requestInterfaceMap` and the inline helper in `InterfaceMap.h` that looks it up: the export only exists from OStim 7.5.1c. The SKSE message works on every release with the interface.
- The furniture overloads `FurnitureType::isChildOf` and `FurnitureInterface::getType`: their place in the virtual table may have moved at 7.5.1, so calling them on an older OStim could call the wrong function. `FurnitureType.h` is copied only because `Thread.h` and `Node.h` include it.
- `GraphInterface.h`, the `Furniture`, `Settings` and `SexToys` interfaces: not needed.
