# Native planar transport repairs

Owner: [#1189](https://github.com/Weichwolf/outshine/issues/1189), parent #1003.
The actual MVT stair/path fixture and unchanged Python layout supply one shared
pre-noded, triangulated XY input. This is a native weld/conformance consumer,
not a native complete transport solver or a repaired runtime Place.

`ConformPlanarMesh` welds connected proximity components to their first input
representative, rejects excessive transitive drift, removes zero-area faces,
and splits edges at existing vertices. Shared `PlanHierarchy` queries bound
candidate work. Input/output sizes and candidates have explicit budgets;
stop requests cannot publish a partial result. Worst-case BVH traversal is not
claimed linear or globally optimal. Traversed nodes and allocator traffic are
not instrumented by the candidate counters.

`PointRemap` maps input vertices to output vertices. `FaceParents` retains each
output triangle's original face. Shared XY never licenses joining independent
layers or heights: the subsequent generator keeps `(source, vertex)` variables
separate until an explicit qualified connection. Numerical edge tolerance is
not a physical height, surface or screen-space error bound.

```sh
make suite SUITE=outshine/src/base/spatial/PlanarMesh
make db
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 "$PYTHON" \
  -B -W error test/experiments/transport/native_planar_mesh.py \
  --output "${TMPDIR:-/tmp}/outshine-planar-native-<fresh-name>"
```

The existing Python environment needs NumPy/SciPy/Shapely. The driver compiles
four native units and its small probe using complete current compilation-database
flags; only source/object/dependency outputs change. The standalone CPU probe
links its actual math dependencies with the compiler's standard library.
`--binary` can reuse a previously built matching probe. Builds run at nice10;
measurement children retain the caller's priority.

The original repair, three identity-preserving rigid controls, original point
words/index sequences/remap and independent hanging-edge checks must agree.
The ablation leaves existing coverage/footprint checks active: area equality alone
can accept hanging nodes. Native timings cover only repair; Python timings cover
the same phases. Input/formatting/build/IO, noding, height fit, ownership admission,
3D bodies, navigation, terrain, GPU and whole-world budgets remain separate.
Allocation counts, copied bytes and process peak remain unmeasured. Observed timings
and terminal results belong to the issue/commit receipt, not this method contract.
