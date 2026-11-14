# Romantasy 4.0.0

Romantasy tracks the adventures shared with active followers in Skyrim SE/AE,
applies each companion's likes and dislikes, and maintains an independent bond
ledger. Follower mods supply their own dialogue, scenes, and reactions.

## What's new in 3.0

- The ledger and relationship popups are a page hosted by Magelight UI.
  **Meridian UI and PRISMA are not required.**
- TOML follower profiles and actor-specific dialogue queries, with no romance
  factions, enrollment scripts, or new globals needed for this integration.
- **New bond** creates player-owned TOML profiles. Personality edits save to disk
  and apply immediately; player-created bonds can be edited, reset, or removed.
- Controller navigation and contextual button hints. Open through the Favorites
  power; no gamepad opening chord is registered.

## Relationships

Romantasy responds to 58 vanilla statistic events across exploration, quests,
combat, crafting, crime, persuasion, vampirism, and lycanthropy. A companion can
like, dislike, or ignore each statistic. Automatic gains and losses apply only
while the companion is actively following. Relationship-change popups wait for a
calm moment (see Settings) and can be disabled there.

| Tier | Minimum points | Dialogue level |
|---|---:|---:|
| Stranger | 0 | 1 |
| Acquaintance | 500 | 2 |
| Friend | 1,000 | 3 |
| Confidant | 1,500 | 4 |
| Lover | 2,000 | 5 |
| Spouse | 2,500 | 6 |

A companion can also have up to three favorites and three pet peeves. These count
double when earned. Personality files may add short unlock lines for each tier above Stranger;
the ledger shows the next tier's lines and the tier popup lists them when you
reach it. See [FileProfiles.md](docs/FileProfiles.md).

Romantasy does not replace follower management or perform Skyrim's marriage
quest. Adding a bond does not create voiced dialogue for an NPC that has none.
The Spouse tier is a relationship milestone, not a wedding trigger.

## Installation and controls

