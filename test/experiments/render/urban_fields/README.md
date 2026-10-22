# Native urban field control

Owners: #1102 / #1111. A shared indexed box is the native opponent for aggregate
urban masks. This is a model-space renderer/control, not the world integration.

Requires a fresh native product archive, compilation database and shader bundle.
Builds one experimental unit with the complete Engine.cpp flags; uses the existing
Metal/Vulkan renderer, MR material path and asset service. No extra dependencies.

```sh
python3.12 test/experiments/render/native_urban_field.py --output "$(mktemp -d)"
python3.12 test/experiments/render/native_urban_field.py --output "$(mktemp -d)" --validation
```

`--field` reads text: `columns rows cellM fillFraction`, then row-major top heights
in metres. X follows columns, +Z follows rows, +Y is up. Zero means no column.
The maximum is 256×256 cells. Default cases are synthetic scaling controls and an
empty field; they are not equal-quality source reconstructions. `--radius` changes
camera distance without changing the stored field or asset identity.

Each fresh process opens the prototype DB, loads the native field and prepares
instances. The cold process imports the field; the warm process receives no input
file. Package bytes, final PNG and D32 must remain equal. Three settling frames
precede exactly 60 measured moving-camera frames/one orbit; readbacks add no render
frames. GPU submission intervals and host frame times are separate. Median uses the
two middle samples; nearest-rank p95/p99 use ceil(p·60), so p99 is the maximum.
Validation explicitly requests the backend's validation mode; normal runs remove
the Metal validation/profiling environment. Validation runs are functional evidence.
The scene declares an actual shadow frame. Fixed world/light may reuse its initial
shadow during the orbit; no steady-state caster work is inferred from the setup.

`--sequence verify` adds four source-free functional runs to the cold/warm orbit:
`relight` changes the sun bearing at frame 30, `replace` uploads boxes with half
their previous heights and then releases the old piece. `relit` and `replaced`
start from those final states. Each transition's final PNG/D32 must exactly match
its fresh counterpart. Light preserves primary depth; a nonempty replacement must
change it. The baseline image must respond to changed light for a nonempty field.
All package keys/bytes/CRC remain unchanged: these controlled runtime products do
not alter their source asset. Empty fields remain valid without uploading boxes.

The submission hook records the actual preceding completion. A zero-wait Poll
immediately before release reports whether the old frame is still Pending; absence
of an observed pending fence leaves that lifetime case unproven. This is diagnostic
coverage, not a timing gate. Mutation-run timings include runtime preparation and
must not be used as steady-state FPS evidence. Exactly 60 measured frames and three
settling frames remain explicit for every process; readbacks add no render frames.

```sh
python3.12 test/experiments/render/native_urban_field.py --output "$(mktemp -d)" --field ready.field --sequence verify --validation
```

Reports retain exact commands, source/archive hashes, device, field/matrix/buffer
counters and encoded geometry draw calls. Triangle references describe the box
input, not measured shader invocations. Buffer counters are not
process/driver/unified-memory peaks. The fixed-light model scene
contains no world acquisition, real terrain, atmosphere, vegetation or simulation
load. It does not establish a complete-scene device budget or a LOD error bound.

## Logical heights and terrain placement

`field_height_coordinates.py --output <empty-system-temp-directory>` uses the
research NumPy/Pillow environment to illustrate #1111's World boundary. A fixed
field-origin subtraction handles datum shifts but cannot undo spatially varying
terrain/curvature displacement. Logical mask coordinates must survive placement.
Float32 point classification and a 2D wall-section comparison cover flat, sloped
and parabolic reference cases. The parabolic interpolation bound applies only to
that analytic reference; this is no native shader, DEM, upright-building or World
qualification. Images remain local, not pins or a presentation gallery.
