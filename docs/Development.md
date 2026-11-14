# Development notes

Working notes for building, deploying and extending Romantasy. User-facing
documentation lives in [README.md](../README.md); profile authoring in
[FileProfiles.md](FileProfiles.md). The dashboard is the page in `view/` (see
Magelight page below).

## Build

Build from the repository root only:

```
xmake -y
```

The CommonLib build rule auto-installs the DLL when `XSE_TES5_MODS_PATH` is set.
By default it installs into `<XSE_TES5_MODS_PATH>\Romantasy`; when the installed
mod folder has a different name, persist it once and every later build lands
there:

```
xmake f --modfolder="<mod folder name>"
```

On this workstation builds install into `Romantasy - Magelight`
(`xmake f -p windows -a x64 -m release --modfolder="Romantasy - Magelight" -y`), and
`scripts/deploy.ps1` defaults to it.

Never run `xmake -P <repo>` from another working directory: the build directory
can mis-resolve and the DLL ships without the generated version resource, which
SKSE then refuses to load. After a build, check that the DLL's `FileVersion`
matches `set_version` in `xmake.lua`.

Close Skyrim, the Creation Kit and xEdit before building or deploying; the
install step cannot replace a loaded DLL.

Other targets (not built by default):

| Target | Purpose |
|---|---|
| `romantasy-page-tests` | Page payload validation, popup wait policy and the health report (`xmake build romantasy-page-tests && xmake run romantasy-page-tests`) |
| `romantasy-profile-tests` | TOML profile, persistence, lock, view, throttle and atomic-file tests |
| `romantasy-profile-check` | Command-line profile validator shipped with the mod |

## Deploy

`scripts/deploy.ps1` signs the built DLL (Authenticode, RFC 3161 timestamp) and
copies the complete payload into the mod folder with hash verification: DLL,
the ledger page (`view/` without `view/dev/`, to `Magelight\Romantasy`), ESP,
scripts, sounds, licenses, example profiles, the profile checker and the shipped
documentation. Page files a previous deploy left in `Magelight\Romantasy` that
the payload no longer names are deleted. Pass `-ModRoot` to target another folder.

Local settings in the mod folder are never overwritten.

## Check before a deploy

