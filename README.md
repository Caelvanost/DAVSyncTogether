# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** intended to synchronize the visual result of **Dynamic Armor Variants (DAV)** between players in **Skyrim Together Reborn**.

## Scope

DAVSync Together is responsible only for DAV-controlled equipment visuals, such as armor-addon replacements and hidden/shown equipment variants.

It intentionally does **not** synchronize:

- RaceMenu face or body morphs
- overlays, makeup, body paint or skin appearance
- hair/head-part customization
- Immersive Equipment Displays nodes or favorite-item displays

Those concerns belong to MorphSync Together and IEDSync Together respectively.

## Current status

**v0.2.1 — targeted DAV armor-state probe**

This milestone replaces the broad scene-graph diff used by v0.2.0 with a DAV-specific representation.

For every worn ARMO that has Armor Addons, DAVSync now:

- scans the live player 3D only for Skyrim biped armor geometry names
- parses rendered `ARMA -> ARMO` pairs such as `"(FE02380B)[0]/ (FE023803) [100%]"`
- compares rendered ARMA forms with the ARMO's original Armor Addons
- classifies the effective visual state as:
  - `VISIBLE` — rendered ARMA belongs to the original ARMO
  - `HIDDEN` — the ARMO is worn but no matching biped ARMA is rendered
  - `REPLACED` — at least one rendered ARMA is not one of the ARMO's original Armor Addons
- logs `UNEQUIPPED` when a previously tracked ARMO is removed

The state hash no longer includes unrelated RaceMenu, SMP, IED or other scene nodes, so those systems should not generate DAVSync state changes.

This milestone is still read-only. It does not transmit or apply state to Skyrim Together proxies yet.

## DAV compatibility strategy

The original Dynamic Armor Variants exposes Papyrus functions such as `GetVariants`, `GetEquippedArmorsWithVariants`, `ApplyVariant`, `ResetVariant` and `ResetAllVariants`, but does not expose a native getter for the currently active variant.

DAVSync Together therefore observes DAV's **effective rendered Armor Addon result** rather than reading private DAV state. This keeps the current implementation compatible with the original `DynamicArmorVariants.dll`.

Dynamic Armor Variants Extended (DAVE) may be supported later as an optional richer integration, but it is not a required dependency.

## Planned architecture

1. **DAV local-state capture** — determine the effective DAV-controlled visual state of the local player.
2. **Stable equipment identity** — encode armor/armor-addon identities in a load-order-safe form.
3. **Transport** — exchange DAV state between Skyrim Together clients.
4. **STR proxy resolution** — associate remote player identities with their proxy actors.
5. **DAV state application** — reproduce the sender's DAV visual state on the corresponding proxy.
6. **Refresh handling** — refresh only the affected equipment/3D state and avoid unnecessary full actor rebuilds.

## Requirements

Runtime:

- Skyrim Special Edition / Anniversary Edition
- SKSE
- Skyrim Together Reborn
- Dynamic Armor Variants

Build:

- Visual Studio with the C++ workload
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

with the DLL packaged as:

```text
SKSE/Plugins/DAVSyncTogether.dll
```

## Test procedure for v0.2.1

After loading a save:

1. keep the helmet equipped and visible
2. use DAV to hide it
3. wait about 2 seconds
4. use DAV to show it again
5. wait about 2 seconds
6. unequip the helmet
7. re-equip it

Inspect `DAVSyncTogether.log`. The relevant lines are now only:

```text
DAVST DAV_STATE ...
DAVST ARMOR_STATE armo=... state=VISIBLE ...
DAVST ARMOR_STATE armo=... state=HIDDEN ...
DAVST ARMOR_STATE armo=... state=REPLACED ...
DAVST ARMOR_STATE armo=... state=UNEQUIPPED ...
```

For the Iron Plate Helmet test already identified in v0.2.0, the expected visible state should resolve approximately as:

```text
armo=FE023803 state=VISIBLE baseARMA=[FE02380B] activeARMA=[FE02380B]
```

and the DAV-hidden state as:

```text
armo=FE023803 state=HIDDEN baseARMA=[FE02380B] activeARMA=[]
```

## Versioning

- small fixes increment PATCH
- larger feature changes increment MINOR and reset PATCH to zero
- README and release archive version follow the project version

## License

MIT
