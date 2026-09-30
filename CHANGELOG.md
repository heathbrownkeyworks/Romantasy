# Changelog

## 3.0.0 - 2026-09-30

- Replace the external web dashboard with native Dear ImGui/DX11 screens.
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
