# A complete narrow footway at wider frozen profiles

Issue #606, child of #600. Python reference experiment at
`b39c7b18c0a6e63602adc79377ba3237854aff2e`. The verified candidate set grows
from 71 to 72 whole original sources. This is a small visible network extension;
the larger six-source construction and procedural overlap contacts remain
unresolved in #600/#602. No native/runtime/frame/worldwide acceptance.

## Actual defect and bounded correction

[The fixed whole-source recipe](fixtures/stair-narrow-root-footway-vienna.json)
selects `openfreemap:14/8937/5678:transportation:80922970:62`:101.100813112 m
of original axis, 1.8 m footway width. Its original endpoint[1270,2023] joins
old 12998091542:0 and 92807082:0, both already present. The old whole 5.5 m
profiles and source IDs/attributes/axes/bodies remain unchanged. This contact
uses original source nodes, without a constructed graph connection.

The support disk used only new widths: radius 1.8/2+0.75=1.65 m. The old
half-profile reaches 2.75 m. Its construction hull already contains that whole
profile, but clipping the *reference* to the smaller disk removed outer
intervals. Disabling only the correction on the same actual case reproduces
`whole profile has an uncovered interval` in `frozen_profile_checks`.

`profile_masks` completes only an incomplete support mask with each already
bound frozen profile buffered by the existing 0.75 m margin. Whole-node matching
must be unique. Already complete masks are returned unchanged. Applied only
when the frozen row has its verified varying-profile cap; flat-family masks
are untouched. No road/domain/hull/height/pin/wall geometry changes, no larger
metric tolerance, no solver/budget change and no foreign code or dependency.

Five focused controls cover old clipping/full trace, unchanged complete masks,
shared/distinct nodes, missing/ambiguous nodes and 1 mm height disagreement in
Double/Float32. Both complete old profiles, the whole new inner profile, walls,
owned source area, closed bodies, slopes, beds and headroom retain their gates.
The original outer endpoint remains an explicitly unconnected frontier.

The larger six-source hypothesis could pass road checks after several private
recipe changes but still fails terrain precision/coverage. Those changes are
not included or admitted. The remaining five sources stay original candidates;
this smaller success does not prove them individually impossible.

## Fresh independent evidence

Generation and fresh source/installed readers pass: 6.317918/8.461568/8.978977 s,
process RSS 132501504/152203264/152420352 B respectively. All seven declared
product exports are hashed/read, rather than trusting process exit. Installed
reader uses a disposable hardlink clone excluding receipts/display; protected
original receipts are never overwritten.

New surface 13 vertices/13 faces; closed body 26 vertices/48 faces, 0.3 m depth.
New source area 175.680794628 m² plus 8.981645777 m² of explicitly bound old
surface reuse. Maximum actual grade 0.03166718 in Float32 versus 0.1 limit.
No local bridge overlap; no new bridge. All 124 old piers stay present.
Terrain replacement 327→423 faces/239 points, 53 exact boundary vertices,
205 finite pins; 29955 outside facets unchanged. Context 644 faces, two of 11
old road assets/10 local sources/956 body faces/43024 B arrays. Normal limits
8192-context/2048-replacement/16-assets/64-local-sources and solver 8 s/100
iterations remain unchanged.

Independent full construction 27.970730 s/187785216 B RSS matches all 30378
XYZ facets/attributes and every module/layout/field dtype, shape and byte.
The complete 71-source predecessor is separately reconstructed: all saved product
bytes and all masks exact, result hash
`2f07eb607371baa21f0530bf9bae7fdc2340ce6f1eb3b2a10e391150aaa2fe96`.
Its protected receipt remains
`d6dff6bb919412e8d44bb817796a4929822d7517eac1dabaab81b9f2e18fcbac`.
Original 1657 line parts/71 areas retained; 1585 lines still unused with reasons.
`admitted_source_ids=[]` continues to mean no native admission.

71 source/71 installed affected tests pass with `-W error`; 205 unchanged
library modules are byte-identical to the existing PR#592 wheel. Ruff/format,
functions≤120, diff-check and docs lint pass separately from native gates.

## Costs and actual whole images

Linux x86_64/i5-1135G7 VM, Python3.13/NumPy2.2.4/Shapely2.2.0, one BLAS/OMP
thread. Same preloaded input and identical result bytes in both modes;
local preserves untouched outside, full control reconstructs/rechecks it.

