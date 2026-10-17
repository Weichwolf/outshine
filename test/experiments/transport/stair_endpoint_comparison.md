# Secondary original endpoint contacts

Owner: [#545](https://github.com/Weichwolf/outshine/issues/545), parent
[#434](https://github.com/Weichwolf/outshine/issues/434).

The earlier automatic adapter considered new endpoints at old interiors, but
missed an old endpoint meeting a new through source. The original provider node
`[2017,2560]` joins old `80922970:211@0` to new `80922970:280@1`. Explicit
`endpoint_contacts` retain both source IDs and both original vertex indices.
Their combined limit with interior contacts remains four. Actual old ground,
identical original axes/frame, unique nodes and complete sampled old profiles
are mandatory. A nearby overlap creates no connectivity or reuse permission.

The immutable old end need not cover the entire transverse profile of the new
through road. Its own complete profile still must agree with the new/reused
surface, as must the opposite contact walls. At a single new through source,
the additional transverse check compares positive-area facets on opposite
sides of its own profile, including explicitly borrowed old facets. Line-only
touches and sub-`1e-7`-m slivers do not prove a side; all trace intervals and
heights must agree in Double and Float32 with the existing derived guards.
Other junction checks remain. This corrects the previous end/through
specification; it does not enlarge its numerical or physical tolerances.

SIGGRAPH starting point: Chen et al., *Interactive Procedural Street Modeling*
(2008), page 5 figures 7–8 and section 6.1 inspected visually again. Keeping
original graph topology separate from geometric construction is useful here;
this is not their tensor-field generator. No SDK or foreign code is used.

`fixtures/stair-secondary-endpoint-vienna.json` retains two complete original
footways from the bounded five-source prefix proposal. The reproducible parent
is the four-module, 51-source saved specimen from #537. This is an alternative
53-source specimen, not a replacement for the earlier 53-source prefix specimen
from #539, and not a combined 55-source world. The public four-parent and
64-owner limits remain; the general local generator adapter from #542 is open.

## Frozen products and validation

Generator: `3a58c64f6d74da1a7284b4c1dd08578ac104a440`. Final independent
reader and real ablations: `007af061f34b071204ab8b02dc2a738a93573de8`.
The latter extracts the unchanged pin validation to keep functions bounded.
All 128 source / 128
installed-library controls pass, including seven endpoint/through controls.
The installed PR #544 wheel is reused: all 205 Python library modules are
byte-identical to source; this delivery changes experiments only.

Two complete original footways, `80922970:280` (74.735021 m) and `138663000:6`
(159.476881 m), add 234.211902 m to the 51-source parent. The module has
49 surface vertices / 66 facets and a closed 98-vertex / 192-face body.
The first source explicitly reuses 3.026924 m² of root `80922970:208` and
2.111298 m² of old `80922970:211`; the second reuses no old area. Four opposite
wall segments total 6.224984 m. Root, old-end and new-through complete profiles
pass both precisions; maximum Float32 old-end disagreement is 2.681413 µm,
new through-side disagreement 1.868219 µm. The two new sources have no
positive-area overhead-bridge overlap, so they do not demonstrate a new
underpass. Earlier bridge/curve/step evidence remains tied to unchanged bodies.

Local terrain: 612 old facets become 920 facets / 526 vertices, with 130 exact
boundary vertices and 466 pins. Three protected parent facets and two existing
piers are affected. The whole diagnostic field has 14218 vertices / 28078
facets; 27158 outside XYZ triangles are byte-identical. All 124 whole piers
remain checked; maximum Float32 contact error is 15.170145 µm. The independent
saved-product reader verifies original sources, DEM targets, graph membership,
four actual profile-hull pad uses, local/world links, physical contacts and
actual display arrays. All 1657 original line parts and 71 area features retain
their source data/IDs/reasons; native `admitted_source_ids` remains empty.

`stair_endpoint_controls.py` reproduces identical original-source recipes and
contacts with indexed and scanned discovery. It rejects removal of the real
end declaration, a different original node, nonground old geometry and the
nearby undeclared `538150640:8` overlap. The five-source whole proposal is still
rejected: automatic declarations encounter multiple cached profiles at one
node, a combination the current one-contact-per-node adapter does not support.
The earlier undeclared version also produced a Float32-degenerate facet; no
tolerance is raised to admit it. All old road arrays remain byte-identical
after these ablations. Gap, height-step and line-only two-sided trace controls
fail in both precisions. No individual impossibility or maximality is claimed.

Four fresh 1280×720 3D before/after images were completely decoded, their mesh,
scene and PNG hashes checked, and all opened. Overview changes 1373 RGB pixels;
network view 1932. The two added links are faint but present; the result remains
sparse and pale, with substantial terrain/coverage/presentation work open.
These private research views are not four completed Places or an engine gate.
There is no public image or extra gallery in this delivery.

## Costs and retained evidence

Host: four-logical-CPU i5-1135G7 VM, Python 3.13.5, NumPy 2.2.4,
Shapely 2.2.0, single BLAS/OMP thread. Generation and the final reader ran with
library tests briefly overlapping at startup. Actual render backend is llvmpipe LLVM
19.1.7, not a measured RTX/A18 backend. Processes are limited to 120 CPU seconds
and 3 GiB address space; each height solve keeps eight seconds / 100 iterations.

Generation takes 20.647580 s / 181551104 B peak RSS; independent reader
18.463566 s / 191262720 B. Generation phases: surface/body 0.176002 s,
module admission 0.416887 s, local terrain planning 0.530064 s, fitting
0.064149 s, whole diagnostic assembly 2.394361 s and whole admission
15.960202 s. These timings include distinct work and are not frame-time claims.
The new height solver uses nine iterations; no cost or quality winner over a
different complete generator is inferred from a failed contact recipe.

Actual work includes 120 arrangement segments, 12 surface-weld candidate pairs,
898 terrain-weld inputs / 537 outputs / 361 candidate pairs and 14 conformed
edges / 128 added triangles. Explicit terrain intermediates record 38208 B
raw XY, 8416 B unique XY, 19104 B remap; indexed and difference weld arrays
each 14368 B plus 7184 B displacement results. The module arrays own 10653 B;
local layout 40462 B and local field 4675 B. Whole diagnostic assembly records
344208 B global labels and 3910752 B outside-triangle comparison arrays.
These are measured array sizes/work counters, not a complete allocation census:
GEOS/solver/Python allocations and their total copied bytes are unmeasured here.

Identical six-root source proposals take 109 indexed incidence tests versus
114975 scanned tests, plus an index of 1405 nodes / 1572 entries. This is
different work, not equal-cost operations or a whole-generator speedup.
The real five-case ablation process takes 1.611630 s / 124858368 B peak RSS.
Four actual renders take 11.587831 / 11.469883 / 13.602701 / 13.361552 s.

Products: `/tmp/outshine-transport-endpoint-full-3a58c64f6/`;
commands, raw probes, terminal logs, installed-source proof, image completeness
and pixel comparisons: `/tmp/outshine-transport-local-3e9d36578/`.
The before display is deduplicated by symlink only after whole-file byte
equality with the retained 51-source parent's display was verified.

SHA256 receipts:

- Fixture: `acce3eba92148178b5221f6c45a5553744984ae9f9ef5b846428a57b4b9be17d`.
- Comparison: `c1a59c9e557271c165f3adff3e95cc694e00235a9a6b2368a9f01a0e7c2d17b7`.
- Independent verification: `5988a81019ef74d0c47f712b53546b7c033beacd9af6bc99ba18b4e362b2c948`.
- Ablations: `a5aef4d902f1c3d1552cda26f4bb792e25165f93afc72a000e9b37134e4c18ee`.
- After display: `843bfa52924c60285c60d0d53af977bd6399e59230d29d1ba4f5413f4c178274`.

## Reproduction

Use the base/support outputs and four ordered parent modules from
[the frontier comparison](stair_frontier_comparison.md). The fourth parent is
`outshine-module-0` in `/tmp/outshine-transport-frontier-auto-2a74dbeab/` with
its sibling `recipe-0.json`; the first three parents are unchanged. Pass these
as `--base`, `--supports` and four `--cached-module DIRECTORY RECIPE` pairs.
Set `--fixture` to `fixtures/stair-secondary-endpoint-vienna.json` and `--output`
to a fresh explicit system-temp `outshine-*` directory. Then execute, using
the same arguments and a clean frozen checkout:

```sh
python test/experiments/transport/stair_module_experiment.py ...
python test/experiments/transport/stair_module_verify.py ...
python test/experiments/transport/stair_endpoint_controls.py ...
```

The third command requires the successful independent verification receipt;
its fresh `endpoint-controls.json` never changes the input recipes or accepted
geometry. For source controls add `stair_endpoint_checks` to the test command
in [the exact-patch comparison](stair_patch_comparison.md). For installed-library
controls replace `lib/src` in `PYTHONPATH` with the unchanged installed wheel.
Use the existing renderer on the four generated scene JSON files; check fresh
complete PNG/receipts, open every image, and run `test/scripts/pixels.py` on
matching before/after views. No new package dependency is required.
