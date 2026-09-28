Type: defect
State: active
Architecture: ready
Parent: 2123
Supersedes: 2232
Depends: 2312
Priority: P0
Area: generators, world, engine, render
Tags: buildings, lod, determinism, streaming, hockenheim

# Structure LOD products are source-keyed and selected at render time

## Problem and current evidence

Legacy camera-local baking produced 51 Fine footprints after a jump versus 38 after
motion for Hockenheim tile 24 with the same source/DEM/street inputs. The 64 m reuse
rule hid geometry identity behind camera history. 2232's camera-bake contract is retired.

Implemented foundation: explicit Fine/Shell/Massed products; stable 8x8 tile-local
cell assignment and full cross-cell/dateline bounds; bounded pinned cell requests;
hidden staging and complete wall/roof publication; atomic same-source mask replacement;
canonical DEM identity sets and raster digest; semantic-only BuildingField footprints.
Shell retains native rings, roofs and foundations, omitting secondary details. Roof,
concavity and sloped-contact controls pass; triangle reduction alone proves no speedup.
2310/2311 supply terrain scopes and bounded certificates; readiness performs no preparation.
At 5de351d40: 45 focused tests/full lint pass, 255/255 tidy units, zero findings.
New geometry and runtime-error selection still need fresh visual/performance evidence.

## Binding contracts

- Generator geometry keys contain source, terrain/street revisions, material/generator
  revision, cell and explicit detail. Eye, focal length, frame time and world anchor
  never define a source product. Assets, instances and contact products keep separate owners.
- BuildingField stores semantics; BakedTile/TilePieces store derived representation,
  bounds, detail and optional source-matching surface error. No second geometry model.
- Every visible cell retains a coarse drawable safety net. Fine/Shell prepare on
  bounded asynchronous demand from the same pinned source. Failed/stale uploads retain
  previous products; GPU retirement follows final use. Do not pin rasters to fake readiness.
- render selects exactly one resident level per cell by conservative projected error,
  bounds and hysteresis. Shadows use the same published source. Selection never changes
  collision/navigation or enters a generator callback. 2314 owns the common scene budget.
- Refined requires the selected level to meet its declared pixel error and no required
  source upgrade pending. Playable may expose a coarse fallback with its actual error.
  Missing proof retains the whole-cell guard. Near-boundary/invalid views choose Fine.

## Remaining implementation order

1. 2313 completes independent adaptive CPU evidence; 2312 transfers certified error
   through task/product publication and encloses transforms. This blocks tighter selection.
   2310/2311's implemented CPU contracts are available now; their open visual acceptance
   does not prevent checking independent renderer/streaming paths.
2. StructureCellDetail becomes a renderer-owned resident-level decision using each
   level's proved error. StructureCellPlanner admits only bounded detail requests by
   visible benefit/cost; use source-stable hysteresis, not rebaking around camera travel.
   Preserve the current whole-cell guard until the entire proof chain validates.
3. TilePieces activates complete source selections; test missing cells, wall/roof
   refusal, stale terrain/street/source and whole-tile/cell switches in both directions.
4. Static/paced Hockenheim at 74.85 s after Refined/settle and opened decile frames must
   converge without giant silhouette changes. Compare jump, drive, fast crossing and
   backtrack; retain contact/navigation IDs and source-valid shadows.

## Acceptance and owners

- Files: building/{StructureBake,StructureSurfaceError,StructureSurfaceRefinement};
  world/ground/BuildingField; streaming/{StructureBuildTask,StructureBuildQueue,
  StructureCellPlanner}; render selection and TilePieces publication.
- Independent source/order/eye variants preserve geometry identity. Forced coarse near
  the eye violates the pixel oracle; forced fine changes cost, not source identity.
  Deliberately stale source and incomplete publication must fail before GPU activation.
- Report p50/p95/p99, over-budget frames, CPU/GPU peaks, requests, uploads and work/frame
  against the same trace/build. CPU certificates do not substitute for opened PNGs.
- make format; affected BuildingMesh/StructureBake/StructureBuildTask/StructureBuildQueue,
  StructureCellDetail/TilePieces suites; full lint including clang-tidy and API guards.
