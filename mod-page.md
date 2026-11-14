# Romantasy 4.0.0

*A Ledger of Hearts.*

Romantasy turns the adventures you share with your followers into a relationship
that changes with your choices. Explore, fight, craft, steal, or show mercy: each
companion responds according to their own likes and dislikes.

## New in 4.0

- **The Rosewater ledger.** The ledger and tier popups are a page hosted by
  Magelight UI, which is now required for them.
- **Safer saves.** A bond keeps its points while its follower's mod is missing,
  and bonds you create are saved with the game.
- **Favorites and pet peeves** count double, and a tier can carry short unlock
  lines that the popup shows when you reach it.
- **Mood.** Each bond is Strained, Settled or Warm. Gains warm it, losses strain
  it, and it drifts back to Settled over a few days.
- **Pacing.** Tier popups wait for a calm moment: out of combat, in an inn, a home
  or a town, or after sleeping or waiting, and never longer than a day. Choose
  **Relationship pace**: `Slow burn` (half points, and days together before each
  higher tier), `Standard` or `Fast` (double points). Losses are never changed.
- **The journal.** Each bond keeps its last 30 changes, a chart of the last 60
  days, its biggest gains and losses, and milestones.
- **Whispers.** Short cards on the HUD say what just moved a bond, such as
  "Dungeons Cleared · +2" or "Glad to have you back". They never pause the game.
- **Roster tools.** **Find a companion**, an order (**Closest**, **Name** or
  **Recent**), and **Following** and **Away** groups.
- **Comfort.** **Ledger size**, **Stronger contrast** and **Reduce motion**.
- **Health.** Settings says whether Romantasy is working and lists anything wrong.
  **Detailed log** writes every point change to Romantasy.log for bug reports.
- **Translation.** Almost every word the ledger shows is in one strings file.
- **Optional SkyrimNet and OStim support**, and a test kit for follower authors.

## Features

- 58 tracked gameplay statistics, with a separate opinion for each companion.
- Six relationship tiers, from Stranger to Spouse, and a mood beside each bond.
- A ledger showing companions, points, mood, likes and dislikes, recent changes
  and a journal for each bond.
- "Romance Deepened" and "Romance Wounded" popups, each with its own switch.
- New bond, personality editing, bond reset, and removal for player-created bonds.
- Keyboard, mouse, and controller navigation with button hints.
- Points, moods, journals and player-created bonds saved with each game.

| Tier | Minimum points |
|---|---:|
| Stranger | 0 |
| Acquaintance | 500 |
| Friend | 1,000 |
| Confidant | 1,500 |
| Lover | 2,000 |
| Spouse | 2,500 |

## Requirements and installation

Install SKSE matching your Skyrim SE/AE runtime, Address Library for SKSE
Plugins, and Magelight UI 1.6 or newer. Install the complete Romantasy package
with your mod manager, keep its page files, scripts and sounds, and enable
CS_Romantasy.esp. Without Magelight UI, points are still tracked, but the ledger
and popups do not appear.

SkyUI, MCM, Meridian UI and Prisma UI are not required. The ledger does not support
Skyrim VR.

## Upgrading

**From 3.x:** install Magelight UI 1.6 or newer and replace the old Romantasy with
4.0.0. Keep the PlayerProfiles folder and settings.json under
SKSE/Plugins/Romantasy. Your saves keep their points and bonds. Keep a save from
before the upgrade: **a save written by 4.0.0 must not be loaded in 3.0.1 or
older**, or its bond points are lost. Tier popups now wait for a calm moment; set
**Tier changes arrive** to `At once` for the old timing.

**From 2.x:** the same, and Meridian UI and Prisma UI are no longer needed. Bonds you created
yourself migrate the first time each follower is loaded, keeping likes, dislikes
and points. The LB + X shortcut is gone; use **Open with Favorites Menu** and the
Romantasy power instead.

## Opening and controls

**Left Ctrl + R** opens the ledger by default. For a controller, enable
**Open with Favorites Menu** in Settings, then cast the Romantasy power from
Favorites. Favorites mode replaces the keyboard opening shortcut until disabled.
There is no dedicated gamepad button combination.

