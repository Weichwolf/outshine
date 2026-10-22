# Native urban field control

Owners: #1102 / #1111. A shared indexed box is the native opponent for aggregate
urban masks. This is a model-space renderer/control, not the world integration.

Requires a fresh native product archive, compilation database and shader bundle.
Builds one experimental unit with the complete Engine.cpp flags; uses the existing
Metal/Vulkan renderer, MR material path and asset service. No extra dependencies.

```sh
python3.12 test/experiments/render/native_urban_field.py --output "$(mktemp -d)"
python3.12 test/experiments/render/native_urban_field.py --output "$(mktemp -d)" --validation
python3.12 test/experiments/render/native_urban_field.py --output "$(mktemp -d)" --field ready.field --representation field
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

Reports retain exact commands, source/archive hashes, device, field/matrix/buffer
counters and encoded geometry draw calls. Triangle references describe the box
input, not measured shader invocations. Buffer counters are not
process/driver/unified-memory peaks. The fixed-light model scene
contains no world acquisition, real terrain, atmosphere, vegetation or simulation
load. It does not establish a complete-scene device budget or a LOD error bound.

## Iris Xe mask comparison

The resolved mask candidate loses against boxes on the i5-1135G7/Iris Xe.
It remains an experimental native comparison, without World integration or a
recommendation to replace instances. Both modes use the same MR material, light,
camera and actual caster pass. Colour culls backs; opaque casters cull fronts.
The field path uses explicit 48-byte vertices, R32Float heights at 2/12 and a
48-byte std140 block at 3/2. Colour and depth/casters share `fieldMask.glsl`.
Categorical descriptors and the exact stored roof height are flat varyings.

Ready input: Research91a9ed4d8, SHA256
`2ae64ca9fca45372227a93025e1763272d6a250c8ddb81e6340dd4d372304cbd`,
72×72 cells, 25.994770136968626 m pitch, fill 0.75, 2,795 masses.
Normal A/B/B/A warm processes each render 60 measured frames/360° at 1280×720,
one sample per pixel and no TAA. The complete-patch and bounded-envelope variants
use separate preserved binaries. Instrumented query runs measure work separately;
their timings are excluded. Native work includes the common floor.

| Representation | GPU p50, ms | Lit triangles/60 | Vertex calls/60 | Fragment calls/60 |
|---|---:|---:|---:|---:|
| Box instances | 1.348–1.376 | 2,012,520 | 4,025,040 | 44,300,396 |
| Complete patch planes | 2.094–2.101 | 281,040 | 562,080 | 151,762,124 |
| Occupied envelopes | 1.797 | 275,760 | 551,520 | 96,613,012 |

The bounded variant limits each wall to its row/column maximum and each roof
level to its occupied XY bounds. Empty rows/columns emit no walls. This reduces
fragment calls by 36.3%, preserving the occupancy predicate, but remains about
31% slower than boxes. Reduced vertex work alone does not justify this path.
Requested field buffer/height payloads are 526,536 and 517,032 bytes respectively;
these exclude the common floor, samplers, driver allocations and process peaks.
The box piece counter is 403,480 bytes including its floor; these are distinct
counters, not a complete memory comparison.

315 CPU geometry checks and 138 native Vulkan checks cover exact roof levels,
partial/empty patches and independent ray/box depth intersections in six axis
views with both culling directions. Core/Sync validation passes. Cold/source-free
warm processes preserve package bytes and each variant's PNG/D32 exactly.
Different representations are not pixel-identical: Float32 geometry/raster/shadow
edge differences remain explicit; the analytical fixture does not prove arbitrary
poses, terrain placement, moving light or a whole-world quality bound. Source,
commands, frozen commits and terminal receipts are linked from #1111/the PR.

## Logical heights and terrain placement

`field_height_coordinates.py --output <empty-system-temp-directory>` uses the
research NumPy/Pillow environment to illustrate #1111's World boundary. A fixed
field-origin subtraction handles datum shifts but cannot undo spatially varying
terrain/curvature displacement. Logical mask coordinates must survive placement.
Float32 point classification and a 2D wall-section comparison cover flat, sloped
and parabolic reference cases. The parabolic interpolation bound applies only to
that analytic reference; this is no native shader, DEM, upright-building or World
qualification. Images remain local, not pins or a presentation gallery.
