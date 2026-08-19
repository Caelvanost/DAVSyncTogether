# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** that synchronizes the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together handles only DAV-controlled equipment visuals. It intentionally does not synchronize RaceMenu morphs/overlays/head parts or Immersive Equipment Displays content; those remain the responsibility of MorphSync Together and IEDSync Together.

## Current status

**v0.4.1 — DAV relevance filtering and head/hair-safe hidden helmets**

v0.4.0 validated real remote `HIDDEN` / `VISIBLE` application through STRPM, but exposed two side effects:

- unrelated technical ARMO could be mistaken for DAV state changes;
- hiding a helmet mesh on the proxy did not restore the face/hair that the still-equipped head/hair slots continued to suppress.

v0.4.1 addresses both problems.

### DAV config relevance index

At `DataLoaded`, DAVSync scans:

```text
Data/SKSE/Plugins/DynamicArmorVariants/**/*.json
```

and indexes source Armor Addons referenced by `replaceByForm` plus source biped slots referenced by `replaceBySlot`.

Before a state is sent through STRPM, the worn ARMO must have at least one base ARMA that is actually covered by that DAV configuration. Unrelated technical armors are logged as:

```text
DAVST STRPM TX_FILTERED ... reason=not-in-dav-config
```

This prevents false DAV traffic from OCum or other systems that merely happen to lose a rendered node.

### Head/hair-safe hidden helmets

Remote application still targets only Skyrim biped nodes of the form:

```text
(<ARMA>)[...]/ (<ARMO>) [...]
```

For an ARMO using slot 30 (Head) and/or slot 31 (Hair):

- `HIDDEN` culls the armor node and explicitly restores the proxy face/hair nodes;
- `VISIBLE` restores the armor node and returns the affected head/hair nodes to the hidden state expected from those slots;
- `UNEQUIPPED` clears DAVSync culling;
- `REPLACED` remains transport-only until a true replacement variant is validated.

The receive log now includes `headFixes=` for this correction.

## Multiplayer architecture

```text
DAV local probe
    -> load-order-safe ARMO/ARMA identity
    -> DAV config relevance filter
    -> STRPM channel DAVSyncTogether.State.v1
    -> STRPM Sender.connectionID
    -> STRPM ProxyResolver
    -> remote proxy FormID
    -> targeted DAV visual apply
```

DAVSync creates **no custom UDP transport**. Messaging and proxy identity are provided exclusively by STRPluginMessagingAPI.

## Stable form identity

ARMO and ARMA forms are represented as:

```text
plugin filename + local FormID
```

rather than client-specific runtime FormIDs. Runtime IDs remain diagnostic only and are validated with `TESDataHandler::LookupForm(localFormID, pluginName)`.

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

## Building

```bat
build_release.bat
```

The Vortex-ready archive is generated under:

```text
dist/DAVSyncTogether-<version>.zip
```

## Test procedure for v0.4.1

Install the same build on Player1 and Player2 and connect both through Skyrim Together Reborn.

For Kahel and Elir, test a helmet DAV cycle:

1. helmet visible;
2. hide through DAV;
3. confirm the remote helmet disappears while the face and hair remain correct;
4. show through DAV;
5. confirm helmet/head/hair all return to the expected state;
6. repeat once in the opposite player direction.

Relevant logs:

```text
DAVST CONFIG_INDEX loaded ...
DAVST STRPM TX ...
DAVST STRPM TX_FILTERED ...
DAVST STRPM RX_STATE ... headFixes=... apply=1
```

## Versioning

- small fixes increment PATCH;
- larger feature changes increment MINOR and reset PATCH to zero;
- README and deployment ZIP version follow the project version.

## License

MIT
