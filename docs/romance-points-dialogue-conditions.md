# Romantasy 3.0 dialogue conditions

Use the Creation Kit's standard **GetGraphVariableInt** condition on **Subject**
to query the speaker's Romantasy state. These reserved queries are provided by the
native DLL; they do not require romance factions, dialogue fragments, or new globals.

First supply the follower's [TOML profile](FileProfiles.md), or enroll an eligible
follower through New bond. Always include the registration condition:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
```

| Variable name | Managed actor | Unmanaged actor |
|---|---|---|
| `Romantasy_Registered` | 1 | 0 |
| `Romantasy_Level` | 1 through 6 | 0 |
| `Romantasy_Points` | Exact earned points | -1 |

Enter names exactly, without quotation marks in the CK parameter field. These are
reserved condition queries, not animation variables to set with Papyrus. Run On
Subject selects the speaker; running on Player would query the player instead.

## Examples

Add each group to the INFO with AND grouping.

Friend or higher:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Level       >= 3
```

At least 2,000 points:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Points      >= 2000
```

From 1,000 through 1,499 points:

```text
GetGraphVariableInt  Romantasy_Registered  == 1
GetGraphVariableInt  Romantasy_Points      >= 1000
GetGraphVariableInt  Romantasy_Points      < 1500
```

The Registered guard matters especially with `<`, `<=`, or `!=`: an unmanaged
actor's sentinel value can otherwise satisfy the comparison.

## Tier conversion

| Tier | New level | Legacy faction rank | Minimum points |
|---|---:|---:|---:|
| Stranger | 1 | 0 | 0 |
| Acquaintance | 2 | 1 | 500 |
| Friend | 3 | 2 | 1,000 |
| Confidant | 4 | 3 | 1,500 |
| Lover | 5 | 4 | 2,000 |
| Spouse | 6 | 5 | 2,500 |

When converting `ROM_RomanceLevel` gates, add one to the rank comparison value
and retain the operator, run-on target, and original logical grouping. Exact point
thresholds are unchanged. Preserve all unrelated conditions and script fragments.

The new queries do not need `CS_Romantasy.esp` as a follower master. Remove that
master only after auditing every remaining reference. Legacy `GetFactionRank`
queries against `ROM_RomanceLevel` and the `ROM_RomancePoints` proxy remain for
unconverted integrations; use the new queries for TOML-based followers.

## Verification

Save and reopen the INFO in CK to verify string parameters and grouping. Test
below, at, and above thresholds; compare two actors with different balances;
save/reload; and verify an unmanaged actor cannot enter guarded dialogue.
The dashboard and `Romantasy.GetPoints(actor)` expose balances for inspection.

Condition reads do not enroll actors or mutate progress. Without the DLL, these
are ordinary engine graph-variable conditions; verify guarded behavior with the
actual follower before claiming missing-runtime compatibility.
