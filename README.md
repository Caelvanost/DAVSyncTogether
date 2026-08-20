# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** that synchronizes the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together handles only DAV-controlled equipment visuals. RaceMenu appearance remains MorphSync Together's responsibility and IED display objects remain IEDSync Together's responsibility.

## Current status

**v0.6.0 — REPLACED variant synchronization validation**

v0.5.2 established the head-safe DAV-native path for hidden helmets. v0.6.0 extends the same architecture to visible replacement variants (`REPLACED`) while keeping ambiguous cases conservative.

### REPLACED variant selection

When DAV renders one or more Armor Addons that differ from the equipped ARMO's base Armor Addons, DAVSync classifies the state as `REPLACED`.

DAVSync compares the observed active ARMA set against the expected result of every relevant DAV variant. If several variants produce the same rendered result, v0.6.0 ranks them by specificity:

- exact `replaceByForm` matches are strongly preferred;
- matching `replaceBySlot` rules are secondary;
- narrower rules are preferred over broad generic rules;
- if two candidates remain equally specific, DAVSync does **not** guess.

A selected replacement is sent through STRPM and applied by DAV itself:

```text
DynamicArmor.ApplyVariant(proxy, variant)
DynamicArmor.ResetVariant(proxy, armor)
```

### REPLACED diagnostics

When a real replacement is detected, the sender now logs:

```text
DAVST REPLACED_MATCH armoStable="..." activeARMA=["..."] candidates=N names=["..."]
```

If a variant is safely selected:

```text
DAVST VARIANT_MATCH ... state=REPLACED variant="..." candidates=N action=send-selected
```

If candidates remain indistinguishable:

```text
DAVST VARIANT_MATCH ... state=REPLACED candidates=N action=ambiguous-not-sent
```

This keeps replacement synchronization safe while exposing enough information to refine any remaining edge case.

### Head-safe hidden helmets

The v0.5.2 behavior remains unchanged:

- ambiguous hidden helmet variants are ranked using DAV's `overrideHead` semantics;
- `showAll` / `showHead` are preferred over head-hiding variants;
- Head/Hair ARMO records never use the direct `CullNode()` fallback;
- DAV itself handles head/hair visibility and 3D refresh.

## Multiplayer architecture

```text
local DAV rendered state
    -> classify VISIBLE / HIDDEN / REPLACED
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

## Test procedure for v0.6.0

Install the same build and DAV configuration on Player1 and Player2 and connect both players before changing variants.

Use any DAV item that has a **visible alternate variant** rather than a hidden variant. A valid test must produce an `ARMOR_STATE ... state=REPLACED` line where `activeARMA` differs from `baseARMA`.

Test sequence:

1. equip the item in its normal variant;
2. switch to a visible DAV alternate variant;
3. confirm the remote client sees the same alternate model;
4. switch back to the base variant;
5. confirm the remote model returns to normal;
6. repeat in the opposite player direction.

Expected sender log:

```text
DAVST ARMOR_STATE ... state=REPLACED baseARMA=[...] activeARMA=[...]
DAVST REPLACED_MATCH ... candidates=N names=[...]
DAVST VARIANT_MATCH ... state=REPLACED variant="..." action=send-selected
DAVST STRPM TX ... state=REPLACED variant="..." result=ok
```

Expected receiver log:

```text
DAVST DAV_API ApplyVariant ... dispatched=1
DAVST STRPM RX_STATE ... state=REPLACED variant="..." davDispatch=1 fallbackNodes=0 apply=1
```

Returning to the base model should produce `VISIBLE` and `ResetVariant ... dispatched=1`.

## Versioning

- small fixes increment PATCH;
- larger feature changes increment MINOR and reset PATCH to zero;
- README and deployment ZIP version follow the project version.

## License

MIT
