# Testing your follower with Romantasy

This guide is for people who write follower mods that use Romantasy. It shows how to
make things happen now, so you do not have to clear a dungeon to see a favorite
work, and how to read the log when nothing happens.

Everything here is off by default. A normal player's game is not changed.

## Switch author mode on

Close Skyrim. Open `Data\SKSE\Plugins\Romantasy\settings.json` and add or change
one line:

```json
"authorMode": true
```

Start Skyrim. The ledger's Health section now shows `Author mode is on.` It is a
note, not a warning. There is no switch for it in the ledger's Settings; you set it
by hand. Set it back to `false` before you play for real.

With author mode off, every function below logs one line in `Romantasy.log` (once
per function in a session), changes nothing, and returns False (or an empty
string).

Turn **Detailed log** on in the ledger's Settings too. The "why" lines below are
written only when it is on.

## The seven functions

All seven are global functions on the script `Romantasy`. Each one needs an actor
that has a bond with the player. For an actor with no bond, each returns False (or
an empty string). A follower does not have to be following the player.

In the console, click the follower to see their reference ID, then call a function
with `cgf`. Put names with spaces in quotes. The console does not print what a
function returns, so read the result in `Romantasy.log`. In a script or a
fragment, use the return value.

### DebugDeed

```papyrus
Bool Function DebugDeed(Actor follower, String statistic, Int count = 1)
```

Runs one built-in deed for that follower as if the player had done it. The
follower's feeling about the deed, the player's Relationship pace and the Slow
burn gate all apply, so you see what a player would get. `statistic` is the name
used in personality files, such as `Quests Completed` or `ROM_QuestsCompleted`.
`count` must be 1 or more.

Returns True only when the follower's points really changed. It returns False when
the follower has no feeling about the deed, the rule gives no points, the pace
rounds the gain to 0, a gate holds all of it, or a loss could not go lower. An
unknown deed name logs the list of known names once per session, and after
that only the unknown name.

```text
cgf "Romantasy.DebugDeed" <refid> "Quests Completed" 1
```

A tier change it causes is announced or held for a calm moment, as for a real deed.

### DebugSetPoints

```papyrus
Bool Function DebugSetPoints(Actor follower, Int points)
```

Sets the total to `points` (0 or more). It is not paced and not gated, so it is
the way around a Slow burn gate. A tier change shows the popup when that popup is
on, and the events are sent, but no whisper is raised. Mood moves as for any
change. The journal records it as "Debug set points". Returns True when the total
is now that number.

```text
cgf "Romantasy.DebugSetPoints" <refid> 1000
```

### DebugSetDaysTogether

```papyrus
Bool Function DebugSetDaysTogether(Actor follower, Float days)
```

Sets the days together, which the Slow burn gates and the journal heading use.
The value is kept between 0 and 100000. Returns True when set.

```text
cgf "Romantasy.DebugSetDaysTogether" <refid> 1.5
```

### DebugSetMood

```papyrus
Bool Function DebugSetMood(Actor follower, Int score)
```

Sets the mood score, kept between -100 and 100, stamped with today. Warm starts at
25 and Strained at -25. Returns True when set.

```text
cgf "Romantasy.DebugSetMood" <refid> 80
```

### DebugAnnounceNow

```papyrus
Bool Function DebugAnnounceNow()
```

Announces every waiting tier change now, as a calm moment would. In combat nothing
is announced. Returns True when no change is left waiting (also when none was
waiting) and False while some still wait.

```text
cgf "Romantasy.DebugAnnounceNow"
```

### DebugDescribe

```papyrus
String Function DebugDescribe(Actor follower)
```

Returns one line, and writes the same line to the log: points, tier, mood, days
together, the next gate, where the personality came from (`author file`,
`player`, `legacy` or `none`), and the likes, dislikes, favorites and pet peeves.
A very long list is cut short. Returns an empty string when the actor has no bond.

```text
cgf "Romantasy.DebugDescribe" <refid>
```

Example line:

```text
describe: Ambellina | 1012 Friend | mood Warm (30) | 4.2 days together | gate Confidant needs 7 days | personality author file | likes Quests Completed, Dungeons Cleared | dislikes Murders | favorites Quests Completed | pet peeves none
```

The gate shows only under Slow burn.

### DebugSharedMoment

```papyrus
Bool Function DebugSharedMoment(Actor akFollower, String asKind, Float afSeconds = 60.0, Bool abOthers = False, Bool abForce = False)
```

Judges a shared moment as the end of an OStim scene would, and awards it the same way,
with or without OStim installed. `asKind` is `affection`, `closeness` or `intimacy`.
`afSeconds` is how long the scene ran, `abOthers` adds a third person and `abForce`
marks the scene as force. Returns True only when points moved. It returns False for an
unknown kind, an actor with no bond or a bond that is switched off, and for a moment
that counted but gave no points (a second one the same day, a reserved companion, or a
gate that held it).

```text
cgf "Romantasy.DebugSharedMoment" <refid> "affection" 60 0 0
```

See [OStim.md](OStim.md) for what counts and the `[moments]` table.

### From a script

If the console does not reach a function in your setup, call it from a short
script and read the result:

```papyrus
Actor follower = Game.GetFormFromFile(0x00000800, "MyFollower.esp") as Actor
If Romantasy.DebugDeed(follower, "Quests Completed", 1)
    Debug.Notification("Points moved")
EndIf
Debug.Trace(Romantasy.DebugDescribe(follower))
```

