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

**v0.2.0 — DAV local visual-state probe**

The repository was refocused on Dynamic Armor Variants after an early experimental RaceMenu probe was identified as overlapping MorphSync Together.

This milestone is read-only and does not yet send or apply remote state. It:

- detects whether `DynamicArmorVariants.dll` is loaded
- monitors the local player every 500 ms
- records currently worn armor forms
- captures a stable signature of the player's live 3D scene graph
- logs scene-node additions/removals whenever the visual state changes

This lets us identify exactly what DAV changes when a variant is applied without depending on Dynamic Armor Variants Extended.

## DAV compatibility strategy

The original Dynamic Armor Variants exposes Papyrus functions such as `GetVariants`, `GetEquippedArmorsWithVariants`, `ApplyVariant`, `ResetVariant` and `ResetAllVariants`, but does not expose a native getter for the currently active variant.

DAVSync Together therefore starts by observing DAV's **effective visual result** rather than reading private DAV state. This keeps the initial implementation compatible with the original `DynamicArmorVariants.dll`.

Dynamic Armor Variants Extended (DAVE) may be supported as an optional richer integration later, but it is not a required dependency for the current probe.

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

## Test procedure for v0.2.0

After loading a save, perform a DAV-relevant equipment sequence, for example:

1. equip the helmet
2. change its DAV variant / hidden state
3. restore its visible/default DAV state
4. unequip and re-equip it if useful

Then inspect `DAVSyncTogether.log` for:

- `DAVST DAV_STATE`
- `DAVST WORN_ARMOR`
- `DAVST SCENE_DIFF`
- `DAVST SCENE_NODE +`
- `DAVST SCENE_NODE -`

## Versioning

- small fixes increment PATCH
- larger feature changes increment MINOR and reset PATCH to zero
- README and release archive version follow the project version

## License

MIT