`scripts/check.ps1` runs every check in one go. Close Skyrim first, then run it from
anywhere (it moves to the repo root):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check.ps1
```

It prints one line per step, `PASS`, `FAIL` or `SKIP` with a short detail, and a last
line `PASS` or `FAIL`. The exit code is 0 for PASS and 1 for FAIL. The steps are:
build (`xmake -y`, then `xmake build romantasy-profile-check`; fails if SkyrimSE.exe
is running), the profile tests and the page tests (each built, then run), the view
tests, the contract test, the SkyrimNet layer test, the check script test
(`tests/CheckScript.mjs`), the profile checker with `--strict` over
`examples\profiles`, the checker cli test (`tests/ProfileCheckCli.mjs`), the version (the same in `xmake.lua`,
`src\plugin.h`, the first line of `README.md`, the top heading of `CHANGELOG.md` and
the built DLL; an `Unreleased` section may sit above it), and the `.pex` files (each
not older than its `.psc`). A step that cannot run because an earlier one failed shows
`SKIP`.

The build step installs the unsigned DLL into the mod folder before any test runs
(the project's normal build rule). Only `scripts\deploy.ps1` signs it.

- `-SkipBuild` runs no xmake and uses the binaries already built. Its SKIP is not a
  failure, and a suite whose program is not built fails. `-Deploy` with `-SkipBuild`
  is refused with FAIL.
- `-Deploy` runs `scripts\deploy.ps1` after a PASS. A failed deploy makes the
  result FAIL.
- `-DryRun <folder>` runs only the version and `.pex` steps, on a copy of the files
  in that folder. `tests/CheckScript.mjs` uses it.

`scripts/deploy.ps1` is not changed by it.

## Save format

Romantasy stores its data in the SKSE cosave. The records are:

| Record | Version | Holds |
|---|---|---|
| `RoPt` | 3 (1 and 2 still load) | Points per follower |
| `RoPb` | 1 | Player-created bonds |
| `RoPm` | 1 | Manual preferences |
| `RoPe` | 1 | Legacy enrollment |
| `RoMd` | 1 | Mood and company per follower |
| `RoPn` | 1 | Tier changes waiting for a calm moment |
| `RoJn` | 1 | Bond journals |
| `RoMo` | 1 | Shared moments per bond |

An identity is the plugin file name plus the local FormID. Light plugins keep a
12-bit local ID. An entry whose plugin is not loaded stays in memory as an orphan
and every save writes it back, so it returns with its plugin. When orphaned points
are kept the player sees one HUD line, once per game session, and again if the
number changes. A player file in `PlayerProfiles` whose plugin is not loaded is kept
the same way when a save starts from the files.

A truncated or malformed record keeps the complete entries before the damage, and
loading carries on with the other records. The count only bounds the read loop:
the complete entries before the data runs out are kept, and the memory reserved up
front is capped by the bytes left and by a fixed limit.

For player-created bonds the save is the truth. Create and edit also write the TOML
file in `PlayerProfiles` as a copy. Remove changes the save only. A save with the
`RoPb` record ignores files on disk for other NPCs; a save without it (made by
3.0.x) and a new game start from the files. `RoPb` does not store
`starting_level`, so a bond loaded from the save starts at level 1 unless saved
points say otherwise.

`RoMd` is a `u32` count, then per bond: the reference identity and the base
identity (each plugin name, local ID and raw ID, as in `RoPt`), the mood score
(`i16`) and its game-day stamp (`f32`), one flag byte (1 seen following, 2 away,
4 time-apart event sent), then three `f32`: game day the follower went away, game
day last seen following, and in-game days together. The reader clamps the score
and sets a negative or non-numeric days-together to 0. A save without the record
starts every bond Settled and together. The record is written after `RoPt`.
Entries whose plugin is not loaded are kept as orphans and written back, like
orphaned points.

`RoPn` is a `u32` count, then per waiting change: the reference identity and the
base identity (as in `RoPt`), the old level (`i8`), the new level (`i8`), the points
delta (`i32`) and the game day it began waiting (`f32`). The writer refuses an
entry whose levels are outside 1..6 or equal, before the first byte. A waiting
change whose follower cannot be found at a look is kept for a later look. It is
dropped on load when its plugin is not loaded, at release when its bond is switched
off or the bond is back at the level it started from, and when the bond is removed
or reset. `Save` writes the records in this order: `RoPt`, `RoMd`, manual preferences,
legacy enrollments, `RoPb`, `RoPn`, then the journals. A save without `RoPn` has
nothing waiting.

`RoJn` is a `u32` count, then per journal: the reference identity and the base
identity (as in `RoPt`), then the history (`u16` count, then per change a label
as a `u8` length and bytes, the points `i32` and the game day `f32`), the days
(`u16` count, then per day the day `i32`, gained `i32` and lost `i32`), the gains
and the losses (each a list of changes like the history), and the milestones
(`u16` count, then per milestone a kind `u8`, a level `u8` and a game day `f32`).
Limits: 30 history entries, 60 days, 5 gains, 5 losses, 26 milestones (Journal
began, 5 reached, 20 fell) and a label of at most 120 bytes. The writer refuses a
list over its limit, a label over 120 bytes, or an impossible milestone, before the first byte; the reader
treats a count over its limit as damage and keeps the complete journals before it.
A journal whose plugin is not loaded is kept as an orphan and written back by every
save. Each journal read from a save goes through `journal::Normalize` before it is
used. Each record is written on its own, so a journal that cannot be written never
costs `RoPt`.

`RoMo` is a `u32` count, then per bond: the reference identity and the base identity (as in
`RoPt`), the moment count (`u32`), then three `f32` game days: the first moment, the last
moment and the last moment that gave points. A day of -1 means never. Any other day must be
finite and at least -1; a bad day is damage, like a short record, and the complete entries
before it are kept. The writer leaves out a bond with no moment and no day, and refuses a list
holding a bad day before the first byte. An entry whose plugin is not loaded is kept as an
orphan and written back, a bond's removal erases it and a points reset keeps it. A build
without this feature ignores the record. The journal record does not change: the "A first
moment together" milestone is added to the journal when it is sent to the page, from the
first-moment day here.

The codec is `src/romance/SaveCodec.h` and `SaveCodec.cpp`, namespace
`romantasy::save`. It includes no Skyrim headers. Its tests are in
`tests/profiles/SaveCodecTests.cpp` and run with `romantasy-profile-tests`.

### Save samples

`tests/fixtures/save/` holds one binary sample per record the save codec reads:
`points_v3.bin` (`RoPt`), `player_bonds_v1.bin` (`RoPb`), `moods_v1.bin` (`RoMd`),
`pending_v1.bin` (`RoPn`), `journals_v1.bin` (`RoJn`) and `moments_v1.bin` (`RoMo`). Each is the record body only.
`tests/profiles/SaveSamplesTests.cpp` reads each one and compares it with values
written in the test, then reads every shorter prefix (a truncation sweep) and checks
the reader never crashes and never invents entries. Never regenerate a sample to make a
test pass: a new layout gets a new version number, a new sample and a reader that
keeps the old one. The folder's `README.md` says how they were made.

Three things have no sample because their readers are not in the pure codec: the
older `RoPt` versions 1 and 2, the manual preferences record (`RoPm`) and enrollment (`RoPe`). They
are read inside `RomanceManager`.

### Author test kit

The seven `Debug*` natives are in `src/papyrus/PapyrusBridge.cpp`. Each checks
`Settings::AuthorMode()` as its first statement. The why lines are built by
`src/romance/Why.{h,cpp}` (pure, no game headers) and written only inside an
`if (Settings::GetSingleton().DetailedLog())` block; the contract test checks that.
The guide for authors is `docs/AuthorTesting.md`, shipped with the mod.

### SkyrimNet bridge

Files: `src/skyrimnet/BondLine.{h,cpp}` (pure texts), `src/skyrimnet/ConversationLimits.h`
(pure limits), `src/skyrimnet/SkyrimNetBridge.{h,cpp}` (finds the DLL, events, Papyrus
registration), `Source/Scripts/Romantasy_SkyrimNet.psc` (and its .pex), and the shipped layer in
`SkyrimNet/external/coldsun.romantasy/` (manifest and one prompt file). `docs/SkyrimNet.md` is the
user guide.

Why Papyrus: SkyrimNet is built with the static C runtime (/MT) and Romantasy with the dynamic one
(/MD). Its C++ exports for decorators and actions pass `std::function` and `std::string` across
that boundary, so Romantasy calls none of them. Only `PublicGetVersion` and `PublicRegisterEvent`
(plain `const char*` and integers) are used. The decorator `romantasy_bond` and the two actions
are registered through `SkyrimNetApi.RegisterDecorator` and `RegisterAction` on every
`kPostLoadGame` and `kNewGame`, so they exist only after a save has loaded. The actions are
dispatched from the end of `RegisterDecorator`, so one call does both. Each `RegisterDecorator`
call adds one to a load counter, and every dispatch of that load carries the number it saw. When
an answer arrives (`OnDecoratorResult`, `OnActionResult`), an answer whose number is not the
current one is dropped. Any integer answer counts as registered, because `SkyrimNetApi.psc` does
not say what the function returns; a missing or non-integer answer counts as failed. SkyrimNet
asks for the decorator through Papyrus and keeps the answers for a short time, so the first prompt
after a load may come without the line, and a change can take a turn to show.

The registrations run when SkyrimNet was seen at all: found, or too old (API below 11, or no
`PublicRegisterEvent`). They need no C export, and the shipped prompt file calls `romantasy_bond`
either way. With a SkyrimNet too old the natives answer `""` and `false`, so it gets a bond line
function that returns nothing and no actions it can take. Health shows that case as a note.

Events are `persistent_generic`, sent with no originator, the follower as target and the follower
as the only audience entry. SkyrimNet prints an event whose originator is the speaking NPC as that
NPC's own line, and shows it to everyone else as speech. With no originator the follower sees it as
narration about them, and other NPCs do not see it. The export answers with the event's id, or 0 when it stored
nothing (a follower who is not loaded, or a shape it does not take). An answer of 0 is logged once
per session at info level; there is no fallback to the old shape. The texts name the player by the player
character's display name, read on the thread that builds the text (`PlayerName()` in
`RomanceManager.cpp`).

Conversation limits live in `RomanceManager::_conversations`, a map beside the mood records,
keyed like them. It is cleared on load and on revert, erased with a bond when the bond is removed,
never saved, and kept when a bond's points are reset. `ConversationEligible` only reads it
(SkyrimNet asks while it chooses an action). `ConversationNudge` checks everything again, then
applies the change and notes the attempt. A gain that Slow burn's gate holds back is still noted,
so a held attempt uses the hour and counts toward the day's cap. Both entry points do nothing,
not even take the lock, when SkyrimNet was not found or either switch is off.

The prompt file runs for `render_mode` "full" and "thoughts". It sets the decorator result once
and prints it inside a whitespace-controlled `if` that tests truthiness first and then for `""`,
so an empty result of any kind prints nothing. The printed line puts a second-person framing
sentence with `{{ player.name }}` before the result. `tests/SkyrimNetLayer.mjs` checks the manifest, the prompt
file and the deploy payload; `scripts/check.ps1` runs it as the "SkyrimNet layer" step. Run it by name:
`node tests/SkyrimNetLayer.mjs`.

### OStim bridge

Files: `src/ostim/Moments.h` (pure: kinds, action lists, welcome tiers, `Judge`, `CheckStart`),
`src/ostim/SceneTracker.h` (pure: the player's scene as the game thread sees it),
`src/ostim/OStimBridge.{h,cpp}` (finds OStim, listeners, starting a moment, the watch),
`src/ostim/api/` (OStim's interface headers, copied unchanged) and `src/ostim/THIRD_PARTY.md`
(source, tag, what is used, licence). `docs/OStim.md` is the user guide. No script of ours names
an OStim script, and `CS_Romantasy.esp` has no OStim master.

Finding OStim: at data loaded, after `Settings::Load`, the bridge sends OStim's own SKSE message
(`'OST'`, an `InterfaceExchangeMessage`, to the plugin `"OStim"`) and reads `queryInterface("Threads")`.
It never calls `LoadLibrary` and never links OStim. States are Absent, TooOld and Found; the log
says which in one line. `CanStart()` is Found with interface 2 or newer (OStim 7.4).

Thread rules. OStim calls the listeners on its own threads and under its own locks. A listener
reads only the `Thread*` or `Node*` it was given, copies plain data (ids and strings), sets or
clears the atomic `inScene` flag and hands everything else to the game thread with
`SKSE::GetTaskInterface()->AddTask`. It never calls `getThread()` or `getActor(void*)` and never
keeps a `Thread*` or `ThreadActor*`. `getThread(0)` is called only on the game thread, once a
second while a scene is tracked: a null answer means OStim dropped the thread without a stop
event, and the scene is forgotten. The scene state is cleared on a load and on a new game.
Starting a scene and stopping one go through the Papyrus VM by name
(`OLibrary.GetRandomSceneSuperloadCSV` with 46 packed arguments and `OThread.Stop`), only when
OStim is Found, with a 5 second limit on the search, a 40 second fallback stop and a 60 second
limit on a pending start that does not run while the Dialogue menu is open. Only interface 1 methods are called, plus `createThreadBuilder` and the
builder behind `CanStart()`. The `Safe` builder methods and the two furniture overloads are never
called; `src/ostim/THIRD_PARTY.md` says why.

The manager side is in `RomanceManager.cpp`: `OnSceneEnded` judges the scene, `AwardMoment` notes it
and adds the points, `TryShareMoment` and `CanShareMoment` check and begin a start. The count of
`AddTask(` in that file stays at 2, and game-thread effects go through `_lock.Defer`. A moment's
events use the same bark path as the others.

Test without OStim: `DebugSharedMoment` (author mode) runs the same `Judge` and award path. The
pure rules are in `tests/profiles/OStimMomentTests.cpp` and `OStimSceneTests.cpp`, run with
`romantasy-profile-tests`; the contract test checks the handshake shape and the listener rules. Nothing
here has been run against a real OStim: the checks that need one are in section 18 of
`docs/testing/InGameChecklist.md`. `tests/OStimDocs.mjs` checks the guide, the notices and the
deploy payload; `scripts/check.ps1` runs it as the "OStim docs" step.

## Branches and publishing

`master` is the development trunk. The GitHub repository carries source-only
release snapshots on `main`: one commit per published change, without internal
plans, conversion audits, deploy and signing scripts, or binary assets (the ESP,
sounds and images ship in the mod archive instead). The local `public` branch
tracks GitHub `main`; to publish, bring the public files up to date from
`master` on that branch, commit, and push.

## Conventions

### Game-thread dispatch

`SendBarkEvent()` in `src/romance/RomanceManager.cpp` (it powers the
`Romantasy_OnLevelChanged` and `Romantasy_OnPreference` ModEvents) dispatches
through `SKSE::GetTaskInterface()->AddTask(...)` rather than an inline
`SendEvent()`. Callers can reach it off-thread, and Papyrus must receive the
event on the game thread. Dashboard requests that mutate the romance layer
(enrollment, personality edits, reset, remove, debug actions) use the same
task interface. Keep the `AddTask` wrapper.

A note from 2026-06-02: silent barks on Ambellina were briefly blamed on this
wrapper. The event was reaching her script; the cause was in the follower
plugin, where four bark topics had no dialogue branch. Fixed in that plugin,
not here.

### UI and controller support

The UI is the page in `view/`, hosted in game by Magelight UI
(`src/ui/magelight/MagelightHost.*`, driven by `src/ui/RomantasyUI.*`). Test it
with `node tests/view/all.mjs`; preview it in a browser at `index.html?mock`
(serve the repository root and open `view/index.html?mock`). Preserve the
contextual controller legends, the keyboard and mouse fallback, and game-thread
requests. Keep automated, browser-preview and in-game validation results
separate.

### Magelight page

The ledger is one HTML page in the Rosewater look, hosted by Magelight UI 1.6.0
or newer (`kMinHostVersion = 10600`). Without a usable host, tracking carries on,
popups are dropped, and opening the ledger shows the HUD line `Romantasy's ledger
needs Magelight UI 1.6 or newer.` The design and plan are local design notes,
untracked.

**Files**

| Path | What it holds |
|---|---|
| `src/ui/magelight/MagelightHost.*` | API, mod `Romantasy`, view `main` (fullscreen, `Panel` layer), listeners, UI mode, gamepad |
| `src/ui/magelight/Channels.h` | Channel names and payload keys (mirrors `view/js/bridge.js`) |
| `src/ui/RomantasyUI.*` | The facade: modes, popup queue, music, request handlers |
| `src/ui/PageRequests.*`, `src/ui/PopupQueue.*` | Payload validation and the popup wait policy, testable without Skyrim |
| `view/index.html`, `view/css/`, `view/js/` | The page; installed to `<mod>\Magelight\Romantasy\` |
| `view/icons/`, `view/fonts/` | 26 cut icons plus `rays.png` (the rays' fallback), and the WOFF fonts (Bodoni Moda, Jost, Pinyon Script) |
| `view/dev/` | Browser harness: `mock.js` (stand-in host), fixtures, Magelight's gamepad helper copy. Never shipped |
| `tools/view-icons/` | Icon pipeline: manifest, generator, cutter, contact sheet |
| `assets/icons/masters/` | Approved 1024 px icon masters |
| `tests/view/` | Node tests for the page (`all.mjs` runs them) |

Each page script is an IIFE that exports its pure functions to Node and attaches to
`window.RM` in the page. Parts register on `window.RM.parts` as
`{ name, render(state, view), actions, init(app) }`; `app.js` renders them in order
and owns the one delegated click listener. `data-action` names are one global
namespace: the ledger's are unprefixed, the rest use `settings.*`, `modal.*`,
`enroll.*` and `popup.*`. `view/js/pad.js` holds the page's one `document` key
listener (Escape, Enter, Space, arrows, Tab and the Left Ctrl + R chord) and the
gamepad scopes; the Reset and Remove sheets open focused on Cancel.

**Channels** (JSON text both ways; from `Channels.h` and `bridge.js`)

| Direction | Channel | Payload |
|---|---|---|
| plugin → page | `romantasyState` | `BuildStateJson` plus `revision` and `developerToolsUnlocked` |
| plugin → page | `romantasyMode` | `{ mode: "ledger" \| "popup" \| "hidden" }`; "hidden" goes out before the plugin leaves, and the page sleeps at once. "ledger" and "popup" also carry `ledgerSize`, `highContrast` and `reduceMotion` (as in the state), because the plugin sends no state outside the ledger: a popup before the first ledger open follows them |
| plugin → page | `romantasyLevelChange` | `BuildLevelChangeJson` plus `seq` (the popup's number in the queue) |
| page → plugin | `romantasyReady` | none; the plugin answers with mode and state. Sent again when UI mode comes on while the page is hidden; popups are not sent before the first one |
| page → plugin | `romantasyOpen` | none; Left Ctrl + R while a popup is alone on screen (the plugin opens the ledger in place, popup mode only, not in Favorites mode) |
| page → plugin | `romantasyClose` | none; also Escape on a page that believes it is hidden (close-only in the plugin) |
| page → plugin | `romantasyRefresh` | none |
| page → plugin | `romantasySettings` | any of `openWithFavorites`, `showGainModals`, `showLossModals`, `showAwayFollowers`, `detailedLog`, `pace`, `tierMoment`, `whispers`, `whisperCorner`, `rosterOrder`, `ledgerSize`, `highContrast`, `reduceMotion`, `skyrimNetBridge`, `skyrimNetConversation` |
| page → plugin | `romantasyBond` | `{ kind, identity, preferences }` (enroll, replace, reset, remove) |
| page → plugin | `romantasyDevUnlock` / `romantasyDevLock` | `{ password }` / none |
| page → plugin | `romantasyDebug` | `{ points }` or `{ stat, delta }`; refused while developer tools are locked |
| page → plugin | `romantasyPopupShown` / `romantasyPopupDismissed` | `{ seq }`; ignored unless it matches the head popup |

The plugin validates every payload and ignores what it does not recognise.
`tests/view/Contract.mjs` keeps `bridge.js` and `Channels.h` in step. The host
also publishes `__uimode` (whether the page holds UI mode); the page treats a release as final only
after it has stayed off for 250 ms.

**Harness and tests**

- Serve `view/` with any static server (port 8100 in the examples) and open
  `http://localhost:8100/index.html?mock`. Switches after `?mock`: `&empty` (no
  bonds), `&popup=gain|loss` (a level change after the first state),
  `&mode=popup` (popup alone), `&fail` (every bond action refused), `&pad` (a
  gamepad, fed through `__romantasyMock.pad`), `&health=note|warn|error` (health
  items of those levels; plain `?mock` is healthy). `view/dev/index.html` links them.
  The mock's developer password is `mock`.
