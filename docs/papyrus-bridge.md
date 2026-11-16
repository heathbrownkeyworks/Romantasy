# Romantasy Papyrus bridge

New to Romantasy? Start with the [author guide](AuthorGuide.md).

The native Papyrus API supports follower scripts, scenes, and dialogue fragments.
The complete declarations are in [Romantasy.psc](../Source/Scripts/Romantasy.psc).
The API capability version is **9**; it is independent of release version 4.0.1.

## API versions

`GetApiVersion()` says which functions and events a Romantasy install has. Check it
before you call anything newer than your minimum.

| Version | Release | What it added |
|---|---|---|
| 4 | 1.1 to 2.0.0 | `GetApiVersion` and the preference functions: `ClearPreferences`, `SetPreference`, `GetPreference`, `SetPreferencesManual`, `IsPreferencesManual`. `ModifyPoints`, `ApplyPreference`, `GetPoints`, `GetLevel` and `GetLevelName` are older. |
| 6 | 2.0.1 to 3.0.1 | `RegisterAuthorFollower` and `RegisterAuthorFollowerPointsMirror`. |
| 7 | 4.0.0 | `GetMood`, `GetMoodName`, the `Romantasy_Mood` dialogue condition and the events `Romantasy_OnLevelLost`, `Romantasy_OnTimeApart` and `Romantasy_OnReunion`. |
| 8 | 4.0.0 | The author test functions (`DebugDeed` to `DebugDescribe`). |
| 9 | 4.0.0 | `GetMomentCount`, `GetDaysSinceMoment`, `TryShareMoment`, `CanShareMoment`, `DebugSharedMoment` and the events `Romantasy_OnSharedMoment` and `Romantasy_OnMomentRefused`. |

Versions 7 and 8 were never released on their own: 4.0.0 reports 9 and has all of
the above. 3.0.1 reports 6.

## Points and preferences

Use `ModifyPoints(actor, delta, reason, showLevelUp)` for an authored point change.
`reason` is the label Recent changes and the journal show; it defaults to
`Authored event` when empty. A delta of 0 returns True and changes nothing, even for
an actor with no bond.
Use `ApplyPreference(actor, statistic, count, showLevelUp)` to apply that companion's
opinion and Romantasy's statistic weight. `count` must be 1 or more. Both require an
actively following, managed actor and return False when the adjustment cannot be
applied. `ApplyPreference` also returns False when the companion has no opinion
about that statistic. Under Slow burn, a gain that a gate holds in full also returns
False and changes nothing; this does not mean the actor is not a bond. Neither
function sends `Romantasy_OnPreference`; a tier they cross sends the tier events.

```papyrus
Romantasy.ModifyPoints(akSpeaker, -10, "Broke a promise")
Romantasy.ApplyPreference(akSpeaker, "Necks Bitten", 1)
```

Statistic names accept supported labels or stable identifiers such as
`ROM_NecksBitten`. Those identifiers name preferences; new TOML integrations do
not require matching faction memberships. An unset preference produces no change.

| Function | Behavior |
|---|---|
| `GetApiVersion()` | Returns 9, the current bridge capability version. |
| `GetPoints(actor)` | Current points; 0 for an unmanaged actor. |
| `GetLevel(actor)` | 1-6 for Stranger through Spouse; 0 when unmanaged. |
| `GetLevelName(actor)` | Display name of the current relationship tier; empty when unmanaged. |
| `GetMood(actor)` | 1 Strained, 2 Settled, 3 Warm; 0 when unmanaged. |
| `GetMoodName(actor)` | "Strained", "Settled" or "Warm"; empty when unmanaged. |
| `GetPreference(actor, stat)` | -1 dislike, 0 neutral/unset, 1 like; invalid input returns 0. |
| `ClearPreferences(actor)` | Clears writable preferences; supplied/protected profiles reject the call. |
| `SetPreference(actor, stat, direction)` | Positive is like, negative is dislike, 0 removes that opinion. |
| `IsPreferencesManual(actor)` | True for player-created personalities or a managed external coordination flag. |
| `SetPreferencesManual(actor, manual)` | Coordinates external editors; cannot release a player-created personality or claim a protected one. |
| `GetMomentCount(actor)` | How many shared moments the bond has had; 0 for an unmanaged actor. |
| `GetDaysSinceMoment(actor)` | In-game days since the last shared moment; -1.0 when there has been none or the actor is unmanaged. |
| `TryShareMoment(actor, kind = "")` | Starts a shared moment: `"affection"`, `"closeness"`, `"intimacy"` or `""` for the deepest kind the bond welcomes. Returns a code from 0 (accepted) to 5. Needs OStim 7.4 or newer. |
| `CanShareMoment(actor, kind = "")` | The same answer, starting nothing. |

