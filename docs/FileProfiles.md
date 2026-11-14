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
schema_version = 2

[npc]
plugin = "ExampleFollower.esp"
form_id = "0x000800"

[romance]
starting_level = "Stranger"
likes = ["Quests Completed"]
favorites = ["Dungeons Cleared"]
dislikes = ["Items Stolen"]
pet_peeves = ["Murders"]

[unlocks]
Friend = ["She tells you where she grew up"]
Lover = ["A key to her house", "Her mother's ring"]
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

## Favorites, pet peeves and unlock lines

Version 2 files add three things. Version 1 files still load and behave as before.
Set `schema_version` to 1 or 2 (3 adds `[moments]`, below).

- `favorites` in `[romance]` lists statistics like `likes` does, but each one
  counts double when earned.
- `pet_peeves` lists statistics like `dislikes` does, but each one costs double.
- A personality may have at most 3 favorites and at most 3 pet peeves. A statistic
  may appear in one list only. A file that breaks either rule is rejected.
- `[unlocks]` is a table keyed by tier name: Acquaintance, Friend, Confidant, Lover
  or Spouse. Stranger cannot be named. Each tier takes a list of 1 to 4 lines, and
  each line is 1 to 60 characters.

Unlock lines are text for the player, not conditions. They do not unlock anything
by themselves. The ledger names the next tier's lines on the follower's page, and
the tier popup lists them under "Unlocked" when the tier is reached. Only files in
`Profiles/` may have `[unlocks]`. A player profile may not.

## Moments (schema version 3)

Version 3 adds one optional table, `[moments]`, for followers who share time with the
player in an OStim scene. See [OStim.md](OStim.md) for what a moment is. Version 1
and 2 files load as before. Set `schema_version` to 3 to use the table.

```toml
schema_version = 3

[moments]
affection = "Friend"      # a tier name, or "never"
closeness = "Confidant"
intimacy  = "Lover"
feeling   = "warm"        # "warm" (default), "treasured" (double points), "reserved" (counts, no points)
```

- Each key is optional. A welcome tier is one of Stranger, Acquaintance, Friend,
  Confidant, Lover or Spouse, or `"never"`. The defaults are Friend, Confidant and Lover.
- An unknown key, an unknown tier or a bad `feeling` is an error and the file is rejected.
  `[moments]` in a version 1 or 2 file is an unknown field.
- The checker warns when a deeper kind is welcome at a lower tier than a lighter one.
- Only files in `Profiles/` may have `[moments]`. A player profile may not, so player-made
  bonds use the defaults.

Romantasy writes a player-made bond file as version 2 only when the bond has a
favorite or a pet peeve. Otherwise it stays version 1. Old saves and version 1
files work unchanged.

## New bond and player-owned profiles

**New bond** saves the bond's personality in your save game. It also writes a TOML
copy to `Data/SKSE/Plugins/Romantasy/PlayerProfiles/`. Edit personality updates
both; neither action adds romance or preference factions. Player profiles use the same
plugin/local NPC-base identity, statistic names, and dialogue queries as supplied
profiles. No Creation Kit or follower-plugin changes are needed to add a bond.
This does not create voiced dialogue for an NPC that has none.

Player ownership comes from the separate directory. Mod-supplied `Profiles/`
definitions and existing author integrations remain protected. A supplied profile
takes priority over a player profile for the same NPC. A player profile continues
to show **Player-created personality**, with edit, reset, and remove controls.
NPC bases generated at runtime have no stable plugin identity and are not offered.

The save is the truth for player-created bonds. The files in `PlayerProfiles` are
copies. Romantasy reads them for new characters and for saves made before this
version. A save that already holds its bonds ignores the files for other NPCs, so
editing or deleting a file does not change that save. Earned points and bonds both
live in each save's SKSE cosave. For non-unique NPCs, references share the base
profile's preferences but keep independent balances. Reset bond resets only the selected actor in the current
save and does not change profile defaults. Profiles load at startup; in-game edits
also apply immediately.

