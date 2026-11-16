# Romantasy and OStim

Romantasy can work with OStim Standalone when OStim is installed. Without OStim
nothing changes: Romantasy writes one line to its log (`OStim not found; the bridge is off.`)
and the rest of this guide does not apply.

The first part is for players. The second part is for follower authors.

# For players

## What it does

- **It keeps quiet during a scene.** While you are in an OStim scene, Romantasy holds
  tier popups and whispers. They appear after the scene ends. A whisper that waited
  more than 30 seconds is dropped, as whispers always are.
- **A shared moment counts toward a bond.** When a scene with one companion ends,
  Romantasy decides whether it was a shared moment. If it was, the bond gains a few
  points, at most once a day. The journal and Recent changes name it by the kind of
  moment, never by what happened in it.
- **A companion can start one.** A follower mod can ask Romantasy to start a moment
  that suits the bond. Romantasy then asks OStim for a short, gentle scene. This
  needs a follower mod that makes the request. Romantasy has no button for it.

## What counts

A scene counts as a shared moment when all six of these are true. They are checked in
this order, and the first one that fails is the reason it did not count.

1. **Shared moments** is on in Settings.
2. Exactly two people were in it: you and one companion who has a bond that is switched on.
3. Nothing in it was marked as force. Romantasy looks for the tags OStim scenes carry
   for a dominant, aggressor, victim or submissive position, and for a scene marked aggressive.
4. Something actually happened in it. A scene of nothing but standing about has no kind.
5. It ran at least 20 seconds.
6. The bond is at or above the tier where the companion welcomes that kind of moment.

There are three kinds, named by the deepest thing seen in the scene:

| Kind | What the scene held | Label in your journal | Welcome from (default) |
|---|---|---|---|
| Affection | Only gentle gestures: holding hands, hugging, a kiss on the cheek or hand, patting or stroking the head, dancing | A tender moment | Friend |
| Closeness | Those, and kissing, cuddling, spooning, holding or a massage | A quiet hour together | Confidant |
| Intimacy | Anything else | Time alone together | Lover |

A companion may welcome a kind later, or never. See the profile table below.

## What never counts

- A scene with a third person in it, or with two bonded companions.
- A scene with a bond that is switched off.
- A scene marked as force.
- A scene shorter than 20 seconds.
- A kind of scene the companion does not yet welcome at the bond's tier.
- A scene you left by loading a save, or one OStim dropped without ending it.
- A gentle scene that Romantasy started and then had to stop because it turned into
  something else.

## Once a day

One counted moment a day gives points: 5, or 10 when the companion treasures moments.
The day is the in-game day. A second moment the same day still counts as a moment,
and the bond remembers it, but it adds no points. A companion who is reserved about
moments counts them and never gives points.

Points go through the same rules as any other gain. Under Slow burn, a gate can hold
a gain back, and the day is still used.

A moment never takes points away and never strains the mood. Its points warm the mood
like any other gain. The first moment with a companion adds a milestone to the journal, **A first
moment together**.

## The two switches

Both are in Settings and appear only when OStim was found.

- **Shared moments** (on by default): "Time alone with a companion in an OStim scene
  adds a little to the bond, once a day." Off means no scene counts. It does not stop a
  follower mod from starting a moment.
- **Quiet during scenes** (on by default): "Tier changes and whispers wait until an
  OStim scene is over." Off means they show as they would at any other time.

The same keys are in `settings.json`: `ostimMoments` and `ostimHold`.

Health in Settings shows one note: `OStim found: shared moments count toward a bond.`,
`OStim found: shared moments are not counted.`, or, when OStim is loaded and Romantasy
got no answer from it, `OStim is installed, but Romantasy could not reach it.`

## What you need

- OStim 7.3.3 or newer for Romantasy to hold popups and count moments.
- OStim 7.4 or newer for a follower mod to start a moment.
- OStim 7.5.2 or newer is the safest choice. Romantasy asks it directly. An older OStim
  is asked through SKSE, and that can fail on Skyrim 1.5.97 or when another SKSE plugin
  failed to load. If Health says Romantasy could not reach OStim, update OStim.

