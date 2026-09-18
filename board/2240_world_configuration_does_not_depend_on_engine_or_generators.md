Type: refactor
State: ready
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: world, engine, generation, include, test
Tags: api, world, modules, lod

# World configuration does not depend on engine or generators

## Evidence

`GroundStack.h` includes `Outshine.h` only to receive the engine-wide `Roots` aggregate.
`BuildingField.h` includes `generation/Generate.h` only to store `Generators::Detail`. These reverse
the intended engine -> world -> generator data flow. Their transitive public headers also make the
restricted engine/streaming tier appear to depend on scenario and audio.

## Decision

World owns a minimal source/storage configuration containing only shipped-data and cache roots plus
offline policy if actually consumed. Engine converts its host-facing root configuration at world
setup. `GroundStack::Open` receives that world value; it never sees asset-import roots.

Move the representation-detail ladder to a neutral content/base contract because world admission,
generator production and renderer residency share it. Give it one name and one ordering operation;
remove `Generators::Detail` and every compatibility alias. World may choose requested detail but
must not depend on generator interfaces. Preserve units, defaults and current selection behavior.

## Acceptance

- [ ] World sources contain no `Outshine.h`, engine type or generation public header.
- [ ] Ground setup still resolves shipped tables, cache and offline sources identically.
- [ ] One neutral detail enum serves world and every generator; ordering tests cover all rungs.
- [ ] Engine/streaming regains only its declared world/generator/render dependencies.
- [ ] Exact WI 2240 layer findings are removed; focused source/LOD tests and `make lint` pass.
