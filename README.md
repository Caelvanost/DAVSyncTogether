# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** that synchronizes the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together handles only DAV-controlled equipment visuals. RaceMenu appearance remains MorphSync Together's responsibility and IED display objects remain IEDSync Together's responsibility.

## Current status

**v0.5.2 — head-safe hidden helmet variant selection**

v0.5.1 restored remote `HIDDEN` synchronization when DAV variant inference was ambiguous, but multiplayer logs showed that culling the matching helmet ARMO node on an STR proxy can also cull the remote head geometry. That fallback is therefore unsafe for slot 30/31 equipment.

v0.5.2 removes that behavior for head/hair equipment and instead ranks ambiguous DAV variants using their actual configuration semantics.

### Head-safe variant selection

When multiple DAV variants produce the same `HIDDEN` rendered result, DAVSync now scores the candidates using:

- exact `replaceByForm` matches before generic slot rules;
- slot 30 / slot 31 relevance;
- `overrideHead=showAll` as the strongest head-safe preference;
- `overrideHead=showHead` as the next preference;
- `hideHair` as lower priority;
- `hideAll` as unsafe for a hidden-helmet synchronization target.

The selected DAV variant is transmitted through STRPM and applied through DAV's own Papyrus API:

```text
DynamicArmor.ApplyVariant(proxy, variant)
DynamicArmor.ResetVariant(proxy, armor)
```

This lets Dynamic Armor Variants itself handle Armor Addons, head/hair override semantics and the 3D refresh.

### No head-slot node-cull fallback

DAVSync no longer uses direct `CullNode()` fallback for ARMO records that occupy Head or Hair slots. If DAV-native application is unavailable for such an item, DAVSync logs a head-safe no-apply result instead of risking removal of the proxy head.

The generic node fallback remains available only for non-head equipment.

### Variant inference

At `DataLoaded`, DAVSync parses:

```text
Data/SKSE/Plugins/DynamicArmorVariants/**/*.json
```

and indexes:

- `name`
- `linkTo`
- `overrideHead`
- `replaceByForm`
- `replaceBySlot`

For an ambiguous hidden helmet, sender logs should now resemble:

```text
DAVST VARIANT_MATCH ... state=HIDDEN variant="..." candidates=N action=send-selected
```

instead of the old `action=send-fallback` behavior.

## Multiplayer architecture

```text
local DAV rendered state
    -> infer/rank DAV variant from DAV JSON config
    -> load-order-safe ARMO/ARMA identity
    -> STRPM channel DAVSyncTogether.State.v2
    -> STRPM Sender.connectionID
    -> STRPM ProxyResolver
    -> remote proxy Actor
    -> DynamicArmor.ApplyVariant / ResetVariant
```

DAVSync creates **no custom UDP transport**. Messaging and proxy identity are provided exclusively by STRPluginMessagingAPI.

## Stable form identity

ARMO and ARMA forms are represented as:

```text
plugin filename + local FormID
```

rather than client-specific runtime FormIDs.

## DAV state model

- `VISIBLE` — rendered ARMA belongs to the original ARMO.
- `HIDDEN` — ARMO remains worn but no matching biped ARMA is rendered.
- `REPLACED` — rendered ARMA differs from the ARMO's original Armor Addons.
- `UNEQUIPPED` — a previously tracked DAV ARMO is no longer worn.

## Requirements

Runtime:

- Skyrim Special Edition / Anniversary Edition
- SKSE
- Skyrim Together Reborn
- Dynamic Armor Variants
- STRPluginMessagingAPI (STRPM)

Build:

- Visual Studio with C++ workload
- CMake 3.24+
- vcpkg
- CommonLibSSE-NG
- nlohmann-json

## Building

```bat
build_release.bat
```

The Vortex-ready archive is generated under:

```text
dist/DAVSyncTogether-<version>.zip
```

## Test procedure for v0.5.2

Install the same build and DAV configuration on Player1 and Player2. Connect both players before toggling helmets.

For Kahel and Elir:

1. helmet visible;
2. hide through DAV;
3. confirm the remote helmet disappears while the head remains visible;
4. check that hair matches the local DAV result;
5. show through DAV;
6. confirm helmet/head/hair return correctly;
7. repeat in the opposite direction.

Expected sender log:

```text
DAVST VARIANT_MATCH ... state=HIDDEN variant="..." candidates=N action=send-selected
DAVST STRPM TX ... state=HIDDEN variant="..." result=ok
```

Expected receiver log:

```text
DAVST DAV_API ApplyVariant ... dispatched=1
DAVST STRPM RX_STATE ... state=HIDDEN variant="..." davDispatch=1 fallbackNodes=0 apply=1
```

A head-slot fallback must never occur. If DAV dispatch is unavailable, the receiver should log:

```text
DAVST HEAD_SAFE no node-cull fallback ...
```

## Versioning

- small fixes increment PATCH;
- larger feature changes increment MINOR and reset PATCH to zero;
- README and deployment ZIP version follow the project version.

## License

MIT
