# Isolated recipe graph pilot

[#494](https://github.com/Weichwolf/outshine/issues/494) implements the Buildings
part of [#483](https://github.com/Weichwolf/outshine/issues/483). This is an
experimental Python contract, not a new public `lib/`/native/cache API. Transport
and Vegetation provide their real operators and physical negatives before the
shared production contract is admitted. Master owns the context index/capacity
comparison and native port. Existing visual research remains independent.

## Small callable interface

With `.venv` and `PYTHONPATH=lib/src:test/experiments/assets:<domain-folder>`:

```python
from recipe_graph import Emission, Operator, Prepared, external
from recipe_graph_data import Budget, Files, Key

op = Operator(Key("domain.operation", "1"),
              inputs={"source": "domain.source/1"},
              outputs={"plan": "domain.plan/1"}, apply=trusted_function)
# trusted_function(parameters, inputs) -> {"plan": Emission(value, nbytes, metadata)}
loader = Files({Key("domain.recipe", "1"): definition_path})
recipe = Prepared(Key("domain.recipe", "1"), loader, [op],
                  capabilities=("domain.capability",), budget=Budget())
source = external("domain.source/1", immutable_domain_value,
                  complete_normalized_content_or_digests, accounted_bytes,
                  {"frame_id": "local-frame", "axes": "east,north,up", "units": "m",
                   "height_datum": "explicit-local-datum", "representation": "explicit-format"})
result = recipe.evaluate({"source": source}, outputs=("plan",))
```

`external` hashes all normalized content plus type/metadata. Owners must include
all relevant source geometry, provenance, parameters, neighbors and support
changes; an instance ID alone is insufficient. Payloads are opaque trusted domain
values, never callables loaded from JSON. The core freezes definitions/metadata;
domain owners must snapshot/read-protect their payloads and validate geometry,
units, datums, frame compatibility and actual precision. A string port type alone
cannot certify a physical product. Known byte accounting is owner-declared,
explicitly distinct from total Python/native/solver heap use.

## Loadable data

Actual data: [roof](recipes/buildings-industrial-envelope-v1.json) and
[hall](recipes/buildings-industrial-hall-v1.json). Each file contains exactly:
`schema_version` (string `"1"`), `id`, `version`, `requires`, `inputs`,
`dependencies`, `nodes`, `outputs`. IDs/versions remain strings, including `"01"`;
all data is finite JSON, integers fit exactly in binary64. Duplicate object keys
and unknown fields are rejected. No pickle, callable, script, conditional,
iteration or universal geometry language is interpreted.

`inputs` maps names to exact versioned domain type strings. `dependencies` maps
aliases to `{id, version, inputs}`; child-input bindings name parent external
inputs. Cross-stage composition uses operator-node ports; arbitrary nested recipe
calls/bindings from parent nodes are outside this first pilot. Node fields are
`operator: {id, version}`, literal `parameters`, named `inputs` references and
`max_output_bytes`. References are exactly `{input: name}`, `{node: name,
output: port}` or `{dependency: alias, output: exported_port}`. Exports use the
same reference forms. Trusted registered operators declare exact input/output
ports and return exact `Emission` mappings. Operators must be deterministic and
bump their version for semantic implementation changes; no source-code hash is
inferred. File names have no semantic identity.

The loader is `(Key, remaining_definition_bytes) -> bytes`; `Files` reads at most
that budget plus one byte before decode. It does no directory scan, index search,
source acquisition or cache management. Definition closure is deduplicated by
exact key. All declared dependencies, node references, operator versions/types,
capabilities and DAGs are validated before any domain callback. All compiled
nodes are checked, but only nodes needed by requested exports execute.

`Budget` bounds definition count/bytes, compiled node count, graph depth and the
sum of accounted external inputs plus selected-node output reservations. The
reservation check precedes callbacks; returned accounted bytes must also fit
each declaration. This does not interrupt a solver or cap its internal heap/time.
Domain solvers retain their own cancellation, work limits and valid-part fallback.
Intermediate products live only for this evaluation; no persistent residency or
memoization/cache policy is introduced. No result is published after a failure.

## Identities and reuse

`definition_fingerprint` includes canonical finite data for the complete loaded
exact-version closure. `instance_fingerprint` adds all declared external product
signatures and stays independent of the requested export subset. Each product
fingerprint includes its own definition, operator version/ports, literal params,
actual upstream fingerprints/metadata and output metadata. A child form product
can therefore survive a parent placement/neighbor change. Changing recipe bytes
even under the same version invalidates dependents; authors should issue a new
version. Python canonical JSON hashing is reproducible in this pilot, not an
admitted cross-language floating-point/hash ABI; master must define/test that
encoding before native cache integration.

Actual light, wind phase and game state are absent from static definitions.
Consumers apply such states separately; a render geometry key must include its
relevant state/detail inputs. This prototype does not persist anything or change
#10's asset/cache policy. `Run.stats` reports definitions/bytes, compiled/evaluated
nodes, reservations and accounted emitted bytes, not RSS or frame time.

## Buildings port representations

All spatial products use one named local right-handed ENU frame in metres and
an explicit flat-ground0 datum. This is storage/coordinate convention, not a
survey-precision or native Float32 admission. Ports are exact versioned names:

| Port suffix (prefix `buildings.`, version `/1`) | Value and actual representation |
| --- | --- |
| `source` | Read-only source snapshot, retained attributes/ID, valid2D provider WKB with binary64 coordinates |
| `site` | Stable-ID-sorted read-only neighbor snapshots in the same frame/datum; geometry/height/context changes are fingerprinted |
| `roof-choice` | Existing frozen industrial RoofChoice, metric binary64 descriptors and explicit inference origin |
| `roof-envelope` | HeightfieldEnvelope: XYZ/normals/UV float64, faces uint32, material uint16, parts int32, groups uint32, tags uint8, topology roots uint64 |
| `hall-programme` | Read-protected common facade/room/door/window/ramp plan, metric binary64 values and WKB; opening IDs tied to the same source |
| `hall-shell` | Read-only body: XYZ/normals/UV/facade extent float64, faces int32, material uint16; PrismCut topology roots int64 |

Envelope/shell array types, stream dimensions and finite attributes are checked
before emitting these ports. A deliberately downcast Float32 body is rejected;
this does not certify a later Float32 export. Existing physical source/roof,
actual passage/support and negative checks remain necessary. Full source bundles
conservatively invalidate within their dependent stage; fine-grained field
projection and production cache reuse policy remain outside this pilot.

## Real Buildings consumer and reproduction

`../buildings/recipe_hall_operators.py` declares source/site, roof-choice,
roof-envelope, hall-programme and hall-shell ports. All use the same explicit
local ENU frame/metres/flat-ground datum. Provider WKB/height/color/context and
stable IDs are retained; neighbor inputs are sorted and snapshotted. Products
name binary64 descriptors or actual float64 meshes/topology roots. Material1 is
the stable wall slot; source palette binding happens after generation. The graph
uses the existing industrial roof/facade/entrance/CSG functions, not a replacement
geometry solver. Programme-only requests generate no roof mesh/heightfield/CSG;
full output retains the same source-bound simple hall. Tor/personnel state is
applied afterward and does not mutate the plan or regenerate its definitions.

Run `python -m unittest verify_recipe_graph verify_recipe_halls -q` with the paths
above plus `test/experiments/buildings`. After freezing source,
`recipe_hall_probe.py --output <system-temp> --render-inputs <existing-hall-inputs>`
checks fresh-process replay, actual prior render-input arrays/dtypes and scoped
21-run direct-versus-graph overhead on three actual provider halls. Direct calls
omit graph typing/fingerprints; this measures their added cost, not an equal-
feature evaluator contest or a claimed native speedup. Existing render images
are references only after actual input equivalence; no image improvement is
claimed from this infrastructure. SIGGRAPH2006 Procedural Modeling of Buildings
p4 was optically inspected: source-bound solids, facade scopes and occlusion
remain domain operations. The existing Esri/Ghidra primitive evidence is #489;
no Esri parser/VM/source implementation is claimed for this own portable pilot.

## Measured pilot5bf2fc32b

62 source controls and the same62 controls with the freshly installed unchanged
reference wheel pass, no skips. All82 building dependency modules match the
pinned wheel (`aac088cf20ef7b05728ba62cdc57d85735fccb92a71e4650d9085d3d4d3315b6`).
The additional source-height invalidation/stable-room-ID control passes with
both dependency modes, giving63 distinct checked controls. The pilot itself
remains experiment code, not a packaged API. Three real halls
replay exactly in two fresh processes each: canonical definitions/instances/
product fingerprints/plans/stats and actual shell arrays/dtypes. Eight existing
hall/state/interior/opposite render inputs are exactly preserved, including the
same source palette binding. The shared facade-axis extraction also reproduces
all15 preceding whole/near/far/tile input streams/dtypes/cameras/materials. Those
images were already optically inspected in #493; no new image or visual gain is
claimed. The actual main open-gate image was re-opened against this equivalence.

The selected closure loads2 definitions/2291bytes and compiles4 nodes. Programme
requests execute2 callbacks, no roof mesh/heightfield/CSG; complete products run4.
Across the three cases, returned accounted data is21387/15538/22593bytes for
plans versus789248/438551/803390bytes for complete exports. Reservations remain
conservative196608 versus5439488bytes, plus the declared external input bytes.

21-run shared-host medians (2 warmups) for plans: direct25.315/35.383/30.404ms,
graph28.296/22.702/32.859ms; complete: direct102.596/69.939/106.116ms,
graph107.742/69.065/111.228ms. No stable speed advantage is inferred; the direct
control omits graph typing/fingerprints and the host load is uncontrolled. The
proved saving is avoiding unrequested operators/products. Separate traced peaks
are in the reproducible report, excluding native allocations. Fresh-process
prepare2.155–3.767ms and first evaluation67.969–137.358ms; process RSS high-water
before evaluation90.85–92.88MB, afterward96.35–97.90MB (decimal MB, imports
included). These are process high-water observations, not per-operator heap
allocation or native/device/frame bounds. Known final buffer writes/live traced
allocations and work/byte counts remain scoped as described in the probe.
