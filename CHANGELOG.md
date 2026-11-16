# Changelog

## 4.0.1 - 2026-11-16

- Fix Health saying OStim is too old when a current OStim is installed. Romantasy now
  asks OStim directly (OStim 7.5.1c and newer) and uses the SKSE message only for an
  older OStim. The SKSE message could miss OStim on Skyrim 1.5.97, or when another SKSE
  plugin failed to load. When OStim is loaded and gives no answer, Health now says
  **OStim is installed, but Romantasy could not reach it.**

## 4.0.0 - 2026-11-14

- Add roster tools to the ledger. Type in **Find a companion** to filter by name.
  Choose an order: **Closest** (the default), **Name** or **Recent**. Bonds are
  grouped under **Following** and **Away**. Rows stay where they are while the
  ledger is open; the order is applied when it opens and when you change the order
  or the find text. The order is kept in `settings.json` as `rosterOrder`.
- Add three comfort settings: **Ledger size** (`Standard`, `Large` or `Largest`;
  makes everything in the ledger bigger), **Stronger contrast** (brighter text,
  solid panels and thicker outlines) and **Reduce motion** (stops the drifting
  backdrop and the larger animations). The tier popup follows all three, also
  before the ledger is first opened. Whispers follow the last two. The keys in
  `settings.json` are `ledgerSize`, `highContrast` and `reduceMotion`.
- The bond tools (Edit personality, Reset bond, Remove bond) now sit in the
  Milestones card, so they fit at every size.
- Almost every word the ledger shows now lives in one file, `Magelight\Romantasy\js\strings.js`,
  so it can be changed or translated. See `docs/Translating.md`.
- Add an optional bridge to SkyrimNet. When SkyrimNet is installed, Romantasy adds one
  line to a companion's prompt (tier, mood, days together), sends tier changes,
  reunions and time apart as events, and lets SkyrimNet's AI nudge a bond for a kind
  word (+5) or a harsh word (-5), at most one of each per in-game hour and 15 a day
  each way. Settings has two new rows, Tell SkyrimNet about bonds and Conversations can
  move the bond (both on by default, shown only when SkyrimNet is found). Without
  SkyrimNet nothing changes. Romantasy ships a small SkyrimNet layer, `coldsun.romantasy`.
  Details: `docs/SkyrimNet.md`.
- Add an optional bridge to OStim Standalone. When OStim 7.3.3 or newer is installed,
  Romantasy holds tier popups and whispers while you are in a scene and shows them
  afterwards. A scene with one bonded companion counts as a shared moment when it was
  just the two of you, nothing in it was marked as force, something happened, it ran at
  least 20 seconds and the bond has reached the tier where the companion welcomes that
  kind of moment (Friend, Confidant or Lover for a tender moment, a quiet hour and time
  alone, by default). A moment gives 5 points (10 for a companion who treasures them), once
  a day, never takes points and never strains the mood. The journal and Recent changes name it
  by its kind, and the first one adds the milestone "A first moment together". A profile
  file can set each welcome tier and a feeling in a new `[moments]` table (`schema_version = 3`).
  With OStim 7.4 or newer, a follower mod can call the new `TryShareMoment`,
  `CanShareMoment`, `GetMomentCount` and `GetDaysSinceMoment` functions to start a gentle
  30-second moment or to ask whether one would be welcome, and listen for the new
  `Romantasy_OnSharedMoment` and `Romantasy_OnMomentRefused` events. `DebugSharedMoment`
  tests the rules without OStim. Settings has two new rows, Shared moments and Quiet during
  scenes (both on by default, shown only when OStim is found). OStim has no consent flag, so
  Romantasy only sees the tags a scene's author set. Without OStim nothing changes. The
  bridge has not been tried in the game yet. Romantasy copies OStim's interface headers
  (GPL-3.0) into its source. Details: `docs/OStim.md`.
- Add an author test kit for people who write follower mods. With `"authorMode": true`
  set by hand in `settings.json` (no row in Settings; the Health section shows
  "Author mode is on."), six Papyrus functions work: `DebugDeed`, `DebugSetPoints`,
  `DebugSetDaysTogether`, `DebugSetMood`, `DebugAnnounceNow` and `DebugDescribe`.
  With it off, each logs one line and does nothing. `DebugDeed` is paced and gated
  like a player's deed; `DebugSetPoints` is the way around a gate. The Papyrus API
  is now capability version 9. With Detailed log on, every deed writes a `why:` line
  in `Romantasy.log` that says what changed and why, or why nothing did. The
  profile checker warns about a personality with no opinions, an unknown key, an
  unlock line that will wrap on the popup and a plugin name with no ending; it accepts a
  folder, `--strict` (exit 2 on warnings) and `--json`. New guide:
  `docs/AuthorTesting.md`.