Use the D-pad to move and scroll, A to select and B to go back or close. A or B
dismisses a tier popup. Typing, such as **Find a companion**, needs a keyboard.
The ledger pauses the game; tier popups and whispers do not.

## Personalities and saves

Automatic points change only while the companion is actively following you.
Romantasy responds to the game's statistic events; it does not poll on a Papyrus
timer. Each statistic can be liked, disliked, or left neutral, and up to three can
be favorites and three pet peeves.

Bring an eligible unmanaged follower, choose **New bond**, select their
personality, and choose **Begin bond**. **Edit personality** changes the bond and
its live preferences immediately. Personalities supplied by follower mods remain
protected.

Bonds you create are saved with the game, along with points, mood and the journal.
Keep the .skse cosave with each save. A copy of each personality is also written to
SKSE/Plugins/Romantasy/PlayerProfiles for new characters; keep that folder when
updating Romantasy. With MO2, new generated files normally appear in Overwrite or
the configured output mod.

**Reset bond** resets the selected companion in the current save. **Remove bond**
removes the bond from the current save only; other saves keep it, and New bond can
add it again.

If a follower's mod is missing, Romantasy keeps that bond's points and journal, and
the bond returns with the mod. A HUD line says how many bonds are being kept.

## SkyrimNet and OStim (optional)

With SkyrimNet, a companion's AI-driven dialogue knows the bond's tier, mood and
days together, and a kind or harsh word can move the bond a little, at most one of each
an hour and 15 a day each way. With OStim Standalone 7.3.3 or newer, popups and whispers stay quiet during a
scene, and time alone with one bonded companion can count as a shared moment that
adds a little to the bond, once a day. A moment never takes points, and the journal
names it only by its kind. Without these mods nothing changes. Each has rows in
Settings that appear only when the mod is found.

## Follower authors

Start with the author guide (docs/AuthorGuide.md). Ship one TOML profile per NPC
under SKSE/Plugins/Romantasy/Profiles, and gate dialogue with GetGraphVariableInt on
Subject: Romantasy_Registered == 1, plus Romantasy_Level (1-6; Friend is 3, Lover is
5), Romantasy_Points or Romantasy_Mood. No romance factions, globals or enrollment
scripts are needed, and legacy faction integrations still work.

Detailed guides in the source repository:

- docs/AuthorGuide.md: where to start
- docs/FileProfiles.md: profiles, favorites, unlock lines and moments
- docs/papyrus-bridge.md: Papyrus API (capability version 9) and reaction events
- docs/AuthorTesting.md: author mode, debug functions and the why log
- docs/FollowerConversion.md: moving an existing follower to a profile
- docs/SkyrimNet.md and docs/OStim.md: the optional bridges
- docs/Translating.md: changing or translating the ledger's words

## Compatibility and troubleshooting

Romantasy keeps a relationship ledger alongside follower management. It does not
replace the follower framework, add voiced dialogue to unsupported NPCs, or perform
Skyrim's marriage quest when a bond reaches Spouse. Follower-authored scenes remain
the responsibility of that follower mod.

Open Settings and read the Health section first: it lists anything wrong, such as
a missing Magelight UI, a missing or outdated CS_Romantasy.esp, or a rejected
personality file. For a bug report, switch on **Detailed log** and include
Romantasy.log.

If a follower is missing, check their profile identity and Romantasy.log, or bring
an unmanaged eligible follower and use New bond. Runtime-generated NPC bases cannot
be added because they lack a stable plugin identity.

If points do not change, confirm the companion is actively following and has an
opinion about that statistic. For missing authored dialogue, inspect all topic and
INFO conditions as well as the relationship gate.

Keep the complete installation, player profiles, and paired save/cosave files.
Supplied profile edits require a game restart; in-game player edits apply immediately.

## Permissions and credits

Created by ColdSun. Built with SKSE, CommonLibSSE-NG, Magelight UI, and toml++.
Native source is GPL-3.0-or-later with the documented modding/linking exceptions.
Public Papyrus declarations remain MIT. See the bundled license files.

Source and integration guides: https://github.com/heathbrownkeyworks/Romantasy