- `node tests/view/all.mjs` (page), `xmake build romantasy-page-tests && xmake run
  romantasy-page-tests` (validation and popup queue), `node
  tests/RuntimeCompatibilityContractTests.mjs`.
- `xmake build romantasy-profile-check`, then `node tests/ProfileCheckCli.mjs` (the
  checker's exit codes, folder mode and `--json`), and `node tests/CheckScript.mjs`
  (the version and `.pex` steps of `scripts/check.ps1`, run dry).
- New icons: `tools/view-icons/README.md`; `tests/view/Icons.mjs` checks the manifest.

**Ultralight rules** (Magelight renders with Ultralight, not Chromium)

- No `backdrop-filter`, inset `box-shadow`, polygon `clip-path`, rounded boxes whose
  border sides differ, `<select>`, range inputs, `<audio>`, `<video>`, `Intl`,
  `toLocaleString`, `fetch(`, `XMLHttpRequest`, `.woff2`, `.webp`, or any
  `http://` / `https://` URL. Fonts are `.woff`.
- `view/js/bridge.js` is the only file that names `window.magelight`.
- `tests/view/Structure.mjs` checks most of these by text search over the page's
  CSS, JS and HTML: the forbidden properties, elements, APIs and URLs above, no
  `.woff2` or `.webp` file shipped, `window.magelight` outside `bridge.js`, and
  `@keyframes` animating only `transform` and `opacity`. It does not check
  rounded boxes with differing border sides, the solid sheet base, the stage
  zoom, the petal cap, the asleep pause or escaping; keep those by review.
- Any sheet over text (modals, takeover, popup card) has the solid base
  `linear-gradient(160deg,#6b1c4c,#34092a)` under its glass.
- The stage scales with CSS `zoom` on `#app`, not `transform`. Its size is 1600 × 900 at Standard (see Ledger reach below).
- Ambient motion animates `transform` and `opacity` only; at most 14 petals; every
  animation pauses while `html` has the class `asleep`.
- Every string from state passes through `RM.format.esc` before `innerHTML`.
- Listeners run on Magelight's page thread: copy the argument and post with the
  host's `PostGameTask`; never call SKSE `AddTask` from a listener.

**Ledger reach** (roster tools, comfort settings, words)

- `view/js/roster.js` is the pure `arrange` function: it filters by the find text
  (literal, case-insensitive), groups bonds into Following and Away (Away only
  while away companions are shown) and orders each group. Orders: `closest`
  (points, then name), `name`, `recent` (bonds with a recent change first, newest
  first: by the plugin's game day number when both bonds have one, else by its day
  words (Today, Yesterday, N days ago); the rest in closest order). Rows do not move
  while the ledger is open: it re-arranges when the ledger opens and when the order
  or find text changes, never on a state update. Between those, the last
  arrangement is pinned, and a new row goes to the end of its group. The plugin
  sends the mode before the state, so `app.js` marks an opening (and a sleep) and
  drops the pinned order once, on the first state that follows: a reopen shows the
  order of the new points.
- Ledger size picks the stage in `view/js/fit.js`: `standard` 1600 × 900, `large`
  1422 × 800, `largest` 1280 × 720. A smaller stage fitted to the same window draws
  everything bigger. An unknown size is Standard. `app.js` fits again only when the
  size changes.
- `app.js` puts the comfort settings on the root from every state and from every
  `romantasyMode` message that carries them: `data-size` (always),
  `data-contrast="high"` and `data-motion="reduced"` (only while on). The whisper
  card gets contrast and motion through its payload instead and sets the same two
  attributes. It keeps one size: it is a small HUD card in a screen corner, so
  Ledger size does not apply to it. The stylesheets answer the attributes. Under
  Reduce motion the points in the dossier show at once, with no count-up.
- The contrast tokens live in one block, `html[data-contrast="high"]` in
  `view/css/tokens.css`. Add a new edge or fill as a token there, not a one-off
  override. `tests/view/Comfort.mjs` measures the contrast ratios.
- The bond tools (Edit personality, Reset bond, Remove bond) sit in the head of the
  Milestones card, not the hero. They could not fit the hero at Largest, or with a
  longer language. A tool too long for its place ends in an ellipsis and carries
  its full text as a title.
- `view/js/strings.js` holds the page's words, key to text. The plugin words that
  stay English for now are listed in `docs/Translating.md`. `t(key, values)` fills each `{place}` from `values` as text; the caller escapes the
  result with `RM.format.esc` where it is drawn. `or(key, fallback)` gives the
  table's text, or the fallback (the plugin's own words) when there is no line; it
  is how tier names (`tier.1` to `tier.6`) and deed labels (`deed.<editorID>`)
  can be translated. A missing key shows as `[key]` and is logged once. Counts use
  `.one` (exactly 1) and `.many`. `strings.js` loads right after `bridge.js` on the
  ledger page and first on the whisper page, before any script that calls it.
