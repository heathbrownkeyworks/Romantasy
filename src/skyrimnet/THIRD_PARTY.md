# Third-party declarations in src/skyrimnet

## SkyrimNet's C plugin API (events)

`SkyrimNetBridge.cpp` holds a copy of two function declarations from SkyrimNet's plugin API header:

- Source: `sdk/cpp/PublicAPI.h` in https://github.com/MinLL/SkyrimNet-ExamplePlugin (SDK version 0.29.0, API version 14), read on 2026-11-13.
- Declarations copied: `PublicGetVersion` and `PublicRegisterEvent`, as function pointer types. Nothing else from the header is used, and the header itself is not in this repository.
- License: MIT. "Copyright (c) 2025-2026 Fateless.Ai. SkyrimNet's plugin API headers are MIT licensed: include them in any mod, open or closed source. SkyrimNet itself is not open source."

## SkyrimNet's Papyrus API (the bond decorator and the conversation actions)

The bridge calls two functions of SkyrimNet's Papyrus API by name, through the Papyrus VM, with no compile-time reference:

- Source: `Source/Scripts/SkyrimNetApi.psc`, shipped with SkyrimNet 1.1.1.
- Used: `int function RegisterDecorator(String decoratorID, String sourceScript, String functionName) Global Native`, called as `RegisterDecorator("romantasy_bond", "Romantasy_SkyrimNet", "BondDecorator")`. The decorator function's shape, `String Function BondDecorator(Actor akActor) Global`, follows the comment in `SkyrimNetApi.psc` and the decorators in SkyrimNet's `SkyrimNetInternal.psc`.
- Used: `int function RegisterAction(String actionName, String description, String eligibilityScriptName, String eligibilityFunctionName, String executionScriptName, String executionFunctionName, String triggeringEventTypesCsv, String categoryStr, int defaultPriority, String parameterSchemaJson, String customCategory="", String tags="") Global Native`, called once for `RomantasyTouched` (`TouchedEligible`, `TouchedExecute`) and once for `RomantasyHurt` (`HurtEligible`, `HurtExecute`), all on `Romantasy_SkyrimNet`, with category `PAPYRUS`, priority 1 and every other string empty. All twelve arguments are passed, because a call through the VM fills no defaults. The eligibility and execution shapes, `Bool Function X(Actor akActor, String contextJson, String paramsJson) Global` and `Function X(Actor akActor, String contextJson, String paramsJson) Global`, follow SkyrimNet's `SkyrimNetInternal.psc`.
- License: MIT. "Copyright (c) 2025-2026 Fateless.Ai. SkyrimNet's plugin API is MIT licensed: compile against it in any mod, open or closed source. SkyrimNet itself is not open source." Nothing from the file is copied into this repository, and no Romantasy script refers to `SkyrimNetApi`.

## What is not used, and why

The C header asks plugins to match SkyrimNet's compiler and C runtime because several exports pass `std::string` and `std::function` across the DLL boundary. SkyrimNet.dll is built with the static C runtime (`/MT`: it imports no `MSVCP140.dll` or `VCRUNTIME140.dll`). Romantasy.dll is built with the DLL runtime (`/MD`: it imports both). So the bridge calls only exports that take plain C values (`const char*`, integers) and resolves none that take a C++ object: `PublicRegisterDecorator`, `PublicRegisterCPPAction`, `PublicRegisterCPPSubCategory` and `PublicRegisterEventCallback` are not used. The decorator and the actions go through the Papyrus API instead, where only Papyrus values cross.
