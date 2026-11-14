# Romantasy author guide

This guide is for people who have made a Skyrim follower and want it to work well
with Romantasy 4.0.0. It is the map. The other documents in this folder are the
reference; each section here links to the part you need.

The examples use a made-up follower, Mirelle, from `Mirelle.esp`, whose scripts use
the prefix `MIR_`.

## 1. What Romantasy gives your follower

Romantasy keeps a bond between the player and each companion. Deeds the player does
(quests, dungeons, crafting, crimes and many more) add or take points. Points make
six tiers: Stranger, Acquaintance, Friend, Confidant, Lover and Spouse. Each bond
also has a mood (Strained, Settled or Warm), days together, and a journal. The player
sees it all in the ledger.

There are three levels of integration. Each one builds on the one before.

1. **Do nothing.** The player can make a bond with your follower in the ledger with
   **New bond** and choose what they like and dislike. Your mod needs no change.
2. **Ship a personality file.** One small TOML file says what your follower likes,
   dislikes, treasures and cannot stand, and what each tier means for them. No
   plugin change, no script and no master is needed.
3. **Add scripts and dialogue that react.** Your follower speaks when a tier is
   reached or lost, greets the player after time apart, unlocks lines by tier and
   reacts to deeds.

## 2. Quick start

The smallest personality file that works:

```toml
schema_version = 3

[npc]
plugin = "Mirelle.esp"
form_id = "0x000800"

[romance]
likes = ["Dungeons Cleared"]
```

- `form_id` is the plugin-local ID of the follower's **NPC base record**, not her
  placed reference and not the load-order ID. `05000800` in the game becomes
  `0x000800`.
- Put the file in your mod at `SKSE/Plugins/Romantasy/Profiles/Mirelle.toml`. Any
  file name ending in `.toml` works. One file per NPC.
- Romantasy reads profiles once, when the game loads its data. Restart Skyrim after
  each change.

Check it with the profile checker, which is in the Romantasy mod's `tools` folder:

```powershell
.\tools\romantasy-profile-check.exe --strict 'path\to\your mod\SKSE\Plugins\Romantasy\Profiles'
```

Exit code 0 means clean. Every problem is one line with the file, the line number
when known, and the reason. The checker does not open your plugin, so it cannot tell whether the NPC
exists; Romantasy checks that when Skyrim loads.

See it in the game:

- `Romantasy.log` (in the SKSE log folder) has a line
  `Romantasy loaded profile ... for <FormID>`, or the reason the file was refused.
  The ledger's Health section also lists refused files.
- Recruit your follower and open the ledger (Left Ctrl + R by default). Her page
  shows **Author-defined personality**, sealed by the follower author.
- Points from deeds are earned only while she is following.

Reference: [FileProfiles.md](FileProfiles.md).

## 3. What is new for authors in 4.0.0

