Type: defect
State: active
Architecture: ready
Priority: P0
Parent: 2209
Depends:
Area: engine, world, render, audio
Tags: memory, budget, realtime

# Engine residency budget counts every owned product

## Problem

`GroundStack::kHoldsBytes` counts classes, OSM fields, water, streets, buildings and
TilePool residency. It excludes Engine state such as generated pieces, height sheets,
instances, crowns, bake candidates, audio state and renderer-owned GPU buffers/textures.
It therefore cannot be reported or enforced as an Engine memory ceiling. Global process
heap instrumentation also includes the host and cannot substitute for this contract.

`HeightSheets` reports its owned container capacities and nested page-node capacities.
Its former `std::map` page index hid allocator-node overhead; a deterministic `FlatMap`
now gives an exact retained-capacity value and keeps index failure atomic with height-page
upload. Its stitch and pre-publication tile indices also use short-lived contiguous maps:
duplicate identity or allocation failure rejects before either can mutate resident terrain.
HeightSheets now retains canonical source rasters through publication and counts their
referenced allocation capacities (fef5b6197). Shared references can overlap stream counts;
the future global owner ledger must deduplicate allocations, not omit the retained fields.
`StructureBakeProgress` uses the same bounded, contiguous `FlatMap` for its massing
accumulator. Its slot traversal is deterministic for a fixed input and allocation failure
reaches the existing structure-bake error boundary; per-node `std::map` overhead no longer
escapes a future progress-memory contract.
`TilePieces` likewise counts only its handle slots and diagnostic capacity; renderer-owned
piece source and GPU storage stay in the renderer category.
Native `Geometry::storageBytes` counts its owner record, retained part/material/image/light
slots and nested attribute, name and pixel capacities. Renderer copies remain a separate
category, which exposes the real overlap while a ground candidate continues after cloning.
Ground `Patchwork` and `EarthworkStamp` report phase-local sheet/node and ring/seam capacities;
these values disappear from the candidate snapshot when their owning phase products retire.

Host evidence: Graz at 8d094066c reached 14.0 GB peak process footprint and blocked
in Metal fence waits (/tmp/outshine-repair-8d094066c-graz-sample.log). This exceeds
the entire 8-GB target device capacity; it is not a measured Engine allocation total.

## Active repair: GPU byte-total overflow

Probe: two 2-GiB buffers report 0 instead of 4294967296 bytes (exit 1).
Owners: SubjectResidency::HeldBytes, SubjectDraw::HeldBytes,
SceneRenderer::PieceBytesHeld and Effort::DeviceBytes. Widen totals to uint64_t;
individual SDL buffer capacities stay uint32_t. Existing measurement flow converts
only the completed total to double. No allocation, rendering or error contract changes.
Test empty, 4-GiB boundary, all streams at maximum and removal without GPU allocation;
verify the draw/renderer reporting types cannot truncate. Restored uint32_t must FAIL.
Gate: format, SubjectResidency/SubjectDraw/SceneResources, Places/PNGs, full lint/tidy/API.
This repairs an invalid meter; it does not establish why process memory peaks.

## Active repair: immutable terrain-stamp accounting

320e3c9a2 Graz sample: 448/676 main-thread samples in TerrainPressJob::HeapBytes,
called by CurrentProductBytes during pressing; 8.0-GB host peak, not Engine bytes.
Owner: generators/terrain/TerrainPressJob.cpp. Its owned stamps never mutate;
2c31992cd makes stamps immutable and sums outer/nested retained capacities at adoption.
HeapBytes retains dynamic work-vector and press-job capacities; no counters are removed.
Gate: format, TerrainPress/EarthworkPress suites, all Places/PNGs, full lint/tidy/API.
Repair: immutable stamps and cached owned bytes. Format 1206 files PASS; independent
capacity/move fixture 5 checks PASS; omitted nested capacities produce 3 actual FAILs.
4844c3196: 17 focused PASS, full lint/tidy/API PASS; Places 38/44, three Places still red.
New Graz sample: scan absent, Metal waits dominate; snapshots cover different phases.
Stale-earthwork variants now PASS, but restored scan also PASS with 23 checks: no causal
claim for the earlier readiness failure. /tmp/outshine-terrain-accounting-runtime-verification.log.

## Decision

Each long-lived Engine product exposes owned CPU allocation capacity and requested GPU
resource bytes separately. SDL does not expose exact physical VRAM residency; report GPU
figures as requested bytes, never as measured driver allocation. Count vector capacities,
not merely element counts; shared allocations are counted once at their owner.
A State snapshot counts published products and in-flight work, keeping CPU, GPU,
disk cache and transient candidate bytes separate. Worker and GPU products report
an atomic or synchronized snapshot with their publication revision. A budget check
rejects or sheds a candidate before publication; it never frees a running product first.
The ground 512-MiB limit remains a GroundStack limit until an Engine budget replaces it.

## Sequence
1. Inventory `Surrounds`, `State`, renderer residency and worker/candidate ownership.
2. Add byte contracts from leaves upward. Empty retained capacity still counts; only
   genuinely unallocated products report zero. Start with Live slots/payloads, HeightSheets,
   StructureBuildQueue queued/idle storage and WorldContent buffers/textures.
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

## Nächster ausführbarer Slice und Abhängigkeiten

Kapazitätsmessung braucht keine abgeschlossenen Publikations-/GPU-Umbauten. Keine
synthetische Depends-Kette; atomare Enforcement-Integration benötigt später deren konkreten
Commitvertrag. Zuerst StructureBuildQueue/StructureBuildTask und VegetationStreaming:
Queue-/Idle-Kapazitäten, RawTile, HeightField-Borrows, MeshScratch/Progress, Models,
Prototype-/Atlas-Metadaten einmal am tatsächlichen Besitzer erfassen. Neue Proof-Scratch
2312 ergänzt dieselbe Kategorie; keine Doppelzählung gepinnter gemeinsamer Referenzen.
Worker publiziert eigene Capacity-Werte nach jedem Slice synchronisiert; Renderthread
liest keine parallel mutierten Vektoren. Renderer liefert requested buffer/texture bytes
separat. Host-Prozesspeak ist zusätzliche Evidenz, kein Engine-Budgetzähler.
Owner-Dateien: engine/streaming/StructureBuild{Queue,Task}, VegetationStreaming,
render/scene/SceneResources und stages/SubjectResidency; Tests bei diesen Ownern.
Negativkontrolle: zweiten In-flight-/Idle-Owner oder eine retained capacity auslassen.
Gate: make format, betroffene Owner-Suites, full make lint. 8 GB Unified Memory ist
keine frei verfügbare Engine-Allokation; gemessenen OS/Driver-Reservebedarf berücksichtigen.

Konkretes Code-Gate: `make format`; `make suite SUITE='outshine/src/engine/streaming/StructureBuildQueue outshine/src/render/scene/SceneResources outshine/src/render/stages/SubjectResidency'`;
`LINT_JOBS=2 make lint`. Neue Slice-Orakel liegen bei den genannten Ownern.
