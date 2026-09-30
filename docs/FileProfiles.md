# Romantasy 3.0 profiles and dialogue queries

Romantasy 3.0 supports TOML NPC profiles and actor-specific dialogue queries.
Supplied profiles and player-created bonds operate without romance factions.
Legacy integration remains available for unconverted follower mods.

For existing followers, follow [the conversion procedure](FollowerConversion.md).
Mornhilde's Friend-tier trade response was confirmed in Skyrim during development;
see the [release notes](../CHANGELOG.md) for verification boundaries.

## Author an NPC profile

Install one `.toml` file per NPC in `Data/SKSE/Plugins/Romantasy/Profiles/`.
MO2 mods use `SKSE/Plugins/Romantasy/Profiles/` within the mod folder. The shipped
`ExampleFollower.toml.example` is inert until renamed and configured.

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

Use the NPC_ base record's **plugin-local** hexadecimal ID, not an ACHR reference
or the load-order-prefixed runtime FormID. For example, a normal plugin's
`05000800` becomes `0x000800`; an ESL's local ID is at most `0xFFF`. Pair it with
the plugin that defines that NPC record. Skyrim resolves the loaded form at startup.
Unique NPCs and separate references of non-unique NPCs use separate runtime balances.

`starting_level` defaults to Stranger; omitted likes/dislikes are empty. Use the
statistic labels below or their stable `ROM_*` identifiers. Preferences use the
existing point weights. A saved balance, including zero, takes precedence over
the starting level. Editing this file does not reset earned points.

Profiles load once when game data loads. Restart Skyrim after editing them.
Unknown keys, misspelled statistics, invalid levels, conflicting opinions, and
duplicate NPC definitions are rejected with diagnostics in `Romantasy.log`.
Duplicate files for the same plugin/local-ID pair are all rejected; filenames
do not establish an override order. An invalid file does not discard other valid
profiles. Missing plugins and IDs that do not resolve to eligible NPCs are skipped.

The file owns that NPC's personality. Player edits and the legacy author
registration API cannot replace it. Do not call `RegisterAuthorFollower` for a
file-defined NPC. Existing point adjustments and preference events can still use
Romantasy's Papyrus API; native file profiles need no enrollment script or globals.

## New bond and player-owned profiles

In 3.0, **New bond** writes a TOML profile to
`Data/SKSE/Plugins/Romantasy/PlayerProfiles/`. Edit personality updates that file;
neither action adds romance or preference factions. Player profiles use the same
plugin/local NPC-base identity, statistic names, and dialogue queries as supplied
profiles. No Creation Kit or follower-plugin changes are needed to add a bond.
This does not create voiced dialogue for an NPC that has none.

Player ownership comes from the separate directory. Mod-supplied `Profiles/`
definitions and existing author integrations remain protected. A supplied profile
takes priority over a player profile for the same NPC. A player profile continues
to show **Player-created personality**, with edit, reset, and remove controls.
NPC bases generated at runtime have no stable plugin identity and are not offered.

Configuration is shared by all saves that use these files. Editing preferences or
removing a bond affects those saves; **earned points stay in each save's SKSE
cosave**. For non-unique NPCs, references share the base profile's preferences but
keep independent balances. Reset bond resets only the selected actor in the current
save and does not change profile defaults. Profiles load at startup; in-game edits
also apply immediately.

Generated filenames are `<plugin-name>.<local-id>.toml`. Existing player filenames
are retained when editing. Under MO2, newly generated files normally appear in
Overwrite or the configured output mod. Existing mapped files may be updated in
their providing mod. Keep PlayerProfiles when updating Romantasy; deployment does
not replace them. An MO2 profile isolates configuration only when it enables a
different set of profile files.

Remove bond writes `enabled = false` under `[romance]` and removes it from the
ledger. This small disabled file is intentional: it prevents old save enrollment
markers or lower-priority MO2 files from reviving the bond. New bond can re-enable
it with fresh preferences and zero points for the selected actor. Do not delete
the disabled file if you want that removal to survive loading an older save.
`enabled` is accepted only in PlayerProfiles, not in mod-supplied Profiles.

Old player-enrolled bonds migrate when their actors become available. Romantasy
reads their existing preferences, writes the player file, preserves saved points,
and then clears their legacy romance/preference factions. Other factions are
untouched. A write failure leaves the previous state intact and logs the reason;
no success is reported for a failed player edit. Migration can retry when the
actor is refreshed. If several old instances share one NPC base, the first
successfully migrated instance supplies that base's preferences.

Validate generated profiles with:

```powershell
.\tools\romantasy-profile-check.exe 'D:\path\to\PlayerProfiles' --player
```

## Dialogue conditions

Use the Creation Kit's existing **GetGraphVariableInt** function, **Run On:
Subject**, with the following text in its variable-name parameter:

