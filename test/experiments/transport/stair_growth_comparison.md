# Automatic original-source growth from resident road assets

Owner: [#555](https://github.com/Weichwolf/outshine/issues/555), parent
[#434](https://github.com/Weichwolf/outshine/issues/434).
This extends the [local generator](stair_local_comparison.md) using the existing
installed `transport.frontiers` operator and ordinary-ground source semantics.
SIGGRAPH starting point: Chen et al., [Interactive Procedural Street Modeling](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page 5 / figures 7–8 and section 6.1 inspected visually. Original provider-node
discovery and whole geometry admission remain separate. No tensor-field method,
SDK code/dependency, Plan/Reservation/Provider/native API or library change.

## Actual 55→57-source result

Frozen producer, fresh saved reader, full control and tests:
`317c709ecf1f2e27b6e10410cdcff727fd4e8782`. The actual 55-source input comes
from #547; its independently checked two-source addition is now an ordinary
resident asset, not another legacy global parent. The selected new root is its
`138663000:6`, original end `-1`. Whole original additions `32651280:1` /
38.604462 m and `80922970:213` / 7.945220 m total 46.549682 m. Source IDs have
prefix `openfreemap:14/8937/5678:transportation:`. Existing Vienna PBF SHA256
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`, DEM,
origin and original source catalogue remain as in the linked comparisons.
All 1657 original line parts / 71 areas remain; 1600 lines are still unplanned.
Native admitted-source IDs remain empty. This is a small real extension.

New actual surface: 29 vertices / 35 facets, closed body 58 vertices / 112
faces. Double/Float32 volumes are 26.070055 / 26.069916 m³. Whole profile,
wall, source ownership, headroom, positive volume and cyclic vertex links pass.
Source width/grade/clearance bounds and old solver/geometry capacities remain.
292 selected terrain faces become 364 faces / 229 vertices, with 92 exact
boundary edges/vertices and 222 pins. Complete world: 28342 terrain facets;
27978 outside triangles remain unchanged. Independent whole assembly matches
every resulting XYZ facet/attribute and new module/layout/field dtype, shape
and byte, including NaNs. Diagnostic closure matches that full body exactly.

231 whole source-bed facets pass D/Q (18552 pair tests, maximum Float32 contact
10.693471 µm), plus 149 actual old body undersides (12550 pairs, 12.826486 µm).
All 124 whole old piers pass (7150 pairs, maximum 15.170145 µm). This local
terrain has no positive-area overlap with an overhead bridge in either
precision. That is explicitly **not applicable**, not a headroom measurement.
The complete control still checks the unchanged bridges across the full field.
Fresh independent inspection of the previous 55-source specimen also passes;
its protected original receipt was preserved using a disposable input clone.

## Discovery, contacts and bounded continuation

`stair_growth_sources.py` checks actual original interior/end profiles against
each immutable road asset independently. Its temporary source-owner mask has
at most that asset's 32 columns; no global 64-bit road assembly. Mask conversion
and profile-state body sharing are recorded separately. Whole-cap samplers
still allocate; a zero body-copy counter describes profile-state wiring only.
The input prefilter uses terrain bounds, not an asserted exact domain; actual
local terrain/whole-body admission remains mandatory. Original generalized
provider-node coordinates are not a claim of exact OSM topology.

The global limit remains 64 actual profile probes, eight proposed roots,
eight new sources per module and three actual geometry attempts per invocation.
Index and complete source scan produce identical roots, recipes and reasons.
The present input has 13 actual profile probes / 12 verified roots and 18
full/prefix recipe alternatives. Both proposal schedules are explicit:
`prefix_first` preserves the old grouped prefix order; `roots_first` tries
distinct roots before their smaller prefixes. Neither dominates universally.

Early frozen experiments fail whole-profile coverage, declared source reuse,
local height/interface or bounded junction requirements. A three-attempt run
does not exhaust 18 alternatives. `--proposal-offset` explicitly continues
within the same deterministic proposal list, bounded to 0…31; this is not
automatic persistent world scheduling. The accepted specimen uses offset 6
and its first attempt succeeds. Skipped proposals are not declared impossible;
no optimality/maximality or universal policy win is claimed.

Two exposed adapter errors are corrected with physical counterexamples:

- Distinct old sources can share a proven original node. Reject duplicate
  `(old source, node)` declarations, group the distinct complete profiles into
  one hull, and require equal frame/new-source membership. The existing maximum
  four profiles remains; no proximity-based connection or missing profile is
  authorized. This actual case previously failed the blanket duplicate-node rule.
- A local terrain patch may be disjoint from all bridge undersides. Preserve
  the clearance operator's explicit `no_positive_area_overlap` outcome and mark
  it inapplicable. Other unresolved results still reject; actual penetration
  remains rejected. The previous local wrapper wrongly required a witness even
  for disjoint geometry; the global witness-requiring diagnostic is unchanged.

`stair_growth_verify.py` reconstructs the saved selection and exact bounded
attempt window, recipes, source dispositions and hashes in a fresh process;
the existing independent module reader checks each actual saved body/terrain.
Changed recipes, stale attempt windows and lost retained source IDs reject.
Real frame/payload/stale-face/old-receipt/missing-neighbour/1-mm-underside controls
also pass. After replaying 57 sources, a new trial with one-face context capacity
retains all 57 sources and complete geometry hash
`95675393f82d472a5b7cce384bd8b126e0854a94cf08806c1fe5b7d5465138f3`.

## Measured work and actual images

Same i5-1135G7 four-logical-CPU VM, Python 3.13.5, NumPy 2.2.4, Shapely 2.2.0,
one BLAS/OMP thread as #547. Normal local generation: 9.978792 s / 132616192 B
process peak RSS. Automatic generation plus its in-process independent reader:
24.198965 s / 152801280 B. Fresh complete automatic reader: 16.220521 s /
151224320 B. Independent whole control: 38.607727 s / 180764672 B, overlapping
that fresh reader and nice-priority tests; these are different operations, not
equal-cost speedup estimates. Generator/reader orchestration is capped at
300 CPU seconds / 3 GiB address space, individual full control at 120 / 3 GiB.

The delivered selection benchmark is frozen at
`edd6909ba46490bd4027d8d0909a4c734ace8dbf`. It preloads the same verified
inputs before timing/tracking and runs after earlier workers finish. All four selections
(normal/profiled × index/scan) have identical recipes. Memray 1.20.0 tracks
native/Python allocations, Python allocators on, native call stacks off;
official statistics return terminal zero with fresh completed JSON metadata.

| Selection | Indexed | Complete scan |
|---|---:|---:|
| Normal seconds | 0.114058 | 0.163417 |
| Profiled seconds | 0.336487 | 0.428716 |
| Node lookups | 91 | 91 |
| Incidence/node comparisons | 141 | 141869 |
| Allocation events | 569996 | 782411 |
| Cumulative requested bytes | 57353480 | 65811322 |
| Peak traced live heap, B | 1527909 | 1291788 |

Different counters describe different work; requested bytes are not copied
bytes/RSS. Tracker excludes preloaded inputs/imports. Normal timings are not
profiles, and the scan has a smaller tracked peak despite more total work.
Context query tests 720 initial / 1574 auxiliary facets, returning 1038 context
faces. Three of seven old road assets enter generation: 42 temporary old source
bits / 10408 body faces / 463008 B arrays. Changed old masks contain 3230 B;
copy/immutable backing makes two passes. The existing solver and explicit
terrain welding/index/copy counters remain in the report; other GEOS/Python/
solver copied bytes are not fully measured. No frame/device/runtime bound.
Benchmark process peak RSS includes preload: 132055040 B. Recipe digest in all
four runs: `5c39d5f46babf3b5d9f78fc0598bfe30b48edc5b6d0bfef5d1167b45fcfa805f`.

Four fresh complete 1280×720 3D before/after PNGs were decoded, bound to actual
scene/mesh hashes and opened. Overview changes 198 RGB pixels / 921600, network
241; the short links are barely visible. The tile is still pale and sparse.
This is functional growth, not a broad visible quality improvement or a completed
four-Place showcase. Actual llvmpipe LLVM 19.1.7 software render times are
11.908220 / 18.336498 / 29.071446 / 27.972446 s, overlapping nice-priority
tests and an old-product regression reader. No public gallery/server/images.

145 source / 145 installed-library controls pass with `-W error`; the same
unchanged 205-module wheel from #547 is used. Ruff/format and documentation lint
are separate checks, not native/engine/device gates.

## Products and reproduction

Accepted automatic output: `/tmp/outshine-transport-growth-full-89b394227/`;
actual module/images are in `outshine-module-0/`. Complete independent output:
`/tmp/outshine-transport-growth-reference-317c709ec/`. Commands, terminal logs,
allocation traces/statistics, counterexamples and image/pixel receipts:
`/tmp/outshine-transport-growth-89b394227/`. Current benchmark reports, original
allocation traces/statistics: `/tmp/outshine-transport-growth-cost-frozen/`.
Preserve the original source and
53/55-source parent inputs from #547. Superseded derived attempts are compacted;
required original inputs and accepted products remain.

Run `stair_growth_experiment.py` and then `stair_growth_verify.py` with the same
explicit `--resident PREPARED53 --prior-module VERIFIED55 --output FRESH_TMP
--proposal-offset 6` arguments. Method, strategy, schedule and attempt limit
must match in both. Individual local products replay using the existing local
adapter; this delivery also permits an exact complete control as the next
diagnostic baseline, bound to the actual prior report/reader and all payloads.
`stair_local_reference.py` takes that prior complete 55-source baseline plus the
new recipe/local result. `stair_local_display.py` additionally takes explicit
`--before VERIFIED55/display/after-display.npz`; compare its four scene JSONs
using the existing research renderer and `test/scripts/pixels.py`.

`stair_growth_compare.py --resident PREPARED53 --prior-module VERIFIED55
--output FRESH_TMP --profile` reproduces the equal-recipe benchmark; the optional
Memray module must be available. Its two traces can be inspected with the
official `memray stats --json` CLI. No runtime dependency is added.

Accepted automatic report SHA256:
`52529feb5ec7a9194cfa33783493de9db30f0706c7fcb4abac5f5c3840b52238`;
actual module report:
`da0047434b439a2ce4e0ea97c8ac4eff6c82a9f9bcb4a65b01ad5fbe9ab799d4`;
complete independent report:
`8027a356c5040a5e2aa1c58add180b35401ba7b4401458b07abb1d499e5061af`.

Automatic persistent seed/module growth, richer contacts/profiles/levels,
global ports/routes/frames, worldwide coverage, reservations/LOD/streaming,
statics and native/device integration remain #434/#50/#10/#12/#48.