- **Favorites and pet peeves.** Up to three of each in `[romance]`. A favorite counts
  double; a pet peeve costs double. [FileProfiles.md](FileProfiles.md#favorites-pet-peeves-and-unlock-lines)
- **Tier unlock lines.** `[unlocks]` gives 1 to 4 short lines per tier. The ledger
  shows the next tier's lines and the tier popup lists them. They are text only.
  [FileProfiles.md](FileProfiles.md#favorites-pet-peeves-and-unlock-lines)
- **The `[moments]` table.** With `schema_version = 3`, say from which tier each kind
  of shared moment is welcome, and how your follower feels about them.
  [OStim.md](OStim.md#the-moments-table)
- **Mood.** Strained, Settled or Warm, read with `GetMood`, `GetMoodName` or the
  `Romantasy_Mood` dialogue condition. It never changes points.
  [papyrus-bridge.md](papyrus-bridge.md#mood)
- **New events.** `Romantasy_OnLevelLost` (tier lost), `Romantasy_OnTimeApart`
  (3 days away), `Romantasy_OnReunion` (back after a day or more),
  `Romantasy_OnSharedMoment` and `Romantasy_OnMomentRefused`.
  [papyrus-bridge.md](papyrus-bridge.md#voiced-reactions)
- **New getters.** `GetMood`, `GetMoodName`, `GetMomentCount`, `GetDaysSinceMoment`,
  and `CanShareMoment` and `TryShareMoment` for moments.
  [papyrus-bridge.md](papyrus-bridge.md#points-and-preferences)
- **The bond journal.** Every change is written with its label, so the `reason` your
  script passes to `ModifyPoints` is what the player reads. Keep it short and in
  your follower's world. Removing a bond erases its journal.
- **Whispers.** Short cards on the HUD for a deed, a mood that turns, a reunion or
  time apart. A script award shows no deed whisper. Whispers never change anything.
- **Pacing.** By default a tier popup and the tier events wait for a calm moment, up
  to one in-game day. Points, `GetLevel` and dialogue conditions change at once.
  A script can trust the getters right away; it should not expect the tier event in
  the same moment, or at all if the bond goes back to its old tier first. Under the
  player's Slow burn pace, a gate can hold a gain until enough days together have
  passed. [papyrus-bridge.md](papyrus-bridge.md#when-tier-events-arrive)
- **Save safety.** When your plugin is missing from a load order, Romantasy keeps
  the bond's points, mood, days together and journal in the save, and they come
  back with the plugin. [FileProfiles.md](FileProfiles.md#persistence-and-migration-boundaries)
- **The author test kit.** With author mode on, seven functions set points, days,
  mood and deeds directly, and the why log says what each deed did.
  [AuthorTesting.md](AuthorTesting.md)
- **SkyrimNet.** When the player has SkyrimNet, Romantasy tells it about each bond
  and lets conversations move a bond a little. Your mod needs nothing for this.
  [SkyrimNet.md](SkyrimNet.md)
- **OStim.** When the player has OStim Standalone, time alone together can count as a
  shared moment, and your dialogue can ask for one. [OStim.md](OStim.md)

## 4. Recipes

### Set up once: a reaction quest

Mod events must be registered again after every load. Make one quest for your
reactions:

- A quest `MIR_RomantasyQuest`, Start Game Enabled, with the script below.
- A reference alias filled with the player (Specific Reference: PlayerRef), with the
  alias script below.
- A property for Mirelle's placed reference and one per topic.

Topics said with `Say()` must be **Idle**-subtype topics. A custom-subtype topic
with no dialogue branch is silent. Write one INFO per line under each topic.

```papyrus
Scriptname MIR_RomantasyReactions extends Quest

Actor Property MirelleRef Auto
Topic Property MIR_TierReached Auto
Topic Property MIR_TierLost Auto
Topic Property MIR_Reunion Auto
Topic Property MIR_ReunionLong Auto
Topic Property MIR_DeedReaction Auto
GlobalVariable Property MIR_MissedPlayer Auto

Event OnInit()
    RegisterEvents()
EndEvent

Function RegisterEvents()
    RegisterForModEvent("Romantasy_OnLevelChanged", "OnRomantasyLevelChanged")
    RegisterForModEvent("Romantasy_OnLevelLost", "OnRomantasyLevelLost")
    RegisterForModEvent("Romantasy_OnReunion", "OnRomantasyReunion")
    RegisterForModEvent("Romantasy_OnTimeApart", "OnRomantasyTimeApart")
    RegisterForModEvent("Romantasy_OnPreference", "OnRomantasyPreference")
EndFunction

Bool Function IsMirelle(Form akSender)
    Return MirelleRef != None && akSender != None && akSender == MirelleRef.GetActorBase()
EndFunction
```

```papyrus
Scriptname MIR_RomantasyPlayerAlias extends ReferenceAlias

Event OnPlayerLoadGame()
    MIR_RomantasyReactions reactions = GetOwningQuest() as MIR_RomantasyReactions
    If reactions != None
        reactions.RegisterEvents()
    EndIf
EndEvent
```

Every handler has the signature `(String eventName, String strArg, Float numArg,
Form sender)`. The sender is the follower's NPC base, so always check it: the event
is sent for every bond the player has.

### A voiced reaction when a tier is reached, and when one is lost

Add these to `MIR_RomantasyReactions`. `numArg` is the new tier, 1 to 6.

```papyrus
Event OnRomantasyLevelChanged(String eventName, String strArg, Float numArg, Form sender)
    If !IsMirelle(sender) || MIR_TierReached == None
        Return
    EndIf
    MirelleRef.Say(MIR_TierReached)
EndEvent

Event OnRomantasyLevelLost(String eventName, String strArg, Float numArg, Form sender)
    If !IsMirelle(sender) || MIR_TierLost == None
        Return
    EndIf
    MirelleRef.Say(MIR_TierLost)
EndEvent
```

Under `MIR_TierReached`, give each INFO a tier condition, Run On Subject:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       == 3
```

When the event arrives, the level it names is the level the bond holds, so the
condition and the event agree. `strArg` is empty for a gain and the new tier's name
(in English) for a loss. The events arrive with the tier popup, which by default waits for a
calm moment.

### A line when the player returns after time apart

`Romantasy_OnReunion` comes when Mirelle follows again after at least one in-game
day away. `numArg` is the days apart.

```papyrus
Event OnRomantasyReunion(String eventName, String strArg, Float numArg, Form sender)
    If !IsMirelle(sender)
        Return
    EndIf
    If numArg >= 7.0 && MIR_ReunionLong != None
        MirelleRef.Say(MIR_ReunionLong)
    ElseIf MIR_Reunion != None
        MirelleRef.Say(MIR_Reunion)
    EndIf
EndEvent

Event OnRomantasyTimeApart(String eventName, String strArg, Float numArg, Form sender)
    If IsMirelle(sender) && MIR_MissedPlayer != None
        MIR_MissedPlayer.SetValue(1)
    EndIf
EndEvent
```

`Romantasy_OnTimeApart` comes once, after 3 in-game days away, while she is not with
the player, so it is a poor time to speak. The script above only notes it in a
global; a dialogue line can then require `GetGlobalValue MIR_MissedPlayer == 1` and
set the global back to 0 in its fragment. "Away" starts when Romantasy sees her stop
following; a short wait or a cell load is not an absence.

### Dialogue that unlocks by tier

No script is needed. On the INFO, add these AND conditions, Run On Subject:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 4
```

That line plays from Confidant up. Use `Romantasy_Points` for an exact number and
`Romantasy_Mood == 3` for a line only while she feels warm. Always keep the
Registered guard. Type the names without quotes.
Reference: [romance-points-dialogue-conditions.md](romance-points-dialogue-conditions.md).

### A reaction to something she likes or dislikes

`Romantasy_OnPreference` comes when a deed she has an opinion about moves her points,
or would have but for a Slow burn gate, without crossing a tier. `strArg` is the
deed's name and `numArg` the signed points. The name comes from `CS_Romantasy.esp`,
so a translation can change it. Pass it to `GetPreference`; do not compare it with
English text.

```papyrus
Event OnRomantasyPreference(String eventName, String strArg, Float numArg, Form sender)
    If !IsMirelle(sender) || MIR_DeedReaction == None
        Return
    EndIf
    If numArg < 0 && Romantasy.GetPreference(MirelleRef, strArg) < 0
        MirelleRef.Say(MIR_DeedReaction)
    EndIf
EndEvent
```

Deeds can come many times a minute. Keep your own cooldown, for example a game time
stored in a global, so she does not repeat herself. A deed that crosses a tier sends
the tier event instead of this one.

### Awarding or removing points from a quest script

The same call works in a quest stage, a scene or a dialogue fragment. In a TopicInfo
fragment:

```papyrus
;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname MIR_TIF__05000D62 Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
If akSpeaker != None
    Romantasy.ModifyPoints(akSpeaker, 25, "Kept your word")
EndIf
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment
```

Use a negative number to remove points. What to expect:

- She must be following. Otherwise nothing changes and the call returns False.
- Your amount is never doubled or halved by the player's pace. Under Slow burn, a
  gate can hold a gain at the top of the tier below until enough days together have
  passed; then the call can return False although she is a bond.
- Points, `GetLevel` and the dialogue conditions change at once. The tier popup and
  tier event can come later. Do not say a tier line here; let the event do it.
- The `reason` is what the journal and Recent changes show.

### A shared moment asked from dialogue

Gate a line by tier, call `Romantasy.TryShareMoment(akSpeaker, "affection")` in its
fragment, and end the conversation on that line. A result of 3 or 4 means not yet
or not in the mood. It needs OStim 7.4 or newer; without it the answer is 1. The full
recipe and every result code are in [OStim.md](OStim.md#a-recipe).

### Testing all of the above

1. Close Skyrim, add `"authorMode": true` to Romantasy's `settings.json`, and turn on
   **Detailed log** in the ledger's Settings.
2. Click Mirelle in the console to get her reference ID, then:

```text
cgf "Romantasy.DebugDescribe" <refid>
cgf "Romantasy.DebugSetPoints" <refid> 999
cgf "Romantasy.DebugDeed" <refid> "Dungeons Cleared" 1
cgf "Romantasy.DebugAnnounceNow"
cgf "Romantasy.DebugSetMood" <refid> 80
```

- `DebugDescribe` writes her points, tier, mood, gate and personality to
  `Romantasy.log`. Check the likes and dislikes are yours.
- `DebugDeed` is paced and gated like a real deed. Under Slow burn, first run
  `cgf "Romantasy.DebugSetDaysTogether" <refid> 3`.
- `DebugSetPoints` to 999, then a deed she likes, crosses into Friend.
  `DebugAnnounceNow` delivers the waiting tier change, so your tier line plays.
- `DebugSetPoints` below the start of her tier, then `DebugAnnounceNow`, tests the
  tier lost line.
- `DebugDeed` with a deed she has an opinion about that crosses no tier sends
  `Romantasy_OnPreference`. A deed she has no opinion about sends nothing.
- `DebugSetMood` 80 or -80 tests Warm and Strained lines.
- For a reunion, dismiss her, wait three days with the wait menu, then recruit her
  again and close the dialogue.

Each deed writes a `why:` line in `Romantasy.log` that says what changed, or why
nothing did (not following, no feeling, a gate). Set `authorMode` back to false
before you play for real. Reference: [AuthorTesting.md](AuthorTesting.md).

## 5. Rules of thumb

- **Check the API version.** `Romantasy.GetApiVersion()` says what is installed.
  Points, levels and preferences need 4 or more. Mood and the tier lost, time apart
  and reunion events need 7. The test functions need 8. Moments
  (`TryShareMoment`, `CanShareMoment`, `GetMomentCount`, `GetDaysSinceMoment`,
  `DebugSharedMoment` and the two moment events) need 9. Romantasy 4.0.0 reports 9;
  3.0.1 reports 6. [papyrus-bridge.md](papyrus-bridge.md#api-versions)
- **Never depend on OStim or SkyrimNet scripts.** Call only `Romantasy`. Romantasy
  talks to both for you, and your mod needs neither as a master.
- **Do not poll.** Use the events. Read the getters when you need a value, not on a
  timer.
- **Filter on the sender, and register again after every load.**
- **Your mod works with no personality file.** The player can make the bond in the
  ledger. Do not assume a bond exists: check `GetLevel` (0 means no bond) or the
  Registered condition.
- **Compile against `Romantasy.psc`.** It ships in the mod's `Source/Scripts`
  folder. Your plugin does not need `CS_Romantasy.esp` as a master.
- **Keep labels and lines tasteful.** The player reads your `reason` text and unlock
  lines in the journal and on the popup.
- **Do not combine** a personality file with `RegisterAuthorFollower` for the same
  NPC.

## 6. The documents

| Document | Read it when |
|---|---|
| [FileProfiles.md](FileProfiles.md) | You write a personality file: every key, the limits and the checker's messages. |
| [papyrus-bridge.md](papyrus-bridge.md) | You write a script: every function, event, argument and API version. |
| [romance-points-dialogue-conditions.md](romance-points-dialogue-conditions.md) | You gate dialogue by tier, points or mood. |
| [AuthorTesting.md](AuthorTesting.md) | You test: author mode, the seven test functions, the why log and the checker. |
| [FollowerConversion.md](FollowerConversion.md) | Your follower used Romantasy's old factions and you are moving it to a personality file. |
| [OStim.md](OStim.md) | You want shared moments: the `[moments]` table, the moment functions and their codes. |
| [SkyrimNet.md](SkyrimNet.md) | You want to know what SkyrimNet hears about your follower's bond. |
| [Translating.md](Translating.md) | You translate the ledger, or want to know which words come from your files. |