| Variable name | Result for the subject NPC | Unmanaged subject |
|---|---|---|
| `Romantasy_Registered` | 1 when Romantasy has a profile/state for this actor | 0 |
| `Romantasy_Level` | Current level, 1 through 6 | 0 |
| `Romantasy_Points` | Current earned points | -1 |

Example for a line requiring Friend or higher: add these two **AND** conditions
to its INFO, both running on Subject:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

For a point threshold, replace the second condition with:

```text
GetGraphVariableInt  Romantasy_Points      >= 1250
```

Always include the Registered guard, especially for `<`, `<=`, and `!=` tests.
Use the reserved names exactly, without quotes in the CK text field. The subject
must be the intended NPC, normally the speaker. Running on Player would query the
player instead. These are reserved queries handled by the DLL, not animation
variables that follower scripts should set.

| Level | Name | Minimum points | Old faction rank |
|---|---|---|---|
| 1 | Stranger | 0 | 0 |
| 2 | Acquaintance | 500 | 1 |
| 3 | Friend | 1000 | 2 |
| 4 | Confidant | 1500 | 3 |
| 5 | Lover | 2000 | 4 |
| 6 | Spouse | 2500 | 5 |

Do not copy old faction-rank comparisons unchanged: the new level uses the public
API's **1-based** numbering. The Spouse tier is a romance tier; it does not perform
Skyrim's marriage quest or wedding.

Condition reads do not enroll NPCs, add points, write factions, or emit events.
Other variable names are passed to the previously installed Skyrim callback with
the same subject, parameters, and result reference. Native Papyrus
`GetAnimationVariableInt` calls and the console command are not this CTDA bridge.

The bridge verifies its command-table signature, replacement pointer, and a
synthetic CTDA's parameter passthrough before enabling decoding. Expected log:
`Romantasy dialogue bridge installed; CTDA BSFixedString parameter probe passed`.
That startup probe does not prove that CK persisted a real INFO correctly.

Without the DLL, the engine sees ordinary graph-variable queries. The intended
Registered guard should prevent romance dialogue, but this behavior must be
verified with the actual follower. No missing-DLL compatibility claim is made yet.

## Persistence and migration boundaries

TOML contains supplied or player-created configuration. Earned points remain in each save's SKSE cosave and
use the existing serialized actor-reference identity. Legacy base-keyed balances
may be inherited by unique file-defined NPCs, while spawned instances must not
inherit another instance's balance. Loading/reverting saves retains configuration
and rebuilds runtime state from the appropriate save.

Romantasy does not strip factions or masters from follower ESPs, rewrite
their scripts/dialogue, or delete the legacy Romantasy records. A follower's own
scripts could still write its old factions until converted. Removing a profile
from an unconverted legacy follower may leave that follower managed by the old
path. Full faction retirement requires a separate migration after acceptance.

## Validate and build

The standalone `tools/romantasy-profile-check.exe` accepts a directory containing
profiles. It validates syntax and profile rules without Skyrim. It does not check
that the declared plugin or NPC exists in the load order.

```powershell
.\tools\romantasy-profile-check.exe 'D:\path\to\SKSE\Plugins\Romantasy\Profiles'
```

Build from the repository root. Close Skyrim and editors before building. For an
isolated candidate, set `XSE_TES5_MODS_PATH` to a staging folder in that shell:

```powershell
$env:XSE_TES5_MODS_PATH = 'C:\RomantasyBuild\auto-install'
xmake -y
xmake build romantasy-profile-check
xmake build romantasy-profile-tests
xmake run romantasy-profile-tests
xmake build romantasy-ui-tests
xmake run romantasy-ui-tests
node tests/RuntimeCompatibilityContractTests.mjs
```

Deploy the complete mod payload, including fonts, the inert example, documentation,
and validator. Preserve installed settings and PlayerProfiles. The private release
workflow signs and hash-verifies the explicit payload.

## Manual acceptance: incomplete

Mornhilde has passed a limited loaded-save condition check and Friend-tier trade
playback test. The full matrix below is still
incomplete; untested cases remain **NOT RUN**.

1. Audit/back up the selected test follower. Remove its romance/preference faction
   dependencies in a candidate copy; preserve unrelated factions and behavior.
2. Create her real TOML profile and replace dialogue conditions with the queries
   above. Save and reopen in CK; confirm text, Subject, AND flags, and comparisons.
3. Launch the candidate; confirm profile and bridge-probe success in the log.
   Confirm ordinary graph-variable conditions still work.
4. Test below, at, and above thresholds; point loss; a zero-point balance; two
   separately configured NPCs; non-unique instances if supported by the follower.
5. Save/reload and load a different save; verify points never cross actors or saves.
   Verify a load-order change resolves the same NPC and retained save identity.
