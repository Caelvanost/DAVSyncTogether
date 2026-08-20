# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** that synchronizes the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together handles only DAV-controlled equipment visuals. RaceMenu appearance remains MorphSync Together's responsibility and IED display objects remain IEDSync Together's responsibility.

## Current status

**v0.5.1 — restore remote HIDDEN fallback when variant inference is ambiguous**

v0.5.0 introduced DAV-native remote application by inferring the sender's DAV variant and calling `DynamicArmor.ApplyVariant` / `ResetVariant` on the remote STR proxy. Multiplayer testing showed that hidden helmets can legitimately match multiple DAV variants when several variants replace the same head slot with an empty Armor Addon list. Refusing to send ambiguous states caused a regression where the remote helmet no longer disappeared.

v0.5.1 keeps DAV-native application when inference is unique, but restores a conservative HIDDEN fallback when it is not.

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

When the local probe observes `HIDDEN` or `REPLACED`, DAVSync computes the effective ARMA result for relevant variants and compares it with the rendered ARMA set.

If exactly one variant matches:

```text
DAVST VARIANT_MATCH ... candidates=1 action=dav-api
```

DAVSync sends the exact variant name and the receiver delegates the result to DAV.

If a **HIDDEN** result is ambiguous but the ARMO is confirmed DAV-relevant:

```text
DAVST VARIANT_MATCH ... state=HIDDEN candidates=N action=send-fallback
```

DAVSync still sends the state with an empty variant name. The receiver then culls only the matching ARMO/ARMA biped node. This restores remote helmet disappearance without touching face or hair nodes directly. The state remains tracked so the later `VISIBLE` or `UNEQUIPPED` transition can undo the fallback.

Ambiguous `REPLACED` states remain unsent until their exact DAV variant can be identified safely.

### DAV-native remote application

For uniquely identified variants, the receiver resolves the STRPM sender to the correct STR proxy and dispatches DAV's existing Papyrus API:

```text
DynamicArmor.ApplyVariant(proxy, variant)
DynamicArmor.ResetVariant(proxy, armor)
```

This delegates Armor Addon replacement, 3D refresh and `overrideHead` behavior to Dynamic Armor Variants itself. DAVSync does not directly manipulate the proxy FaceNode or hair head part.

When a HIDDEN variant is ambiguous or DAV dispatch is unavailable, a conservative armor-node cull is used as fallback. The fallback never manipulates face or hair nodes.

## Multiplayer architecture

```text
local DAV rendered state
    -> infer DAV variant from DAV JSON config
    -> load-order-safe ARMO/ARMA identity
    -> STRPM channel DAVSyncTogether.State.v2
    -> STRPM Sender.connectionID
    -> STRPM ProxyResolver
    -> remote proxy Actor
    -> DynamicArmor.ApplyVariant / ResetVariant
       or targeted HIDDEN fallback if ambiguous
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

## Test procedure for v0.5.1

Install the same build and DAV configuration on Player1 and Player2. Connect both players before toggling helmets.

For Kahel and Elir:

1. helmet visible;
2. hide through DAV;
3. confirm the remote helmet disappears;
4. check whether face and hair remain correct;
5. show through DAV;
6. confirm the helmet returns;
7. repeat in the opposite direction.

For the currently observed helmets, sender logs are expected to show ambiguous fallback, for example:

```text
DAVST VARIANT_MATCH ... state=HIDDEN candidates=2 action=send-fallback
DAVST STRPM TX ... state=HIDDEN variant="" result=ok
```

or:

```text
DAVST VARIANT_MATCH ... state=HIDDEN candidates=49 action=send-fallback
```

Receiver logs should then show the targeted fallback being applied. A later unique match should instead show `DAVST DAV_API ApplyVariant ... dispatched=1`.

## Versioning

- small fixes increment PATCH;
- larger feature changes increment MINOR and reset PATCH to zero;
- README and deployment ZIP version follow the project version.

## License

MIT