- `format.js` installs a stand-in when `strings.js` did not load (a broken
  translation): every word shows as `[key]`, so the page still opens, closes and
  answers the plugin.
- `tests/view/Strings.mjs` is the words test. It scans every page script and both
  HTML files and fails on a literal word outside `strings.js`, unless it is an
  argument of `t(`, `tx(` or `or(`, a plugin word listed with its reason, or a
  class or identifier. Its render sweep draws every pane, sheet, popup, legend and
  card from a pseudo copy of the table (each text becomes its key between ⟪ ⟫) and
  proves that every key asked for is in the table, nothing is logged as missing,
  and no stray letters or `[key]` remain. To add a new pane to the sweep, draw it
  in the sweep's section with the data it needs, and every key it asks for is then
  checked. Every table key must also be used by some script.
- Translators are pointed to `docs/Translating.md`, which ships with the mod.

**Unverified in game** (only a browser has drawn the page so far): the rays
(`repeating-conic-gradient`, else set `no-conic`), non-Latin names falling back to
Windows fonts, the focus ring, switch glow, caret, placeholder and scrollbars in
Ultralight, `requestAnimationFrame` firing in popup mode (the popup's Shown message
and its music depend on it), and one action per Enter press as Magelight injects
key-down, char and key-up.

### Inside the manager: defer, never queue

