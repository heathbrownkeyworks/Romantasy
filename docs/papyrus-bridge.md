# Romantasy 3.0 Papyrus bridge

The native Papyrus API supports follower scripts, scenes, and dialogue fragments.
The complete declarations are in [Romantasy.psc](../Source/Scripts/Romantasy.psc).
The API capability version remains **6**; it is independent of release version 3.0.1.

## Points and preferences

Use `ModifyPoints(actor, delta, reason, showLevelUp)` for an authored point change.
Use `ApplyPreference(actor, statistic, count, showLevelUp)` to apply that companion's
opinion and Romantasy's statistic weight. Both require an actively following,
managed actor and return False when the adjustment cannot be applied.

```papyrus
Romantasy.ModifyPoints(akSpeaker, -10, "Broke a promise")
Romantasy.ApplyPreference(akSpeaker, "Necks Bitten", 1)
```

Statistic names accept supported labels or stable identifiers such as
`ROM_NecksBitten`. Those identifiers name preferences; new TOML integrations do
not require matching faction memberships. An unset preference produces no change.

| Function | Behavior |
|---|---|
| `GetApiVersion()` | Returns 6, the current bridge capability version. |
| `GetPoints(actor)` | Current points; 0 for an unmanaged actor. |
| `GetLevel(actor)` | 1-6 for Stranger through Spouse; 0 when unmanaged. |
| `GetLevelName(actor)` | Display name of the current relationship tier. |
| `GetPreference(actor, stat)` | -1 dislike, 0 neutral/unset, 1 like; invalid input returns 0. |
| `ClearPreferences(actor)` | Clears writable preferences; supplied/protected profiles reject the call. |
| `SetPreference(actor, stat, direction)` | Positive is like, negative is dislike, 0 removes that opinion. |
| `IsPreferencesManual(actor)` | True for player-created personalities or a managed external coordination flag. |
| `SetPreferencesManual(actor, manual)` | Coordinates external editors; cannot release a player-created personality or claim a protected one. |

Use GetLevel to distinguish an unmanaged actor from a managed actor with zero
points. Dialogue conditions instead use `Romantasy_Registered` as their guard.

## Ownership and persistence

- **Supplied TOML profiles** in Profiles are protected. Edit those files externally
  and restart Skyrim. Do not register the same NPC through the legacy author API.
- **Player-created profiles** in PlayerProfiles are editable through the dashboard
  or the public preference write APIs. Successful changes persist to TOML, update
  the live preferences immediately, and refresh an open dashboard. A failed file
  write leaves the previous profile intact.
- **Legacy author-defined profiles** from base records or optional registration
  remain protected.
- **Legacy externally managed profiles** retain the faction-backed preference
  and coordination behavior for existing integrations.

Player configuration is shared across saves using the same files. Earned points
remain per actor reference in the SKSE cosave. Multiple instances of a generic NPC
share profile preferences but retain separate balances. Player-created profiles
always report manual ownership; supplied profiles do not.

For writable profiles, clear-then-set applies each call individually, not as one
transaction. The dashboard's Seal personality action saves the complete selection
in one operation. Never bypass a rejected write by modifying factions directly.

## Dialogue conditions

Use the native reserved `GetGraphVariableInt` queries on Subject:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

This gates a response at Friend or higher. For exact thresholds use
`Romantasy_Points`. See the [dialogue guide](romance-points-dialogue-conditions.md)
for grouping, sentinel values, and conversion from zero-based faction ranks.
These conditions need no dialogue fragment or follower-owned global.

The legacy `ROM_RomancePoints` GetFactionRank proxy remains available to existing
mods. It returns the subject's points, or -1 when unmanaged, without storing real
faction ranks. New TOML integrations should use the reserved queries above.

## Legacy optional author registration

`RegisterAuthorFollower(actor, stats, directions, startingLevel, rankMirror)`
retains the API-5 faction-based registration workflow. It needs the legacy
Romantasy factions and accepts parallel arrays with directions exactly -1 or 1.
Starting level is 1-6. Validation rejects invalid/duplicate statistics or a
conflicting TOML definition before changing the profile. Existing points are
preserved; an untracked actor starts at the chosen tier's minimum.

The optional rank global receives the old 0-5 rank. API 6 also provides
`RegisterAuthorFollowerPointsMirror(actor, pointsMirror)` for an exact-point
global; register it after successful author registration.

Re-register these optional integrations and mirrors after each load, usually
from a player reference-alias script's OnInit and OnPlayerLoadGame events. Legacy
registration is not needed for a TOML profile and must not be combined with one
for the same NPC.

## Voiced reactions

Romantasy delivers ModEvents on the game thread:

- `Romantasy_OnLevelChanged`: emitted on an upward tier change when level-change
  notifications are requested; numArg is the new 1-based level, sender is the
  follower's NPC base.
- `Romantasy_OnPreference`: emitted for a tracked-stat point adjustment that does
  not cross a tier; strArg is the statistic label, numArg is the signed adjustment,
  and sender is the NPC base.

Filter events to the intended sender. Re-register listeners after game load.
Followers supply their own topics, voice assets, cooldowns, and story gates.
For Say() reactions, verify that the topic subtype and conditions can resolve an
INFO. Do not gate romance dialogue or reactions on Horde being absent.

## TopicInfo fragment example

Keep the Creation Kit fragment markers when authoring or recompiling fragments:

```papyrus
;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname Example_TIF__05XXXXXX Extends TopicInfo Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akSpeakerRef)
Actor akSpeaker = akSpeakerRef as Actor
;BEGIN CODE
Romantasy.ApplyPreference(akSpeaker, "Necks Bitten", 1)
;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment
```
