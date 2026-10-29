# Preserve DEM observations and avoid uniform tile sorting

Owner: #1677 under #1402. Python-only optimization of the existing installed
`outshine_ref.places.height.Elevation`; no API, source, recipe or native change.

The actual seven-source rail contact from #1671 makes eleven DEM calls / 3,797
points, including 3,600 local quality points. Profiling shows repeated tile-group
sorting inside `Elevation.sample_with_zoom`. Each batch in this case belongs to
one tile. The existing sampler still calls `np.unique(axis=0, return_inverse=True)`.

The new path checks the already-required integer cell array against its first
row. If every row is equal, the old unique result is exactly that cell, its
inverse contains only zeros, and its selected indices are exactly `arange(N)`.
Use those indices directly. Otherwise retain the old grouping. Empty unresolved
sets exit before this check. All coordinate validation, highest-available-zoom
fallback, declared-source/hash checks, pixel boundaries, bilinear arithmetic and
original query order remain. There is no memoization, mutable sample cache,
quantization or reduction in required observations.

## Alternatives and actual quality

Batch-local exact coordinate deduplication reduced the refinement's 3,768
requests to 3,101 actual points. NumPy row uniqueness took 25.171 ms CPU median,
byte-row uniqueness 22.676 ms, versus direct 21.961 ms. A bounded per-refinement
dictionary reduced points to 1,954 but took 23.655 ms versus direct 21.285 ms in
that paired run. Both preserve the whole mesh but cost more CPU; neither is
retained. These are separate seven-alternating-round prototype measurements,
not comparisons across changing host load or a native performance claim.

The reported maximum vertical LOD difference of 1.895047 m is the fine/coarse
mesh displacement, **not residual DEM error**. An independent fixed 97×97 grid
excludes the rail footprint and measures 8,107 soil positions. The actual DEM
maximum/p95/RMS errors are 3.847746/2.794643/1.679633 m coarse and
2.891172/2.110818/1.109729 m fine. This finite error remains unchanged by the
optimization. It is not a continuous terrain accuracy certificate. Fixed bed,
outer chord profiles, all soil grades and the existing four-pass budget remain.

## Qualified comparison

Producer **74dfe53303748aceb9c6a9be8a161d31b84dfa9f**, original sampler from
**b9d9f5aaec192da712ec83776e04f46a9408eec5**, original height source SHA-256
`41a9da1fc896d93038969d84a49bdd6927db58f49e1d76c21c8fcb3401d04d4b`.
The replay extracts that trusted repository revision into its fresh temp output
and imports it as an independent original sampler. Original inputs are the
versioned rail-ground manifest and existing source-qualified three-arm context.

Linux6.12/i5-1135G7, Python3.13.5, NumPy2.5.4, Memray1.20, BLAS/OMP1; foreign
native builds ran on the host. Seven alternating warm full-rail constructions
at normal priority, source/index preparation and raster decoding outside timing:

| Metric | Original sorted batches | Uniform-cell shortcut |
|---|---:|---:|
| Median full-product CPU | 24.438 ms | 20.451 ms |
| Separate allocation requests | 66,375 | 63,185 |
| Cumulative requested bytes | 13,558,082 B | 13,047,656 B |
| Incremental tracked heap peak | 456,997 B | 447,254 B |
| DEM calls / actual points | 11 / 3,797 | 11 / 3,797 |
| Guaranteed Mesh-copy lower bound | 3,192 B | 3,192 B |

All fifteen complete rail fields and eight three-arm contact fields are bit
equal to the frozen original. The latter retains 123 road vertices,154 road
faces and ninety whole shared bed/soil edges. The independent full-profile,
conforming/Euler/whole-grade/outer-XYZ checks pass. An interleaved 4,096-position
control spanning both actual DEM tiles keeps height and selected zoom exactly;
its paired medians are 3.528/3.215 ms, with no general multi-tile speedup claimed.

Allocation traces are separate from timings and exclude preexisting sources and
decoded rasters. Requested bytes are not copies; the known lower bound counts
guaranteed owned-buffer Mesh constructors only. Total internal copies remain
unmeasured. This saves grouping work, not geometry, source IO or observations.
Python timings and tracked heap do not establish native GPU/UMA/p99 acceptance.

## Reproduction and validation

Use existing `.venv` dependencies (NumPy, SciPy/HiGHS, Shapely, Pillow, Matplotlib,
Memray and existing source readers). No new package is introduced. Set
`PYTHONPATH=lib/src:lib/tests:test/experiments/transport`, `OPENBLAS_NUM_THREADS=1`
and `OMP_NUM_THREADS=1`.

- `python -m unittest test_places ground_refinement_checks test_rail_ground_bed test_constructive_earthwork_plane test_coincident_ground_cover mvt_source_profile_checks`:58/0.
- `python test/experiments/transport/replay_terrain_sampling.py --output /tmp/outshine-terrain-sampling --plot`: fresh explicit temp output, frozen producer, terminal0; actual image opened locally.
- Build the library wheel with existing setuptools84 in a temp copy, install
  into an isolated temp target, prepend that target instead of `lib/src`, and
  repeat the same58 checks. The installed height module hash and all fifteen
  whole rail fields match the source producer. Build/installation/test status0.

Additional boundary cases cover scalar/repeated/broadcast/empty queries, missing
fine tiles, exact tile boundaries, ordering, coarse fallback and changed source
bytes. No existing oracle/tolerance changes. No new images in doc/README/gallery.

SIGGRAPH first: Hoppe et al., *Mesh Optimization* (1993), locally inspected p.2
and distance objective in `doc/references/transport/siggraph/1993-mesh-optimization.pdf`.
It distinguishes geometry/sample fit from representation size; no claim of its
mesh optimization algorithm being implemented here. The actual shortcut follows
the integer grouping identity above. Native Source/EndContact/World and unresolved
terrain accuracy from #1671 remain with their original consumers.
