# Whole raised rail hardware

Owner #1814, Parent #434, delivery #1820. Python reference only. Reuses the
existing installable `outshine_ref.transport.rail_hardware` recipe from #250/#376;
no new solver, native implementation or source/routing authority.

## Complete inputs and physical chains

The input is the independently checked whole115-ground-source consumer snapshot,
retaining its three raised decks,94piers,69buildings and four whole SITE plants.
`bound_input` checks the actual building receipt, artifact pins, source attributes
and shared frame before construction. It uses the selected finite bed facets,
not an infinite plane or an expanded road approximation. The original source IDs
and MVT endpoints remain attached to every original segment.

Three sources form two maximal physical chains, using only existing constructed
endpoint pads from the qualified raised geometry:

- `openfreemap:14/8937/5678:transportation:4935280960:0` and
  `openfreemap:14/8937/5678:transportation:81129490:4` share a physical pad.
- `openfreemap:14/8937/5678:transportation:81129490:5` remains independent.

This creates no original OSM/routing node identity. Coincident XY alone never
joins sources. Direction reversal preserves the complete original segment/source
mapping. The constructor receives all original stations at the joint; it does not
cap each input separately and leave overlapping end sections. The checked axes
contain six/four stations and622.717/505.514m, total1,128.231m.

Gauge1.435m is an explicit procedural assumption because these source tags do not
supply gauge. Source gauge, if later supplied, cannot be silently replaced. The
existing simplified twelve-point rail section is0.172m high, standing on0.18m
sleepers; running surface is0.352m above the retained bed. Sleeper spacing/size,
rail budget and the complete original recipe are recorded in the artifact.
Steel RGB/metallic/roughness and timber parameters are procedural PBR assumptions,
not observed source measurements. Defaults for all old renderer materials remain
metallic0/roughness0.86; the new optional material record only changes the hardware.

## Saved product, precision and ownership

Schema2 stores complete Float64 rail positions, UInt32 faces and Float64 Nx3x4
shared-sleeper transforms for each chain, plus one common unit sleeper template.
The product has240rail vertices/464faces and1,881sleepers, with no omitted source.
Material roles and the common object pose apply equally to compact/baked opponents.
The bed/deck/pier/ground/building/plant artifacts remain unchanged.

One common object origin `[0,0,168.85902944058444]`, identity axes, removes the
height datum from every local rail and sleeper transform. XY keeps its original
source coordinates. This is essential for the declared binary32 interpretation:
with absolute transforms the independently evaluated sleeper top differed from
the rounded rail foot by one ULP,1.52587890625e-5m. The preserved original world-
coordinate counterexample is not admitted as the corrected product. With local
height and one common origin, literal Float32 local contact and the declared
Float32 origin addition are exactly equal. This is a reference operation order,
not an assertion about unspecified GPU matrix/FMA execution.

Every whole Double footprint, rounded baked footprint and literal Float32
instance footprint is covered by the finite bed. Whole profile, manifold/winding,
gauge, lower contact, sleeper orientation/phase and same-kind nonintersection are
checked independently. All added volumes remain inside the previously reserved
flat transport volume. No old consumer is rebuilt to make room for the hardware.

The old subway reservation is5.7m above bed; railhead uses0.352m of that space,
leaving5.348m above the running surface. This is explicitly **not** a new vehicle,
clearance, load-bearing or train-function certificate. A native vehicle/profile
consumer must use the recorded running-surface offset and establish its own
clearance contract before integration; the old datum cannot be relabelled.

## Independent saved reader and negative controls

`elevated_rail_hardware_verify.py` reloads the complete Source/frame/consumer
binding and artifact hashes. Exact member inventory, aggregate32MiB expanded
payload, bounded numeric NPY headers and exact shape×itemsize/member payload
length are checked before NumPy array decoding. This follows the existing
prepared-prototype header-admission pattern in `stair_plant_prior.py`.

The original ZIP-size-only reader reached the decoder for a tiny truncated NPY
member claiming1,000,000,000×3 doubles. A safe instrumented regression replaced
the decoder with a failure instead of allocating that array; the old path failed
the predecode requirement. The fixed reader rejects the mismatched length before
the decoder. This is a real admission correction, not a measured giant allocation.