While an `OutboxLock::Scope` is open on `RomanceManager`, never call
`SKSE::GetTaskInterface()->AddTask` directly. Call `Defer`. Every public entry
point that reads or changes manager state opens a scope; `QueryPoints` and the
view-only fast paths of the point getters do not. Deferred work is queued only
after the lock is released, so no task is ever queued while the lock is held. `Romantasy_OnLevelChanged` and `Romantasy_OnPreference` are
still sent from inside an `AddTask` task, so Papyrus gets them on the game thread.
`src/romance/OutboxLock.h` holds the pure part (tests:
`tests/profiles/OutboxLockTests.cpp`).

### The points view

Dialogue conditions run on the game's threads, so the hot readers avoid the
manager's lock. They read a `PointsView` (`src/romance/PointsView.{h,cpp}`), an
immutable snapshot of three maps: points by follower reference; each personality
file's enabled flag and starting points by base NPC; and a copy of the
saved-points table.

- The `GetGraphVariableInt` queries (`RomanceConditionHook` calls `QueryPoints`)
  read only the view and never take the lock.
- The `GetFactionRank ROM_RomancePoints` condition (`PointsConditionHook` calls
  `TryGetPoints`) and the Papyrus getters (`GetPoints`, `GetLevel`,
  `GetLevelName`) answer from the view when the reference is already in it. For
  any other actor they take the lock and may discover the actor.

Every non-const manager entry point sets `_viewDirty`; the view is rebuilt and
published in the lock's before-release hook, so a reader sees either the old view
or the new one, never a half-changed one. A condition returns what it returned
before for the same state.

### Favorites, pet peeves and unlock lines

The weight is the preference value in `RomanceProfiles` and in a bond's saved
preference byte: -2 pet peeve, -1 dislike, 1 like, 2 favorite. The multiplier is
the weight itself. `CheckPreferenceWeights` (`RomanceProfiles.h`) rejects other
values and more than three of either strong kind. Unlock lines live in
`profile.unlocks`, indexed by level minus 1. Three pure helpers in the same module
carry the logic and are tested: `EnforceStrongCap` demotes extra strong weights
(lowest keys stay) when a save loads, `PreferencePoints` is the clamped
`delta * points * weight`, and `PapyrusDirection` maps a weight to -1, 0 or 1.

The state JSON adds, per follower, `favorites` and `petPeeves` (labels) and
`unlocks` (`[{ level, items }]` for levels 2 to 6). The level-change payload adds
`unlocks`, a flat list of the lines of every tier gained, and the popup lists the lines of
every tier gained. `view/js/state.js`
normalizes all three. New tests: weight and unlock cases in
`tests/profiles/RomanceProfileTests.cpp` and `SaveCodecTests.cpp`,
`tests/page/PageRequestsTests.cpp`, and the stamps, tags and popup lines in
`tests/view/Enrollment.mjs`, `EnrollmentFlow.mjs`, `Ledger.mjs` and `Popup.mjs`.

### Mood and company

`src/romance/Mood.h` is pure (no game headers). It holds the score rules (`+4` per
point capped at `+20`, `-6` per point capped at `-30`, drift of 25 per game day,
clamp to -100..100, Warm at 25 or more, Strained at -25 or less) as `AfterPoints`,
`Nudged`, `Drifted` and `StateOf`, and the company rules as `Observe`: it takes a
`Company` (seen following, away, time-apart sent, three game-day stamps and days
together), whether the follower is following now, and the game day, and returns the
next `Company` and at most one event (`TimeApart` at 3 days, `Reunion` after at
least 1 day away). Drift keeps the fraction of a point, so the result does not
depend on how often it runs.

`RomanceManager` keeps a `BondMood` per bond in `_moods`, keyed like `_savedPoints`.
Points changes reach it through `AddPoints`, which uses the requested change even
when the stored points cannot move. A lost tier nudges it by -20 and sends
`Romantasy_OnLevelLost`; a reunion nudges it by +15. Both events go through
`SendBarkEvent`, like the two older events. A follower whose profile is off is
skipped, so it has no mood and sends no events.

`ObserveCompany()` is called from `OnDialogueClosed` (the Dialogue menu closing),
`OnGameLoaded`, and `BuildStateJson` (the ledger opening and each state build). The
statistic path also reaches it. `OnGameLoaded` first sets any stored time that is
negative or in the future to the current game day, so an older save sends no false
time apart or reunion.

The view carries moods too: `PointsView::moodByReference` and `FindMood` let
`QueryMood` answer dialogue conditions (`Romantasy_Mood`) and Papyrus (`GetMood`,
`GetMoodName`) without the lock. An actor with no points entry reads 0; a bond with
no mood record reads Settled. The state JSON adds `mood` (`"strained"`, `"settled"`
or `"warm"`), which the dossier shows as a chip beside the Following chip.