## The honest limit

OStim has no flag for consent. Romantasy cannot know whether a scene was wanted. It
only sees the tags that the scene's author set: a position tag such as victim or
aggressor, or a scene tag of aggressive. A scene from a pack that does not set those
tags looks gentle to Romantasy even if it is not. If you use a pack like that, turn
**Shared moments** off.

OStim's own settings and add-ons still decide everything that happens inside a scene.
A mod that starts its own OStim scenes needs no change: when such a scene ends, it is
judged by the same six rules.

# For authors

## The `[moments]` table

A profile can say how a companion feels about moments. The table needs
`schema_version = 3`. Every key is optional.

```toml
schema_version = 3

[npc]
plugin = "ExampleFollower.esp"
form_id = "0x000800"

[romance]
likes = ["Quests Completed"]

[moments]
affection = "Friend"      # a tier name, or "never"
closeness = "Confidant"
intimacy  = "Lover"
feeling   = "warm"        # "warm", "treasured" or "reserved"
```

- `affection`, `closeness` and `intimacy` are the lowest tier at which that kind is
  welcome. Use one of Stranger, Acquaintance, Friend, Confidant, Lover or Spouse, or
  `"never"`. The defaults are Friend, Confidant and Lover.
- `feeling` is `"warm"` (5 points, the default), `"treasured"` (10 points) or
  `"reserved"` (the moment counts and gives no points).
- A table in a version 1 or 2 file is an unknown field and the file is refused.
- A player-made profile may not have `[moments]`. Player-made bonds use the defaults.
- The parser does not require affection to come before closeness, or closeness before
  intimacy. `romantasy-profile-check` warns when they are out of order, for example
  `moments: closeness is welcome before affection`.
- A misspelt key or tier is an error that names it: `unknown field 'afection'`, or
  `unknown tier 'Freind' in [moments]`.

## Four functions

All four are global functions on the script `Romantasy` (API version 9). None of them
names an OStim script, and your mod needs no OStim master.

```papyrus
Int Function TryShareMoment(Actor akFollower, String asKind = "") Native Global
Int Function CanShareMoment(Actor akFollower, String asKind = "") Native Global
Int Function GetMomentCount(Actor akFollower) Native Global
Float Function GetDaysSinceMoment(Actor akFollower) Native Global
```

`asKind` is `"affection"`, `"closeness"`, `"intimacy"`, or `""` for the deepest kind
the bond welcomes now. `TryShareMoment` starts a moment and `CanShareMoment` gives the
same answer without starting anything. Following is not required: a spouse at home
can be asked. Both return one of these codes:

| Code | Meaning |
|---|---|
| 0 | Accepted: Romantasy is starting it |
| 1 | Not available: OStim is absent, too old to start scenes, or the kind is unknown |
| 2 | Not a bond: no bond with this actor, or it is switched off |
| 3 | Too soon: the bond's tier is below the kind's welcome tier, or the kind is never welcome |
| 4 | Strained: the follower's mood is Strained |
| 5 | Not now: the player is in combat or in a scene, or a start is already waiting |
| 6 | Could not start: no fitting scene, or OStim refused (sent only as an event) |

`GetMomentCount` returns how many moments the bond has had, and 0 for an actor who is
not a bond. `GetDaysSinceMoment` returns the in-game days since the last one, and
-1.0 when there has been none. Both work without OStim.

On a refusal of 3 or 4, `TryShareMoment` also whispers under the follower's name,
`Not yet` for 3 and `Not in the mood` for 4. It sends no whisper for 1, 2 or 5.

### What happens after code 0

A gentle moment (`"affection"` or `"closeness"`) is a short scene that the player
watches. Romantasy asks OStim for a scene whose actions all belong to the kind, then
starts it for 30 seconds with no player control, no undressing and no furniture. If
OStim has no fitting scene, or does not answer within 5 seconds, the result is code 6.
If the scene turns into anything outside the kind, Romantasy stops it and sends code 6.
A scene that is still running after 40 seconds is stopped. OStim's source applies the
30 second length to the player's scene too. The stop is a fallback in case a scene does
not end by itself.

