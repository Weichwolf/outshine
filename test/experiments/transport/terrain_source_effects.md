# Explicit terrain effects on transport body undersides — #1215

`terrain_source_effects.qualify_effects(plan, body_data, declarations)` qualifies
an already verified indexed body. Each retained local `source_id` has an explicit
generator declaration: `effect`, `contact_key`, `clearance_m`, `relief_m`, `recipe`.
Repeated provider identities do not identify independent parts or instances.
Declarations preserve original source facts and record `origin=generator_declaration`.
Height order, XY overlap and source layer are never used to infer intent.
Missing/unknown/Unspecified roles reject locally; NoEarthwork is explicit.

The native `SurfaceGroundSource` contract in `SurfaceEarthworks.h` and
[native earthworks](native_surface_earthworks.md) owns effect semantics:

| effect | actual cap-face target | resolution on the same indexed face |
|---|---|---|
| GroundContact | body underside, offset 0 | nonzero UInt64 contact key; ground wins; conflicting ground keys reject; maximum ground relief |
| OverheadClearance | underside minus clearance, ceiling only | maximum clearance and clearance relief; resolved key 0; never a fill request |
| NoEarthwork | none | leaves terrain unchanged |

Priority applies only to owners of one actual indexed bottom face. Independent
sheets at identical XY remain independent. Clearance/relief are finite and
nonnegative. Original source records remain untouched, including unused values.

The selected output arrays are `body_cap_face_ids` (UInt32), `effects` (UInt8,
native enum order), `contact_keys` (UInt64), `height_offsets_m` and `relief_m`
(Float64). IDs address `bottom_faces` in `body_vertices`, including thickness.
No position gather, second height fit or DEM stamp is generated. NoEarthwork
contributes no target array entries. Source declarations remain available.

`shared_products(..., terrain_declarations=...)` includes this optional table as
JSON under `terrain_effects`. The default and all existing reservations, body,
navigation and external profiles retain their existing values. Reservations
referencing `reference_vertices` remain unqualified reservations, not DEM stamps.

The per-call mask table resolves each repeated ownership word once; direct
per-face resolution (`reuse_masks=False`) is an exact comparison path, using the
same role operator and quality requirements. Neither path caches geometry or
retains state across calls. At most 64 sources and 8192 cap faces (native default)
are accepted. Budget/cancellation and all invalid roles reject without publishing
a partial result or mutating inputs. Geometry validity is the upstream body
contract; this qualifier checks index/mask addressability.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B -W error \
  test/experiments/transport/compare_mvt_terrain_effects.py \
  --case crossed --repeats 21 --render \
  --output /tmp/outshine-terrain-effects-crossed
```

Existing original/stacked/crossed bodies are the only inputs. The driver declares
controlled lower support and upper clearance recipes; these are not new provider
facts or generally approved source-role inference. It exports existing body arrays,
selected effect arrays, declarations/products and scoped costs. Optional plots
show actual Float32 underside geometry and offset targets, not a deformed DEM.
The reported work counts logical owner incidences and actually resolved bits.
Selected array bytes/zero position-copy bytes exclude Python list/dict storage,
JSON expansion and total allocations. Separate tracing includes role-only Python
allocations; process RSS also includes imports and the prepared body.

This is a small existing-consumer contract, with no new geometric algorithm or
additional research-paper claim. Parent geometry sources remain in #1003/#1189
and the existing transport review. Commit-specific tests, actual image review,
costs and limitations belong in the PR. Real DEM deformation, tunnel openings,
native generator roles, cache/World and device admission remain separate work.
