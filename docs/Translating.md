# Translating the ledger

Almost every word the ledger page and the whisper card show sits in one file.
You can change a word, or translate the whole page, by editing that file. No tools
are needed beyond a text editor. A few words are not in the file yet; see "What is
not in the file" below.

## Where the file is

Inside the mod folder: `Magelight\Romantasy\js\strings.js`

A mod update replaces this file. Keep a copy of your translation outside the mod
folder and put it back after each update.

## How to change a line

Each line has a key on the left and the text on the right:

```
'roster.find': 'Find a companion',
```

- Change only the text on the right of the colon. Never change the key.
- Keep the quotes and the comma at the end of the line.
- A text that contains an apostrophe goes in double quotes, like this:
  `'settings.title': "The scribe's quill",`
- Keep every `{place}` exactly as written, braces included. The page fills each
  one with a name or a number. A place may move within the sentence.
- A word that changes with a count has two keys: `.one` is used when the count is
  exactly 1, and `.many` for 0 and for every other number.
- The `· ` at the start of the `roster.following` texts is part of the text.
  Keep it or replace it with your own separator.

## What `[some.key]` on screen means

A word that shows as `[some.key]` has no line in your file. Add a line with that
key. The page also writes a note about it once to its console.

If every word shows as a key in brackets, the file has a mistake, most often a
quote: an apostrophe inside single quotes, or a missing comma. The ledger still
opens and closes while the file is broken. Fix the line, or put back your copy.

## What is not in the file

These come from a follower's own files or from the game, and stay in the
language their author wrote them in:

- Names of followers.
- Unlock lines. A follower's author writes them.
- Notes that come from the health check.
- The journal's entries.
- The text of a whisper.

These words come from the plugin and stay in English for now. The file has no
lines for them:

- The day words in Recent changes: "Today", "Yesterday" and "N days ago".
- The deed category headings in New bond.
- The messages in the notice at the foot of the ledger.
- Deed labels in Recent changes and in the Journal.

Tier names, moods and deed labels are page words, and the file can translate them:

- Tiers: `tier.1` to `tier.6`. The page uses these in place of the names the
  plugin sends. A tier name a follower author invented is shown as sent.
- Moods: the `mood.*` lines.
- Deeds: a line named `deed.` and the deed's editor ID, for example
  `deed.ROM_LocationsDiscovered`. A follower's likes and dislikes are deeds, so a
  `deed.` line translates them in the dossier and in New bond. The shipped file has
  no `deed.` lines, so deeds show the plugin's own label until you add one.

Searching for a deed in New bond still matches the plugin's own label, not your
translation.

## Long translations

Buttons and headings keep their place. These end in "…" when the text is too
long for its place, and show their full text when you hold the pointer over them:

- The companion's name at the top of the dossier.
- "Your Company" at the top of the roster.
- The three order choices under the find field.
- The Milestones heading.
- The bond tools in the Milestones card.

In Settings, a row whose choices do not fit beside its title moves the choices
under the title. The row grows taller and the Settings list scrolls.

## Fonts

The bundled fonts (Bodoni Moda, Jost and Pinyon Script) cover Latin letters.
Other alphabets are drawn with the Windows fonts Segoe UI, Microsoft YaHei, Yu
Gothic and Malgun Gothic when they are installed.
