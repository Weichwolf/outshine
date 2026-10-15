# Bounded shared graminoid tuft meshes

```python
from outshine_ref.vegetation.graminoid import GRAMINOID_BLADE, graminoid_tuft_mesh

mesh = graminoid_tuft_mesh(seed=17, blades=12, segments=8)
```

This is a seeded, unidentified grass-like form family for offline reusable
prototype construction. It is not a botanically accepted species, whole meadow
planner, grass LOD system, wind model or runtime cache manager. #22 owns physical
organs; #208 owns actual source-landcover binding and Place image comparison.

The immutable base `GRAMINOID_BLADE` is the existing folded grass recipe from
the organ experiment. `graminoid_tuft_mesh` composes analytical `Blade` ribbons
in metres with seeded length/width/bend/twist, upright lean/azimuth and small
root offsets. Its own authored ranges are0.25..0.48m length,6..12mm width and
8mm base embedding, without a claim of measured morphology or ground conformity
at every offset root on steep terrain. Twelve sparse blades do not represent a
complete turf patch or ecological stocking density.

Inputs: Python uint64 seed, integer1..64 blades, integer2..1024 longitudinal
segments. Transverse resolution is the two folded sides. Returned caller-owned
NumPy arrays: Float64 `vertices` (N,3), Int32 `faces` (F,3), Float64 `normals`
(3F,3) in face/corner order, Float64 `uv` (N,2) and UInt8 `blade_index` (N,).
For B blades/S segments, N=B(3S+1), F=B(4S−2); the pointed tips have no zero-area
triangles. Maximum196,672 vertices/262,016 triangles follows from these bounds.
This is an allocation bound, not a latency/frame/device guarantee.

Each blade's normal/width/fold comes from the same analytical surface used to
construct its positions. Standard SciPy rotations preserve proper frames.
Original blade indices and seeded parameters do not depend on tessellation;
shared sample positions agree to Float64 rounding. Extending blade count keeps
the existing prefix exactly. No global mutable RNG or camera/frame input.
PCG64 is explicit; persistent assets store generated geometry and producer
version rather than depending on a future dependency version's random replay.
The generator does not decode untrusted runtime assets.

## Measured consumer and limits

The actual-source experiment uses eight shared12-blade/360-triangle variants,
2,880 triangles in the complete catalog. Build on frozen `86e5b9b94`:0.136s
inside the builder; standalone process0.667s wall/0.619CPU-s/72.45MiB peak RSS.
All arrays from all eight promoted-library variants equal the rendered source
product. These costs are local CPU evidence for a small authored catalog only.

In the complete Koerbersee plan,9,853 source-grass sites use these forms instead
of the old height-based conifer proxy. The other43,347 tree sites keep their
prior form. All53,200 IDs, contact positions, frames, source indices and scale
variation remain exact. Existing physical view/sun bounds request1,103 camera
and1,313 shadow grass sites; their union uses one shared geometry instance per
site rather than duplicate camera/sun sheets. Shape counts/density are authored,
not observed ecology. The cached export forbids `Blade.mesh` and still succeeds.

The actual2560×1440/one-sample before/after images were opened: inappropriate
trees over the source-grass hills are gone. Eight cached forms were also opened
at2560×1440/four samples in the private near control with reconstruction
forbidden. Source geometry/ancestry tests, original blade controls and actual
images support this construction, not botanical/SpeedTree/LOD or native/device
acceptance. Coarse terrain/materials, sparse grass stocking, matching grass
far products, habitat/species recipes and wind remain open.

Current evidence lives under `/tmp/outshine-vegetation-place-grass-86e5b9b94/`
and `/tmp/outshine-vegetation-grass-near-38a2f8690/`. The
[Place experiment report](https://github.com/Weichwolf/outshine/blob/fc06c4182/test/experiments/vegetation/place_grass.md#completed-actual-source-and-near-inspection)
owns procedure/source references and full Place costs. Ten source library tests
and ten fresh installed-wheel tests pass; all library module origins are from
that wheel, and all eight rendered variant arrays match byte-for-byte. The
compact proof is `/tmp/outshine-vegetation-graminoid-ad099345c/gates.json`.
Native transfer is separate (#48); finished catalog assets may be shared by
many world instances and cached without per-instance blade reconstruction.