- Add whispers: short cards on the game HUD that say what just moved a bond, such
  as "Dungeons Cleared · +2", "Feels warm toward you" or "Glad to have you back". They
  show for a deed that moved points (the follower must be following), a mood that
  turns warm or strained, a reunion or time apart, and a tier gained or lost when
  that tier's popup is switched off. One card shows at a time, for about four
  seconds, in a corner you choose. The first deed shows at once; deeds for the
  same follower within two seconds of each other, while a card is up or waiting,
  merge into one card. A whisper never pauses the game, takes the cursor or changes points,
  tiers or mood. It waits while a menu, dialogue or loading screen is up, for up
  to 30 seconds, then it is dropped. A load drops the old game's whispers. No
  whisper shows when a Slow burn gate held the whole gain. Settings has two new
  rows, Whispers (on by default) and Whisper corner (top right by default). If
  the second Magelight view is not available, whispers use the game's own
  notifications, and the Health section says so.
- Add a journal to every bond. Open it with the Journal button at the foot of the
  Recent changes card in a bond's page in the ledger. It keeps the last 30
  changes, the points gained and lost on each of the last 60 in-game days, the 5
  biggest gains and the 5 biggest losses, and milestones: when the journal began,
  each tier reached (written the first time, and again after a reset), and each fall to a lower tier (the last 20).
  Days are shown as Day N. Day 1 is the first day of the save, so it is one more
  than the game's Days Passed. Hovering a day in the chart with the mouse shows that
  day's numbers; with keys or a gamepad, the totals line under the chart is what
  shows. A change is written when it happens, with the points
  really applied, even when its tier popup is still waiting for a calm moment.
  Reaching a tier again is not written a second time, but each fall is. Resetting a
  bond is a fresh start: History gets one "Bond reset" row with the points lost, and
  a "Fell to Stranger" milestone if the bond stood higher. The reset is not counted
  among "Wounded most" or in the day totals in the chart. The tiers the bond had
  reached are cleared so they are written again when it reaches them again.
  Removing a bond erases its journal. A bond from before this version starts its
  journal at its next change. The journal is kept in the save and is kept for a
  follower whose mod is missing, as points are. It only records: it never changes
  points, tiers or mood. Recent changes shows the change asked for; the journal
  shows the points applied, so a loss at 0 points is not in the journal.
- Tier popups now wait for a calm moment by default: out of combat, and in an inn,
  a home, a town or another settlement, or just after waking or waiting. A change
  never waits longer than one in-game day. The `Romantasy_OnLevelChanged` and
  `Romantasy_OnLevelLost` events wait with the popup. Several changes for one bond
  arrive as one, and nothing is announced if the bond is back where it started.
  Points and the tier shown in the ledger change at once, and waiting changes are
  kept in the save. To get the old timing back, open Settings and set "Tier changes
  arrive" to "At once". With a calm moment, the deed that crosses a tier produces no
  follower reaction at that moment; the reaction comes with the popup.
- Add Relationship pace in Settings: Slow burn, Standard (the default) or Fast.
  Fast doubles the points from deeds. Slow burn halves them and asks for days
  together before each higher tier: Friend 3, Confidant 7, Lover 14, Spouse 21. The
  dossier shows the gate ahead. Losses are never changed, and a follower's own script
  amounts are never doubled or halved, though under Slow burn a gate can still hold
  a script's gain until enough days together have passed. A gate never takes points away and never stops gains inside the
  tier a bond already holds.
- Add a mood to every bond: Strained, Settled or Warm. Gains warm it, losses
  strain it, and it drifts back to Settled by 25 for each in-game day. It never
  changes points or tiers. The dossier shows it, dialogue can read it with
  `Romantasy_Mood`, and Papyrus can read it with `GetMood` and `GetMoodName`
  (capability version 7). Mood and days together are kept in the save.
- Add three events for follower scripts: `Romantasy_OnLevelLost` when a bond drops
  a tier, `Romantasy_OnTimeApart` once after 3 in-game days away, and
  `Romantasy_OnReunion` when a follower away for at least a day follows again.
  `Romantasy_OnLevelChanged` still fires only for a gain.
