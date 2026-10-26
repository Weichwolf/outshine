# Planned window extents before detail construction

Research [#1070](https://github.com/Weichwolf/outshine/issues/1070), under #33,
extending [explicit window demand](window_detail_demand.md). A small spatial
query now supplies the existing detail manager's IDs without building each window.
This is a generator-level reference, not an asset database or a native LOD policy.

## Metadata and exact query contract

`compile_bounds(prepared)` reads only existing opening dimensions, orientation,
height and the forward core frame for cascade/paired-wing windows without legacy
trim. It emits immutable IDs and metre-valued world AABBs, including original
frame, pane, mullion and sill reaches. Half-width padding .08 m covers .055 m casing
plus .025 m sill extension; inward reach is −.16…+.118 m, vertical reach is
−.10…height+.055 m. The lower bound covers the original sloped sill. A finite
binary64 arithmetic guard expands the transformed corners outward. Unsupported
recipes reject; no template, window/floor mesh, Boolean or room fitting is called.
All 2,936 actual closed windows in the five original Paris sources × two forms
are contained. Forward rotation/shear/large-origin controls also pass. This is
not a universal/native Float32 transform-bound proof or worldwide source inventory.

The independent data reader knows only immutable IDs and `(N,2,3)` binary64 boxes.
Its explicit query is **closed sphere intersects stored AABB**, in the existing
common XY/height frame. Object centers alone are insufficient. It has no camera
visibility, occlusion, projection, radiance or general physical-distance guarantee.
The boxes conservatively bound bodies; their empty corners can produce extra work.
Functional, shadow and lighting requirements remain independent caller inputs.

The NumPy scan and optional GEOS STRtree return the same IDs in original plan
order. The tree filters outward-rounded XY query bounds, followed by the same
3D predicate; degenerate XY boxes stay explicit candidates. Directed binary64
interval operations resolve ordinary cases; uncertain tangencies use exact rational
arithmetic over the supplied float values. Tests use a separate scalar rational
scan, including signed/height contact, subnormals and one-ULP-outside cases.
GEOS and rational paths here are offline Python controls. Native selection keeps
GEOS out of the frame path under the existing project contract; no native port
of these dependencies is proposed.

`WindowRange` owns no residency, policy, cache or update history. Its resulting IDs
feed the unchanged `WindowDetails.update`. Source/radius/position changes can cause
new demand; a stationary position or pure turn needs no repeated radius query.
Benchmark repetitions measure the function, not a proposed per-frame schedule.
Capacity fallback remains complete coarse geometry in the existing manager.

## Checked result and costs

Final scientific producer `a57c15af5`, containing Master `33a9a3fba` through the updated
#1053 base `1fafadc5f`. Python 3.13.5, NumPy 2.5.4, Shapely 2.2.0/GEOS 3.14.1, Linux shared
host, one BLAS thread. Three fresh normal processes alternate selector order by
case; the second reverses it and detail-residency order. A separate fourth process
traces allocations. Earlier measurements before removing a needless full-table
scan copy are superseded; the geometry fields remain word-identical. The final delivery also includes Master
`ee7bbd775`; its intervening changes affect native import cleanup only, with no
change to the measured Python sources or fixtures.

| Original case / windows | Scan median µs, three processes | Tree median µs, three processes | Bounds tested over seven requests, scan/tree |
|---|---|---|---:|
| two-court cascade /252 | 155.0 /149.6 /154.1 | 144.9 /143.0 /145.9 | 1764/78 |
| large concave cascade /508 | 213.5 /213.4 /212.7 | 147.8 /146.4 /148.8 | 3556/168 |
| compact cascade /91 | 111.1 /111.7 /117.9 | 147.5 /142.8 /147.2 | 637/98 |
| compact paired-wing /155 | 128.8 /126.7 /130.6 | 144.5 /142.4 /164.6 | 1085/84 |

These are full Python function costs, not native device times or full tree-node
work counts. Tree construction adds .263–.313 ms in the 252-window case and
.399–.583 ms in the 508-window case. After subtracting scan construction, median
time savings amortize after roughly 28–39 / 6–9 movement requests respectively.
The small two cases favor scanning throughout. No automatic global winner or
native index requirement follows; retain the simple scan option.

252-window metadata construction takes 1.11–1.99 ms, before any detail construction.
Logical boxes plus UTF8 IDs occupy 26,825 B; `.npy`/JSON test files total 27,962 B.
Metadata acquisition's traced peak is 316,952 B. The scan avoids copying its complete
48 B/box table per query; indexed queries copy only candidate boxes. Box freezing
copies 48 B/box once; temporary eight-corner arrays and interval buffers add work.
Recorded incremental peaks omit some native GEOS allocations. This is neither
an application-memory budget nor an actual DB/IO/upload measurement.

For the original 252-window building, seven explicit 5 m requests select
5/5/5/8/8/8/3 windows. Only 8 patches are built in total; unchanged products retain
their identity, departing products are released. The resident-all control builds
each of 252 patches once and draws the same requested subset. All first/last draw
fields match exactly across both strategies and all four processes. No metadata-
envelope extras versus actual mesh AABBs were observed in these 70 finite requests;
that is not a global tightness claim.

The per-window resident-all build costs 698–717 ms plus 2.44–2.85 ms for updates;
selective updates including first details cost 22.84–24.49 ms. Final detail arrays
are 7,846,272 versus 93,408 B; newly generated/frozen detail buffers total 249,088 B
selectively. The existing full batch recipe is cheaper than the per-window-all
control: no speedup against that batch or a physical building is claimed.
The common complete coarse build still costs 132–156 ms and retains 1,507,594 B.
Logical plan preparation is separately charged. Detailed update traced peaks reach
217,910 B; full initial construction reaches8,423,532 B. Snapshot/export copies are
outside those timers. Full-process RSS 141.7–143.2 MB includes both diagnostics,
imports and plans; these figures do not establish native UMA residency savings.

## Verification, picture and open scope

Sixteen current-Master tests pass: eight existing detail tests and eight extent
tests. A fresh reader uses only ten saved ID/box tables and 70 radius requests;
all 140 scan/tree selections agree, with original-data/geometry opens forbidden
and generator modules absent. Radius bounds are not acquired by measuring a full
window mesh; original closed bodies are used only as independent offline controls.

Actual 1280×720, 16-sample Eevee close views were rendered from saved identical draw
fields, with the same diagnostic object name/camera/material/light. Both pictures
were opened; all 921,600 pixels match exactly (`test/scripts/pixels.py`). Backend is software llvmpipe; no native/LOD/PBR/world/
GPU-time admission follows. These compare identical requested subsets, not a
globally fine facade against an image-qualified radius rule. No images published.

Native serialization/DB layout, runtime demand timing, caller source-version
binding, Float32/device bounds, radiance/shadow/collision policy and both device
budgets remain open. Existing coarse ownership, logical floors/rooms and portals
remain unchanged. No new cache framework, native bindings or dependency deployment.

## Reproduce

Use `.venv` and existing NumPy/Shapely/recipe dependencies. Set one BLAS/OMP thread
and `PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets`, then run
`python -B -W error -m unittest test_window_detail_demand test_window_extent_query -q`.
On frozen sources run `compare_window_extents.py --output <fresh-system-temp>`
with `--verify`, two more fresh normal processes (one `--selective-first`) and a
separate `--trace`. Reports retain inputs, IDs, costs and same-output hashes.
Local raw reports, reader/render reproducers and receipts are linked from the PR.

SIGGRAPH 2006 component ownership remains the recipe context. [F-shade, EGSR 2010](../../../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf)
page 3/Figure 2 was visually inspected for compact scope-based access. This is neither
their grammar nor GPU algorithm; it queries extents of existing planned components.
