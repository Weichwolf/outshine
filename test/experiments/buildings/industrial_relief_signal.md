# Distant industrial facade material surfaces

Owner: [#523](https://github.com/Weichwolf/outshine/issues/523), parent #33.
This corrects the distant omission in [#519](industrial_relief.md): its geometric
position budget permitted removal of thin, visibly different metal surfaces.
The ordinary `ArticulatedDetails` path now retains existing front faces at their
actual stand-off and removes only sides when the displacement policy permits.
It preserves more of the measured material signal; it does not bound radiance.

## Representation and correspondence

All39 actual relief regions on eight Grohnde halls retain their closed physical
meshes, existing openings, source IDs and access plans. Front caps occupy the
first triangles in the complete buffers. `cap_prefix` returns immutable views
with the same positions, normals, metric UVs and metal slot10. No geometry
buffer is allocated or copied. These open surfaces are render products only;
independent solid checks reject them for collision. Their full physical owners
remain alive. Camera changes select existing products without generating meshes.

The actual outward depths are0.14/0.20/0.28m. Each side point corresponds to the
same boundary point at full extrusion depth. Displacement is at most that depth;
the retained front is an exact subset. For positive camera depth throughout the
extrusion, perspective maps each such segment to a monotone straight segment.
Its side-to-front screen displacement cannot exceed the wall-to-front endpoint
distance. The existing `displacement_error` bounds that distance over the wall
bounds and declared movement radius. If nearest depth is at most extrusion
depth, it returns infinity. The policy remains0.5px, without a relaxed threshold.
This correspondence establishes neither shading nor shadow agreement.

The alternative `direct_caps` emits the same six arrays/dtypes directly from the
logical region, with no sides or closed-body construction. It also supports the
explicit comparison that projects caps to0.004m off the original wall. That
alternative retains front normals, UVs and materials but moves their positions.
It is not the default. Complete asset/source/region preparation is outside this
direct-emission comparison; the regular prepared path keeps the closed body.

Six new controls cover all39 regions: exact direct/prefix streams, independent
material-aware surface distances, side-to-front membership and projected
displacement, prepared camera changes without mesh work, complete37-source
accounting/default equivalence, and the wall projection/invalid-input controls.
Independent Shapely region membership uses a float64 accumulation allowance;
it avoids treating a closest-triangle routine's near-zero distance residue as
exact membership. The independent spatial depth check remains separate.
Seven previous controls retain physical-solid/volume/UV, real aperture/ray,
neighbor and functional access checks, including moving gate/door states.

## Actual image comparisons

Frozen prototype `8001d2688` produced nine new actual Blender images, all terminal
exit0, at1920×1080, Eevee16 samples, llvmpipe LLVM19.1.7. All were optically opened.
Four earlier comparison records reuse three actual images only after exact
mesh arrays/dtypes, camera, used material bindings and palette checks. Unused
bindings in the earlier near export are not required to match. A software
backend and the inherited post-success bpy exit workaround prove no GPU frame
budget or normal teardown. No images or server are published.

The complete original37-source tile, light, material definitions, ground bounds
and projection remain the same within each paired camera comparison. `Far`
moves the same eye four times away from its target. Other existing detail
selection changes between cameras, so totals must be compared within each row.

| Camera/product | Triangles | Changed pixels vs full /2073600 | Worst channel /255 | Mean RGB /255 |
|---|---:|---:|---:|---:|
|Base full|88649|0|0|0|
|Base previous omission|88235|215|44|0.001162|
|Base retained front, default|88401|130|15|0.000199|
|Base projected wall|88401|208|32|0.000570|
|Far full|60982|0|0|0|
|Far omission|57134|1666|33|0.003471|
|Far retained front, default|58646|1433|24|0.001269|
|Far projected wall|58646|1616|27|0.002123|

Retaining the original stand-off wins both measured comparisons. It still
differs from full sides and shadows. Forcing caps in the actual near view
changes15865 pixels, worst54/255; forcing them at grazing angle changes140215,
worst105/255. Automatic selection in both views is array/dtype/binding/camera
exact to full11810-triangle geometry. These are deliberate negative controls,
not acceptable near products. Generated forced-wall near/grazing models were
not rendered. No general time/angle/night/shadow or temporal-switch bound follows.

Final default integration `4bf5a7598` replays all16 prototype model streams,
dtypes, cameras, bindings and source content exactly. Only the three existing
shell/plan/assembly timing diagnostics differ. Nine image hashes are rechecked;
the prototype render commit remains authoritative, with no new raster claimed.
The13 affected source tests passed in33.745s before the compatible master merge;
the same13 installed tests passed in35.180s on the frozen integration. All198
library modules are byte-exact to the installed wheel, SHA-256
`9aaf486c0f5dc793fe0c8ce28c316156b5ea6997cab5db3d737a3445e49e0179`.
No library, provider, reservation, native or material/shader API changes.

## Scoped cost comparison

Final `4bf5a7598`, normal priority on a shared four-logical-CPU host; nice10
model/verification jobs ran concurrently. Load averages11.80/10.96/10.65.
Twenty-one subsequent calls, first call separately recorded, and a separate
traced-peak call compare exactly equal outputs from different prepared inputs:

| Actual region | Cap triangles | Prefix median ms / peak bytes | Direct median ms / peak bytes | Direct final bytes |
|---|---:|---:|---:|---:|
|paired-bays|14|0.0068 /1064|1.2924 /19120|3892|
|portal-bays|74|0.0068 /1064|4.2378 /101852|20572|
|ribbon-bays|26|0.0086 /1064|1.9262 /35408|7228|

Prefix views allocate Python/ndarray metadata but zero new geometry buffers or
copied geometry bytes. They retain14456/70056/25576 bytes of their already
prepared physical owners, respectively. The direct path creates six final
buffers, excludes body/source/region preparation, and retains only its output.
Neither comparison is an end-to-end cache, total allocation/traffic, GPU or
native result. Scoped tracemalloc peaks exclude parts of the native heap.

All39 reliefs use3848 outer triangles or1512 front-cap triangles:2336 fewer
triangles when all use caps. Full+outer unique geometry storage remains1490080
bytes with all caps added. Thus this is a draw-work reduction with no retained
physical-memory reduction. Far whole-tile savings are2336/60982 triangles;
general complete-building LOD/cache admission stays open under #33/#500.

## Reproduction and references

Run on a frozen clean tree with `.venv` and explicit system-temp destinations:

```sh
export PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings
.venv/bin/python -m unittest verify_industrial_relief_signal verify_industrial_relief -q
.venv/bin/python test/experiments/buildings/industrial_relief_signal_probe.py --output <system-temp>/models
.venv/bin/python test/experiments/buildings/industrial_relief_signal_costs.py --output <system-temp>/costs
.venv/bin/python test/experiments/buildings/blender_city.py <system-temp>/models/GrohndeSignalBaseFront-geometry.json --output <system-temp>/renders --samples 16
```

Compare actual PNGs with `test/scripts/pixels.py`. Private receipts, exact replay,
inputs and rasters are under `/tmp/outshine-buildings-relief-signal-output/`;
the final cost report is under `/tmp/outshine-buildings-relief-signal-final-costs/`.
The [original provider fixture](data/provider/grohnde/README.md) needs no network.

[Simplification Envelopes, SIGGRAPH1996](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf),
p.1 optically inspected, motivates separating geometric bounds from appearance.
[Grammar-based Encoding of Facades, EGSR2010](../../../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf),
p.2 optically inspected, separates compact region/material/displacement encoding
from full geometric expansion. This experiment does not implement F-shade.
[Existing SDK/Ghidra evidence](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md)
identifies top/side face and side-UV mechanisms in SDK3.4.12206 extrusion;
targeted export `004b5210` was reread locally. It supplies no ready-made far
facade recipe, proprietary source, executable image oracle or CityEngine parity.
