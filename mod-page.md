# Romantasy 3.0.1

*A Ledger of Hearts.*

Romantasy turns the adventures you share with your followers into a relationship
that changes with your choices. Explore, fight, craft, steal, or show mercy: each
companion responds according to their own likes and dislikes.

## New in 3.0

The dashboard and relationship popups now use a native interface built into
Romantasy. **No Meridian UI or PRISMA installation is needed.**

Follower personalities use TOML profiles. Authors can supply them with a follower
mod, or you can choose **New bond** to add an eligible follower yourself. Your
personality edits save immediately and affect subsequent actions without a reload.

## Features

- 58 tracked gameplay statistics, with a separate opinion for each companion.
- Six relationship tiers, from Stranger to Spouse.
- A native dashboard showing companions, points, milestones, likes/dislikes, and
  recent changes.
- Relationship gain/loss popups, with independent settings and combat deferral.
- New bond, personality editing, bond reset, and removal for player-created profiles.
- Keyboard, mouse, and controller navigation with contextual button hints.
- Save-specific earned points, plus persistent TOML personality configuration.

| Tier | Minimum points |
|---|---:|
| Stranger | 0 |
| Acquaintance | 500 |
| Friend | 1,000 |
| Confidant | 1,500 |
| Lover | 2,000 |
| Spouse | 2,500 |

## Requirements and installation

Install SKSE matching your Skyrim SE/AE runtime and Address Library for SKSE
Plugins. Install the complete Romantasy package with your mod manager, keep its
fonts/scripts/sounds, and enable CS_Romantasy.esp.

No external UI framework, SkyUI, or MCM is required. The native dashboard does
not support Skyrim VR.

## Opening and controls

**Left Ctrl + R** opens the dashboard by default. For a controller, enable
**Open with Favorites Menu** in Settings, then cast the Romantasy power from
Favorites. Favorites mode replaces the keyboard opening shortcut until disabled.
There is no dedicated gamepad button combination.

Use D-pad to navigate, A to select, B to go back/close, and left stick to scroll.
A or B dismisses a relationship popup. Search and other text entry require a
physical keyboard. The dashboard pauses the game; relationship popups do not.

## Personalities and saves

Automatic points change only while the companion is actively following you.
Romantasy responds to the game's statistic events; it does not poll on a Papyrus
timer. Each statistic can be liked, disliked, or left neutral.

Bring an eligible unmanaged follower, choose New bond, select their personality,
and Begin bond. Edit personality changes both the saved profile and its live
preferences immediately. Personalities supplied by follower mods remain protected.

Player profiles are stored under SKSE/Plugins/Romantasy/PlayerProfiles. Those
preferences are shared across saves using the same files. Earned points belong
to each save's SKSE cosave; preserve the .skse file when copying a save.

Reset bond resets the selected actor in the current save. Remove bond disables
the shared player profile. Its disabled TOML prevents an older save from reviving
the bond; New bond can re-enable it. Keep PlayerProfiles when updating Romantasy.
With MO2, new generated files normally appear in Overwrite or the configured
output mod.

Existing player-created faction bonds migrate when their actors become available,
retaining preferences and points after a successful file write. Saved balances,
including zero, override a supplied profile's starting tier.

## Follower authors

Ship one TOML profile per NPC under SKSE/Plugins/Romantasy/Profiles, using the
defining plugin and plugin-local NPC base ID. Restart Skyrim after external edits.

For dialogue conditions, use GetGraphVariableInt on Subject with the reserved
Romantasy_Registered, Romantasy_Level, and Romantasy_Points queries. Always add
Romantasy_Registered == 1. Levels are 1-6; Friend is 3 and Lover is 5.

This integration needs no romance factions, new globals, or enrollment scripts.
Existing Papyrus APIs and ModEvents remain available for story beats and voiced
reactions. Legacy faction integration is retained for unconverted mods. See the
bundled profile, conversion, dialogue, and Papyrus guides for details.

## Compatibility and troubleshooting

Romantasy keeps a relationship ledger alongside follower management. It does not
replace the follower framework, add voiced dialogue to unsupported NPCs, or perform
Skyrim's marriage quest when a bond reaches Spouse. Follower-authored scenes remain
the responsibility of that follower mod.

If a follower is missing, check their profile identity and Romantasy.log, or bring
an unmanaged eligible follower and use New bond. Runtime-generated NPC bases cannot
be added because they lack a stable plugin identity.

If points do not change, confirm the companion is actively following and has an
opinion about that statistic. For missing authored dialogue, inspect all topic and
INFO conditions as well as the relationship gate.

Keep the complete installation, player profiles, and paired save/cosave files.
Supplied profile edits require a game restart; in-game player edits apply immediately.

## Permissions and credits

Created by ColdSun. Built with SKSE, CommonLibSSE-NG, Dear ImGui, and toml++.
Native source is GPL-3.0-or-later with the documented modding/linking exceptions.
Public Papyrus declarations remain MIT. See the bundled license files.

Source and integration guides: https://github.com/heathbrownkeyworks/Romantasy