`GetApiVersion()` returns 9 for a Romantasy that has these functions. Check it
before you rely on them.

## Read the why log

With Detailed log on, every deed writes one line in `Romantasy.log` for each
follower with a bond. It says what happened, or why nothing did. With Detailed log
off, nothing is built or written.

A gain:

```text
why: Ambellina | Quests Completed x1 | rule 3 | favorite x2 | fast x2 | +12 | 988 -> 1000 Friend
```

| Part | Meaning |
|---|---|
| `Ambellina` | The follower. |
| `Quests Completed x1` | The deed and how many happened. |
| `rule 3` | Points the built-in rule gives for one. |
| `favorite x2` | What the follower feels: `like`, `dislike`, `favorite x2` or `pet peeve x2`. |
| `fast x2` | The player's pace: `fast x2` or `slow /2`. Left out on Standard. |
| `+12` | The points really applied, after the gate and the floor at 0. |
| `988 -> 1000 Friend` | The total before and after, and the tier now. |

A loss carries no pace part, because pace never changes a loss:

```text
why: Ambellina | Murders x1 | rule 5 | pet peeve x2 | -10 | 520 -> 510 Acquaintance
```

When nothing changed, the line ends with the reason:

```text
why: Ambellina | Quests Completed x1 | nothing: stopped by the Friend gate (1.5 of 3 days)
```

The reasons are:

| Reason | What to do |
|---|---|
| `not following` | The follower must be following the player. |
| `no feeling about this deed` | Add the deed to likes, dislikes, favorites or pet peeves. |
| `no rule for this deed` | The follower has a feeling but the rule gives no points. |
| `stopped by the {Tier} gate ({d} of {n} days)` | Slow burn holds the bond until it has {n} days together. |
| `rounded to 0 by slow burn` | Slow burn halved a small gain to nothing. |
| `already at the limit` | A loss on a bond that is already at 0. |

A script award (`ModifyPoints` or `ApplyPreference`) writes a line with no deed:

```text
why: Ambellina | script award | +25 | 975 -> 1000 Friend
why: Ambellina | script award | nothing: stopped by the Friend gate (1.5 of 3 days)
```

A points change from a SkyrimNet conversation (a kind or a harsh word) writes the same
lines as a script award. A change that was applied:

```text
why: Ambellina | script award | +5 | 995 -> 1000 Friend
```

A kind word that a Slow burn gate holds back:

```text
why: Ambellina | script award | nothing: stopped by the Friend gate (1.5 of 3 days)
```

A shared moment writes a line with `shared moment` in place of the deed. The first
two are the same shape as a script award; the third, a held gain, carries the gate:

```text
why: Ambellina | shared moment | +5 | 995 -> 1000 Friend
why: Ambellina | shared moment | nothing: too soon
why: Ambellina | shared moment | nothing: stopped by the Friend gate (1.5 of 3 days)
```

The reasons after `nothing:` for a moment are `moments are off`, `not just the two of you`,
`not a gentle scene`, `nothing happened`, `too short`, `too soon`, `reserved` and `already
counted today`. A moment that counted but gave no points because the companion is reserved
or already had one today reads `nothing: reserved` or `nothing: already counted today`.
A moment that counted at Slow burn's gate reads like the gate line above. The reason
`not the moment asked for` appears only for a gentle scene that Romantasy started and had
to stop.

A change that a limit stops (one of each per in-game hour, 15 a day each way) writes no
why line. Only a debug line in the log says so.

With SkyrimNet installed and Tell SkyrimNet about bonds on, a tier change made with the
debug functions or the developer tools is sent to SkyrimNet like a real one. It stays in
SkyrimNet's history for that companion. Switch Tell SkyrimNet about bonds off while you
test if you do not want that.

## Check your profile file

`romantasy-profile-check.exe` is in the mod's `tools` folder. Give it a profile
file, or a folder (it checks every `.toml` directly inside it):

```powershell
.\tools\romantasy-profile-check.exe --strict 'D:\path\to\SKSE\Plugins\Romantasy\Profiles'
```

Each finding is one line: `file(line): error: message` or
`file(line): warning: message`. Errors are files Romantasy would refuse. Warnings
are likely mistakes: no likes, dislikes, favorites or pet peeves at all, a key the
format does not know (with the nearest known name), an unlock line too long for
the popup (over 48 characters), and a `plugin` name with no `.esp`, `.esm` or
`.esl` ending.

| Exit code | Meaning |
|---|---|
| 0 | Clean. Without `--strict`, warnings still give 0. |
| 1 | At least one error. |
| 2 | Only warnings, and `--strict` was given. |
| 3 | The command line was wrong. |

`--json` prints the findings as one JSON array, for scripts. The checker does not
look at your plugin. Romantasy checks that the NPC exists when Skyrim loads.

## Before you publish

- Run the checker with `--strict` over your Profiles folder. Fix every line.
- Turn author mode off in `settings.json`, or leave it out of what you ship.
- With author mode on, call `DebugDescribe` and check the likes, dislikes,
  favorites and pet peeves are the ones you wrote.
- Use `DebugDeed` for one favorite and one pet peeve. Read the why lines.
- Use `DebugSetPoints` to reach each tier you wrote an unlock line for. Check the
  popup, and that your reaction script runs.
- Use `DebugSetDaysTogether` and a deed to see a Slow burn gate hold, then pass.
- Use `DebugSetMood` to try a Warm and a Strained line.
- Look in `Romantasy.log` for your follower's profile loading without errors.
