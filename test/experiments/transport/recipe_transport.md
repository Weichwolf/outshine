# Real Transport consumer of the isolated recipe pilot (#483)

This consumer uses the common experimental kernel from #494 / PR #497, without
changing it. Two portable definitions compose the existing width-aware profile
solver and the actual closed-body builder/physical reader. Source IDs, positions,
neighbors, priors and selected contacts are instance data. There is no recipe
per road, private interpreter, context index, persistent cache or native/lib API.

`transport-profile-network-v1.json` exports a plan. The checked-network definition
loads that exact-version dependency and adds the body operator. Only the requested
ports execute: plan demand triangulates the bounded profile field and solves its
heights, but calls neither the tread/body builder nor its physical audit. It does
not claim a verified body, terrain, complete route or admitted source network.

The source snapshot contains the actual original case/IDs/attributes, whole axes,
native and DEM priors kept distinct, selected junctions/stairs/protected profiles,
full retained neighbor context, four explicit DEM windows and input receipts.
Required terrain-envelope corners are sampled from the actual declared windows;
the fourth southeast tile cannot disappear simply because the road axes use three.
This preserves terrain input validity; this graph does not rebuild or admit terrain
or the later bridge piers. Existing terrain/support products remain separate.

Numeric payloads are owned immutable bytes-backed arrays, nested descriptors are
read-only, and Shapely geometries are immutable. Complete canonical descriptors
include actual dtype/shape/array SHA256 and geometry WKB, not only an instance ID.
Known source/provenance/neighbor/DEM/selection changes invalidate dependent products.
Definition, instance and product identities remain the common kernel's separate
identities. Operator semantic changes require an explicit version increment;
Python's canonical hashing is not an admitted cross-language cache/hash ABI.

| Exact port type | Actual representation |
|---|---|
| `transport.source-network/1` | Retained JSON provenance/context, binary64 metric XY/height-prior arrays, explicit DEM windows |
| `transport.profile-plan/1` | Binary64 finite-facet layout/heights, integer topology/owners, immutable polygon masks, solver facts and explicitly unconnected outer profiles |
| `transport.checked-body/1` | Actual binary64 closed body/reference envelopes, integer topology/source masks, actual Float32 XYZ export and whole physical facts |

All ports name the same approximate local right-handed ENU frame, origin and
metres. The frame retains the original 111132 / 111320 cos(latitude) conversion.
The vertical label explicitly says **retained source priors, geodetic datum
unverified**; no measured deck or geodetic accuracy is inferred. Incorrect frame,
units, datum and actual array precision are rejected before expensive work.

The body port checks its exact source binding, finite actual array types and
bounded source masks. Existing whole footprint/gradient/tread/underside/clearance,
source-envelope correspondence, all flat landings, protected whole endpoint caps,
closed vertex links and actual Float32 orientation/grades/positive volume remain
mandatory. A matching graph type or successful solver status cannot emit a body
that fails these checks. Wall-clock diagnostics are excluded from deterministic
physical facts; measured distances, topology and work counts remain included.

Controls include a real wrongly selected southern endpoint, missing required DEM,
wrong source/frame/units/datum, disguised Float32 arrays and absent/foreign face
ownership. Without precision constraints, the actual source `1013938082:0` gives
a Double grade about 0.059800 but actual Float32 grade about 0.063152 at a 0.06
limit. The graph rejects that actual body even after a formally valid plan.

The kernel reserves declared output payload bytes before callbacks; these are not
a solver heap/time guarantee. Existing finite arrangement/facet/LP and five-second
solver limits are unchanged; physical readers retain their own finite face/pair/
time bounds. CLI workers also impose 180 CPU seconds and 3 GiB address space;
call them with an external wall timeout. Failure publishes no checked-body product.
This pilot adds no fallback solver; existing valid partial networks remain
independent. A failed local recipe is not a world/frame blocking operation.

The initial local SIGGRAPH reference is Chen et al. 2008,
[Interactive Procedural Street Modeling](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
p2 §3/Figure 2, optically checked again: planning and 3D geometry are distinct
pipeline stages. This paper supplies neither the typed DAG implementation nor a
whole contact/Float32 certificate. The trusted domain procedures and their
physical primary references remain the existing stair/cycle experiments. No SDK
code or dependency is copied; no CityEngine parser or universal grammar is claimed.

## Reproduction and cost scope

With NumPy/SciPy/Shapely and the existing `outshine_ref` environment:

```sh
export PYTHONPATH=lib/src:test/experiments/assets:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest recipe_transport_checks verify_recipe_graph -q
timeout 180s python test/experiments/transport/recipe_transport_probe.py \
  --fixture test/experiments/transport/fixtures/stair-cycle-south-vienna.json \
  --baseline /tmp/outshine-transport-stair-south-237bd38f5 \
  --output /tmp/outshine-transport-recipe-<commit>-fresh --runs 1
```

Use a fresh explicit system-temp output for each worker. The default CLI loads
the 21-source south case; the original cycle and north fixtures supply 19/20
sources with the same two definitions. Run two fresh workers per fixture and
compare definition/instance/product identities, complete semantic descriptors,
actual saved arrays and original checked body/display bindings. A normal test
does not generate or change reference products. Original inputs remain versioned.

`--kind plan` omits all body construction/audit. `--mode direct` calls the same
trusted operators, retaining their physical checks, metadata and content digests,
but omits graph typing, identities and reservations. This measures added wrapper
cost, not an equal-feature evaluator competition or a native speedup. Source
capture, recipe compile, complete evaluation, verification and IO are separated.
An excluded warm diagnostic counts explicit array snapshots and descriptor
`tobytes` payload; JSON/GEOS/solver/internal total copies remain unobserved.
`--profile <temp.bin>` uses existing Memray with Python allocator events/native
stack traces off; requested allocation bytes are not live memory. Normal times
and instrumented phases stay separate. No cache, frame, A18 or RTX claim.

This infrastructure preserves the existing visual products; it adds no visible
geometry. Terrain/support/statics, further sources and seven outer profiles,
complete traffic functionality, lib/native integration and the shared #483
production decision remain open. Master owns the catalogue/context scale study.
