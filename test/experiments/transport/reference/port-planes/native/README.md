# Fixed native port-field reference

Owner: [#192](https://github.com/Weichwolf/outshine/issues/192), reference delivery
from #177 / PR181. These are immutable expected answers extracted from the
already verified **installed** plan; no new solve, mesh refinement or rendering.
The original captured inputs and source identities remain in the parent folder.

`Vienna-pr181-fields.bin` is 5,093,952 bytes. SHA256:
`9b93a2a12bf2afc49916d2183a21cd36190f3ceeab84b69968cc69a7c506da09`.
`Vienna-pr181-fields.json` binds every array offset/type/shape, original input
hashes, verified saved-plan/report hashes, library kernels and extraction source.
Original plan generation: `e76d25da11d945e929d7d8d14b7ba727c80f6055`;
extraction source: `626a447c9`. The plan's whole-world, driving and native-device
acceptance remains separate. This fixed input does not endorse a new solver fit.

## Binary layout

The first eight bytes are ASCII `OSPTREF1`. Remaining sections are tightly packed
C-order arrays, each beginning at a byte offset divisible by eight; padding is
zero. JSON `arrays` maps **names** to absolute byte offsets, byte lengths, dtype
and shape. JSON object iteration order has no meaning. Types are exactly
little-endian IEEE-754 Float64 (`<f8`) and unsigned 32-bit indices (`<u4`).
An empty shape denotes one scalar. There is no compression or platform-native
structure layout. Decode by offset and type; do not cast an unaligned struct.

| Array | Shape | Meaning |
|---|---|---|
| `plane_origins` | 1536×2 | Original affine-parent XY origins, metres |
| `plane_coefficients` | 1536×3 | Relative height, X gradient, Y gradient |
| `plane_ids` | 3317×2 | Two parent indices per original module |
| `cut_centres` | 3317×2×2 | Left/right whole-cut XY centres |
| `inward_normals` | 3317×2×2 | Actual unit inward cut normals for blends |
| `scales` | 3317 | Positive metric patch scales |
| `datum_m` | scalar | Add to relative field heights |
| `road_xy` | 30256×2 | Original indexed road XY vertices |
| `source_faces` | 31780×3 | Original road-vertex indices |
| `source_face_ids` | 31780 | Original face IDs, exactly 0…31779 |
| `face_parts` | 31780 | Original owning module ID per face |
| `module_ids` | 3317 | Original module IDs, exactly 0…3316 |
| `part_roles` | 3317 | Captured role: 0 corridor, 1 junction |
| `grade_frames` | 31780×2×2 | Captured normalized-gradient matrices |
| `face_points` | 31780×2 | Exact XY arithmetic centroid of each face |
| `face_values` | 31780×3 | Absolute height, X gradient, Y gradient |
| `face_bounds` | 31780×2 | Whole-face normalized-gradient / chord bound |
| `port_ids` | 3888 | Original whole-port IDs, exactly 0…3887 |
| `port_edges` | 3888×2 | Original road-vertex indices of whole ports |
| `port_parts` | 3888×2 | Modules on the two sides of each port |
| `port_points` | 3888×3×2 | Endpoint 0, arithmetic midpoint, endpoint 1 |
| `port_values` | 3888×2×3×3 | Port, module side, point, value channel |

All coordinates use the unchanged frame
`Vienna-ENU-48.21N-16.37E-Z-mapterhorn-as-supplied`.
Gradients are metres/metre in local X/Y. Answers are **absolute** heights;
coefficients are relative to `datum_m`. For a parent, evaluate
`P=c0+cx*(x-ox)+cy*(y-oy)` before adding the shared datum. The two-parent
rational construction and its positive-denominator domain are documented in
[the parent README](../README.md). Equal parent IDs mean one affine plane;
unused blend-cut normals on such modules need not be unit vectors.

`face_bounds[:,0]` bounds `norm(grade_frame @ gradient)` everywhere on the
original triangle. `face_bounds[:,1]` bounds the absolute height difference from
the linear interpolant of analytic heights at that triangle's corners. These
are conservative Bernstein certificates, not exact extrema. They refer to
**original coarse source triangles**, so they may exceed the accepted final
mesh's 0.01 m chord target; the refined finite mesh is deliberately not copied.
All 3317 modules are represented by at least one complete source face.

The 23,328 port-side answers bind both module sides at all three whole-edge
points. Channels always mean absolute height, X gradient and Y gradient.
Suggested Float64 replay tolerances: heights 1e-8 m, gradients 1e-10;
certificate values absolute/relative 1e-9. Whole-port C1 checks use height 1e-7 m
and gradient 1e-10. Numerical equality here is an analytic field statement:
actual finite contacts still require an explicitly selected mesh/SurfaceSampler.
No terrain C1, automatic tier selection or full OSM generator is certified.

## Read-only replay

From the repository root, with the external experiment Python environment:

```sh
PYTHONPATH=lib/src:test/experiments/transport python3 test/experiments/transport/port_plane_native_reference.py verify
```

The verifier checks fixed hashes, original source/module/face/port order and XY,
all 31,780 face answers/certificates, both sides of all whole ports and independent
centroid inclusion in the stored whole-gradient bounds. It never writes pins.
Only the explicit `export --source <verified-installed-plan-directory> --output
<empty-external-directory>` command extracts a new candidate. Normal tests do
not run that command. This fixture has no SDK dependency or foreign code.
Vector attribution: © OpenMapTiles · © OpenStreetMap contributors; elevation:
Mapterhorn. The original capture receipts and full source ledger are in the
parent fixture, not regenerated by this extraction.