- Add favorites and pet peeves to personalities. A favorite counts double when
  earned and a pet peeve costs double. Each personality may have three of each.
  The ledger tags them and shows the per-event value, and New bond and Edit have
  Favorite and Pet peeve stamps. Personality files use `schema_version = 2` for
  `favorites` and `pet_peeves`; version 1 files and old saves work unchanged.
- Add `[unlocks]` to personality files: 1 to 4 short lines per tier. The ledger
  names the next tier's lines and the tier popup lists them when a tier is gained.
  The lines are text only and do not unlock anything by themselves.
- Add a Health section to Settings. It says whether Romantasy is working and lists
  anything wrong, such as a missing or old Magelight UI, a missing or outdated
  `CS_Romantasy.esp`, rejected personality files, a damaged save record or
  settings that could not be saved. Warnings and errors show first in the list and
  mark the Settings button; when all is well the section sits at the foot with the
  versions. Bonds kept for mods that are not loaded are shown as a note only.
  A problem also shows once per game session as a single HUD line.
- Fix "Open with Favorites Menu" not working on installs without a mod that keeps
  editor IDs. The Romantasy power is now found by plugin name and local ID.
- Add a Detailed log switch in Settings. The log is quieter by default: per-event
  lines (points awarded, popups queued, Papyrus calls) are written only when the
  switch is on. Load summaries, tier changes, warnings and errors are always kept.
- Fix a rare freeze when a follower's script changed points while the ledger was busy.
- Settings and personality files are now replaced whole, so a crash while saving
  cannot leave a half-written file.
- Keyboard and gamepad focus now reaches the top and bottom of the Settings list,
  so the Health text can be read without a mouse.
- A follower who has just come into the loaded area may take up to five seconds to
  start earning points from deeds.
- Replace the built-in dashboard and popups with the Rosewater ledger, a page
  hosted by Magelight UI 1.6 or newer. Magelight UI is now required for the
  ledger and popups; relationship tracking still runs without it.
- Keep a bond's points when a follower's mod is missing from the load order. The
  bond returns with its mod, and a HUD line says how many bonds are being kept.
  This protection starts with the first save made with this version; a bond that an
  older version already dropped cannot be recovered.
- Save player-created bonds with the game. Removing a bond now affects only the
  current save, not other saves that share the same profile file.
- Do not load a save written by this version in 3.0.1 or older. Those versions skip
  the new save records, so the bond points in that save would be lost.

## 3.0.1 - 2026-10-07

- Names and text in Russian, Greek, Chinese, Japanese, Korean and accented Latin
  (Polish, Czech, Turkish and others) now display with their own letters instead
  of question marks. Letters the bundled fonts lack are drawn with Windows' fonts:
  Segoe UI at a matching weight, then Microsoft YaHei, Yu Gothic and Malgun Gothic,
  ordered by the Windows display language. English text renders as before.
- Names in any alphabet are capitalised in titles and labels, as English names are.

## 3.0.0 - 2026-09-30

- Replace the external web dashboard with screens drawn by the plugin itself.
  Meridian UI and PRISMA are no longer dependencies.
- Add TOML NPC profiles and reserved `GetGraphVariableInt` dialogue queries for
  registration, 1-based relationship level, and exact points.
- Store New bond personalities in player-owned TOML files. Edits apply immediately;
  removal persists as a disabled profile so old saves do not revive it.
- Migrate legacy player enrollments when their actors become available, preserving
  preferences and earned points. Keep supplied personalities protected.
- Preserve independent, save-specific point totals, including saved zero balances.
- Retain controller navigation, contextual hints, and preference-list scrolling.
  Use the Favorites power for controller opening; remove the LB + X opening chord.
- Retain legacy follower integrations and Papyrus API capability version 6.
- Synchronize the DLL resource, SKSE declaration, logs, and UI version at 3.0.0.

### Verification

The release build passed 20 profile/persistence tests, 98 UI tests, and the runtime
compatibility contract. The installed DLL was signed and hash-verified. Controller
support and Mornhilde's Friend-tier trade response were confirmed during earlier
in-game testing. These results do not establish every gameplay path in this build.
New bond/edit/remove migration, full restart/save-load behavior through MO2, the
complete dialogue tier matrix, and missing-runtime behavior retain separate
in-game acceptance checks in the developer documentation.