The result codes are 0 accepted, 1 not available (no OStim, one too old, or an unknown kind),
2 not a bond, 3 too soon, 4 strained and 5 not now. Code 6, could not start, comes later as an
event. Following is not required. Codes 3 and 4 also whisper `Not yet` or `Not in the mood`.
The full table, the `[moments]` profile table and a worked example are in
[OStim.md](OStim.md).

A favorite or pet peeve (see [FileProfiles.md](FileProfiles.md)) counts double.
`GetPreference` still returns -1, 0 or 1: a favorite reads as 1 and a pet peeve as
-1. On a player-made bond, `SetPreference` replaces whatever the deed had: a
positive direction makes it a plain like, a negative one a plain dislike, and 0
clears it. A favorite or pet peeve on that deed is lost. Other deeds keep theirs.
`Romantasy_OnPreference` reports the doubled points for a deed.

Use GetLevel to distinguish an unmanaged actor from a managed actor with zero
points. Dialogue conditions instead use `Romantasy_Registered` as their guard.

## Author test functions

Seven functions help a follower author test a personality file. They work only when
`"authorMode": true` is set by hand in `settings.json`. With author mode off, each
one logs one line (once per function in a session) and returns False, or an empty
string. See [AuthorTesting.md](AuthorTesting.md) for console examples and the
"why" log.

| Function | Behavior |
|---|---|
| `DebugDeed(actor, statistic, count = 1)` | Runs one built-in deed as a player's deed would: the follower's feeling, Relationship pace and the Slow burn gate all apply. The follower need not be following. Returns True only when the total really moved. `count` must be 1 or more. |
| `DebugSetPoints(actor, points)` | Sets the total (0 or more). Not paced and not gated, so it gets past a gate. A tier change shows the popup when that popup is on and sends the events; it raises no whisper. Mood moves as for any change. Returns True when set. |
| `DebugSetDaysTogether(actor, days)` | Sets days together, kept between 0 and 100000. Returns True when set. |
| `DebugSetMood(actor, score)` | Sets the mood score, kept between -100 and 100. Returns True when set. |
| `DebugAnnounceNow()` | Announces every waiting tier change now, as a calm moment would. Returns False while some still wait (in combat). |
| `DebugDescribe(actor)` | Returns one bounded line (points, tier, mood, days together, gate, personality source, likes, dislikes, favorites, pet peeves, moments) and logs it. Empty for an actor with no bond. |
| `DebugSharedMoment(actor, kind, seconds = 60.0, others = False, force = False)` | Judges a shared moment as the end of a scene would, with or without OStim. Returns True when points moved. A moment that counts sends `Romantasy_OnSharedMoment`, as a real one does. |

Each function that takes an actor returns False, or an empty string, for no actor or
an actor with no bond.

```papyrus
Romantasy.DebugDeed(akSpeaker, "Quests Completed", 1)
Romantasy.DebugSetPoints(akSpeaker, 1000)
Debug.Trace(Romantasy.DebugDescribe(akSpeaker))
```

## Mood

Each bond has a short-term mood: Strained, Settled or Warm. It is a feeling, not a
score you can spend. It never changes points or tiers.

