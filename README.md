# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** that synchronizes the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together handles only DAV-controlled equipment visuals. RaceMenu appearance remains MorphSync Together's responsibility and IED display objects remain IEDSync Together's responsibility.

## Current status

**v0.6.1 — actor-specific REPLACED matching**

v0.5.2 established the head-safe DAV-native path for hidden helmets. v0.6.0 added visible replacement (`REPLACED`) synchronization. Multiplayer testing with the vanilla Common Mage Hood and Dynamic Lowered Hoods exposed an ARMA-selection edge case: an ARMO can reference several alternative Armor Addons for sex/race while the actor renders only one of them.

v0.6.1 fixes matching by comparing the observed replacement ARMA against the union of replacements that a DAV variant can produce from the ARMO's possible base Armor Addons. It no longer requires every sex/race alternative ARMA to be rendered simultaneously.

### REPLACED variant selection

When DAV renders one or more Armor Addons that differ from the equipped ARMO's base Armor Addons, DAVSync classifies the state as `REPLACED`.

For `REPLACED`, DAVSync now:

- collects replacement ARMA reachable from each base ARMA through `replaceByForm` or the first matching `replaceBySlot` rule;
- accepts the actor's actually rendered ARMA as a subset of those possible replacements;
- allows unchanged base ARMA to coexist with replacements for multi-addon armor;
- requires at least one observed replacement ARMA before considering the variant a match;
- ranks multiple candidates conservatively and does not guess when specificity is tied.

A selected replacement is sent through STRPM and applied by DAV itself:

```text
DynamicArmor.ApplyVariant(proxy, variant)
DynamicArmor.ResetVariant(proxy, armor)
```

### REPLACED diagnostics

A real replacement produces:

```text
DAVST ARMOR_STATE ... state=REPLACED baseARMA=[...] activeARMA=[...]
DAVST REPLACED_MATCH ... candidates=N names=[...]
```

If safely selected:

```text
DAVST VARIANT_MATCH ... state=REPLACED variant="..." candidates=N action=send-selected
DAVST STRPM TX ... state=REPLACED variant="..." result=ok
```

If candidates remain indistinguishable:

```text
DAVST VARIANT_MATCH ... state=REPLACED candidates=N action=ambiguous-not-sent
```

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

## Test procedure for v0.6.1

Install the same build and DAV configuration on Player1 and Player2 and connect both players before changing variants.

Recommended regression test: **Common Mage Hood** with **Dynamic Lowered Hoods**.

1. equip the Common Mage Hood in its raised state;
2. trigger the lowered-hood DAV variant;
3. confirm the local log reports `state=REPLACED` with an active ARMA from `DynamicLoweredHoods.esp`;
4. confirm `REPLACED_MATCH` finds one or more candidates;
5. confirm the remote proxy shows the lowered hood;
6. switch back to the raised hood and confirm `ResetVariant` restores the remote model;
7. repeat in the opposite player direction.

Expected sender log:

```text
DAVST ARMOR_STATE ... state=REPLACED baseARMA=[...] activeARMA=["DynamicLoweredHoods.esp|..."]
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
