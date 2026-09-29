Type: defect
State: active
Architecture: ready
Parent: 2123
Supersedes: 2232
Depends: 2312
Priority: P1
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
2310/2311 supply terrain scopes and bounded certificates. Certified readiness hits
avoid preparation; the allocating miss path remains a P0 gap in 2311.
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

## Active repair: camera-independent source fallback

Laying posts SourceGeometry with no explicit level. BakeRevision permits eye/focal changes,
but StructureBake's null-level branch still chooses geometry from that original eye/focal.
This violates the source-product contract before cell refinement even begins.
Independent of 2312: StructureBuildQueue resolves an omitted SourceGeometry level to Shell
at both admission and landing. Explicit levels remain explicit; ViewDetail keeps its contract.
The existing Shell generator retains rings, roofs and foundations. This is a source-stable
Playable fallback, never a claim that Shell meets the Refined pixel bound. Cell selection,
whole-cell error guard, qualified DEM/street receipts and atomic activation stay authoritative.
Owner: streaming/StructureBuildQueue.cpp; extend its real whole-tile landing fixture.
Check Shell metadata and identical nonempty geometry at remote eyes and different focal scales,
including movement between posting and landing. Explicit Fine must remain Fine. Removing
normalization must fail; terrain revocation and retry controls must still pass unchanged.
Gate: format, StructureBuildQueue/StructureBake/TilePieces, all Places and opened PNGs,
full lint/tidy/API. Compare preparation cost and requested bytes without promising a timeout fix.
c769a231a implements normalization. The fixture now enters the required refinement phase
before FineOnly admission: 83 checks PASS; original code produces two actual FAILs.
Format 1208 files PASS. a25c12d76: 11 owner tests and full lint/tidy/API PASS;
Places 38/44 PASS, six red cases (2169). All ten client PNGs opened; seven Refined
images pixel-identical to c9d6bb5fe. Full logs: /tmp/outshine-repair-a25c12d76-*.log.
Logs: /tmp/outshine-source-fallback-{control-results,correct,original,final-format}.log.

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

## Runtime diagnosis

4b54277f2 Graz probe at 6144 frames: 45 incomplete, source-current tiles; 2296 required
minus 901 resident = 1395 missing cells, next requests Fine. 975 cell landings and one
queued cell prove ongoing work: the user-facing queue=0 counts WHOLE tiles only.
126/128 height-pin observations exceed 2 MiB; mean resolution 8.41 ms, not a quantile
or isolated causal cost. No source-key mismatches. /tmp/outshine-structure-readiness-4b54277f2-Graz.log.
6e renderer requested subject bytes: Graz 4.9517 GB, Wien 9.8074 GB; not GPU residency.
2311 must coalesce bounded preparation without increasing the cache/frame limits.
Three real Fine/Shell
pairs previously stopped with zero queries and 158–311 m bounds. 0d5962a2a resolves
this CPU limit: Shell uppers 4.10–5.40 m, Massed 63.01–66.04 m, widths <0.02 m;
/tmp/outshine-native-witness-{shell,massed}-Graz.log. Runtime still uses the old guard.
Prioritize useful bounded native proof -> task/product transfer -> resident selection;
raising cache/frame limits cannot replace this chain. Preserve whole-cell safety meanwhile.