- A gain warms it: 4 for each point, at most 20 for one change.
- A loss strains it: 6 for each point, at most 30 for one change.
- A reunion warms it by 15. Losing a tier strains it by a further 20.
- It drifts back toward Settled by 25 for each in-game day.
- Warm starts at 25 and Strained at -25, on a scale of -100 to 100.

Mood reacts to the change that was asked for, even when the stored points could not
move, for example a loss at 0 points. A bond that has never followed the player has no
mood record and reads as Settled. A follower whose profile is turned off has no mood
and sends no events. Mood is kept in the save, and so are the days a follower has
spent with the player. Both stay in the save while the follower's plugin is not
loaded, and come back with it.

Time values read from a save that lie in the future or below zero are set to the
current game day. Loading an older save does not send a false time apart or reunion.

## When tier events arrive

With the player's setting "Tier changes arrive" on `At a calm moment` (the default),
`Romantasy_OnLevelChanged` and `Romantasy_OnLevelLost` arrive with the tier popup.
That can be up to one in-game day after the tier itself changed. The popup and the
two events wait until the player is out of combat and in an inn, a home, a town or
another settlement, or has just woken or finished waiting; after a day they arrive at
the next moment out of combat.

- Several changes for one bond while waiting arrive as one event, with the level the
  bond holds when it is announced. If the bond is back at the level it started from,
  no event is sent.
- Points, `GetLevel`, `GetLevelName`, dialogue conditions and mood change at once.
  `GetLevel` always answers the current tier, whether or not the event has arrived.
- A call that passes `showLevelUp` as False never shows a popup and never sends these
  events, as before. It can change or cancel an event that is still waiting, because
  the event names the level the bond holds when it is sent.
- Waiting changes are kept in the save.
- While the player is in an OStim scene and **Quiet during scenes** is on, a waiting
  change is not announced until the scene ends.
- With `At once`, both events are sent at once, in or out of combat, as in earlier versions. Only the popup waits for combat to end.

What a script can rely on: after `ModifyPoints` returns True, `GetPoints`, `GetLevel`
and the dialogue conditions already show the new tier. What it cannot rely on: that
`Romantasy_OnLevelChanged` or `Romantasy_OnLevelLost` arrives in the same frame, the
same hour, or at all if the bond returns to its old tier first. Voice a tier reaction
from the event, not from the call that changed the points.

`ModifyPoints` and `ApplyPreference` amounts are never changed by Relationship pace.
Under Slow burn a gate can hold a gain at the top of the tier below, so the result
can be smaller than the amount asked for. For a deed the player does, Relationship
pace does apply, and `Romantasy_OnPreference` reports what was awarded. When a Slow
burn gate holds a deed's whole gain, the event is still sent so the follower can
react, and its number is what the deed was worth; no points are added.

## Ownership and persistence

- **Supplied TOML profiles** in Profiles are protected. Edit those files externally
  and restart Skyrim. Do not register the same NPC through the legacy author API.
- **Player-created profiles** are editable through the ledger or the public
  preference write APIs. The save holds the bond; a copy goes to PlayerProfiles.
  Successful changes update the live preferences immediately and refresh an open
  ledger. A failed file write leaves the previous file intact.
- **Legacy author-defined profiles** from base records or optional registration
  remain protected.
- **Legacy externally managed profiles** retain the faction-backed preference
  and coordination behavior for existing integrations.

Player-created bonds and earned points live per actor reference in each save's SKSE
cosave. The PlayerProfiles files are copies, read for new characters and for saves
made before 4.0.0. Multiple instances of a generic NPC share profile preferences but
retain separate balances. Player-created profiles always report manual ownership;
supplied profiles do not.

When a follower's plugin is missing from the load order, Romantasy keeps that bond's
points, mood, days together and journal in the save and writes them back on every
save. They return with the plugin. While the plugin is missing, the follower has no
actor, so the functions above have nothing to answer for it.

