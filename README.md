# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** intended to synchronize the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together is responsible only for DAV-controlled equipment visuals, such as Armor Addon replacements and hidden/shown equipment variants.

It intentionally does **not** synchronize:

- RaceMenu face or body morphs
- overlays, makeup, body paint or skin appearance
- hair/head-part customization
- Immersive Equipment Displays nodes or favorite-item displays

Those concerns belong to MorphSync Together and IEDSync Together respectively.

## Current status

**v0.3.0 — STRPM transport validation**

DAVSync now uses **STRPluginMessagingAPI (STRPM)** for multiplayer messaging and remote proxy identity. DAVSync does not create its own UDP socket, port, discovery protocol, or network configuration.

The v0.3.0 path is:

```text
DAV local probe
    -> load-order-safe ARMO/ARMA identity
    -> STRPM channel DAVSyncTogether.State.v1
    -> remote STRPM Sender.connectionID
    -> STRPM ProxyResolver
    -> remote proxy FormID
```

For this milestone, received states are **not applied** to the remote proxy yet. The receiver only validates that:

- the STRPM message arrived
- the sender ConnectionID is available
- STRPM resolves that ConnectionID to the corresponding proxy FormID
- the transmitted ARMO and active ARMA identities resolve correctly against the receiver's own load order

The log explicitly reports `apply=0`.

### DAV-only traffic filtering

DAVSync does not broadcast every worn ARMO. A form enters the STRPM stream only when DAVSync observes a DAV-relevant non-default visual state:

- `HIDDEN`
- `REPLACED`

Once tracked, DAVSync also sends the corresponding reset transition:

- `VISIBLE`
- `UNEQUIPPED`

This prevents normal equipment, OCum technical armor, IED displays, RaceMenu nodes, and unrelated systems from entering the DAV synchronization stream simply because they are present on the actor.

## Stable form identity

ARMO and ARMA forms are represented as:

```text
plugin filename + local FormID
```

instead of runtime FormIDs assigned by the local load order.

For diagnostics, DAVSync also records the current runtime FormID and verifies a round trip through `TESDataHandler::LookupForm(localFormID, pluginName)`.

## DAV state model

The effective DAV visual state is classified as:

- `VISIBLE` — rendered ARMA belongs to the original ARMO
- `HIDDEN` — the ARMO is worn but no matching biped ARMA is rendered
- `REPLACED` — at least one rendered ARMA is not one of the ARMO's original Armor Addons
- `UNEQUIPPED` — a previously tracked DAV ARMO is no longer worn

## DAV compatibility strategy

The original Dynamic Armor Variants exposes Papyrus functions such as `GetVariants`, `GetEquippedArmorsWithVariants`, `ApplyVariant`, `ResetVariant` and `ResetAllVariants`, but does not expose a native getter for the currently active variant.

DAVSync Together therefore observes DAV's **effective rendered Armor Addon result** rather than reading private DAV state. This keeps the current implementation compatible with the original `DynamicArmorVariants.dll`.

Dynamic Armor Variants Extended (DAVE) may be supported later as an optional richer integration, but it is not required.

## Architecture

1. **DAV local-state capture** — implemented.
2. **Stable equipment identity** — implemented.
3. **STRPM messaging** — implemented in v0.3.0.
4. **STRPM proxy resolution** — implemented in v0.3.0, diagnostic only.
5. **DAV state application** — not implemented yet.
6. **Refresh handling** — not implemented yet.

## Requirements

Runtime:

- Skyrim Special Edition / Anniversary Edition
- SKSE
- Skyrim Together Reborn
- Dynamic Armor Variants
- **STRPluginMessagingAPI (STRPM)**

DAVSync itself has no custom UDP transport dependency.

Build:

- Visual Studio with the C++ workload
- CMake 3.24+
- vcpkg
- CommonLibSSE-NG

The public STRPM client header is vendored under `include/STRPluginMessagingAPI`; the runtime implementation remains provided by `STRPluginMessagingAPI.dll`.

## Building

```bat
build_release.bat
```

The Vortex-ready archive is generated under:

```text
dist/DAVSyncTogether-<version>.zip
```

with the DLL packaged as:

```text
SKSE/Plugins/DAVSyncTogether.dll
```

## Test procedure for v0.3.0

Install the same DAVSync build and STRPluginMessagingAPI on Player1 and Player2, then connect both clients through Skyrim Together Reborn.

On Player1, with the Iron Plate Helmet equipped:

1. keep the helmet visible
2. hide it through DAV
3. wait about 2 seconds
4. show it again through DAV
5. wait about 2 seconds

Optional second sequence to validate `UNEQUIPPED` transport:

1. hide the helmet through DAV
2. while it is still hidden, unequip it

Player1 should log lines such as:

```text
DAVST STRPM ready channel="DAVSyncTogether.State.v1" ...
DAVST STRPM TX armoStable="ccbgssse052-ba_iron.esl|00000803" state=HIDDEN ... result=ok
DAVST STRPM TX armoStable="ccbgssse052-ba_iron.esl|00000803" state=VISIBLE ... result=ok
```

Player2 should log a corresponding receive validation similar to:

```text
DAVST STRPM RX_STATE sender="Kahel" connection=... proxyResult=ok proxyForm=... proxyActor=1 armoStable="ccbgssse052-ba_iron.esl|00000803" state=HIDDEN armoResolved=... valid=1 apply=0
```

The v0.3.0 transport test is successful when:

- Player1 reports `result=ok` for TX
- Player2 receives the same DAV state
- `proxyResult=ok`
- `proxyForm` is non-zero
- `proxyActor=1`
- the ARMO/ARMA forms resolve locally with `valid=1`
- `apply=0` remains present, confirming DAVSync did not modify the proxy yet

## Versioning

- small fixes increment PATCH
- larger feature changes increment MINOR and reset PATCH to zero
- README and release archive version follow the project version

## License

MIT
