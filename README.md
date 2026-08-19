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

**v0.4.0 — remote DAV visibility application**

The v0.3.0 STRPM transport/proxy-resolution test succeeded: state messages arrived on the other client, load-order-safe ARMO/ARMA identities resolved correctly, and STRPM mapped the sender ConnectionID to the correct remote proxy actor.

v0.4.0 enables the first actual visual application on that STR proxy.

The path is:

```text
DAV local probe
    -> load-order-safe ARMO/ARMA identity
    -> STRPM channel DAVSyncTogether.State.v1
    -> STRPM Sender.connectionID
    -> STRPM ProxyResolver
    -> remote proxy FormID
    -> matching Skyrim biped ARMA/ARMO scene node
    -> CullNode(true/false)
```

DAVSync still creates **no custom UDP socket, port, discovery protocol or network configuration**. Multiplayer messaging and proxy identity come exclusively from STRPluginMessagingAPI.

### Supported remote states in v0.4.0

- `HIDDEN` — find only biped scene nodes whose parsed owning ARMO matches the received ARMO, then cull that node and its geometry.
- `VISIBLE` — un-cull the matching biped node.
- `UNEQUIPPED` — clear any DAVSync culling that might remain for that ARMO; normal STR equipment replication remains responsible for the actual unequip.
- `REPLACED` — transported and resolved, but **not applied yet**. A true replacement variant needs a dedicated validation test before implementation.

The biped matcher only accepts Skyrim armor geometry names of the form:

```text
(<ARMA>)[...]/ (<ARMO>) [...]
```

This means DAVSync does not target unrelated scene nodes such as `HelmetGO`, IED custom nodes, RaceMenu overlays, SMP nodes, or other arbitrary NiNodes.

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

DAVSync therefore observes and transmits DAV's **effective rendered Armor Addon result**. For the first remote-application milestone, hidden/shown state is reproduced at the corresponding biped ARMO node. Replacement variants remain deferred until their exact sender-side identity can be validated.

Dynamic Armor Variants Extended (DAVE) may be supported later as an optional richer integration, but it is not required.

## Architecture

1. **DAV local-state capture** — implemented.
2. **Stable equipment identity** — implemented.
3. **STRPM messaging** — implemented.
4. **STRPM proxy resolution** — implemented.
5. **Remote HIDDEN/VISIBLE application** — implemented in v0.4.0.
6. **REPLACED application** — pending dedicated variant test.
7. **Refresh/rebuild resilience** — pending if STR rebuilds prove able to undo the applied cull state.

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

## Test procedure for v0.4.0

Install the same DAVSync build and STRPluginMessagingAPI on Player1 and Player2, then connect both players through Skyrim Together Reborn.

With Kahel's Iron Plate Helmet equipped:

1. confirm the helmet is visible on Kahel and on Kahel's proxy from Player2's view
2. hide the helmet through DAV on Kahel
3. wait about 2 seconds and verify whether it disappears from Kahel's proxy on Player2
4. show the helmet again through DAV
5. verify whether it reappears on the proxy
6. repeat the hide/show cycle once more to check stability

The receiver should log lines similar to:

```text
DAVST STRPM RX_STATE sender="Kahel" ... state=HIDDEN ... applySupported=1 matchedNodes=1 changedNodes=1 apply=1
DAVST STRPM RX_STATE sender="Kahel" ... state=VISIBLE ... applySupported=1 matchedNodes=1 changedNodes=1 apply=1
```

Interpretation:

- `valid=1` — received ARMO/ARMA identities resolved locally
- `proxyResult=ok` and `proxyActor=1` — STRPM found the correct proxy
- `applySupported=1` — the received state is implemented by the current applier
- `matchedNodes>0` — the matching biped ARMO node exists on the proxy
- `changedNodes>0` — its cull state actually changed during this receive
- `apply=1` — DAVSync successfully targeted at least one matching node

If `matchedNodes=0`, the next debugging step is to inspect the proxy's biped scene-node naming rather than changing STRPM or the form-identity protocol.

## Versioning

- small fixes increment PATCH
- larger feature changes increment MINOR and reset PATCH to zero
- README and release archive version follow the project version

## License

MIT