New tests: `tests/profiles/MoodTests.cpp` (score, drift, states, company),
the mood cases in `SaveCodecTests.cpp`, `PointsViewTests.cpp` and
`RomanceConditionTests.cpp`, the chip cases in `tests/view/Ledger.mjs`, and the
mood checks in `tests/RuntimeCompatibilityContractTests.mjs`.

### Pace and calm moments

`src/romance/Pacing.h` is pure (no game headers) and does not depend on how often it
is called. It holds:

- `Pace` (`Slow`, `Standard`, `Fast`) and `TierMoment` (`Calm`, `Immediate`), with
  `ParsePace`, `ParseTierMoment` and `Key`. The settings keys in `settings.json` are
  `pace` (`"slow"`, `"standard"`, `"fast"`) and `tierMoment` (`"calm"`,
  `"immediate"`). The defaults are `standard` and `calm`; an unknown value keeps the
  default.
- `ApplyPace`: a gain is doubled (Fast) or halved with the odd half carried (Slow).
  Losses and Standard pass through. It is called once, in `ApplyStatDelta`, so only
  Romantasy's own statistic events are paced. `ModifyPoints`, `ApplyPreference` and
  `AddPoints` never call it.
- `DaysNeeded`, `GateCap`, `GatedPoints`, `NextGate`: the gates (Friend 3, Confidant
  7, Lover 14, Spouse 21). A gain stops one point short of a higher tier that still
  needs days, never goes below the points the bond already has, and is never capped
  inside the tier the bond already holds. `NextGate` names the gate that is really
  ahead; the state JSON carries it as `gate`, which the dossier turns into the
  "needs N days together" line.
- `GateGain`: the gate arithmetic of `AddPoints` (new points, amount applied, and
  whether a gain was refused). It only gates on Slow burn, for gains, when the call
  asks for gating; the developer tools do not ask. The clamp at 0 stays in the
  manager.
- `CalmFacts`, `IsCalm`, `ShouldAnnounce` and `kCalmPlaceKeywords` (the seven
  Skyrim.esm location keywords: inn, player house, house, dwelling, city, town,
  settlement). Only the player's current location is tested, not its parents. A
  location change to nowhere is never calm. Sleep or wait that was interrupted does not count as rest.
- `DecideRelease`: keep, drop or announce a waiting change.
- `AwardForStat`: one follower's result for one statistic change (applied, reported,
  carry, refused).
- `PendingChange`, `Merge` and `ReleaseTarget`. `Merge` folds a new change into a
  waiting one: it keeps the earliest old level and time, adds the point deltas, and
  returns nothing when the bond ends where it began. `ReleaseTarget` takes the
  waiting change and the level the bond holds now and returns what to announce, with
  `newLevel` set to the current level. It returns nothing when the bond is back at
  its old level or the level is outside 1..6, so a change is never announced for a
  level the bond no longer holds.

`RomanceManager` keeps `_waitingTierChanges` (one per bond, keyed like
`_savedPoints`) and `_paceCarry` (the carried half point, not saved).
`HoldOrAnnounceLevelChange` either announces at once (`At once`) or merges the change
into the waiting entry. `ReleaseTierChanges(justRested, arrivedAt)` builds the
`CalmFacts`, drops entries whose follower is gone, and announces each entry for which
`ShouldAnnounce` holds, through the same popup queue and `SendBarkEvent` as before. It
erases the entry before announcing. It is called from `OnDialogueClosed`, at the end
of every statistic event, when the player's combat ends, when the player's location
changes (with the new location), and on `TESSleepStopEvent` and `TESWaitStopEvent`
(both with `justRested`). The sinks check an atomic flag first, so they take no lock
while nothing waits. The days together that the gates read are those kept by
`ObserveCompany()` (see Mood and company), so a gate sees the count as of the last
look.

New tests: `tests/profiles/PacingTests.cpp` (pace, carry, gates, `GateGain`,
calm facts, `Merge`, `ReleaseTarget`), the `RoPn` cases in `SaveCodecTests.cpp`, the
pace and moment rows in `tests/view/` (Settings and the gate line in the dossier),
and the pacing checks in `tests/RuntimeCompatibilityContractTests.mjs`.

### The bond journal

`src/romance/Journal.h` and `Journal.cpp` are pure (no game headers). A `Journal`
holds `history` (newest first), `days` (oldest first), `gains`, `losses` and
`milestones` (oldest first). The limits are 30 history entries, 60 days, 5 gains and
5 losses, 20 `Fell` milestones, and a label of at most 120 bytes, cut at a UTF-8
boundary. `Began` and `Reached` milestones are never dropped.

- `Record(journal, label, points, oldLevel, newLevel, day)` writes one applied
  change: history, the day's totals, the biggest moments and the milestones. A
  tier is written as `Reached` the first time (and again after a reset clears
  it); every fall is written as `Fell`. Zero points records nothing. An unbegun journal begins at the old level.
- `Begin(journal, level, day)` writes `Began` (and `Reached` for the current tier
  when it is 2 to 6). It does nothing for a journal that has begun.
- `Normalize` repairs a journal read from a save into one the rules could have
  produced (days sorted and merged, lists cut, one `Began` first, a tier reached
  once (so a repaired journal never holds a tier twice), moments of the wrong sign dropped). A journal the rules produced comes back
  unchanged.
- `DayNumber` gives `floor(day) + 1`, shown as `Day N`.

`RomanceManager` keeps `_journals` keyed like `_savedPoints`, and `_orphanedJournals`
for plugins that are not loaded. `Record` is called in `AddPoints`, once per applied change, after the
new level is known and before any popup is queued, with the points really applied
(`follower.points - oldPoints`) and the real old and new tiers, so a change whose popup
waits for a calm moment is dated when it happened. A reset is a fresh
start: `ResetPlayerFollowerPoints` calls `RecordReset`, which writes one `Bond reset`
row in History with the points lost and a `Fell` to Stranger milestone if the bond
stood higher. It is not counted among the biggest losses or in the day totals. The
tiers the bond had reached are cleared from the milestones so they can be written
again; `Began` stays. `Begin` is called in `EnrollPlayerFollower`, which first erases
any journal already held under the bond's keys when the bond is enrolled from the
ledger. `SaveJournals` writes each journal through `Normalize`, so a journal can
never fail to fit the record. `RemovePlayerFollower` erases the
journal. Revert and load clear both maps first. The journal only records: it never
changes points, tiers, mood or events.

The page sends the action `romantasyJournalOpen` with `{ identity }` (key list
`identity`, in `Channels.h` and `view/js/bridge.js`). The plugin answers on the
topic `romantasyJournal`, built by `ui::JournalJson` (`src/ui/JournalJson.h`):
`identity` and `found`, then, when found, `began`, `daysTogether`, `milestones`
(`kind`, `level`, `day`), `days` (`day`, `gained`, `lost`), `gains`, `losses` and
`history` (`label`, `points`, `day`), with every day already a `Day N` number.
`found` is false for an identity that is not a bond, and true with empty lists for a
bond with no journal yet. The request is handled on the game thread through
`AddTask` and published only while the ledger is open.

