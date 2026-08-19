# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** that synchronizes the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together handles only DAV-controlled equipment visuals. RaceMenu appearance remains MorphSync Together's responsibility and IED display objects remain IEDSync Together's responsibility.

## Current status

**v0.5.0 — DAV-native remote variant application**

The v0.4.x tests proved that STRPM transport, stable ARMO/ARMA identity and proxy resolution work, but also showed that directly culling face/hair nodes cannot reproduce DAV's biped `overrideHead` behavior reliably. In particular, Elir's hair could remain hidden even though the hair NiAVObject itself was not app-culled.

v0.5.0 therefore stops manipulating face and hair nodes directly.

### Variant inference

At `DataLoaded`, DAVSync parses the actual DAV JSON files under:

```text
Data/SKSE/Plugins/DynamicArmorVariants/**/*.json
```

It indexes each DAV variant's:

- `name`
- `linkTo`
- `overrideHead`
- `replaceByForm`
- `replaceBySlot`

When the local probe observes `HIDDEN` or `REPLACED`, DAVSync computes the effective ARMA result for each relevant variant and compares it with the rendered ARMA set. If exactly one variant matches, DAVSync logs:

```text
DAVST VARIANT_MATCH ... variant="..." candidates=1
```

and sends that variant name through STRPM.

If the rendered result is ambiguous, the transition is not sent and the log reports `candidates=` so the mapping can be refined without applying the wrong DAV head/slot behavior.

### DAV-native remote application

The receiver resolves the STRPM sender to the correct STR proxy and calls DAV's existing Papyrus API:

```text
DynamicArmor.ApplyVariant(proxy, variant)
DynamicArmor.ResetVariant(proxy, armor)
```

This delegates Armor Addon replacement, 3D refresh and `overrideHead` behavior to Dynamic Armor Variants itself. DAVSync no longer culls the proxy FaceNode or hair head part.

A conservative armor-node cull remains only as a fallback if the DAV Papyrus dispatch itself fails. The fallback never manipulates face or hair nodes.

Relevant receive logs now include:

```text
variant="..."
davDispatch=1
fallbackNodes=0
```

## Multiplayer architecture

```text
local DAV rendered state
    -> infer DAV variant from DAV JSON config
    -> load-order-safe ARMO/ARMA identity
    -> STRPM channel DAVSyncTogether.State.v1
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

## Test procedure for v0.5.0

Install the same build and the same DAV configuration on Player1 and Player2. Connect both players before toggling helmets.

For Kahel and Elir:

1. helmet visible;
2. hide through DAV;
3. verify helmet disappearance and correct face/hair on the remote proxy;
4. show through DAV;
5. verify helmet/head/hair return correctly;
6. repeat in the opposite direction.

Expected sender log:

```text
DAVST VARIANT_MATCH ... candidates=1 variant="..."
DAVST STRPM TX ... state=HIDDEN variant="..." result=ok
```

Expected receiver log:

```text
DAVST DAV_API ApplyVariant ... dispatched=1
DAVST STRPM RX_STATE ... state=HIDDEN variant="..." davDispatch=1 fallbackNodes=0 apply=1
```

On `VISIBLE`:

```text
DAVST DAV_API ResetVariant ... dispatched=1
```

## Versioning

- small fixes increment PATCH;
- larger feature changes increment MINOR and reset PATCH to zero;
- README and deployment ZIP version follow the project version.

## License

MIT