| Mode | Normal seconds | Process RSS, B | Positive allocation events | Allocated B | Tracked peak B |
|---|---:|---:|---:|---:|---:|
| local | 3.785692 | 128458752 | 15323201 | 2055365694 | 1750203 |
| whole | 24.544880 | 187465728 | 136771241 | 15340771026 | 51068730 |

All four normal/profiled results have hash
`1bf91cb0acf6857f330dd48904bea1b696523812b0a93509cf66042360456402`.
Memray 1.20 includes Python/native allocations, excludes input/import/hash/export,
without native stacks. Counts exclude frees and zero-byte requests. Instrumented times 7.718283/63.963927 s are not normal
costs. An optional native Memray statistics crosscheck hit its90 s CPU limit
(exit137); complete positive-allocation record scans above finished with exit0.
No whole native-statistics agreement is claimed. Shared host, other agent work;
early readers/replay overlap normal runs,
software rendering overlaps profiling. These are scoped observations, not an
isolated hardware/device speedup claim. Total internal copy bytes unmeasured.
Terrain raw/unique/map arrays 18432/3824/9216 B; indexed/difference copies 6880 B
each; unchanged outside geometry copied 0 B by patch replacement.

All four whole 1280×720 before/after images were opened. Pairwise camera matrices,
projection/view, lights, samples, renderer and helper hashes match; only visible
input-vertex counts naturally differ. `test/scripts/pixels.py` finds 400/529
changed pixels, 104/148 above one level. The thin footway is visible inside the
upper-left road loop, but coverage/material/lighting deficits dominate. Small
network progress, no broad visual-quality claim, closeup, gallery or server.
EEVEE/llvmpipe: overview 37.850940/10.586467 s, network 9.779189/10.029414 s;
RSS peaks 2283810816/1812545536/1817292800/1816924160 B. No GPU/frame proof.

## Reproduction and retained receipts

Use the ordered 53-source resident and 55/57/65/69/71 modules from
[the predecessor report](stair_shared_root_comparison.md), its independent71er
reference and the new fixture. `TMPDIR=/tmp`, one BLAS/OMP thread,
`PYTHONPATH=lib/src:test/experiments/transport`; fresh explicit outputs only/tmp.
Existing CPU 120 s and AS 3 GiB generator/reader limits remain.

1. `python -m stair_local_generate --resident <53> --prior-module <55>`
   through ordered71, `--fixture <new-fixture> --output <fresh-module>`.
2. `stair_local_verify` with the same chain/fixture/output; installed read in a
   disposable clone excluding verification/display, using wheel target in PYTHONPATH.
3. `stair_local_reference` with that chain/fixture, `--baseline <independent71>
   --local <new-module> --output <fresh-control>`.
4. `stair_local_cost` on the same chain/fixture/baseline, `--mode local` or
   `--mode whole`, fresh `--output <json>`, optional `--profile <trace>`.
5. `stair_local_display` with that chain, `--local <new-module> --reference
   <fresh-control> --before <71-display/after-display.npz>`; render all four
   scenes through `render_transport_research`, compare pixels and open the PNGs.
6. `python -W error -m unittest stair_profile_mask_checks stair_endpoint_checks
   stair_growth_checks stair_frontier_checks stair_seed_checks stair_local_checks
   stair_module_checks stair_module_chain_checks stair_reference_checks
   stair_junction_precision_checks`; replace lib/src with the wheel for installed.

Evidence root `/tmp/outshine-transport-overlap-92e0f4386/`; current module/control,
actual four images, before-fix counter and protected71er replay retained there.

- `outshine-module72/comparison.json` SHA256
  `b8c4ef493a819692a71045686fcef096fb97a45cc2b5f48b93cae28937ad8bc2`.
- `outshine-module72/verification.json` SHA256
  `47da9a798d41b3616d4814c656bdcd7a608809e6708af7ce8029f16d50544e98`.
- `outshine-reference72/comparison.json` SHA256
  `d8a8e382b1630aa01132b83139c0f54bc5176ed828d16fcc69606cf0b81dfed3`.
- `image-proof.json` SHA256
  `7485eb715072b79924f6a8eb912e1ed995067167e0534470292847e104fff53d`.
- `cost-summary.json` SHA256
  `2186e2e281bb03c7219d6abe791b5d985e53f0a271ba6e63c0bc627010e08e7d`.

SIGGRAPH starting point: Chen et al.2008,
[page2/figure2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
visually reread: source graph and actual3D geometry are separate stages. This fix
uses the existing whole-profile contract, no new tensor-field or SDK method.