Generated filenames are `<plugin-name>.<local-id>.toml`. Existing player filenames
are retained when editing. Under MO2, newly generated files normally appear in
Overwrite or the configured output mod. Existing mapped files may be updated in
their providing mod. Keep PlayerProfiles when updating Romantasy; deployment does
not replace them. An MO2 profile isolates configuration only when it enables a
different set of profile files.

Remove bond removes the bond from the current save and from the ledger. It never
touches the file in `PlayerProfiles`, so other saves keep their bond. New bond can
add it again with fresh preferences and zero points for the selected actor.
Romantasy no longer writes `enabled = false`. An `enabled` key in a file is still
read, and only in PlayerProfiles, not in mod-supplied Profiles.

Old player-enrolled bonds migrate when their actors become available. Romantasy
reads their existing preferences, writes the player file, preserves saved points,
and then clears their legacy romance/preference factions. Other factions are
untouched. If the copy in `PlayerProfiles` cannot be written, the bond is still
stored in the save and the failure is logged; the write is not retried automatically.
A migration that does not complete keeps the old enrollment and runs again when the
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
| `Romantasy_Mood` | 1 Strained, 2 Settled, 3 Warm. A bond that has never followed reads 2 | 0 |

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

For a line that plays only while the follower is feeling warm toward the player:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Mood        == 3
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

TOML contains supplied configuration and copies of player-created configuration.
Earned points and player-created bonds are stored in each save's SKSE cosave.
Each is keyed by the NPC's plugin file name and local ID, so a load order change
does not move or lose them. If a follower's plugin is not loaded, Romantasy keeps
that bond's points and writes them back on every save. They return when the plugin
returns. A HUD line says how many bonds are being kept, once per game session, and
again if the number changes. This protection starts with the first save made with
this version; a bond that an older version already dropped cannot be recovered. A damaged
record in the cosave costs only the entries after the damage. Legacy base-keyed balances
may be inherited by unique file-defined NPCs, while spawned instances must not
inherit another instance's balance. Loading/reverting saves retains configuration
and rebuilds runtime state from the appropriate save.

Each bond's journal (its recent changes, daily totals, biggest moments and
milestones) is also stored in the save, in its own record. It is kept and written
back for a follower whose plugin is not loaded, the same as points, and it returns
with the plugin. A journal read from a save is repaired before use, and a journal
that cannot be written never costs the points record. A bond from before the journal
existed starts its journal at its next change. Removing a bond erases its journal. A save loaded and saved with an older Romantasy
loses its journals.

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

You can also give it one or more files, or a folder (every `.toml` directly in it,
not its subfolders). Each finding is one line, `file(line): error: message` or
`file(line): warning: message`; the line is left out when it is not known. Options:

- `--strict` makes warnings change the exit code.
- `--json` prints the findings as one JSON array (file, line, level, message) and
  nothing else.
- `--player` checks the files as player profiles.

Warnings are likely mistakes in a file that still loads: no likes, dislikes,
favorites or pet peeves at all, a key the format does not know (with the nearest
known name suggested), an unlock line over 48 characters (it will wrap on the popup; 48 fit on one line),
and a `plugin` name without an `.esp`, `.esm` or `.esl` ending. Exit codes: 0 clean
(warnings alone give 0), 1 at least one error, 2 warnings only with `--strict`,
3 the command line was wrong. See [AuthorTesting.md](AuthorTesting.md).

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

The player's **Relationship pace** setting changes what a gain from these events is
worth: Fast doubles it, Slow burn halves it (the odd half is carried to the next
gain) and holds a bond at the top of the tier below until enough days together have
passed (Friend 3, Confidant 7, Lover 14, Spouse 21). Losses are never changed, and
the amounts a follower's script sends through `ModifyPoints` or `ApplyPreference`
are never doubled or halved, though under Slow burn a gate can still hold a script's
gain until enough days together have passed. The values in the table are the Standard pace.

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