An `"intimacy"` moment is started with OStim's own defaults and the player keeps
control. OStim chooses the scene.

A start that sees no scene within 60 seconds is forgotten. OStim waits for a dialogue
to close before it starts the player's scene, so time in dialogue does not count. Gentle scenes that Romantasy
had to stop because they left their kind count for nothing.

## Two events

Both are delivered on the game thread, with the follower's NPC base as the sender.
Filter on the sender, and register again after every load.

| Event | strArg | numArg |
|---|---|---|
| `Romantasy_OnSharedMoment` | The kind: `affection`, `closeness` or `intimacy` | The points applied. 0 when none were |
| `Romantasy_OnMomentRefused` | The kind asked for, or the deepest kind welcome when none was asked. `""` when none was asked and no kind is welcome yet | The code: 3, 4 or 6 |

`Romantasy_OnSharedMoment` is sent for every counted moment, including the second one
in a day and one that a gate or a reserved companion left with no points.
`Romantasy_OnMomentRefused` with code 6 is not sent when the follower is no longer
loaded by the time the start fails.

## Test it without OStim

With author mode on (see [AuthorTesting.md](AuthorTesting.md)):

```papyrus
Bool Function DebugSharedMoment(Actor akFollower, String asKind, Float afSeconds = 60.0, Bool abOthers = False, Bool abForce = False) Native Global
```

It judges a moment as the end of a scene would, with the same rules and the same
award, whether or not OStim is installed. `afSeconds` is how long the scene ran,
`abOthers` adds a third person and `abForce` marks it as force. It returns True when
points moved. A moment it counts sends `Romantasy_OnSharedMoment`, so your reaction
script can be tested too. With Detailed log on, the why line says what happened or why
nothing did. It does not test `TryShareMoment`, which needs OStim 7.4 or newer and
returns 1 without it.

`DebugDescribe` shows the bond's moments: the count, the last day, the three welcome
tiers and the feeling.

## A recipe

Gate a line on the bond, and let its fragment ask for a moment. The line's conditions:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

The fragment starts the moment and, on a refusal, says a line instead. End the
conversation on this line: the moment begins once the dialogue closes.

```papyrus
;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname Example_TIF__05XXXXXX Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
Int result = Romantasy.TryShareMoment(akSpeaker, "affection")
If result == 3 || result == 4
    akSpeaker.Say(RefusalTopic)
EndIf
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment

Topic Property RefusalTopic Auto
```

A reaction script can voice the later results. Code 6 arrives only as an event:

```papyrus
Scriptname Example_MomentReactions extends Quest

Actor Property FollowerActor Auto
Topic Property CouldNotTopic Auto
Topic Property GladTopic Auto

Event OnInit()
    RegisterEvents()
EndEvent

Function RegisterEvents()
    RegisterForModEvent("Romantasy_OnMomentRefused", "OnRomantasyMomentRefused")
    RegisterForModEvent("Romantasy_OnSharedMoment", "OnRomantasySharedMoment")
EndFunction

Event OnRomantasyMomentRefused(String asEventName, String asStrArg, Float afNumArg, Form akSender)
    If FollowerActor == None || akSender != FollowerActor.GetActorBase()
        Return
    EndIf
    If afNumArg as Int == 6
        FollowerActor.Say(CouldNotTopic)
    EndIf
EndEvent

Event OnRomantasySharedMoment(String asEventName, String asStrArg, Float afNumArg, Form akSender)
    If FollowerActor == None || akSender != FollowerActor.GetActorBase()
        Return
    EndIf
    FollowerActor.Say(GladTopic)
EndEvent
```

Call `RegisterEvents()` again after every load, for example from a player alias
script's `OnPlayerLoadGame`. As with any `Say()`, check that the topic's subtype and
conditions can resolve a line: use an Idle-subtype topic, because a custom-subtype
topic with no dialogue branch is silent.
