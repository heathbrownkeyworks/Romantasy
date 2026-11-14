# Converting followers to Romantasy

Use this procedure to replace a follower's romance factions with a TOML profile
and native dialogue conditions. Preserve the follower's identity, voice assets,
scripts, quest behavior, and unrelated conditions. A follower with no Romantasy
factions to remove can skip to the [author guide](AuthorGuide.md).

Read [FileProfiles.md](FileProfiles.md), use the
[inert profile template](../examples/profiles/ExampleFollower.toml.example), and
consult the [dialogue condition guide](romance-points-dialogue-conditions.md).
[Mornhilde's profile](../examples/profiles/Mornhilde.toml) is a converted profile;
its plugin and form ID are hers.

## 1. Audit and back up

Identify the winning plugin and all MO2 overrides. Back up the plugin, scripts,
profile files, and file hashes, then work in a separate candidate or overlay.

Record the defining plugin, plugin-local **NPC_ base** FormID, placed references,
starting romance rank, and all romance/preference faction memberships. Resolve
legacy faction IDs against the actual Romantasy plugin. Inventory references in
INFO, quest, scene, and package conditions, VMAD properties, and Papyrus sources
and compiled scripts. Identify any competing progression system.

Preserve local FormIDs, plugin flags/header, voice paths, fragments, branches,
quests, unrelated factions, and follower-specific milestone state. For example,
a global remembering that a bark already played is not the current relationship
level and may still be needed.

## 2. Write the profile

Install one TOML file under `SKSE/Plugins/Romantasy/Profiles/` in the follower mod.
Use its defining plugin and local NPC base ID, not an ACHR or load-order FormID:

```toml
schema_version = 1

[npc]
plugin = "ExampleFollower.esp"
form_id = "0x000800"

[romance]
starting_level = "Stranger"
likes = ["Quests Completed", "Dungeons Cleared"]
dislikes = ["Murders"]
```

Copy the follower's intended opinions; do not invent new ones. Use supported
labels or stable `ROM_*` identifiers. Convert old ranks 0-5 to the corresponding
named starting tier. Saved points, including zero, override that starting tier.
You need `schema_version = 2` or `3` only when you add favorites, pet peeves, `[unlocks]`
or `[moments]`; see [FileProfiles.md](FileProfiles.md).

Validate the candidate directory:

```powershell
.\tools\romantasy-profile-check.exe 'path\to\SKSE\Plugins\Romantasy\Profiles'
```

The tool validates syntax and profile rules, not plugin/NPC existence in a load
order. Supplied profiles load at startup; restart Skyrim after external edits.
Applications exporting profiles should use Profiles, not the player-owned
PlayerProfiles directory.

## 3. Convert dialogue conditions

Use `GetGraphVariableInt` on Subject. Replace each legacy level test with the
Registered guard and a 1-based level comparison:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

That is Friend or higher, equivalent to legacy rank >= 2. `Romantasy_Points` and
`Romantasy_Mood` work the same way. Preserve each operator,
run-on target, AND/OR grouping, and all unrelated predicates. Add one to old
level rank values only; exact point thresholds are unchanged. Replace point
proxy tests with `Romantasy_Points` plus the Registered guard.

Writers must serialize the condition's string parameter correctly, including the
CIS1 subrecord associated with its CTDA. Numeric condition parameters cannot
substitute for these strings. Save/reopen in CK or validate the binary records.

Audit both topic-level and INFO-level gates. Keep Horde absence gates only for
functionality Horde covers. Do not gate trade, favor, romance dialogue, or romance
reactions on Horde being absent.

## 4. Remove the old romance dependencies

Remove the follower's Romantasy romance/preference memberships after transferring
their configuration. Neutralize scripts that would restore those memberships or
compete with Romantasy progression. Keep unrelated factions such as
CurrentFollowerFaction and DismissedFollowerFaction.

The TOML path requires no enrollment quest, new globals, or
`RegisterAuthorFollower` call. Existing Papyrus point APIs and ModEvents can still
drive authored reactions. Do not combine legacy author registration and a TOML
profile for the same NPC.

Remove `CS_Romantasy.esp` as a follower master only when no remaining records,
scripts, or properties reference its forms. Keep Romantasy's core plugin installed
for the complete mod and Favorites power. Removing a follower master does not
remove the DLL runtime requirement.

## 5. Verify and deploy the candidate

- Compare record counts and hashes. Confirm only intended romance data changed.
- Validate the plugin strictly and round-trip its records; confirm string
  conditions, operators, grouping, fragment properties, and original voice paths.
- Launch through the intended MO2 profile. Confirm the profile and native
  condition bridge load without errors in Romantasy.log.
- Test below, at, and above every tier/point gate, including losses and zero.
  Inspect the complete condition list when a dialogue response does not appear.
- Save/reload, restart, and load a different save. Test another NPC with a
  different balance, and multiple instances if the follower supports them.
- Test active/dismissed state, stat gains, voiced reactions, and scenes. Filter
  ModEvents by the sender NPC base; keep fragment and voice behavior intact. Tier
  events can wait for a calm moment; see
  [papyrus-bridge.md](papyrus-bridge.md#when-tier-events-arrive).
- Test absent/malformed profiles and missing runtime in an isolated setup before
  claiming compatibility in those conditions.
- The author test kit in [AuthorTesting.md](AuthorTesting.md) reaches each tier,
  mood and gate without playing up to it.

Automated validation does not replace these checks. Keep the original files and
your backups until the converted follower has passed its own in-game checks.
