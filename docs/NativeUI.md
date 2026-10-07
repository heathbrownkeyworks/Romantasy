# Native UI development

Romantasy 3.0 uses Dear ImGui/DX11 for the paused dashboard and unpaused
relationship popups in Skyrim SE/AE. It has no external web renderer dependency.

## Code and assets

- `src/ui/ImGuiHost.*` owns context, fonts, rendering, and the input queue.
- `src/ui/menus/` provides the paused ledger and unpaused popup menus.
- `src/ui/screens/` provides layout, state, widgets, and screens.
- `src/ui/RomantasyUI.*` connects screens to game-thread requests and state.
- `assets/fonts/` supplies the seven fonts installed under
  `Data/SKSE/Plugins/Romantasy/fonts`. They are Latin subsets. Each one has
  Windows' fonts merged behind it for the letters it lacks (Cyrillic, Greek,
  Latin Extended, Chinese, Japanese, Korean): Segoe UI at a close weight, then
  Microsoft YaHei, Yu Gothic and Malgun Gothic ordered by the Windows display
  language (`src/ui/screens/SystemFonts.*`). The font data is read once and kept
  for the process, because the atlas reads glyphs from it on first use.
- `widgets::Uppercase` capitalises any alphabet through Windows' invariant case
  mapping (`src/ui/screens/TextCase.*`); ASCII text takes the plain path.

Keep romance mutations and Papyrus events on the SKSE game thread. Preserve the
task wrapper in SendBarkEvent and ownership checks on profile mutations.

## Controls and profile editing

Open with Left Ctrl + R, or enable Open with Favorites Menu and cast the Romantasy
power. Gamepad opening uses Favorites; there is no registered opening chord.

Controller navigation uses D-pad, A to select, B to go back/close, and left stick
to scroll. Popups accept A or B. Text entry uses a physical keyboard. Contextual
controller legends switch with input activity through the existing Skyrim input
event route. Do not add competing controller polling or input hooks.

Dossier likes/dislikes scroll independently. Mouse wheel scrolls the hovered
list; A/Enter selects a list for left-stick/arrow-key scrolling.

New bond and player personality editing save through RomanceManager into
PlayerProfiles TOML. A successful edit updates live cached preferences before the
dashboard refresh. File commit failures retain previous preferences. Supplied
Profiles remain protected. Reset affects the current actor/save; removal disables
the shared player configuration. See [FileProfiles.md](FileProfiles.md).

## Build and automated checks

Run at the repository root. With XSE_TES5_MODS_PATH set, the build may auto-install
the DLL; close Skyrim and editors first. Do not use xmake -P from another folder.

```powershell
xmake -y
xmake build romantasy-profile-check
xmake build romantasy-profile-tests
xmake run romantasy-profile-tests
xmake build romantasy-ui-tests
xmake run romantasy-ui-tests
node tests/RuntimeCompatibilityContractTests.mjs
xmake build romantasy-preview
```

The desktop preview shares the screen implementation:

```powershell
build/windows/x64/release/romantasy-preview.exe --shot build/ledger.png --size 1920x1080
build/windows/x64/release/romantasy-preview.exe --controller --pane settings --shot build/controller.png
build/windows/x64/release/romantasy-preview.exe --popup gain --ledger off --shot build/popup.png
```

The private deployment workflow signs the DLL and verifies an explicit payload,
preserving installed settings and player profiles. Public source builds do not
require the release signing configuration.

## In-game acceptance

Automated checks and desktop previews do not establish Skyrim behavior. Record
these checks separately for the tested artifact:

- Open through keyboard and Favorites; navigate/scroll/close with a controller.
  Confirm LB + X does not toggle Romantasy.
- Confirm paused dashboard and unpaused popup behavior, legend switching, text
  entry, modal navigation, and active-follower-only developer actions.
- Create a bond; confirm a PlayerProfiles file appears and edit/remove controls
  remain available. Edit an opinion, then trigger that stat without reloading.
- Save/load and fully restart. Confirm preferences persist, points belong to the
  selected save, and a zero balance overrides defaults.
- Remove, restart, load an older save, and re-add. Confirm the disabled file
  prevents unintended resurrection. Check legacy player migration retains points
  and preferences and leaves unrelated factions unchanged.
- Verify supplied profiles remain protected and invalid/unwritable files produce
  failures without replacing the previous personality.

Controller support has prior user-confirmed in-game testing. The player-profile
migration and full persistence matrix still require artifact-specific gameplay
checks; retain NOT RUN for any unobserved result.
