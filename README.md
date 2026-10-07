# Romantasy 3.0.1

Romantasy tracks the adventures shared with active followers in Skyrim SE/AE,
applies each companion's likes and dislikes, and maintains an independent bond
ledger. Follower mods supply their own dialogue, scenes, and reactions.

## What's new in 3.0

- A native Dear ImGui dashboard and relationship popups rendered through DX11.
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
while the companion is actively following. Relationship-change popups wait until
combat ends and can be disabled in Settings.

| Tier | Minimum points | Dialogue level |
|---|---:|---:|
| Stranger | 0 | 1 |
| Acquaintance | 500 | 2 |
| Friend | 1,000 | 3 |
| Confidant | 1,500 | 4 |
| Lover | 2,000 | 5 |
| Spouse | 2,500 | 6 |

Romantasy does not replace follower management or perform Skyrim's marriage
quest. Adding a bond does not create voiced dialogue for an NPC that has none.
The Spouse tier is a relationship milestone, not a wedding trigger.

## Installation and controls

Requires Skyrim SE/AE, [SKSE](https://skse.silverlock.org/) matching your runtime,
and [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).
Skyrim VR is not supported by the native dashboard.

Install the complete mod through your mod manager and enable `CS_Romantasy.esp`.
Keep the bundled fonts, scripts, sounds, and licenses. No external UI framework,
SkyUI, or MCM is required.

Open with **Left Ctrl + R**. For controller use, enable **Open with Favorites Menu**
in Settings, then cast the Romantasy power from Favorites. Favorites mode replaces
the keyboard opening hotkey until disabled.

- D-pad navigates; A selects; B goes back or closes.
- Left stick scrolls, including selected preference lists.
- A or B dismisses a relationship popup.
- Keyboard and mouse remain available. Text entry requires a physical keyboard.

The dashboard pauses the game; relationship popups do not. Controller hints
appear after gamepad activity and hide when keyboard or mouse input resumes.

## New bond and persistence

Bring an eligible, unmanaged follower with you, choose **New bond**, select their
likes and dislikes, and choose **Begin bond**. Romantasy writes their configuration
to `SKSE/Plugins/Romantasy/PlayerProfiles/`. Personality edits update the file and
live preferences immediately. Supplied follower profiles remain protected.

Configuration is shared by saves using the same profile files. Earned points are
stored separately in each save's SKSE cosave. Keep the `.skse` cosave with the save
and preserve `PlayerProfiles` when updating the mod. Under MO2, new generated files
normally appear in Overwrite or its configured output mod.

**Reset bond** resets the selected actor's points in the current save. **Remove
bond** disables the profile across saves using that configuration. Its small
disabled TOML file prevents an older save from reviving the bond; New bond can
re-enable it. Old player-created faction enrollments migrate when their actors
become available, retaining preferences and saved points after a successful write.

For non-unique NPCs, instances share the base profile's preferences but retain
separate point totals. Runtime-generated NPC bases without stable plugin identity
cannot be added through New bond.

## Follower integration

Ship one TOML profile per NPC under `SKSE/Plugins/Romantasy/Profiles/`, keyed by its
defining plugin and plugin-local NPC base ID. Profiles load at game-data startup;
restart Skyrim after editing supplied files externally.

Gate dialogue with `GetGraphVariableInt`, Run On **Subject**, using the reserved
queries `Romantasy_Registered`, `Romantasy_Level`, and `Romantasy_Points`.
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
- [Native UI development and verification](docs/NativeUI.md)
- [Release notes and validation boundaries](CHANGELOG.md)

The Papyrus API remains **capability version 6**, independent of the mod's 3.0.1
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
