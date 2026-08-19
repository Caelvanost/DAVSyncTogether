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

**v0.2.2 — load-order-safe DAV form identities**

The targeted armor-state probe from v0.2.1 now represents ARMO and ARMA forms with a stable identity:

```text
plugin filename + local FormID
```

instead of relying on the runtime FormID assigned by the local load order.

For each form, DAVSync records:

- the current runtime FormID for diagnostics only
- the source plugin filename from `TESForm::GetFile(0)`
- the plugin-local FormID from `TESForm::GetLocalFormID()`

The stable identity is used by the DAV state hash and by local state comparison. This is intended to let Player1 and Player2 identify the same ARMO/ARMA even when their runtime load-order indices differ.

DAVSync also performs a local round-trip validation with `TESDataHandler::LookupForm(localFormID, pluginName)` and logs whether the stable identity resolves back to the original runtime form.

The effective DAV visual state remains classified as:

- `VISIBLE` — rendered ARMA belongs to the original ARMO
- `HIDDEN` — the ARMO is worn but no matching biped ARMA is rendered
- `REPLACED` — at least one rendered ARMA is not one of the ARMO's original Armor Addons
- `UNEQUIPPED` — a previously tracked ARMO is no longer worn

This milestone is still read-only. It does not transmit or apply state to Skyrim Together proxies yet.

## DAV compatibility strategy

The original Dynamic Armor Variants exposes Papyrus functions such as `GetVariants`, `GetEquippedArmorsWithVariants`, `ApplyVariant`, `ResetVariant` and `ResetAllVariants`, but does not expose a native getter for the currently active variant.

DAVSync Together therefore observes DAV's **effective rendered Armor Addon result** rather than reading private DAV state. This keeps the current implementation compatible with the original `DynamicArmorVariants.dll`.

Dynamic Armor Variants Extended (DAVE) may be supported later as an optional richer integration, but it is not a required dependency.

## Planned architecture

1. **DAV local-state capture** — determine the effective DAV-controlled visual state of the local player.
2. **Stable equipment identity** — encode armor/armor-addon identities in a load-order-safe form. **Implemented in v0.2.2.**
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

## Test procedure for v0.2.2

After loading a save:

1. keep the helmet equipped and visible
2. use DAV to hide it
3. wait about 2 seconds
4. use DAV to show it again
5. wait about 2 seconds
6. unequip the helmet
7. re-equip it

Inspect `DAVSyncTogether.log` for:

```text
DAVST ARMOR_STATE ...
DAVST FORM_ID role=ARMO ... roundtrip=1
DAVST FORM_ID role=BASE_ARMA ... roundtrip=1
DAVST FORM_ID role=ACTIVE_ARMA ... roundtrip=1
```

A successful identity line looks like:

```text
runtime=FE...... plugin="SomePlugin.esl" local=00000... resolved=FE...... roundtrip=1
```

The important validation criterion is that every tracked static ARMO/ARMA has a non-empty plugin name and `roundtrip=1`.

## Versioning

- small fixes increment PATCH
- larger feature changes increment MINOR and reset PATCH to zero
- README and release archive version follow the project version

## License

MIT
