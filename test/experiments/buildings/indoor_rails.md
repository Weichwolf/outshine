# Local indoor handrails and opening guards

[#1565](https://github.com/Weichwolf/outshine/issues/1565) extends the
[joint stair product](indoor_stair_batch.md) without changing its bodies,
plates, source ledger, twelve whole ports,73 floor corridors or30 old XYZ
contacts. Six physical stairs retain seven original source identities.
Inputs are the same checked offline MVT/DEM snapshot; no new packages.

## Recipe and ownership

`indoor_rail_recipe.py` derives two continuous planar handrails per stair from
the actual chosen floor heights, flight counts, treads and landings. The rail
centre is1m above each nosing; the landing transition starts0.3m before the
next flight. The generated path is physically outside the reserved walking
width. Height over an individual tread therefore varies with the chosen rise;
this is an explicit plausible recipe, not a building-code interpretation.

`indoor_rail_geometry.py` emits sealed hollow40×40mm rectangular sections with
2mm sidewalls and resolved planar miters no sharper than ninety degrees. Check
simple finite paths, nonoverlapping thick bands, exact geometric edge pairing,
outward orientation, positive volume and the analytical material volume.
Closed loops have no caps; open paths have real outer/inner sealed ends.
Arbitrary spatial sweeps are unnecessary here. Existing building-prism
construction is reused for brackets; no vegetation or new solver dependency.

`indoor_rail_clearance.py` clips against all73 floor and285 stair traffic
prisms, including branches and endpoints. A40mm radial envelope plus2mm real
clearance bounds every permitted miter. Inflate the polygonal round buffer
by`1/cos(pi/64)` so its arc chords stay conservative. No membership epsilon
releases blocked material. Default limits bound1024 reservations/65536 queried
candidates and1024 sweep stations. The caller supplies one common source frame;
reject inconsistent stair/floor frames before rail construction.

`indoor_rail_supports.py` snaps supports onto actual flat tread/landing domains.
Maximum support pitch is1.2m and end cantilever0.25m. Below-tread40mm brackets
embed60mm into the side; reserve the inner30mm as whole positive-volume contact
inside the actual tread domain. Their depth must fit the actual solid waist.
Post/bracket envelopes also avoid traffic. Height-scaled post templates retain
2mm sidewalls; sealed end thickness is the explicit0.002fraction of post height,
not falsely a constant2mm cap.

`indoor_rail_guards.py` offsets each actual opening outward into its final host
plate and removes traffic portals. A real0.1µm generated-path simplification
removes numerical outline stations before clipping; the plate/opening is
unchanged. Terminal stubs below82mm are removed and pieces below150mm omitted
as explicit recipe resolution. Guard posts require their whole footprint and
80mm embed to fit the actual remaining plate. Open bar/post guards have no
infill or child-safety/statics qualification. Later wall/room/access context
and native functional collision remain separate work.

`indoor_rails.py` composes candidates and preserves per-stair aliases/reasons.
An unsolved rail/support/opening leaves its enhanced stair locally pending,
without removing the valid base stair or independent cases. Shared openings
propagate incomplete ownership locally. Foreign source/World topology is not
inferred. All arrays are caller-owned; construction is offline bounded Python,
not a frame callback, async scheduler or native cache product.

## Actual output and equal-quality cost

`indoor_rail_checks.py` independently checks every emitted closed mesh and
complete filled outer convex envelopes of sweep segments/support bodies.
Clip each small hull at every relevant height band before projecting it;
all vertex-pair crossings include every hull edge. Require each envelope
inside the union of source parts active in that band and outside every traffic
prism interior. This is conservative for hollow material and retains real source
gaps, holes and vertical separation. Common-frame supplied context only.

- `eager` independently constructs/checks every full post and bracket prism.
- `shared` constructs/checks two templates and returns finite positive affine
  transforms; materialized instances receive the same independent final checks.

The comparison checks exact rail/path/pose/ownership streams, bidirectional
actual vertex sets and positive material volumes within reported arithmetic
bounds. Different triangulations are allowed; it is not generic mesh equivalence.
Generation time includes common planning/traffic clipping and actual rail plus
template/full support construction. Full independent output qualification runs
after both methods, outside generation timing; source/floor/stair preparation,
plots/NPZ and profiler processing are separate. Report mesh constructions,
support vertices written, allocation requests/scoped peak, retained mesh/pose
bytes and process RSS. Total copied bytes and native/device/cache/p99 are unmeasured.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -m unittest discover -s test/experiments/buildings -p 'test_indoor_*.py' -q
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/buildings/compare_indoor_rails.py --output /tmp/outshine-research-indoor-rail-comparison --rounds 3 --trace --plot
```

Use a fresh explicit temp directory. NPZ stores actual rails/templates/poses
and base stair buffers; the JSON maps source owners and support kinds. Figures
show actual triangles, with the actual upper plate clipped/translucent only
for display. Never publish these local diagnostics into README/doc.

Primary context: Merrell/Schkufza/Koltun, SIGGRAPH Asia2010,§6/Fig.11 in
the [single-stair reference](indoor_stairs.md): geometric building components,
banisters and coupled access failures. Full text and Figures10–12 inspected.
The bounded clipping, hollow sweep and template comparison here are our own
derivation, not that paper's algorithm. No native/PBR/World/device acceptance.
