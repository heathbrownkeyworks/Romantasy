# Romantasy and SkyrimNet

Romantasy can talk to SkyrimNet when SkyrimNet is installed. Without SkyrimNet nothing
changes: Romantasy writes one line to its log (`SkyrimNet not found; the bridge is off.`)
and the files described here are never read.

## What it does

- **The bond line.** When SkyrimNet builds a prompt for a companion who has a bond,
  it includes one short, factual line from Romantasy: the tier, the mood and the days
  together. A companion with no bond gets nothing.
- **Events.** A tier gained, a tier lost, a reunion and time apart are sent to SkyrimNet,
  so they enter the companion's recent history. SkyrimNet takes them only for a companion
  who is loaded, so a "time apart" event, raised for a companion who is not with you,
  will usually not arrive.
- **Conversation can move a bond, a little.** SkyrimNet's AI can pick "touched" or
  "hurt" for something you said, and Romantasy turns that into a small, capped change.

## The two switches

Both are in Settings and appear only when SkyrimNet was found.

- **Tell SkyrimNet about bonds** (on by default): "Adds each companion's bond to what
  SkyrimNet knows and reports tier changes to it." Off means no bond line and no events.
- **Conversations can move the bond** (on by default): "Lets SkyrimNet's AI nudge a bond
  by a few points for a kind or a harsh word. Capped each day." It does nothing while the
  first switch is off. Off means the two actions are never offered.

The same keys are in `settings.json`: `skyrimNetBridge` and `skyrimNetConversation`.

## The exact text SkyrimNet receives

`{Player}` below is your character's name. If the name is empty once cleaned, the
words `the player` are used in its place.

The bond line, for example:

> Romantasy bond with {Player}: Friend (tier 3 of 6). A trusted friend; warmth without romance. Mood toward {Player} right now: Warm. They have spent 12 days travelling together.

The prompt file puts it after one framing sentence, so the companion reads it as
background about their own feelings:

> Your relationship with {{ player.name }} (background that shapes how you feel and speak; never quote it, and never mention tiers or numbers): {the bond line}

(`{{ player.name }}` is filled in by SkyrimNet.) The line is in the companion's normal
prompts and in their private thoughts.

The sentence after the tier:

| Tier | Sentence |
|---|---|
| Stranger | They barely know each other. |
| Acquaintance | Familiar and polite; not yet close. |
| Friend | A trusted friend; warmth without romance. |
| Confidant | Close enough to share private thoughts; an unspoken attraction may be growing. |
| Lover | In a romantic relationship with {Player}. |
| Spouse | Married to {Player}. |

The mood sentence appears only when the mood is warm (`Mood toward {Player} right now: Warm.`)
or strained (`Things are strained between them right now.`). The days sentence appears
when it is 1 day or more (`They have spent N days travelling together.`).

The four events:

- `{Name}'s bond with {Player} deepened: they are now {Tier}.`
- `{Name}'s bond with {Player} was damaged: they have fallen back to {Tier}.`
- `{Name} and {Player} are together again after {N} days apart.`
- `{Name} has not travelled with {Player} for {N} days.`

`{N} days` reads `1 day` for one day and `less than a day` for a shorter time
(`...together again after less than a day apart.`, `...for less than a day.`).

Events go out at the moment Romantasy tells the follower's own scripts, so a tier change
held for a calm moment is sent when it is announced. The companion sees an event as
narration about them, and SkyrimNet may have them react to it in their next line. Other
NPCs do not see it. Names and tiers are cleaned first: no braces, square brackets,
percent signs or control characters.

## The two actions

SkyrimNet offers these to the AI only for a companion who has a bond and only while
the limits below allow them.

- `RomantasyTouched`: "You are genuinely moved by something kind, thoughtful or loving {{ player.name }} just said or did. Use rarely, only for moments that truly matter to you."
- `RomantasyHurt`: "You are genuinely hurt or angered by something cruel, dismissive or disloyal {{ player.name }} just said or did. Use rarely."

(`{{ player.name }}` is filled in by SkyrimNet.)

| | Points | Label in the recent changes |
|---|---|---|
| Touched | +5 | A kind word |
| Hurt | -5 | A harsh word |

Limits, for each companion:

- At most one touched and one hurt per in-game hour.
- At most +15 and -15 per in-game day (the day is the whole number of the game day).
- Past a limit the action does nothing and is not offered.
- The limits are kept in memory only. They start fresh after a load.
- The companion does not need to be following you.
- Pace never multiplies these changes. The Slow burn gate still applies to a gain.
- A kind word that a Slow burn gate holds back changes nothing. It still uses the hour
  and counts toward the day's 15.
- Mood, the journal and whispers treat them like any other change.

## How it is wired

Romantasy ships one SkyrimNet layer, `coldsun.romantasy`, with a manifest and a single
prompt file (`prompts/submodules/user_final_instructions/0350_romantasy_bond.prompt`).
The prompt file asks the decorator `romantasy_bond` for the speaking NPC and prints the
result, or nothing when it is empty. The decorator and the two actions are registered
through SkyrimNet's Papyrus API each time a save loads (and on a new game), so:

- The bond line appears only after a save has loaded (not on the main menu).
- SkyrimNet asks Romantasy for the line through Papyrus and keeps the answers for a short
  time. The first prompt after a load may come without the line, and a change can take a
  turn to show.
- If the decorator is not registered, SkyrimNet should lose only this one prompt line.
  The file is in the final instructions on purpose, so a failure should not cost a
  companion's whole prompt.
- Both are registered again on every new game and every loaded save. An answer that
  arrives late from an earlier load is ignored. Any whole-number answer from SkyrimNet
  counts as registered.

## Removing it

- To remove only the bond line, delete the folder
  `SKSE/Plugins/SkyrimNet/external/coldsun.romantasy` from the Romantasy mod folder.
  Events and the two actions stay.
- To stop everything Romantasy sends or does, switch both Settings rows off. Romantasy
  then sends nothing and changes nothing. The bond line function and the two actions
  stay registered, and the prompt file still asks for the line and gets an empty one.
  Removing the layer folder is what removes the prompt file.
- If you remove Romantasy but leave the layer folder, SkyrimNet should lose only that
  one prompt line.

## What Settings and Health show

The two Settings rows appear only when SkyrimNet was found. The Health section can show
one of these notes:

- `SkyrimNet found: the bridge is on.`
- `SkyrimNet found: the bridge is off.` (the first switch is off)
- `SkyrimNet found: the bridge is on, but the bond line is not registered.` (this load's
  registration of the bond line failed; events are not affected)
- `SkyrimNet is too old for Romantasy's bridge.` (with "Update SkyrimNet to use the
  bridge. Everything else works without it.")

With no SkyrimNet there is no note.

With a SkyrimNet too old for the bridge, Romantasy still registers the bond line function
and the two actions, so the prompt file finds the function it calls. The function returns
nothing, the actions are never offered, and no events are sent. The two Settings rows do
not appear; the Health section in Settings shows the too-old note above. It is a note, not
a warning, so it never adds a problem line on screen.

## What to check in SkyrimNet's dashboard

- The layer `coldsun.romantasy` is listed and enabled.
- The decorator `romantasy_bond` is known after a save loads.
- In the prompt as sent to a companion with a bond, the bond line is present with the
  right tier and mood, and it arrives as plain text, unchanged. For an NPC with no bond
  there is no line and no error.
- After a tier change, the event shows in the companion's recent events.
- `RomantasyTouched` and `RomantasyHurt` are in the action list, and a kind word shows
  `A kind word` with +5 in Romantasy's recent changes.
