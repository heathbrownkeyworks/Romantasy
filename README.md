# Romantasy

Romantasy is a save-safe relationship framework for Skyrim Special Edition and
Anniversary Edition. It watches the adventures shared with active followers,
applies each follower's individual likes and dislikes, and keeps an independent
bond ledger without replacing the follower framework, dialogue system, or
vanilla marriage system.

## Features

- Tracks 58 vanilla gameplay statistics across exploration, quests, combat,
  magic, crafting, crime, persuasion, vampirism, and lycanthropy.
- Gives every tracked activity its own point weight and lets each follower
  like, dislike, or ignore it.
- Awards or removes automatic activity points only while that follower is
  actively following the player.
- Maintains six relationship tiers: Stranger (0), Acquaintance (500), Friend
  (1,000), Confidant (1,500), Lover (2,000), and Spouse (2,500).
- Stores points, player-created profiles, and preference ownership in the SKSE
  cosave, including independent ledgers for spawned followers that share a base
  NPC.
- Mirrors the current tier to `ROM_RomanceLevel` for dialogue, scene, quest,
  and marriage conditions.
- Supplies exact live point totals through the `ROM_RomancePoints`
  `GetFactionRank` condition proxy, avoiding Skyrim's normal faction-rank
  limits.
- Supports author-defined profiles, player-created profiles, and external
  integrations while protecting author-defined personalities from player or
  third-party overwrites.
- Lets follower authors register profiles at runtime from an optional
  compatibility plugin. The original follower can remain completely
  standalone and does not need `CS_Romantasy.esp` as a master.
- Provides optional relationship-rank and exact-point GlobalVariable mirrors
  for follower plugins with existing dialogue conditions.
- Broadcasts `Romantasy_OnLevelChanged` and `Romantasy_OnPreference` ModEvents
  for voiced reactions and authored scenes.
- Defers relationship-change notifications until combat ends.
- Works alongside the vanilla follower system, Horde, NFF, AFT, EFF, and
  custom follower frameworks.

## Meridian dashboard

The in-game Meridian UI dashboard provides:

- A company-wide list with bond totals, tiers, roles, progress, and active or
  away status.
- Per-follower dossiers showing likes, dislikes, milestones, and the five most
  recent point changes.
- An **Add Companion** workflow for enrolling an eligible active follower and
  choosing that follower's personality.
- Edit, reset, and remove controls for player-created profiles. Author-defined
  profiles remain sealed and read-only.
- Configurable gain and loss notifications, away-follower visibility, and two
  opening modes: `Left Ctrl + R` or a power in the Favorites menu.
- An optional confirmation-gated developer panel for testing stat and point
  changes without accidentally exposing those controls during normal play.
- Graceful degradation: if Meridian UI is unavailable, tracking and the
  Papyrus API continue running while the dashboard remains disabled.

## Requirements and supported runtimes

- Skyrim Special Edition `1.5.97`, or a supported Anniversary Edition runtime
  including Steam `1.7.104`
- [SKSE](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- Meridian UI for the dashboard

Romantasy uses CommonLibSSE-NG and one Address Library-based DLL for SE and AE.
Skyrim VR is not currently supported by the Romantasy dashboard.

## Follower-author integration

Romantasy supports both the original faction-tag integration and the preferred
optional registration workflow. For a standalone follower release, place the
Romantasy dependency in a separate compatibility plugin with a startup quest.
That quest can register the follower, likes and dislikes, starting tier, and
optional legacy mirrors only when the compatibility plugin is installed.

The native Papyrus header is
`Source/Scripts/Romantasy.psc`. Its public API is:

```papyrus
Int Function GetApiVersion() Native Global

Bool Function ModifyPoints(Actor akFollower, Int aiPoints, String asReason = "", Bool abShowLevelUp = True) Native Global
Bool Function ApplyPreference(Actor akFollower, String asStatName, Int aiDelta = 1, Bool abShowLevelUp = True) Native Global

Bool Function ClearPreferences(Actor akFollower) Native Global
Bool Function SetPreference(Actor akFollower, String asStatName, Int aiDirection) Native Global
Int Function GetPreference(Actor akFollower, String asStatName) Native Global
Bool Function SetPreferencesManual(Actor akFollower, Bool abManual = True) Native Global
Bool Function IsPreferencesManual(Actor akFollower) Native Global

Bool Function RegisterAuthorFollower(Actor akFollower, String[] asStatNames, Int[] aiDirections, Int aiStartingLevel = 1, GlobalVariable akRelationshipRankMirror = None) Native Global
Bool Function RegisterAuthorFollowerPointsMirror(Actor akFollower, GlobalVariable akRelationshipPointsMirror) Native Global

Int Function GetPoints(Actor akFollower) Native Global
Int Function GetLevel(Actor akFollower) Native Global
String Function GetLevelName(Actor akFollower) Native Global
```

Preference directions are `-1` for dislike, `0` for neutral or removal, and
`1` for like. `GetLevel` returns `0` for an actor Romantasy does not manage and
`1` through `6` for Stranger through Spouse.

## Building

Clone with submodules and build from the repository root:

```powershell
git clone --recurse-submodules https://github.com/heathbrownkeyworks/Romantasy.git
cd Romantasy
xmake f -p windows -a x64 -m release -c
xmake -y
```

The release DLL is written to
`build/windows/x64/release/Romantasy.dll`. Packaging the game plugin, compiled
Papyrus script, sounds, and Meridian view is handled separately from this
source repository.

## License

Romantasy's original project source is available under the [MIT License](LICENSE).

Third-party components retain their own licenses. In particular, the native
DLL statically links CommonLibSSE-NG, which is GPL-3.0-or-later with its
modding and linking exceptions. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
and the dependency's license files for the applicable binary-distribution
terms.
