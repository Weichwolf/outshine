Type: feature
State: open
Area: render
Tags: architecture, look, owner

# Many lights are CLUSTERED, and each casts its shadow

**Benchmark** -- Unreal: lights are binned into a froxel grid (`r.Forward.LightGridPixelSize`,
clustered forward on mobile, deferred on desktop) and local lights cast shadows through a shadow
atlas with per-light budgets and virtual shadow maps; the sun is cascaded. RAGE: a deferred light
pass with light volumes, local shadow maps per shadow-casting light from a fixed pool, cascaded
sun shadows. **Both agree**: the light count times the object count is a STRUCTURE, never a
loop; every light may shadow; the sun is cascaded.

**Cited beside the two**: Filament's `Froxelizer` bins lights into a 16x8x24 froxel grid on
the CPU per frame and the fragment reads its froxel's light list -- on a phone GPU, which is
this target -- with cascaded shadow maps for the sun and a shadow map per spot light. It is the
readable implementation of what both references do, and the one this item copies.

## Where it stands, measured 2026-09-04

```
  src/render/shaders/subjectBindings.msl:40   Light items[16]     a per-draw array of 16
  src/render/shaders/subjectLit.msl:67        for (at < count)    every pixel walks every light
  src/render/stages/LightVisibilityStage.h    kShadowAtlasPx 2048 ONE shadow, the sun's
  cascades                                    none
```

Sixteen lights per draw, iterated per fragment, one shadow for one light, no cascade. CLAUDE.md
lays the frame budget out for five things and names this one second: *many lights and their
shadows*. A night street with lamps every thirty metres, a car with two headlamps and a city
behind it is the scene this engine exists for and cannot draw today.

## What will be true

- [ ] Lights are binned into a cluster grid once per frame on the GPU and a fragment reads its
      cluster, so the cost is O(lights in this cluster) and the per-draw cap of 16 is gone
- [ ] Every punctual light may cast a shadow: a shadow atlas with a declared budget of faces per
      frame, allocated by screen size, and a light that gets no slot is lit unshadowed rather
      than dropped
- [ ] The sun's shadow is CASCADED, with the split scheme written down and derived from the near
      and far planes
- [ ] The scene declares lamps through the door already (`PunctualLight`); a scenario with a
      hundred of them holds 16.7 ms at p99 on the 720p target, measured through `make shots`
- [ ] Negative control: a place with one hundred lamps and the cluster pass switched off misses
      the budget

## What will show I was wrong

If the clustered path is slower than the loop at 16 lights, the cluster build is costing more
than it saves at this resolution and the crossover is measured before the loop is deleted.