The page is `view/js/journal.js` (`normalize` and `journalHtml` are pure). The
Journal button sits at the foot of the Recent changes card in the dossier
(`view/js/ledger.js`). Opening shows "Opening the journal…"; if no answer comes in 3
seconds the page shows "The journal could not be opened."; a late answer still fills
the page. Every string from the plugin reaches the DOM through the escape helper.

New tests: `tests/profiles/JournalTests.cpp` (`Record`, `Begin`, limits, `Normalize`),
the `RoJn` cases in `SaveCodecTests.cpp`, `tests/view/Journal.mjs`, the Journal
cases in `tests/view/Ledger.mjs`, `Mock.mjs`, `NoStretch.mjs` and `InputFlow.mjs`,
and the journal checks in `tests/RuntimeCompatibilityContractTests.mjs`.

### Whispers

A whisper is a card on the HUD that only shows. It never changes points, tiers,
mood, events, the popup or the ledger.

`src/ui/Whispers.{h,cpp}` is pure (standard headers and `romance/Mood.h`). It has
the `Whisper` record (kinds Deed, MoodWarm, MoodStrained, Reunion, TimeApart,
TierGained, TierLost), `Describe` (the name, the line, the tone and the icon;
names cut at 64 bytes and labels at 80, made valid UTF-8), `NotificationText`
(`{Name}: {line}`), `NotificationMarkup` (the fallback text: `NotificationText`
with U+2212 written as `-` and `&` and `<` escaped), the `Queue`, `Pump` (the
queue plus the seq on the card: `Next`, `Answer`, `ShowFailed`, `Lost`, and the
count of silent cards: two in a row give up on the card for the session),
`PlaceCard`, `MenuBlocks`, `CanShow`,
`IsCurrentAnswer`, `NextSeq` and `MoodCrossing`. The numbers: a deed merges into a
waiting deed of the same follower when it comes within 2000 ms of the last push
(`kMergeMs`); at most 4 wait (`kMaxWaiting`); a waiting whisper is dropped after
30000 ms (`kMaxAgeMs`); the page holds a card for 4000 ms; the plugin treats a
shown whisper as finished after 8000 ms with no answer (`kAnswerMs`). The clock is
passed in (`GetTickCount64()`); a clock that goes backwards is re-anchored and
never underflows. Every string from the plugin reaches the page through `esc` in
`view/js/format.js`.

A card that a menu hides is not counted as a silent card. `Pump` sets `_blocked` when a
card is current but `canShow` is false. If that card then times out, `timedOutBlocked` is
true, so the timeout does not add to the count of silent cards (and does not reset it).
Without this, opening a menu during a whisper twice could make the plugin give up on a
card that works.

