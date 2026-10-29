# Terrain diagonals preserve actual Double and Float32 contact

Owner: [#1750](https://github.com/Weichwolf/outshine/issues/1750), parent #434.
The automatic six-source proposal from #1738/#1743 now succeeds from107 to113
sources on the unchanged absolute Double/Float32 contract. One internal terrain
triangle previously had positive Double doubled area1.32572584e-6m² but zero
Float32 area. The road body itself already passed. The regional contract #717
remains separate; no vertex, source, body or cached outer boundary is moved.

## Bounded construction and complete contact

`stair_terrain_diagonals.repair_diagonals` changes only an internal diagonal.
The ordered four vertices must give two positive replacement triangles both
as exact binary64 rational determinants and in the actual D/Q arithmetic.
The old Double pair must be positive; opposite edge directions and an absent
replacement edge preserve topology. Same ground/free class is required;
protected nonroad parents cannot change. The later full coverage/uniqueness/
orientation and physical checks remain mandatory. No area epsilon is used.
Limits:64swaps/192candidate tests,16384vertices/32768faces. Failure publishes no
unchecked geometry; already-valid inputs return the same index object and no note.

The former height proof required every child to have one entire affine parent.
A legal diagonal can cross an old inner parent edge while preserving contact
within the existing1e-7m bound. This proof requirement is explicitly generalized:
retain the old single-parent fast path, otherwise intersect the child with all
nearby original parents. On each convex intersection, the difference of two
affine height functions is affine; its absolute maximum occurs at a vertex.
Check all intersection vertices, complete child coverage and complete original
parent coverage. The previous XY tolerance/machine guard and derived envelope
`height_error + (child_Lipschitz + source_Lipschitz) * metric_radius` remain.
Overlay limits:64candidate parents/1024intersection vertices per child, within
the existing million-parent-pair bound. A hole, wrong height, missing parent or
budget exhaustion rejects. Matching source vertices alone does not prove contact.

SIGGRAPH-first source: Hoppe et al., *Mesh Optimization* (1993),
[local PDF](../../../doc/references/transport/siggraph/1993-mesh-optimization.pdf),
pages4–5/Figures4–6 inspected optically: legal local edge swaps and locality.
This implementation holds vertices fixed and uses no fitting/energy solver or
foreign code. NumPy, Shapely/GEOS and standard-library Fraction remain dependencies.

## Actual result and reproduction

The original six-source module succeeds after one diagonal swap:167vertices,
249topfaces/668closedfaces, all11body fields/38841B exact to its old pre-terrain
body. D/Q volumes227.190145/227.191628m³; normalized maximum grades
0.99666667/0.99669505. All27old assets/186arrays/957042B and124support contacts
remain. Terrain295oldfaces→279vertices/497faces,59exact outer vertices.
Two children use four parent pieces/16intersection vertices; actual maximum
Double height error1.26074156e-8m, derived contact bound1.78142200e-8m.
The ordinary automatic geometric query now accepts proposal0 after one attempt.
Manual/automatic output24fields/63819B and three metadata documents are exact.
Fresh old110control remains exact in24fields/38537B and three documents.

Use existing `.venv`, BLAS/OMP threads1 and experiment import paths for transport,
assets, buildings, vegetation and render plus `lib/src:lib/tests`.
Run `stair_endpoint_frontier_study --resident ... --prior-module ... --query
fixtures/stair-endpoint-frontier-query.json --output <fresh-system-temp>`.
The resident107prefix is unchanged; each generated product has a fresh saved
reader. `stair_terrain_overlap_study` also captures `pre-diagonal.npz`, all old
assets and exact body/layout reproduction; give the same recipe/product and its
original pre-terrain body. Render both actual scene JSONs through
`render_transport_research`, open PNGs and compare with `test/scripts/pixels.py`.
The1280×720/16sample llvmpipe diagnostics show the additional connected subnetwork;
274369pixels change,111398>1/255, including terrain shading. Coarse terrain,
diagnostic materials and open outer endpoints remain. These are contact views;
the unchanged existing Buildings/SITE consumers on frozen0616308c2 also accept
113sources/65bodies/4wholeplants with fresh independent readers. All394habitats
and1024proposals remain; this is a finite pilot with sparse scene coverage.
Whole-scene diagnostics use their full inputs separately.

Run `stair_terrain_diagonal_cost --study ... --output <fresh-system-temp>` and
`stair_ground_overlay_cost --output <fresh-system-temp>` at normal priority.
Intel i5-1135G7/Linux, Python3.13.5/NumPy2.5.4/Memray1.20.0, nine rotating rounds:
exact/GEOS diagonal choices1.164/1.444msCPU;2536/2184allocation events,
581653/568038requestedB,82696/78040Bscoped peak. Same whole497-face output.
Already-valid cost0.292/0.288ms. The exact proof trades a small allocation/peak
increase for less CPU in this cohort; this is no global optimality claim.
Equal prepared height contexts: overlay/affine kernel0.991/1.745msCPU,
2103/3733events,278142/450022requestedB,22644/22556Bscoped peak. Both return the
same four pieces/16vertices and height error. A smaller whole-extrema predicate
costs1.854ms versus1.987ms for the entire overlay checker including parent
accounting/derived envelope; it is not equivalent full work. Crop timings exclude
whole construction/IO/heights/readers and total internal copies; heap and RSS are
not additive. Four final traces for each comparison are retained privately.

Controls include the actual degenerate facet, concave/tiny-negative alternatives,
class/protected/boundary/merged-vertex/budget refusals, ordering and untouched inputs.
An interior height counterexample has identical source vertices but must reject;
missing source coverage also rejects. No quality bound is raised.
Native admitted IDs remain empty. General regional precision, worldwide coverage,
public routing, native assets/World/device/UMA/p99 acceptance remain open.
