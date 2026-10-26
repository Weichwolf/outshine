# Four whole footways beyond the 65-source resident network

Owner: [#587](https://github.com/Weichwolf/outshine/issues/587), parent
[#434](https://github.com/Weichwolf/outshine/issues/434). This uses the existing
[seed generator](stair_seed_comparison.md), not a new source-selection or geometry
algorithm. The frozen producer is `a67f37ff004d7aefbdd2ed05a0a277c2be68a712`;
complete control, fresh readers, renders and tests use
`fb8efb22d7a6decba5f247a34aea1143e7e8c50d`. The preceding sampler correction is
[separately evidenced](surface_contact_comparison.md) in #589 / PR #592.

## Actual geometry and remaining gaps

Four whole original paved footway parts add **632.064450 m** with the existing
procedural 1.8 m profile. IDs have prefix
`openfreemap:14/8937/5678:transportation:`.

| Part suffix | Original length, m |
|---|---:|
| 80922970:158 | 344.937204 |
| 80922970:319 | 204.358857 |
| 80922970:157 | 67.013123 |
| 80922970:161 | 15.755266 |

The connected new component is explicitly independent of the old network;
no old connection, new underpass or complete worldwide generator is claimed.
The original 1657 line parts / 71 areas remain, with 69 used / 1588 unused
lines and their original geometry, properties and reasons retained. Native
admitted-source IDs remain empty. Input PBF SHA256:
`97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8`.
DEM SHA256: `67a97b8fa2f5dde52f7e7afdfd31e8cb63f24591c21b70e6273d506d78ffa177`.
Frame/origin remain the previous Vienna ENU 48.242427962074906,
16.386505365371704, up0m. The [fixed selected recipe](fixtures/stair-southwest-footway-seed-vienna.json)
pins the original reference input; no private output is a versioned test input.

Surface: 200 vertices / 302 facets; closed body: 400 vertices / 804 faces.
Double/Float32 volumes 339.299770 / 339.300218 m³ and normalized grades
0.733675 / 0.973829 pass. Whole original profiles, internal contacts, a source
hole and cyclic connected vertex links pass both precisions. The existing
2^-11 m outward model and 2.9296875 mm inclusion bound remain unchanged;
measured original-to-model Hausdorff distances are 1.481–1.628 mm.

372 old terrain facets become 1080 facets / 586 vertices, with 92 exact boundary
edges and 517 pins. All 29144 outside triangles remain unchanged. 402 whole
source-bed facets pass 32188 contact pair tests, maximum Float32 error
13.506472 µm; 80 old body-bed facets pass 9946 pairs, maximum 12.947041 µm.
All 124 whole old piers pass 7150 pairs, maximum 15.170145 µm.
This changed local patch contains no bridge underside: local headroom is
explicitly inapplicable. Complete control still checks all old bridge geometry
in Double/Float32; a zero clearance witness at an existing contact is not a new
headroom improvement. No tolerances, solver/iteration or geometry limits change.

The first independent eight-source proposal fails its bounded height fit;
the second four-source proposal succeeds. Attached proposals separately fail
whole-profile coverage, undeclared old-source reuse or actual Float32 vertex
merges. Exploratory analytic sections/junction pads did not solve those full
contracts and were not adopted. Rejected sources remain available with reasons;
three attempts do not exhaust all alternatives. Index and full scan agree on
64 actual roots, 18 recipes, dispositions and original input bindings; measured
selection 2.775322 / 2.820152 s is one shared-host run, not a universal win.

## Independent complete control above 64 sources

`stair_reference_roads.py` assembles geometry from all nine verified old assets:
65 sources / 13264 closed body facets / 550992 B geometry arrays. It retains
each asset's local owner matrix, source IDs and owner hash; it constructs **no
global owner bitmask**. This is only the seed diagnostic control, whose checks
read old geometry. At this frozen revision, attached controls still used the
at-most-64-source temporary mask; the later [shared-root control](stair_shared_root_comparison.md)
supports attached geometry through asset-local owners. Normal generator limits remain 16 local assets / 64 local
sources / 32768 body faces, not a larger production working set.

The new control independently fits and assembles the complete field. All
30224 resulting XYZ facets and attributes match resident replay exactly;
module/layout/field arrays match dtype, shape and bytes. The whole diagnostic
body matches that independent output. Actual owner bindings stay local through
the full verified load; no 65th-bit overflow or ownership claim from a flattened
mask. Controls cover 66 global sources, exact geometry/index offsets, immutable
original owners and unchanged local-mask/whole-geometry budget rejection.

50 affected source / 50 installed-library tests pass with `-W error`:
`stair_reference_checks stair_local_checks stair_seed_checks stair_frontier_checks
stair_growth_checks stair_module_checks stair_module_chain_checks`. The fresh
PR #592 wheel has 205 byte-identical source modules and already passed all 536
source / 536 installed library controls; this delivery changes no library code.
Fresh source/installed automatic readers both verify the complete actual saved
attempt window and accepted geometry. Disposable clones protect original
receipts; those receipts were not rewritten.

## Costs and actual whole 3D images

Linux x86_64 i5-1135G7 VM, Python 3.13, NumPy 2.2.4, Shapely 2.2.0, one
BLAS/OMP thread. Module generation 10.392016 s / 134811648 B process RSS;
automatic selection, failed attempt, generation and reader 29.371783 s /
155975680 B RSS. Fresh source automatic reader 15.365321 s / 153452544 B;
complete independent diagnostic 29.672767 s / 185761792 B. These are distinct
operations, not equal-output speed comparisons. Shared host had other Python
research/render work; no native/device/cache/frame budget acceptance.

Local context: 1721 terrain facets; four of nine road assets / 13 local sources /
1888 body faces / 66832 B road arrays. Retained terrain-copy bytes remain zero;
explicit XY/raw-to-unique/conformance counters remain in the report. Memray 1.20
`run --trace-python-allocators -m stair_local_generate` on identical inputs
records 46036931 allocation events / 6319594868 allocated B / 64248996 B tracked
peak, including imports, replay and output. This is allocation churn, not live
process RSS or total copied bytes. Instrumented generation 29.218897 s is not a
normal-speed measurement; all four resulting NPZ array sets are byte-exact to
the uninstrumented accepted specimen. Internal library copy bytes are unmeasured.

Both actual complete/moderate 1280×720 before/after images were opened. Camera,
projection/view matrices, light, samples and renderer helper hashes match the
previous accepted images. New paths appear at the southwest edge. Overview:
2604 changed pixels, 743 above one level, maximum 22/255; moderate network view:
3365 changed, 1070 above one level, maximum 26/255. This is a small visible
network addition; the scene remains sparse and materials/light are diagnostic.
No closeup, public gallery, server or image publication. EEVEE/llvmpipe actual
renders take 52.866934 / 17.991183 s with peaks 2230374400 / 1821085696 B;
overlapping other software-render work, not an RTX/native performance claim.

## Reproduction and accepted receipts

Inputs are the retained 53-source resident plus 55-, 57- and 65-source modules
documented in the linked earlier reports. Set `TMPDIR=/tmp`, one BLAS/OMP
thread and `PYTHONPATH=lib/src:test/experiments/transport`; use explicit fresh
`/tmp/outshine-transport-<task>-<commit>` outputs. No `/var/tmp` or repository
build tree is used for private experiments. Runtime has no SDK dependency.

1. `python -m stair_growth_experiment --family seed --resident <53>
   --prior-module <55> --prior-module <57> --prior-module <65> --output <fresh>`.
   Default roots-first/prefix, offset0, at most3 attempts selects recipe1.
2. `python -m stair_growth_verify` with identical arguments in a disposable
   hardlink clone excluding `verification.json` and `display`; keep its receipt.
3. `python -m stair_local_reference --resident <53> --prior-module <55>
   --prior-module <57> --prior-module <65> --baseline <independent65>
   --fixture <recipe1> --local <module1> --output <fresh-control>`.
4. `python -m stair_local_display` with the same chain, `--local <module1>
   --reference <fresh-control> --before <65-display/after-display.npz>`; render
   the emitted scene files through `render_transport_research`, then compare
   complete PNGs with `test/scripts/pixels.py`. Keep all outputs under `/tmp`.

Accepted local evidence root: `/tmp/outshine-transport-network-64ce38ddb/`.
Automatic report `outshine-seed/comparison.json` SHA256:
`9ec068b4369adb4253f6034f0a6973df34cf5513484bf3116cf7f992704e3bf6`.
Accepted `outshine-seed/outshine-module-1/comparison.json`:
`a008ed58eaf7321e9a722738665404f1c0d9836fc9eb3388b9418d0a04858be2`;
protected receipt `41df3c1028e144af339ac049050d6c08c6f349baf5bb245ed3b182936ac2ac23`.
Complete `outshine-reference69/comparison.json`:
`d1db3d400cfacbed7c73e4207c1fb7128ce4f835fb8610cb9b3e6e5919cdac0a`.
Allocation stats `73ae1e27d7e1a3304e8dd645a43b3b8dc5677a3f49259f0bd7a4727979b12f9a`;
NPZ equivalence `dbac518b639edad9ed1ee52c7bc941e6b261c49b1d43c913f321e21c4d0cff93`;
matched-image proof `e5c984a4ff33f997ee5e56ba9cc9196b5306391888c40554371dec6ab2169e4a`.

SIGGRAPH starting point: Chen et al. 2008, [page 2 / figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
visually reread: graph proposals remain distinct from actual 3D admission.
No new paper/SDK method or foreign code is incorporated. General source reuse,
attached thin-junction construction, affine consumer callbacks, complete world
coverage and native integration remain open in their respective owners.
