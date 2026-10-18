# Native continuous spatial wood — #1 / #48

`PlantLimbPlan` ports the verified [authored oak wood](../vegetation/oak_continuous_wood.md)
and [continuous limbs](../vegetation/continuous_limbs.md), using the original node IDs
and positions. `PlantLimbSurface` consumes explicit detail/capacity demand before
allocating station or mesh buffers. The existing world still uses flat tree cards;
this is the spatial wood component of their replacement, not world admission.

## Source and representation

The local SIGGRAPH1985 [Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf),
PDFpage3, was opened optically: section rings, centreline curvature and frame
singularities. The port follows the existing Python C1 Hermite/PCHIP and double
reflection implementation, rather than introducing Frenet frames at straight sections.
The prepared plan owns cubic controls and radius coefficients; sampling does not
repeat path selection, source growth, numerical integration or a curve solver.

Input is one explicit ordered source path with2–65 unique anchors and positive
prepared radii. The caller owns continuation choices and the recipe's branch-base
and terminal-radius adjustments. The plan neither guesses parent IDs nor assumes
the bounded-depth axis numbering of the older recursive `PlantPlan`.
Degenerate/nonfinite/reversing intervals are refused. Forward derivative controls
are checked before publication. PCHIP preserves the supplied taper without overshoot.

Detail accepts1–16 subdivisions per source interval and3–64 radial sides, matching
the Python limb experiment. For `n` intervals, subdivision `a` and sides `s`:

```
stations = n*a + 1
vertices = stations*s + 2*(s + 1)
triangles = 2*s*stations
```

The two extra cap rings preserve hard cap normals in the native per-vertex format.
Capacity shortfalls return an error before geometry allocation. Station frames
must advance forward, sampled radius×curvature must stay below1, and every emitted
Float32 triangle must agree with all three corner normals. No partial mesh escapes.
These checks do not prove a globally nonintersecting or welded whole tree, a
continuous surface-error bound, or suitability of a chosen LOD.

## Independent replay

`NativePlantLimbReference.cpp` consumes explicit path requests and writes diagnostic
native-endian arrays on this little-endian host. This private probe format is not
an engine asset codec. Build it with the complete current compile-database flags
of `PlantLimbPlan.cpp`, linking `PlantLimbPlan.cpp`, `PlantLimbSurface.cpp` and
`PlantSurface.cpp`. Then, using NumPy/SciPy and the original saved growth cohort:

```sh
PYTHONPATH=lib/src <python> test/experiments/render/native_plant_limb_reference.py \
  --binary /tmp/<limb-reference> --growth-cohort /tmp/<growth-cohort> \
  --output /tmp/<fresh-limb-replay>
```

The driver restores the exact original seed-parent prefix and authored continuation
hints. It compares every station and triangle corner against the unchanged Python
`path_recipe`/`limb_mesh`, not a port-derived oracle. Float64 station tolerance is
1e-9; Float32 position/normal tolerances are2e-6m/1e-6. Positions at this source's
roughly18m extent have a Float32 spacing up to about1.91e-6m. Separate analytical
frustum, continuity, monotonicity, invalid-input and capacity tests run natively.

An initial mismatch exposed sampling on opposite sides of a C1 knot: second
derivatives may jump. The native grid now uses the same endpoint-fixed uniform
knots and right-hand knot selection as the reference; tolerances were not enlarged.

The complete authored seed682 tree has5472 source nodes,1301 limbs and maximum65
anchors per limb. At2×8 detail it produces121362 vertices and195888 triangles.
Largest observed Float32 position/normal differences are9.54e-7m/2.99e-8;
Float64 station differences stay below9.37e-12. One local process measured about
0.34ms preparation and4.33ms surface construction across all1301 limbs. These
exclude parsing, output, cache, leaves, renderer and world startup; no p99 or
A18-Pro frame-budget claim follows. Mesh streams request
`121362*24 + 195888*12 = 5263344B`, excluding allocator/driver storage.

The other three original cohort graphs contain a69- or73-anchor limb. Both paths
refuse these complete products under the existing65-anchor contract. They are
recorded as rejected, never silently cropped or counted as successful full trees.

## Actual Metal image check and remaining work

`NativePlantLimbPortrait.cpp` renders the native and independent Python products
through the existing native `SceneRenderer`: one opaque wood instance, fixed
material/light/camera,960×1080 whole and near images. Its bounded diagnostic reader
accepts either indexed native vertices or expanded Python corner normals.
It takes the product path, whole PNG path and near PNG path; run from a native
build checkout containing its shaders. Four actual images were opened and compared
with `test/scripts/pixels.py`. Generated data, commands and images stay in system temp.
Both matched views have zero differing pixels out of1036800; this is a two-view
geometry-port control, not a new image-quality or world-coverage certificate.

The main limbs are continuous; separate side branches still meet sharply. The
material has no bark detail, thin branches alias, and there is no foliage, wind,
joint union, shadow comparison or near/far transition.
[Stored wood plans](native_plant_limb_assets.md) now restore the prepared coefficients.
Projected demand, demanded original crown groups and registration in the world
remain to integrate. No production Place or README image is replaced by this probe.