Seven separately saved/rehashed products are rejected by fresh CLI readers:
changed frame, changed whole axis, deformed section, moved sleeper instance,
changed shared template, wrong running-height claim and truncated huge header.
Their original inputs and rejection logs remain in system temp. The actual valid
producer4b56dd855 and fresh readera7c3e67a6 accept all three sources/1,881sleepers,
with0omissions and9,409pair candidates in each Double/baked32/instanced32 scope.
Maximum section/gauge/independent-instance residuals are below1e-12m; literal
Float32 contact error is0. Those finite controls grant no native Source/World,
logical routing, vehicle, shadow/simulation LOD, device or UMA authority.

## Equal-geometry costs and actual pictures

Frozen4b56dd855 on i5-1135G7/Linux, seven alternating normal-priority pairs;
load4.15/4.88/6.46. Both representations own their arrays and reconstruct exactly
the same complete geometry. The baked opponent is the public vectorized library
bake, not a scalar loop. Template bytes are counted once in compact payload.

| Scope | Shared instances | Baked meshes |
| --- | ---: | ---: |
| Median process CPU |0.175ms|3.983ms|
| Complete geometry payload |192,240B|643,344B|
| Mesh constructors |2|4|
| Allocation events |231|568|
| Requested allocation |210,350B|1,686,522B|
| Scoped heap high watermark |196,600B|932,728B|
| Known Mesh copies |11,328B|643,344B|
| Explicit transform copy |180,576B|0B|

Three full construction calls from already held qualified inputs cost
101.4–108.5ms CPU (median101.6ms), each four Mesh constructors and22,656B of
known Mesh copying. Input/receipt validation, original preparation, serialization
and rendering are outside representation timings. Memray traces are separate;
held inputs precede their peaks. Total NumPy copies, physical traffic, cold IO,
process/UMA peaks and native/frame times remain unproved. Shared instances win
this equal-geometry reference comparison; keep them as the saved representation.
The later reader-only preflight change is outside the timed scopes and leaves
the measured construction, representation and rendering code unchanged.

The actual whole scene and the shared-pad detail were rendered before/after at
1280×720 with sixteen EEVEE samples on Mesa Intel Iris Xe, not Cycles or native
Outshine. All four PNGs were opened. Hardware rests on the retained decks and
the visible track/sleeper rows remain continuous. The no-hardware whole control
matches the prior115-source whole image exactly in every RGB pixel; PNG metadata
differs. The helper's optional PBR fields therefore preserve this old control.
The diagnostic bake exists only for these images; the saved product stays compact.
No image is published under doc/README or a replacement gallery.

## Reproduction and architecture acceptance

Use the existing .venv, GEOS/Shapely, NumPy, existing rail/input dependencies and
Blender5.2.2 bpy for diagnostics. Memray is only needed for allocation evidence.
PYTHONPATH includes lib/src and test/experiments/{render,buildings,transport,assets,
vegetation}; OPENBLAS_NUM_THREADS=1 and OMP_NUM_THREADS=1. All CLI output paths are
explicit fresh canonical host-temp paths through the common experiment_output
helper. Exact input roots/receipt hashes and commands are recorded in #1820.

```sh
.venv/bin/python -B -m unittest elevated_rail_hardware_checks rail_hardware_checks \
  tilted_rail_checks rail_growth_checks rail_occupancy_checks \
  rail_occupancy_state_checks stair_replay_append_checks test_experiment_output
.venv/bin/python -B test/experiments/transport/elevated_rail_hardware.py --help
.venv/bin/python -B test/experiments/transport/elevated_rail_hardware_verify.py \
  "$products" --output "$fresh_read"
.venv/bin/python -B test/experiments/transport/elevated_rail_hardware_cost.py \
  --products "$products" --receipt "$receipt" --output "$cost" --profile
```

Architecture acceptance: recipe/constructor remains in the existing library;
these experiment modules bind finite whole inputs, independently read the saved
physical product, measure equal representations and append a diagnostic body.
The shared renderer only gains optional explicit metallic/roughness parameters
with old defaults preserved. No new library dependency, solver/cache/scheduler,
native implementation or source topology is introduced. Old consumers remain
bound by hashes and reserved-volume containment. Local rejection keeps the valid
bed and logs complete omitted sources, following the existing partial-product
contract. Native generation, logical running profile, interaction and World
integration remain distinct future consumers, not implicit acceptance here.
