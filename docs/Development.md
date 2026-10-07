# Development notes

Working notes for building, deploying and extending Romantasy. User-facing
documentation lives in [README.md](../README.md); profile authoring in
[FileProfiles.md](FileProfiles.md); the native dashboard in [NativeUI.md](NativeUI.md).

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

Never run `xmake -P <repo>` from another working directory: the build directory
can mis-resolve and the DLL ships without the generated version resource, which
SKSE then refuses to load. After a build, check that the DLL's `FileVersion`
matches `set_version` in `xmake.lua`.

Close Skyrim, the Creation Kit and xEdit before building or deploying; the
install step cannot replace a loaded DLL.

Other targets (not built by default):

| Target | Purpose |
|---|---|
| `romantasy-ui-tests` | Headless unit tests for the Skyrim-free screen layer (`xmake build romantasy-ui-tests && xmake run romantasy-ui-tests`) |
| `romantasy-profile-tests` | TOML profile and persistence tests |
| `romantasy-preview` | Desktop preview of the dashboard (`--shot`, `--size`, `--select`, `--pane`, `--screen`, `--modal`, `--dev`) |
| `romantasy-profile-check` | Command-line profile validator shipped with the mod |

## Deploy

`scripts/deploy.ps1` signs the built DLL (Authenticode, RFC 3161 timestamp) and
copies the complete payload into the mod folder with hash verification: DLL,
fonts, ESP, scripts, sounds, licenses, example profiles, the profile checker and
the shipped documentation. Pass `-ModRoot` to target another folder.

Local settings in the mod folder are never overwritten.

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

### Native UI and controller support

Read [NativeUI.md](NativeUI.md) before UI or controller work. Preserve the
Skyrim input event route, the contextual controller legends, the keyboard and
mouse fallback, and game-thread requests. Keep automated, desktop-preview and
in-game validation results separate.

### Follower conversion

Before converting another follower to the TOML integration, read
[FollowerConversion.md](FollowerConversion.md). It preserves the Mornhilde
procedure, the 1-based dialogue queries, the Horde policy and the dated
verification evidence. For Genesis or Effigy compatibility work, also read
[GenesisEffigyRomantasyHandoff.md](GenesisEffigyRomantasyHandoff.md).