For writable profiles, clear-then-set applies each call individually, not as one
transaction. The ledger's **Seal personality** action saves the complete selection
in one operation. Never bypass a rejected write by modifying factions directly.

## Dialogue conditions

Use the native reserved `GetGraphVariableInt` queries on Subject:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

This gates a response at Friend or higher. For exact thresholds use
`Romantasy_Points`. See the [dialogue guide](romance-points-dialogue-conditions.md)
for grouping, sentinel values, and conversion from zero-based faction ranks.
These conditions need no dialogue fragment or follower-owned global.

The legacy `ROM_RomancePoints` GetFactionRank proxy remains available to existing
mods. It returns the subject's points, or -1 when unmanaged, without storing real
faction ranks. New TOML integrations should use the reserved queries above.

## Legacy optional author registration

`RegisterAuthorFollower(actor, stats, directions, startingLevel = 1, rankMirror = None)`
retains the API 6 faction-based registration workflow. It needs the legacy
Romantasy factions and accepts parallel arrays with directions exactly -1 or 1.
Starting level is 1-6. Validation rejects invalid/duplicate statistics or a
conflicting TOML definition before changing the profile. Existing points are
preserved; an untracked actor starts at the chosen tier's minimum.

The optional rank global receives the old 0-5 rank. API 6 also provides
`RegisterAuthorFollowerPointsMirror(actor, pointsMirror)` for an exact-point
global; register it after successful author registration.

The preference write functions (`SetPreference`, `ClearPreferences`,
`SetPreferencesManual`) refuse a follower whose personality comes from a supplied
TOML file or from author registration, and return False.

Re-register these optional integrations and mirrors after each load, usually
from a player reference-alias script's OnInit and OnPlayerLoadGame events. Legacy
registration is not needed for a TOML profile and must not be combined with one
for the same NPC.

## Voiced reactions

Romantasy delivers ModEvents on the game thread:

Every handler has the signature
`Event Name(String asEventName, String asStrArg, Float afNumArg, Form akSender)`.

- `Romantasy_OnLevelChanged`: emitted on an upward tier change when level-change
  notifications are requested, at the time described in "When tier events arrive";
  strArg is empty, numArg is the new 1-based level, sender is the
  follower's NPC base. It still fires only for a gain.
