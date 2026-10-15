# Spatial needle patches and correct sample composition

Owner: [#154](https://github.com/Weichwolf/outshine/issues/154).
PR #160 establishes that a single view card loses fine shoot structure. This
experiment compares several fixed spatial planes with the same four complete
cached near shoots, keeping every source triangle in one patch or the untouched
closed-geometry remainder. It is not an approved forest LOD.

`needle_spatial_planes.py` proposes at most 128 area-quantile triangle planes.
Every selected face must have all three vertices within the requested positive
object-space distance. Greedy scores use projected area; removing covered faces
updates scores incrementally. The final plane offset is the midpoint of its
assigned vertex depths. The maximum Euclidean displacement is measured explicitly.
At most 64 patches are selected; all remaining faces stay original. No triangle
is silently dropped, and no minimum plane count or full paper equivalence is
claimed. Only foliage is simplified; wood remains a material/geometry control.

This follows the vertex validity/projected coverage principles of
[Billboard Clouds, SIGGRAPH 2003](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
PDF page 3 sections 2.1/2.2 and Figure 2, opened optically. The paper's full
plane-space search, tangency penalty and global optimum are not implemented here.
The affine orthographic projection gives a corresponding screen displacement
bound; texture coverage, disocclusion and lighting still need independent tests.

Each plane gets two unlit views. Texture density is explicit in metres per texel;
empty borders are removed without resampling, retaining one transparent texel.
True first-depth samples remain separate from the plane depth used for drawing.
Normal/material raw samples are diagnostic data, not a proposed GPU texture ABI
or an approved normal mixture. The same procedural corner-material and constant
wood controls as PR #160 remain; no shader equivalence is claimed.

`needle_composition.py` combines depth-ordered premultiplied samples before the
final pixel filter. Filtering each opaque layer first loses mask correlation:
two disjoint half-pixel masks should cover a whole pixel, not 0.75; coincident
masks should cover 0.5, not 0.75. Independent red/blue/depth cases verify both.
`needle_group_layers.py` now uses this order. PR #160's organ-band measurements
describe the earlier filter-then-compose experiment and are superseded for the
current implementation. Its single-card and closed-source controls are unchanged.

Atomic `needle-spatial-v1` NPZs attach original geometry, plan identity, axis path,
source IDs, both chart sides and the complete remainder. This is a reference
product built from an existing fine cache, not direct cold far construction.
Fresh decode is checked with organ placement/meshing disabled. Products are
camera-independent; camera and shadow callers select a side independently.

```sh
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/needle_spatial_checks.py
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/needle_spatial_experiment.py
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/vegetation/needle_spatial_checks.py --cache
```

Use a frozen commit and the same `conifer-crowns` inputs as PR #151/#160. The
single-card control NPZs remain bound to their original frozen SHA/source inputs.
Four species × 17 independent directions × two displacement settings compare
triangle counts, total coverage, pixel coverage L1, depth RMSE, actual projected
vertex error and CPU/array/storage costs. The 2560×1440 diagnostics are unlit
coverage figures outside the four-image showcase. Source-generation, chart
building, decoding, CPU reference drawing and actual GPU/frame costs are distinct.
Moving whole-tile images, perspective, shadows, normal/R/T response, wind and
native/A18-Pro costs remain separate open approvals. The existing complete
whole-tile showcase remains the physical reference until a new tile is accepted.
