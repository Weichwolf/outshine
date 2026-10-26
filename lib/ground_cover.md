# Bounded ground-cover roots (#304)

`from outshine_ref.vegetation import GroundCoverPlan`

A homogeneous Poisson point process in one caller-owned, fixed planar metre
frame. It supplies root identities/locations and independent marks; it builds
no blades, camera snapshots, terrain, species distribution or engine assets.
Its actual physical grass consumer and measured limitations are in
[the experiment](../test/experiments/vegetation/ground_cover.md).

```python
plan = GroundCoverPlan(seed=113, density_m2=150, cell_m=1)
summary = plan.summary((-2, -2), (2, 2), max_cells=4096)
sites = plan.sites((-2, -2), (2, 2), max_sites=100_000)
```

`density_m2` is expected tuft roots per square metre, not coverage or leaf area.
Canonical half-open square cells partition the plane. A BLAKE2b128 key over the
versioned seed, signed little-endian cell coordinates and purpose seeds PCG64.
One independent count stream per cell draws Poisson(density*cell_m²). A separate
site stream draws five uniforms per ordinal: two for XY, three caller-owned
marks. Spatially independent per-cell Poisson processes form the homogeneous
process; the implementation uses ordinary pseudorandom draws.

IDs are the signed Int64 tuple(cell_x,cell_y,ordinal), not truncated random
hashes. Their namespace includes `plan.identity` and the caller's stable frame.
Do not reinterpret these IDs in another local/camera frame. Changing the query,
row order, camera or detail cannot move a root or alter its marks. Density is
part of the plan identity; shared ordinals retain their site-stream draws when
density changes, but changed Poisson counts need not be nested.

`summary` returns fresh `cells`, `counts`, conservative `candidate_roots` and
`expected_roots_in_query`. It makes no site/pose arrays. Counts include whole
intersected cells, even for a small partial-cell query. `sites` checks their sum
before site expansion and returns `None` if either work budget is insufficient.
A caller may subdivide a refused query. This is an explicit candidate-work
budget, not an exact returned-count admission test. Invalid inputs raise
`ValueError`. Zero area/density returns correctly shaped empty products.

`sites` returns owned Float64 `xy[N,2]`, Int64 `ids[N,3]` and Float64
`marks[N,3]` in[0,1). Roots use lower-inclusive/upper-exclusive bounds. Cropping
adjacent queries partitions the same whole result exactly, including negative
cells. Root-query bounds are not bounds of the rendered plants: consumers must
expand demand for blade extent and shadow needs and must not truncate plants
at root-query seams.

Bounds: seed uint64, density0..2048/m², cell side0.25..16m, finite planar
coordinates within±10,000,000m. Default cell budget4096, maximum65536; root
candidate budget at most1,000,000. These are reference-work limits, not native
frame/device budgets or permission to flatten the Earth.

`recipe()` saves parameters, format and NumPy version; `from_recipe` rejects
another NumPy version because the high-level Poisson transform is not promised
bit-stable across versions. Persist actual arrays/count products for cache
hits. A cache hit loads them; it does not rerun this point generator. Native
ports need a separately versioned distribution/replay agreement.

Counts/point batching is O(C+N), versus the scalar comparison's same C+N work
with a Python loop per root. Both produce exact identical arrays. Four
alternating rounds on78,518 roots return5,025,152 bytes: batch median0.0618s,
scalar median1.4118s. Per-cell random scratch is5*count*8 bytes; result payload
is64 bytes/root. Dense scratch plus returned selection copies, mask and cell
arrays still count; total allocator calls were not instrumented.

Five independent controls check negative-cell seams/crops, exact replay,
count/position stream separation, pre-pose budget refusal, empty/numeric cases
and Poisson/uniform moments. A fresh installed-wheel import/replay additionally
checks the actual public API. Forest/biome ecology, source reservations,
terrain contact, world-frame selection, wind and render LOD are consumer work.