- `Romantasy_OnPreference`: emitted when a deed the follower has an opinion about
  moves points without crossing a tier (a tracked game statistic, `DebugDeed`, or the
  ledger's deed developer tool); strArg is the statistic's name, numArg is the signed
  adjustment, and sender is the NPC base. The name comes from `CS_Romantasy.esp`, so
  a translation can change it. Pass it to `GetPreference`; do not compare it with
  English text. A deed that crosses a tier sends a tier
  event instead. `ModifyPoints` and `ApplyPreference` do not send it.
- `Romantasy_OnLevelLost`: emitted when a bond drops one or more tiers and
  level-change notifications are requested, the same rule as
  `Romantasy_OnLevelChanged`. It is sent even when the player has turned loss popups
  off. strArg is the new tier's name, numArg is the new 1-based level, sender is the
  NPC base. The developer tools apply a change to every bond, so they send it once for
  each follower.
- `Romantasy_OnTimeApart`: emitted once when a bonded follower has been away for 3
  in-game days; strArg is empty, numArg is the days apart, 3 or more, as counted
  when Romantasy next looks. Sender is the NPC base. It is not sent again for the
  same absence, including after a save and load.
- `Romantasy_OnReunion`: emitted when a follower who was away for at least 1
  in-game day follows again; strArg is empty, numArg is the days apart, sender is
  the NPC base. A short wait, a cell load or a horse ride is not an absence.
- `Romantasy_OnSharedMoment`: emitted when a shared moment with a bond counts (an
  OStim scene, or `DebugSharedMoment`); strArg is the kind (`affection`, `closeness`
  or `intimacy`), numArg is the points applied, 0 when none were, sender is the NPC
  base.
- `Romantasy_OnMomentRefused`: emitted when `TryShareMoment` answers 3 or 4, and
  later with 6 when an accepted moment could not begin (not sent when the follower is
  no longer loaded by then); strArg is the kind asked for (`""` when none was asked
  and no kind is welcome yet), numArg is the code, sender is the NPC base.

"Away" starts when Romantasy sees a bonded follower stop following and ends when it
sees them following again. A follower that Romantasy has never seen following is not
away and gets no events until they first follow. Romantasy looks when dialogue
closes, when the wait or sleep menu opens, when the game is saved or loaded, when a
tracked statistic changes, and when the ledger opens. It adds no timer. Time apart and reunion cost no points.

Filter events to the intended sender. Re-register listeners after game load: a
registration does not survive loading a save.
Followers supply their own topics, voice assets, cooldowns, and story gates.
For Say() reactions, verify that the topic subtype and conditions can resolve an
INFO: use an Idle-subtype topic, because a custom-subtype topic with no dialogue
branch is silent. Do not gate romance dialogue or reactions on Horde being absent.

## Reaction script example

A quest script that listens for the two events a follower is most likely to voice.
Register again after every load, and ignore events for other followers:

```papyrus
Scriptname Example_ReactionScript extends Quest

Actor Property FollowerActor Auto
Topic Property ReunionTopic Auto
Topic Property LostTierTopic Auto

Event OnInit()
    RegisterEvents()
EndEvent

; Call this from a player alias script's OnPlayerLoadGame as well:
; (GetOwningQuest() as Example_ReactionScript).RegisterEvents()

Function RegisterEvents()
    RegisterForModEvent("Romantasy_OnReunion", "OnRomantasyReunion")
    RegisterForModEvent("Romantasy_OnLevelLost", "OnRomantasyLevelLost")
EndFunction

Event OnRomantasyReunion(String asEventName, String asStrArg, Float afNumArg, Form akSender)
    If FollowerActor == None || ReunionTopic == None
        Return
    EndIf
    If akSender != FollowerActor.GetActorBase()
        Return
    EndIf
    FollowerActor.Say(ReunionTopic)
EndEvent

Event OnRomantasyLevelLost(String asEventName, String asStrArg, Float afNumArg, Form akSender)
    If FollowerActor == None || LostTierTopic == None
        Return
    EndIf
    If akSender != FollowerActor.GetActorBase()
        Return
    EndIf
    FollowerActor.Say(LostTierTopic)
EndEvent
```

To vary a line by mood, call `Romantasy.GetMood(FollowerActor)` before `Say`.

## SkyrimNet script

`Romantasy_SkyrimNet` is a second script for the optional SkyrimNet bridge. Its
functions are registered with SkyrimNet by Romantasy itself; a follower mod has no
need to call them. They never call SkyrimNet, so the script loads without it. See
[SkyrimNet.md](SkyrimNet.md).

| Function | Behavior |
|---|---|
| `GetBondLine(actor)` | The bond line SkyrimNet receives for that actor, or `""` when there is nothing to say or the bridge is off. |
| `BondDecorator(actor)` | Registered with SkyrimNet as the decorator `romantasy_bond`; returns `GetBondLine`. |
| `ConversationEligible(actor, touched)` | Whether SkyrimNet may offer `RomantasyTouched` (touched True) or `RomantasyHurt` now. Changes nothing. |
| `ConversationNudge(actor, touched)` | Moves the bond by a few capped points for one of the two actions. True when it applied. |
| `TouchedEligible`, `TouchedExecute`, `HurtEligible`, `HurtExecute` | `(actor, contextJson, paramsJson)`: the eligibility and execution functions SkyrimNet calls for the two actions. The two JSON arguments are never read. |

These functions are not counted in `GetApiVersion()`.

## TopicInfo fragment example

Keep the Creation Kit fragment markers when authoring or recompiling fragments:

```papyrus
;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname Example_TIF__05XXXXXX Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
Romantasy.ApplyPreference(akSpeaker, "Necks Bitten", 1)
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment
```