The second view is `src/ui/magelight/WhisperView.{h,cpp}`. It loads
`Data/Magelight/Romantasy/whisper.html` and is created with `clickThrough = true`,
`layer = Hud` and `startVisible = false`. Click-through means the page takes no
mouse. The HUD layer is Magelight's lowest tier, under the ledger's view. The
plugin hides the card itself when a blocking menu opens. Romantasy never asks for UI mode on it, so it cannot pause the game,
show a cursor or take keys. It shares only the API pointer and the registered mod
with `MagelightHost`; the ledger's view is untouched apart from two getters and
an optional event hook. Each `Show` places the card (420 by 96 CSS px, margins
32 from the side and the bottom and 96 from the top, scaled by `h / 1080`, at
least 1 and at most 3), rebuilds the view when the scale changes (`PlaceCard` rounds the card's size up, so the view is never smaller than the card), then calls
`romantasyWhisper` with `{ name, line, tone, icon, seq }`. The channel names live
in `WhisperView.h`, not `Channels.h`, because `tests/view/Contract.mjs` mirrors
`Channels.h` against the ledger's `bridge.js`. `view/js/whisper.js` plays the card
(180 ms in, 4000 ms hold, 220 ms out, on timers, never animation events) and
answers `romantasyWhisperDone` with `{ "seq": N }`.

The done message and the 8 second rule. `whispers::Pump` gives each card a seq
(`NextSeq`, 1 to 2147483647, then 1 again) and decides what to show, which answer
counts and when an unanswered card ends. `PumpWhispers` only carries its
decisions out. `OnWhisperDone` parses the answer with `ParsePopupAck`; the pump
ignores it when it is malformed or `IsCurrentAnswer` says it is not the card on
screen, and otherwise finishes the whisper and goes on to the next. A page that
never answers is not waited for: after 8000 ms the card ends and the next
whisper may go. A late answer for a card that timed out carries an old seq and is
ignored. `PumpWhispers` runs from `Whisper()`, from `OnWhisperDone`, from the menu
sink, from `CloseAll`, `LeaveEmptyPopup`, `OnHostExited` and `OnHostRefused`, and
once a second from the popup ticker while a whisper waits or is on the card
(`_whispersPending`), so the 8 second and 30 second rules run at most about a
second late.

When a whisper may show (`CanShowWhispers`): the world is ready, the game is not
paused, Dialogue Menu is not open, no blocking menu is open (HUD Menu, Fader Menu
and Cursor Menu never block; Dialogue, Main and Loading always do; any other menu
blocks when it pauses, uses the cursor or menu context, or is modal) and neither
the ledger nor a popup is up. The card is hidden when that stops being true.
Opening the ledger, entering a popup, a load and switching whispers off all call
`ClearWhispers`. Whispers show in combat.

If `MagelightHost::Api()` is null, or the view cannot be made, fails to load or
dies, `Show` returns false (as it does while the display size is still 0) and the
pump sends `NotificationMarkup` through `NotifyWhisper` (which calls
`RE::SendHUDMessage::ShowHUDMessage`) instead and finishes the whisper at once.
If the view dies with a whisper on the card (load failed, destroyed, renderer
dead), `OnWhisperCardLost` takes it back from the pump and sends it to the game's
notification; the waiting ones follow. A card that does not answer twice in a row
makes `WhisperView::GiveUp()` mark the view dead for the session. The Health item `whispers` (a note, not
a warning) says "Whispers are using the game's own notifications." when the host
is Ok or Missing and the view is unavailable.

Raise sites, all in `RomanceManager`, all through `RaiseWhisper`, which returns
when `Settings::Whispers()` is off or `_quietWhispers` is set, and otherwise hands
over inside `_lock.Defer` (never `AddTask` while an `OutboxLock::Scope` is open):

- `ApplyStatDelta`, beside `Romantasy_OnPreference`: a Deed, for the points the
  deed applied (`follower.points` before and after), only when that is not 0 and
  the follower is following. A Slow burn gate that held the whole gain applied 0,
  so it raises none. Favorites and pet peeves are marked strong.
- `AddPoints`, after the tier block: MoodWarm or MoodStrained when `MoodCrossing`
  of the mood before and after says so; following only.
- `ObserveCompany`, beside `Romantasy_OnReunion` and `Romantasy_OnTimeApart`:
  Reunion and TimeApart.
- Tier changes: `AnnounceLevelChange` passes `!_quietWhispers` through
  `QueueOrShowLevelChange` and `ShowLevelUpModal(payload, whisperIfOff, generation)` to
  `ShowLevelUpModalNow`, which raises TierGained or TierLost only inside the
  branch where that popup is switched off. A held tier change keeps the popup's
  moment, and keeps where it came from in `pacing::PendingChange::whisper` (not
  saved). With no usable host, `ShowLevelUpModalNow` whispers the tier change
  instead when Whispers is on.

Load generation: `RomantasyUI::Close()` (kPreLoadGame, kNewGame) raises
`_whisperGeneration`. Every whisper carries the generation it was raised in, and
`Whisper()` drops one from before the last load.

`DebugAddPoints` and `DebugApplyStat` (the ledger's developer tools) and
`DebugSetPoints` (a Papyrus author function) hold a `QuietWhispers` scope, so they
raise none. `DebugDeed` opens no
scope and whispers like a real deed.

Settings: `whispers` (bool, default true) and `whisperCorner` (`topLeft`,
`topRight`, `bottomLeft`, `bottomRight`; default `topRight`), read through
`ParseWhisperCorner`. The corner row is dimmed and passed over by keys while
whispers are off (`OFF_WHEN` in `view/js/settings.js`).

Tests: `tests/page/WhispersTests.cpp` (rules, queue, placement, menu rule, seq,
mood crossing), `tests/page/HealthTests.cpp` (the health item), the `Whispers_Pump*` tests,
`Whispers_NotificationMarkup`, `Whispers_PlaceCardCoversTheCard`,
`tests/profiles/PacingTests.cpp` (`Pacing_MergeKeepsTheWhisper`),
`tests/view/Whisper.mjs` (the page: card HTML, escaping, the sequence, files, a throwing send) and
the whisper block in `tests/RuntimeCompatibilityContractTests.mjs` (settings
keys, defer-only raise sites, a view that never asks for UI mode). The page can
be tried in a browser with `view/whisper.html?mock` (`&long`, `&bg=light`);
`view/dev/whisper-mock.js` is not deployed. `scripts/deploy.ps1` refuses to ship
without `whisper.html`.

Log lines (Detailed log on): `whisper {seq} on the card: {name} ({kind})` and
`whisper to the game's notification: {name} ({kind})` at debug, and two warnings
that always show: the page did not answer whisper {seq} within 8 s (the card is
taken down), and the card did not answer 2 times in a row (whispers use the game's
notifications for this session).

### The actor walk

The statistic path walks the loaded actors to find followers. It runs at most
every 5 seconds (`src/romance/Throttle.h`), and at once when the player's cell
has changed since the last walk (`_actorWalkCell`). The cost of the throttle: a
follower Romantasy has not seen yet (one who entered the loaded area since the
last walk and was never queried by dialogue) misses every statistic event until
the next walk. While the player stays in the same cell that is at most 5 seconds.

### Health

`src/health/Health.{h,cpp}` is pure: it turns plain facts into a report (items
with id, level `note`/`warn`/`error`, title and detail; status is the worst
level, `ok` when there is no `warn` or `error`). `src/health/HealthMonitor.{h,cpp}`
gathers the facts in the plugin. The facts come from:

- `MagelightHost`: whether the host loaded, its version, page files, view creation.
- `RomanceManager`: profile rejections, kept bonds for missing mods (a note, not
  a problem), a damaged save record, and whether the faction sets exist.
- `Settings`: whether the file was unreadable or the last write failed.
- the two condition hooks: whether each installed.
- `InputMode::LookupPower`: whether the Romantasy power exists. It looks up
  `CS_Romantasy.esp`, local ID `0x83C`, and falls back to the editor ID. (Before
  this, "Open with Favorites Menu" failed on installs without a mod that keeps
  editor IDs.)

A player personality file whose plugin is not loaded is a kept bond (a note).
An author file whose plugin is not loaded is neither a note nor a problem. A
light-plugin local ID out of range is still a problem.

To add a check: add the fact to `Health::Facts` in `Health.h`, add one rule in
`Health.cpp` with its id, level, title and detail (plain words, no em dashes),
fill the fact in `HealthMonitor.cpp`, and add a case to
`tests/page/HealthTests.cpp`. Do not add a check that can fire on a healthy
install: a false alarm is worse than no check.

The report goes to the page inside the state as `health`, added where the UI
layer stamps the state. It shows once per session as a HUD line, and is always
written to the log. In Settings the Health section shows first when there is a
`warn` or `error`, and at the foot when all is in order. Notes never move it or
mark the Settings button. When keyboard or gamepad focus reaches the first or
last control of the Settings list, the list scrolls to that end
(`reachEnds` in `view/js/pad.js`), so the Health text is reachable without a mouse.

### Logging and files

Per-event log lines are at debug level. The `detailedLog` setting (off by
default) switches the log to debug at startup and at once when toggled.
`settings.json` and player personality files are written with
`src/util/AtomicFile.{h,cpp}` (write `<name>.tmp`, then rename over the target).
A tier-change popup queued as combat ends is flushed at once (re-check in
`QueueOrShowLevelChange`).

### New test files

| File | Covers |
|---|---|
| `tests/profiles/OutboxLockTests.cpp` | `OutboxLock.h` |
| `tests/profiles/PointsViewTests.cpp` | `PointsView` |
| `tests/profiles/ThrottleTests.cpp` | `Throttle.h` |
| `tests/profiles/AtomicFileTests.cpp` | `AtomicFile` |
| `tests/profiles/MoodTests.cpp` | `Mood.h` |
| `tests/profiles/PacingTests.cpp` | `Pacing.h` |
| `tests/profiles/JournalTests.cpp` | `Journal.h` |
| `tests/view/Journal.mjs` | `view/js/journal.js` |
| `tests/page/WhispersTests.cpp` | `Whispers.{h,cpp}` |
| `tests/view/Whisper.mjs` | `view/js/whisper.js`, `view/whisper.html` |
| `tests/page/HealthTests.cpp` | `Health` |
| `tests/view/Boot.mjs` | boots the real page scripts against the mock host |

The mock harness (`view/dev/mock.js`) publishes the mode before UI mode, as the
plugin does. `?mock&health=warn|error|note` shows each health level.

### Follower conversion

Before converting another follower to the TOML integration, read
[FollowerConversion.md](FollowerConversion.md). It preserves the Mornhilde
procedure, the 1-based dialogue queries, the Horde policy and the dated
verification evidence. For Genesis or Effigy compatibility work, also read
[GenesisEffigyRomantasyHandoff.md](GenesisEffigyRomantasyHandoff.md).
