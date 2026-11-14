# Save samples

Binary samples of the cosave records the save codec reads (`src/romance/SaveCodec.cpp`).
`tests/profiles/SaveSamplesTests.cpp` reads each one and checks it against the values
written in the test, then reads every shorter prefix of it to prove a damaged record
never crashes the reader and never invents entries.

| File | Record | Version | Holds |
|---|---|---|---|
| `points_v3.bin` | `RoPt` | 3 | 3 entries: named, light plugin with no reference, dynamic reference |
| `player_bonds_v1.bin` | `RoPb` | 1 | 2 bonds: one with all four preference weights, one switched off |
| `moods_v1.bin` | `RoMd` | 1 | 2 moods with every company flag |
| `pending_v1.bin` | `RoPn` | 1 | 2 waiting tier changes, one up and one down |
| `journals_v1.bin` | `RoJn` | 1 | 2 journals: a full one (history, days, gains, losses, every milestone kind) and one that only began |
| `moments_v1.bin` | `RoMo` | 1 | 2 shared-moment entries: one with a reference, 4 moments and fractional days; one with no reference, 1 moment and no points-giving day yet (-1) |

Each file is the record body only (what `OpenRecord` data holds), not the record header.

## How they were made

The writer in the codec produced all six. The test `SaveSamples_WriteSamples` writes a
missing one when the environment variable `ROMANTASY_WRITE_SAMPLES=1` is set and never
touches one that exists; a normal run never writes. The first bytes were checked by hand against the layout in `docs/Development.md`
(a `u32` count, then per entry the identities as a `u8` name length, the name, a `u32`
local ID and a `u32` raw ID). No sample was built by hand: the codec reads exactly one
version of each record, so the current writer can still produce every one.

`RoPt` versions 1 and 2 still load, but they are read inside `RomanceManager` from raw
structs, not by the codec, so no test here can read them. `RoPm` and `RoPe` are read there
too.

## Changing the format

A sample is a promise about old saves. Never regenerate one to make a test pass. A new
layout gets a new version number, a new sample file and a reader that keeps the old one
working; the old file stays.

`.gitattributes` marks `*.bin` as binary so a checkout cannot change line endings in them.