Requires Skyrim SE/AE, [SKSE](https://skse.silverlock.org/) matching your runtime,
[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444),
and Magelight UI 1.6 or newer. Without Magelight UI, tracking still runs but the
ledger and popups do not appear. The ledger is not supported in Skyrim VR.

Install the complete mod through your mod manager and enable `CS_Romantasy.esp`.
Keep the bundled page files, scripts, sounds, and licenses. SkyUI and MCM are not
required.

Open with **Left Ctrl + R**. For controller use, enable **Open with Favorites Menu**
in Settings, then cast the Romantasy power from Favorites. Favorites mode replaces
the keyboard opening hotkey until disabled.

- D-pad navigates; A selects; B goes back or closes.
- Left stick scrolls, including selected preference lists.
- A or B dismisses a relationship popup.
- Keyboard and mouse remain available. Text entry requires a physical keyboard.

The dashboard pauses the game; relationship popups do not. Controller hints
appear after gamepad activity and hide when keyboard or mouse input resumes.

## Finding and ordering companions

The roster groups bonds under **Following** and **Away**. Type in **Find a
companion** to show only the names that match; clear the text and everyone
returns. The order buttons sort each group: **Closest** (most points first, the
default), **Name** (A to Z) or **Recent** (bonds with the newest change first).
Rows do not move while the ledger is open; the order is applied when it opens
and when you change the order or the find text. Your choice is remembered.

## Settings

Open Settings in the ledger. Along with the popup, away-companion and
Favorites-menu switches, it has:

- **Ledger size.** `Standard` (the default), `Large` or `Largest`. Makes everything
  in the ledger bigger.
- **Stronger contrast.** Brighter text, solid panels and thicker outlines. The tier
  popup and whispers follow it.
- **Reduce motion.** Stops the drifting backdrop and the larger animations. The
  tier popup and whispers follow it.
- **Health.** Says whether Romantasy is working. Warnings and errors (Magelight UI
  missing or old, `CS_Romantasy.esp` missing, rejected personality files, a damaged
  save record, settings that could not be saved) show first, and the Settings
  button carries a small mark. When all is well, "Everything is in order." sits at
  the foot with the versions. Bonds kept for mods that are not loaded appear as a
  note only. A problem also shows once per session as a HUD line.
- **Tier changes arrive.** `At a calm moment` (the default) or `At once`. With a calm
  moment, the tier popup and the `Romantasy_OnLevelChanged` and
  `Romantasy_OnLevelLost` events wait until you are out of combat and in an inn, a
  home, a town or another settlement, or have just woken or finished waiting. A
  change never waits longer than one in-game day; after that it arrives at the next
  moment you are out of combat. Several changes for one bond arrive as one, from the
  tier the bond started at to the tier it holds. If the bond is back where it
  started, nothing is announced. Points, the tier shown in the ledger and dialogue
  conditions change at once; only the popup and those two events wait. Waiting
  changes are kept in the save. `At once` is the old timing. With a calm moment, the deed that crosses a tier
  produces no follower reaction at that moment; the reaction comes with the popup.
- **Relationship pace.** `Slow burn`, `Standard` (the default) or `Fast`. Fast
  doubles the points from deeds, meaning the events Romantasy watches itself. Slow
  burn halves them (an odd half is carried to the next gain, so nothing is lost) and
  asks for days together before each higher tier: Friend 3, Confidant 7, Lover 14,
  Spouse 21. Losses are never changed. Neither choice doubles or halves points a
  follower's own script awards, or the like and dislike amounts a script sends, but
  under Slow burn a gate can still hold a script's gain until enough days together
  have passed. A gate only
  holds a bond at the top of the tier below. It never takes points away and never
  stops gains inside the tier a bond already holds, so switching Slow burn on in
  the middle of a game does not freeze a bond that is already past a gate. The
  dossier shows the gate ahead as "Friend needs 3 days together (1.5 so far)". Days so far are rounded down to one decimal place (2.96 shows 2.9),
  so the line never shows the needed number before the gate opens. Days
  together are counted as described under mood and company in
  [docs/Development.md](docs/Development.md). The developer tools are not gated.
  Changing the pace applies from then on and never rewrites points already earned.
- **Whispers.** On by default. A whisper is a small card on the game HUD that says
  what just moved a bond, for example "Dungeons Cleared · +2", "Feels warm toward you",
  "Things are strained", "Glad to have you back" or "Has been away from you for
  days". It shows for a deed that moved points (the follower must be following), a
  mood that turns warm or strained, a reunion or time apart, and a tier gained or
  lost when that tier's popup is switched off in Settings. The first deed shows at
  once. Deeds for the same follower that follow within two seconds of each other,
  while a card is up or waiting, merge into one card ("3 deeds · +12"). No whisper
  shows when a Slow burn gate held the whole gain. The ledger's developer tools
  raise none. One card shows at a time, for about four seconds. A whisper waits
  while a menu, dialogue or loading screen is up, for up to 30 seconds; after that
  it is dropped. A menu that neither pauses the game nor takes input (an
  always-open HUD mod) does not block whispers. A load or a new game drops the
  whispers of the old game. Whispers raised while the ledger is open (a reunion
  noticed when it opens) show when it closes. A whisper only shows what happened:
  it never pauses the game, takes the cursor or changes points, tiers or mood. If
  the second Magelight view is not available, whispers use the game's own
  notification text instead (`Name: line`), and Health says "Whispers are using
  the game's own notifications." There a loss is written with a plain hyphen, for
  example `Aelira: Murders · -9`. If the whisper card does not answer twice in a
  row, Romantasy uses the game's notifications for the rest of the session, and
  Health says so. With Magelight UI missing, a tier change is shown as a game
  notification too (when Whispers is on), since the popup cannot show.
- **Whisper corner.** `Top left`, `Top right` (the default), `Bottom left` or
  `Bottom right`. This row is dimmed while Whispers is off.
- **Detailed log.** Off by default. When on, `Romantasy.log` records every point
  change. Use it for bug reports.

## Translating

Almost every word the page shows is in `Magelight\Romantasy\js\strings.js` in the mod
folder. See [docs/Translating.md](docs/Translating.md) to change a word or
translate the page.

## New bond and persistence

Bring an eligible, unmanaged follower with you, choose **New bond**, select their
likes and dislikes (and up to three favorites and three pet peeves), and choose
**Begin bond**. Romantasy stores the bond in the save and writes a copy of its configuration to
`SKSE/Plugins/Romantasy/PlayerProfiles/`. Personality edits update both and apply
to live preferences immediately. Supplied follower profiles remain protected.

Player-created bonds and earned points are stored in each save's SKSE cosave. Keep
the `.skse` cosave with the save. The files in `PlayerProfiles` are copies used for
new characters and for saves made before this version; preserve them when updating
the mod. Under MO2, new generated files normally appear in Overwrite or its
configured output mod.

**Reset bond** resets the selected actor's points in the current save. **Remove
bond** removes the bond from the current save only. It leaves the file in
`PlayerProfiles` alone, so other saves keep their bond; New bond can add it again.
Old player-created faction enrollments migrate when their actors
become available, retaining preferences and saved points. If the copy in
`PlayerProfiles` cannot be written, the bond is still stored in the save and the
failure is logged.

For non-unique NPCs, instances share the base profile's preferences but retain
separate point totals. Runtime-generated NPC bases without stable plugin identity
cannot be added through New bond.

## Follower integration

Ship one TOML profile per NPC under `SKSE/Plugins/Romantasy/Profiles/`, keyed by its
defining plugin and plugin-local NPC base ID. Profiles load at game-data startup;
restart Skyrim after editing supplied files externally.

Gate dialogue with `GetGraphVariableInt`, Run On **Subject**, using the reserved
queries `Romantasy_Registered`, `Romantasy_Level`, `Romantasy_Points`, and
`Romantasy_Mood`.
For Friend or higher, use both conditions joined with AND:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

No `CS_Romantasy.esp` master is needed solely for TOML or these conditions.
Existing unrelated plugin dependencies must still be preserved. Legacy faction
integrations and optional registration APIs remain available for unconverted mods.

- [Profile schema, statistics, and persistence](docs/FileProfiles.md)
- [Follower conversion guide](docs/FollowerConversion.md)
- [Creation Kit dialogue conditions](docs/romance-points-dialogue-conditions.md)
- [Papyrus API and voiced reaction events](docs/papyrus-bridge.md)
- [Testing your follower: author mode, debug functions, the why log](docs/AuthorTesting.md)
- [Using Romantasy with SkyrimNet](docs/SkyrimNet.md)
- [Using Romantasy with OStim](docs/OStim.md)
- [Release notes and validation boundaries](CHANGELOG.md)

Each bond also has a short-term mood: Strained, Settled or Warm. Gains warm it,
losses strain it, and it drifts back to Settled over a few in-game days. It never
changes points or tiers. The ledger shows it in the dossier. Three more events
reach follower scripts: `Romantasy_OnLevelLost` when a tier is lost,
`Romantasy_OnTimeApart` after 3 in-game days away, and `Romantasy_OnReunion` when
a follower who was away for a day or more follows again.

Each bond also keeps a journal. In the ledger, open a bond's page and choose
**Journal** at the foot of the Recent changes card. It shows the last 30 changes,
the points gained and lost on each of the last 60 in-game days, the 5 biggest gains
and 5 biggest losses, and milestones: when the journal began, each tier reached
(written the first time, and again after a reset), and each fall to a lower tier (the last 20 are kept). Days are shown
as Day N. Day 1 is the first day of the save, so it is one more than the game's
Days Passed. Hovering a day in the chart with the mouse shows that day's numbers;
with keys or a gamepad, the totals line under the chart is what shows. A change is written when it happens, even when its tier popup is waiting
for a calm moment. Reaching a tier a second time is not written again. Resetting a
bond is a fresh start: History gets one "Bond reset" row with the points lost, and
a "Fell to Stranger" milestone if the bond stood higher. The reset is not counted
among "Wounded most" or in the day totals in the chart. The tiers the bond had
reached are cleared, so they are written again when it reaches them again. Removing
a bond erases its journal. A bond from before this version starts its journal at its
next change. So does a bond that a follower mod or a profile file brings. A bond made
in the ledger starts its journal that day. The journal is kept in the save, and is
kept for a follower whose mod is missing, as points are. It only records: it never
changes points, tiers or mood.

Romantasy can also work with OStim Standalone when it is installed. It keeps tier
popups and whispers quiet during a scene, and a scene with one companion can count as
a shared moment that adds a little to the bond, once a day. A follower mod can ask
Romantasy to start a gentle moment. OStim has no consent flag, so Romantasy only
sees the tags a scene's author set. Without OStim nothing changes. See
[docs/OStim.md](docs/OStim.md).

The Papyrus API is **capability version 9**, independent of the mod's 4.0.0
release number. Its declarations are in `Source/Scripts/Romantasy.psc`.

## Building

Use Windows, Visual Studio C++ build tools, and xmake 3.0.1 or newer. Clone with
submodules and build from the repository root:

```powershell
git clone --recurse-submodules https://github.com/heathbrownkeyworks/Romantasy.git
cd Romantasy
xmake f -p windows -a x64 -m release -c
xmake -y
xmake build romantasy-profile-check
xmake build romantasy-profile-tests
xmake run romantasy-profile-tests
xmake build romantasy-ui-tests
xmake run romantasy-ui-tests
node tests/RuntimeCompatibilityContractTests.mjs
```

The DLL is `build/windows/x64/release/Romantasy.dll`. Deploy fonts from
`assets/fonts` to `SKSE/Plugins/Romantasy/fonts`. The profile validator can be
included as `tools/romantasy-profile-check.exe`. The complete mod download also
contains `CS_Romantasy.esp`, the compiled Papyrus script, and sounds.

When `XSE_TES5_MODS_PATH` is set, the CommonLib build rule auto-installs the DLL.
Close the game and editors before building into an installed mod. Never invoke
`xmake -P` from another working directory; build at the checkout root so generated
SKSE metadata is included. Local signing credentials and workstation deployment
scripts are not required to compile the public source.

## License

The native implementation and DLL are GPL-3.0-or-later with the permissions in
[EXCEPTIONS.md](EXCEPTIONS.md). Public Papyrus declarations are separately available
under MIT. See [LICENSING.md](LICENSING.md) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency licenses and
corresponding-source information.