6. Test absent profile, malformed profile, missing DLL, and missing dependency in
   a disposable test profile. Confirm guarded dialogue stays unavailable.
7. Verify dashboard, stat gains, active-follower checks, Papyrus point adjustments,
   and bark events. Complete this proof before removing the legacy system.

## Statistic identifiers

The following table is the shared parser/runtime vocabulary. Likes add the listed
points per event/delta; dislikes subtract them, subject to normal Romantasy rules.

| Label | Stable identifier | Points |
|---|---|---|
| Locations Discovered | `ROM_LocationsDiscovered` | 2 |
| Dungeons Cleared | `ROM_DungeonsCleared` | 2 |
| Days Passed | `ROM_DaysPassed` | 2 |
| Standing Stones Found | `ROM_StandingStonesFound` | 2 |
| Chests Looted | `ROM_ChestsLooted` | 1 |
| Skill Increases | `ROM_SkillIncreases` | 2 |
| Skill Books Read | `ROM_SkillBooksRead` | 1 |
| Barters | `ROM_Barters` | 1 |
| Persuasions | `ROM_Persuasions` | 1 |
| Bribes | `ROM_Bribes` | 1 |
| Intimidations | `ROM_Intimidations` | 1 |
| Diseases Contracted | `ROM_DiseasesContracted` | 1 |
| Days as a Vampire | `ROM_DaysVampire` | 2 |
| Days as a Werewolf | `ROM_DaysWerewolf` | 2 |
| Necks Bitten | `ROM_NecksBitten` | 2 |
| Vampirism Cures | `ROM_VampirismCures` | 2 |
| Werewolf Transformations | `ROM_WerewolfTransformations` | 5 |
| Mauls | `ROM_Mauls` | 2 |
| Quests Completed | `ROM_QuestsCompleted` | 3 |
| Misc Objectives Completed | `ROM_MiscObjectivesCompleted` | 3 |
| Main Quests Completed | `ROM_MainQuestsCompleted` | 5 |
| Side Quests Completed | `ROM_SideQuestsCompleted` | 5 |
| The Companions Quests Completed | `ROM_CompanionsCompleted` | 10 |
| College of Winterhold Quests Completed | `ROM_CollegeCompleted` | 10 |
| Thieves' Guild Quests Completed | `ROM_ThievesCompleted` | 10 |
| The Dark Brotherhood Quests Completed | `ROM_DarkBrotherhoodCompleted` | 10 |
| Civil War Quests Completed | `ROM_CivilWarCompleted` | 10 |
| Daedric Quests Completed | `ROM_DaedricCompleted` | 10 |
| Dawnguard Quests Completed | `ROM_DawnguardCompleted` | 10 |
| Dragonborn Quests Completed | `ROM_DragonbornCompleted` | 10 |
| Questlines Completed | `ROM_QuestlinesCompleted` | 20 |
| People Killed | `ROM_PeopleKilled` | 1 |
| Animals Killed | `ROM_AnimalsKilled` | 1 |
| Creatures Killed | `ROM_CreaturesKilled` | 1 |
| Undead Killed | `ROM_UndeadKilled` | 1 |
| Daedra Killed | `ROM_DaedraKilled` | 1 |
| Automatons Killed | `ROM_AutomatonsKilled` | 1 |
| Critical Strikes | `ROM_CriticalStrikes` | 1 |
| Sneak Attacks | `ROM_SneakAttacks` | 1 |
| Backstabs | `ROM_Backstabs` | 1 |
| Weapons Disarmed | `ROM_WeaponsDisarmed` | 1 |
| Bunnies Slaughtered | `ROM_BunniesSlaughtered` | 1 |
| Spells Learned | `ROM_SpellsLearned` | 2 |
| Dragon Souls Collected | `ROM_DragonSoulsCollected` | 2 |
| Shouts Learned | `ROM_ShoutsLearned` | 2 |
| Souls Trapped | `ROM_SoulsTrapped` | 1 |
| Magic Items Made | `ROM_MagicItemsMade` | 1 |
| Weapons Made | `ROM_WeaponsMade` | 1 |
| Armor Made | `ROM_ArmorMade` | 1 |
| Potions Mixed | `ROM_PotionsMixed` | 1 |
| Poisons Mixed | `ROM_PoisonsMixed` | 1 |
| Locks Picked | `ROM_LocksPicked` | 1 |
| Pockets Picked | `ROM_PocketsPicked` | 1 |
| Items Stolen | `ROM_ItemsStolen` | 1 |
| Assaults | `ROM_Assaults` | 2 |
| Murders | `ROM_Murders` | 5 |
| Horses Stolen | `ROM_HorsesStolen` | 2 |
| Trespasses | `ROM_Trespasses` | 2 |
