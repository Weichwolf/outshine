# Native urban field control

Owners: #1102 / #1105. A shared indexed box is the native opponent for aggregate
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

Reports retain exact commands, source/archive hashes, device, field/matrix/buffer
counters and encoded geometry draw calls. Triangle references describe the box
input, not measured shader invocations. Buffer counters are not
process/driver/unified-memory peaks. The fixed-light model scene
contains no world acquisition, real terrain, atmosphere, vegetation or simulation
load. It does not establish a complete-scene device budget or a LOD error bound.
