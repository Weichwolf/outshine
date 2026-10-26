# Shared stair/path profiles from the constructed graph (#1003)

This extends the retained three-part `mvt_stair_t_junction.json` counterexample
and the explicit2D proposals in `mvt_t_connections.md`. It constructs a common
local2.5D plan, actual stair treads and four **proposed** outer profiles. Native
world repair and actual outer-neighbour admission remain open.

```sh
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python -m unittest test_mvt_t_connections test_mvt_t_profiles stair_junction_checks -v
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python test/experiments/transport/compare_mvt_t_profiles.py --output /tmp/outshine-stair-shared-profiles
```

The existing research environment needs NumPy, Shapely, SciPy/HiGHS and Matplotlib.
Outputs are private diagnostics in the explicit temporary directory. The script
compares simplex and interior-point algorithms in fresh workers and generates
`shared-plan.json`, `products.npz`, `report.json` and `profiles.png`.

## One geometry plan before heights

Reuse `stair_junction_layout`, the existing bounded `stair_seed_precision` recipe,
whole-facet constraints in `stair_junction_solve`, tread slicing/body closure and
the independent source-footprint, normal, Float32, riser and vertex-link audits.
The shared primitives gain optional plane-variable offset, stair direction and
height-bound inputs; defaults preserve the existing four-way consumers.

The unrounded source arrangement fails the existing Float32 precision condition;
it is retained as a negative control. The selected binary grid is2^-11m, with
the existing outward containment bound6grid=2.9296875mm. Every prepared source
surface must contain its complete original constructed corridor, preserve its
holes and remain inside that bounded outward neighbourhood. No missing source,
reduced width, altered original identity or relaxed grade/riser test is allowed.

Snap rounding can move noded lines even when the original polygon vertices were
already on a grid. Therefore freeze the generated per-source XY face unions and
landing patches **before solving any height**. Landing patches are unions of the
prepared triangle cells selected by the original landing recipe. Their departure
from the intended clipped landing must remain inside the same declared XY bound.
The original recipe masks and containment checks remain in the input plan.

Auditing the later stair cap against these precomputed domains remains exact;
each cap must cover its planned domain once. The old intermediate buffer shapes
are not the final noded-domain oracle. A negative control rebuilding landings
from those old buffers fails the whole shared-plane check. Consumers must use
the common frozen plan, rather than independently regenerate boundaries.

## Joint finite height fit and real treads

Interpolate supplied design height samples as soft priors over their original
XY polylines. At every retained source/mesh vertex, bound the deviation by one
minimized scalar, capped at the declared2m recipe budget. This is a finite
vertex-prior objective, not a continuous DEM-error or global reconstruction bound.
There are no invented fixed neighbour heights or hard old stair height targets.

Reuse16-sided conservative whole-face gradient constraints, horizontal common
landing planes and monotonic stairs. Prepared straight flights use shared indexed
station cuts and one flat-linear-flat height rule between the two landing heights;
see `mvt_t_stair_loft.md`. Reconstructing analytical cross-sections by proximity
after XY rounding missed every section in the original fixture. That diagnostic
path remains available, but cannot qualify level transverse envelope gradients.
The Float32
coefficient check uses a height magnitude bounded by the priors plus the declared
budget. Opposite stair source orientation changes monotonic direction, while
source provenance remains unchanged. Solver failure/time exhaustion yields no
profile; preparation and solver time are measured separately and are offline.

Both landings, the continuous grade envelope and every original way are solved
together. Existing level-set slicing creates actual treads/risers and a closed
body. Independent audits check the actual saved caps, original containment,
whole normals, actual transverse envelope gradients, shared flat patches,
≤0.18m risers, tread support disks, volume,
Float32 orientation/road grades and cyclic vertex links. The0.1m prior-budget
countercase stays infeasible; it is never silently increased.

## Shared consumer products and limits

`shared-plan.json` contains two construction nodes with one prepared height each,
the exact mesh interface edges between landing and corridor, retained source
references, whole centreline profiles including facet crossings, a connected
source graph, explicit stair levels, terrain footprints and source-face indices.
Original provider feature IDs, tiles, tags, XY lines, adapter assumptions and
the supplied frame accompany the fixture-local part keys. Reservation rings
are numeric XY arrays with explicit holes; consumers need no WKT parser.
All are derived from the same XY/height arrays saved in `products.npz`.

The four outer profiles include every intersection of their full cross-section
with the solved facet edges. They are explicitly unbound proposals: the fixture
does not provide actual complete external neighbour cross-sections. Replacing
those neighbours' cached geometry without qualification would be invalid.
The graph plus stair levels is not a swept-body walking-route, handrail or
regulatory certificate. No native cache codec, renderer, navigation runtime or
terrain consumer is changed by this experiment.

The report owns work, sparse nonzeros, array payload sizes, selected explicit
copy paths, uninstrumented stage timings and a separate traced-memory run.
Live traced blocks do not count every allocation; RSS includes imports and GEOS.
Payload/copy annotations are not native IO/GPU measurements. CPU success proves
neither a native world improvement nor the target device/frame budget.

## Sources

Chen/SIGGRAPH2008 Fig.2, reviewed for the2D step, separates graph and geometry.
Galin et al., [Eurographics2010, p.8/§6/Fig.17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
was visually inspected again for this step: profiles/road construction precede
the related terrain modification. We implement neither paper's complete system.
The finite minimax wrapper and frozen-cell handoff reuse Outshine's existing
stair experiments; the grid contract is unchanged.
[SciPy/HiGHS](https://docs.scipy.org/doc/scipy/reference/optimize.linprog-highs.html)
documents simplex/interior-point methods and solver limits. Actual package
versions, frozen commit, measured costs and inspected image belong in the PR.
