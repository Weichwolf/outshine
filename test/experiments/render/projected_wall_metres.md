# Projected wall metre coordinates

Owner #407, parent #306, material goal #189. This fixes existing colour/roughness
integration, independently of relief Draft #405. No new geometry, material mode,
shader binding or API.

StructurePlanSelection::EmitProjected appended the native projection's vertices,
indices and optional colours, but discarded Raised::WallMaterialM. TilePieces
correctly fell back to Facade1 when that stream was absent. Direct native wall
probes passed, while actual projected world tiles could never select FacadeMetres.

The append now preserves both optional attributes, reserving their final size once.
Missing prefix/suffix colours use1; missing metre components use0. The LOD request
key changes building-lod-3→building-lod-4 so incomplete previous geometry is rebuilt
once. Source tiles, prepared shape/basis products and old files remain intact.
The existing cache encoding and UV1 lifecycle/ownership contract are unchanged.

## Native regression

ProjectionCrossesCellsAndPreservesNearGeometry uses the actual native BuildingMesh
and68 mixed near/far source plans. Its observed Project result contains metre data;
the final tile must retain that exact owned stream. Existing geometry/UV0/normal,
source coverage and view-reuse assertions remain. A second boundary test covers
all four optional prefix/added-stream combinations, including distinct neutral
values for colour and metres. Same inputs:

- original paired2a6f211c7 libraries:3371 checks/1 failure, only the appended metre stream;
- corrected native libraries:3371 checks/0 failures; mixed-stream boundary100/0
  (original boundary94 checks/3 failures).

Both native archive populations contain the generator objects. The component
countercheck updates exactly StructurePlanSelection and PreparedBuildingGeometry
in both archives. Their66/72 owned dependency headers match the retained baseline
byte-for-byte; no newer Device header is paired with an old archive. The complete
client binary is checked for the new product-key version before measurement.
Production changes are0d91b2f51; focused regression cleanup99431e1f8. Format and
focused clang-tidy cover both changed production units and both tests. No own
findings; unchanged Check.h retains14 existing diagnostics. No second full lint.

## Actual world and cost

Original incomplete near-Flensburg cache input is retained:606583 wall vertices,
33829239 encoded bytes, variant6 UVs but no metre stream. The corrected matching
product has38681903bytes: exactly4852664 additional bytes,8bytes/wall vertex.
Geometry/UV0/indices/paint are unchanged; the existing digest includes metre data.
The append adds one owned8N-byte stream copy, and the renderer consumes the existing
8N-byte UV1 stream. This is an element/byte cost, not device peak memory.

At1920×1080/60 with unchanged declared fill0.6, full585262 triangles/zero bare tiles,
the actual retained SDL client produces71c88aff→763e24d6.11980 display pixels change,
mean RGB difference0.003757/255, worst7/255. The improvement is modest. A separate
nonphysical category marker proves the route: before285276 legacy pixels and no
metre category; after259881 metre nonbrick and20837 brick pixels, no legacy category.
Thresholded display counts are diagnostic coverage, not exact surface-area counts.
The same marker is positive on the retained actual native wall crop. Local PDFs
are optically opened, including the dated2025-07-15 12:00CEST Foto-Webcam.eu view.
No gallery or general geometry/lighting improvement is claimed.

The old SDL client's first-frame gate remains red in both variants; this does not
establish a new coordinate regression. The complete native Vulkan countercheck
uses Performance's frozen187a5b380 renderer +6156a8010 descriptor backend (#410),
all39 backend objects rebuilt against its matching old Device header. Its42 link
inputs are copied and hashed read-only; only the two verified generator objects
differ. All their owned headers also match that native renderer freeze. No shared
archive/worktree is mutated and no second renderer port is introduced.

The diagnostic native pair uses fresh offline processes, same shader package, camera, clock,
content and1920×1080/60. Each measures exactly60 frames/360degrees, no warmup frames:
before p99=12.4672ms, after13.7663ms, no frame exceeds16.67ms; warm6.874/7.077s.
The corrected initial native sample also passes p99=14.7726ms, warm9.872s. The first
native baseline preload failed at10.028s before any frame; separate native
preparation then finishes in6.811s, followed by the full passing pair. That failure
is retained. The new product's initial legacy build87.443s is a versioned cache
miss, not warm loading. New #328 evidence (6206374931/6206664592) reports unresolved shadow/foliage
parity against SDL. These native times therefore do not complete the equal-quality
frame gate; the fix PR remains Draft until that concrete countercheck passes.
Performance confirms the freeze remains available read-only.
GPU timestamps/device peak remain unmeasured; reported
process heap is separate. This is one native load case, not completion of #328,
all Places, Metal/A18 or the material goal. Master owns integrated publication.

## Reproduction

Use the normal build/test entry points with the configured native toolchain:

```sh
make suite SUITE=outshine/src/generators/building/StructureBake
build/outshine-client shots --scenario-overrides \
  '{"render":{"widthPx":1920,"heightPx":1080}}' \
  --rows --stats --offline --preload-seconds 10 Flensburg
```

Prepare a new product version separately before the warm command. Exact native
freeze, source/header/object/archive/binary hashes, terminal commands, raw cache
inputs, paired PNGs and PDFs are retained in explicit system-temp proof outputs
and linked from the PR. The native renderer Draft is not silently substituted for
the current master's renderer; its scope is explicit above.
