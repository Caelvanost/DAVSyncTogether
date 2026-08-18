# DAVSync Together

DAVSync Together is an SKSE/CommonLibSSE-NG plugin for **Skyrim Special Edition / Anniversary Edition** designed for Skyrim Together Reborn setups.

Its goal is to synchronize player **appearance** between clients, complementing IEDSync Together's equipment-display synchronization.

## Planned synchronization scope

- RaceMenu face morphs
- RaceMenu body morphs
- RaceMenu face overlays
- RaceMenu body overlays
- RaceMenu makeup / tint-related appearance
- Hair and other head parts
- Body texture / skin variation identity

## Project status

**v0.1.0 — initial development scaffold**

The first milestone establishes the native plugin, appearance snapshot model, serialization boundary, logging, build tooling, and packaging layout. Runtime RaceMenu/SKEE capture and remote proxy application are implemented incrementally on top of this foundation.

## Architecture

DAVSync Together is intentionally split into independent layers:

1. **Appearance capture** — reads the local player's RaceMenu/SKEE and Skyrim appearance state.
2. **Appearance snapshot** — stable, versioned representation of morphs, overlays, head parts and skin identity.
3. **Transport** — sends snapshots between Skyrim Together clients.
4. **Proxy resolution** — identifies the remote player's Skyrim Together proxy actor.
5. **Appearance application** — applies the received snapshot to the corresponding proxy and refreshes the relevant geometry/material state.

This separation is deliberate: appearance extraction/application is more fragile than networking and must be testable independently.

## Requirements

Runtime requirements will ultimately include:

- Skyrim Special Edition / Anniversary Edition
- SKSE
- Address Library where required by the selected CommonLibSSE-NG runtime
- RaceMenu for RaceMenu-specific synchronization features
- Skyrim Together Reborn

Build requirements:

- Visual Studio 2022/2026 with C++ workload
- CMake 3.24+
- vcpkg
- CommonLibSSE-NG

## Building

Configure and build with CMake/vcpkg, or use:

```bat
build_release.bat
```

Release packaging is written under `dist/`.

## Versioning

The project uses semantic versioning:

- patch version for small fixes
- minor version for larger feature milestones
- major version for incompatible protocol/release changes

## Related projects

- IEDSync Together — synchronizes Immersive Equipment Displays state between Skyrim Together players.
- MorphSync Together — earlier appearance/morph synchronization experiments whose RaceMenu/SKEE findings inform DAVSync Together.

## License

MIT
