Type: defect
State: active
Parent: 2209
Depends: 2191, 2222, 2223, 2224, 2225
Area: engine, world, render, audio
Tags: memory, budget, realtime

# Engine residency budget counts every owned product

## Problem

`GroundStack::kHoldsBytes` counts classes, OSM fields, water, streets, buildings and
TilePool residency. It excludes Engine state such as generated pieces, height sheets,
instances, crowns, bake candidates, audio state and renderer-owned GPU buffers/textures.
It therefore cannot be reported or enforced as an Engine memory ceiling. Global process
heap instrumentation also includes the host and cannot substitute for this contract.

## Decision

Each long-lived Engine product exposes an exact `ResidentBytes()` for CPU and GPU
storage it owns. A State snapshot sums only complete, published products and keeps CPU,
GPU, disk cache and transient candidate bytes separate. Worker and GPU products report
an atomic or synchronized snapshot with their publication revision. A budget check
rejects or sheds a candidate before publication; it never frees a running product first.
The ground 512-MiB limit remains a GroundStack limit until an Engine budget replaces it.

## Sequence

1. Inventory `Surrounds`, `State`, renderer residency and worker/candidate ownership.
2. Add exact byte contracts from leaves upward; zero-sized/unopened products report zero.
3. Publish a revision-consistent Engine residency snapshot and measured high-water marks.
4. Set CPU/GPU budgets from device measurements and enforce them at candidate boundaries.

## Acceptance

- A constructed Engine reports each owned CPU/GPU product once; a host allocation changes
  no Engine value.
- Adding/removing each product changes only its declared category; the summed total is
  analytically checked and includes candidate overlap during replacement.
- Worker completion, renderer target switch and rejected candidate preserve the prior
  snapshot and budget state.
- A deliberately excessive candidate is rejected before publication; a later smaller
  candidate succeeds without a restart.
- Moving-camera Places report p50/p95/p99 CPU/GPU residency and peak memory separately.
